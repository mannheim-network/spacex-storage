#include "OCalls.h"

spacex::Log *p_log = spacex::Log::get_instance();
std::map<uint32_t, uint8_t *> g_ocall_buffer_pool;
std::mutex g_ocall_buffer_pool_mutex;
std::map<ocall_store_type_t, ocall_store2_f> g_ocall_store2_func_m = {
    {OCALL_FILE_INFO_ALL, ocall_store_file_info_all},
    {OCALL_STORE_WORKREPORT, ocall_store_workreport},
    {OCALL_STORE_UPGRADE_DATA, ocall_store_upgrade_data},
};

// Used to store ocall file data
uint8_t *ocall_file_data = NULL;
size_t ocall_file_data_len = 0;
// Used to validation websocket client
WebsocketClient *wssclient = NULL;

/**
 * @description: ocall for printing string
 * @param str (in) -> string for printing
 */
void ocall_print_info(const char *str)
{
    printf("%s", str);
}

/**
 * @description: ocall for printing string
 * @param str (in) -> string for printing
 */
void ocall_print_debug(const char *str)
{
    if (p_log->get_debug_flag())
    {
        printf("%s", str);
    }
}

/**
 * @description: ocall for log information
 * @param str (in) -> string for printing
 */
void ocall_log_info(const char *str)
{
    p_log->info("[Enclave] %s", str);
}

/**
 * @description: ocall for log warnings
 * @param str (in) -> string for printing
 */
void ocall_log_warn(const char *str)
{
    p_log->warn("[Enclave] %s", str);
}

/**
 * @description: ocall for log errors
 * @param str (in) -> string for printing
 */
void ocall_log_err(const char *str)
{
    p_log->err("[Enclave] %s", str);
}

/**
 * @description: ocall for log debugs
 * @param str (in) -> string for printing
 */
void ocall_log_debug(const char *str)
{
    p_log->debug("[Enclave] %s", str);
}

/**
 * @description: ocall for wait
 * @param u -> microsecond
 */
void ocall_usleep(int u)
{
    usleep(u);
}

/**
 * @description: Free app buffer
 * @param value (in) -> Pointer points to pointer to value
 * @return: Get status
 */
spacex_status_t ocall_free_outer_buffer(uint8_t **value)
{
    if(*value != NULL)
    {
        free(*value);
        *value = NULL;
    }
    
    return SPACEX_SUCCESS;
}

/**
 * @description: Get block hash by height
 * @param block_height -> Block height from enclave
 * @param block_hash (in) -> Pointer to got block hash
 * @param hash_size -> Block hash size
 * @return: Get result
 */
spacex_status_t ocall_get_block_hash(size_t block_height, char *block_hash, size_t hash_size)
{
    std::string hash = spacex::Chain::get_instance()->get_block_hash(block_height);

    if (hash.compare("") == 0)
    {
        return SPACEX_UPGRADE_GET_BLOCK_HASH_FAILED;
    }

    memcpy(block_hash, hash.c_str(), hash_size);

    return SPACEX_SUCCESS;
}

/**
 * @description: For upgrade, send work report
 * @return: Send result
 */
spacex_status_t ocall_upload_workreport()
{
    std::string work_str = EnclaveData::get_instance()->get_workreport();
    remove_char(work_str, '\\');
    remove_char(work_str, '\n');
    remove_char(work_str, ' ');
    p_log->info("Sending work report:%s\n", work_str.c_str());
    
    if (!spacex::Chain::get_instance()->post_sworker_work_report(work_str))
    {
        p_log->err("Send work report to spacex chain failed!\n");
        return SPACEX_UPGRADE_SEND_WORKREPORT_FAILED;
    }

    p_log->info("Send work report to spacex chain successfully!\n");

    return SPACEX_SUCCESS;
}

/**
 * @description: Entry network
 * @return: Entry result
 */
spacex_status_t ocall_entry_network()
{
    return entry_network();
}

/**
 * @description: Do srd in this function
 * @param change -> The change number will be committed this turn
 * @return: Srd change return status
 */
spacex_status_t ocall_srd_change(long change)
{
    return srd_change(change);
}

/**
 * @description: Store storage identity
 * @param id (in) -> Pointer to identity
 * @return: Upload result
 */
spacex_status_t ocall_upload_identity(const char *id)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    json::JSON entrance_info = json::JSON::Load(&spacex_status, std::string(id));
    if (SPACEX_SUCCESS != spacex_status)
    {
        p_log->err("Parse identity failed! Error code:%lx\n", spacex_status);
        return spacex_status;
    }
    entrance_info["account_id"] = Config::get_instance()->chain_address;
    std::string sworker_identity = entrance_info.dump();
    p_log->info("Generate identity successfully! Sworker identity: %s\n", sworker_identity.c_str());

    // Send identity to spacex chain
    if (!spacex::Chain::get_instance()->wait_for_running())
    {
        return SPACEX_UNEXPECTED_ERROR;
    }

    if (!spacex::Chain::get_instance()->post_sworker_identity(sworker_identity))
    {
        p_log->err("Send identity to spacex chain failed!\n");
        return SPACEX_UNEXPECTED_ERROR;
    }
    p_log->info("Send identity to spacex chain successfully!\n");

    return SPACEX_SUCCESS;
}

/**
 * @description: Store enclave id information
 * @param info (in) -> Pointer to enclave id information
 */
void ocall_store_enclave_id_info(const char *info)
{
    EnclaveData::get_instance()->set_enclave_id_info(info);
}

/**
 * @description: Store upgrade data
 * @param data (in) -> Upgrade data
 * @param data_size -> Upgrade data size
 * @return: Store status 
 */
spacex_status_t ocall_store_upgrade_data(const uint8_t *data, size_t data_size)
{
    EnclaveData::get_instance()->set_upgrade_data(std::string(reinterpret_cast<const char *>(data), data_size));
    return SPACEX_SUCCESS;
}

/**
 * @description: Get chain block information
 * @param data (in, out) -> Pointer to file block information
 * @param data_size -> Pointer to file block data size
 * @param real_size (out) -> Pointer to real file size
 * @return: Get result
 */
spacex_status_t ocall_chain_get_block_info(uint8_t *data, size_t data_size, size_t *real_size)
{
    spacex::BlockHeader block_header;
    if (!spacex::Chain::get_instance()->get_block_header(block_header))
    {
        return SPACEX_UNEXPECTED_ERROR;
    }

    json::JSON bh_json;
    bh_json[CHAIN_BLOCK_NUMBER] = block_header.number;
    bh_json[CHAIN_BLOCK_HASH] = block_header.hash;

    std::string bh_str = bh_json.dump();
    remove_char(bh_str, '\n');
    remove_char(bh_str, '\\');
    remove_char(bh_str, ' ');

    *real_size = bh_str.size();
    if (*real_size > data_size)
    {
        return SPACEX_OCALL_NO_ENOUGH_BUFFER;
    }

    memcpy(data, bh_str.c_str(), bh_str.size());

    return SPACEX_SUCCESS;
}

/**
 * @description: Store file information
 * @param cid (in) -> File content identity
 * @param data (in) -> File information data
 * @param type (in) -> File information type
 */
void ocall_store_file_info(const char* cid, const char *data, const char *type)
{
    EnclaveData::get_instance()->add_file_info(cid, type, data);
}

/**
 * @description: Restore sealed file information
 * @param data (in) -> All file information
 * @param data_size -> All file information size
 * @return: Store status
 */
spacex_status_t ocall_store_file_info_all(const uint8_t *data, size_t data_size)
{
    EnclaveData::get_instance()->restore_file_info(data, data_size);
    return SPACEX_SUCCESS;
}

/**
 * @description: Store workreport
 * @param data (in) -> Pointer to workreport data
 * @param data_size -> Workreport data size
 * @return: Store status
 */
spacex_status_t ocall_store_workreport(const uint8_t *data, size_t data_size)
{
    EnclaveData::get_instance()->set_workreport(data, data_size);
    return SPACEX_SUCCESS;
}

/**
 * @description: Ocall save big data
 * @param t -> Store function type
 * @param data (in) -> Pointer to data
 * @param total_size -> Total data size
 * @param partial_size -> Current store data size
 * @param offset -> Offset in total data
 * @param buffer_key -> Session key for this time enclave data store
 * @return: Store result
 */
spacex_status_t ocall_safe_store2(ocall_store_type_t t, const uint8_t *data, size_t total_size, size_t partial_size, size_t offset, uint32_t buffer_key)
{
    SafeLock sl(g_ocall_buffer_pool_mutex);
    sl.lock();
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    bool is_end = true;
    if (offset < total_size)
    {
        uint8_t *buffer = NULL;
        if (g_ocall_buffer_pool.find(buffer_key) != g_ocall_buffer_pool.end())
        {
            buffer = g_ocall_buffer_pool[buffer_key];
        }
        if (buffer == NULL)
        {
            buffer = (uint8_t *)malloc(total_size);
            if (buffer == NULL)
            {
                spacex_status = SPACEX_MALLOC_FAILED;
                goto cleanup;
            }
            memset(buffer, 0, total_size);
            g_ocall_buffer_pool[buffer_key] = buffer;
        }
        memcpy(buffer + offset, data, partial_size);
        if (offset + partial_size < total_size)
        {
            is_end = false;
        }
    }

    if (!is_end)
    {
        return SPACEX_SUCCESS;
    }

    spacex_status = (g_ocall_store2_func_m[t])(g_ocall_buffer_pool[buffer_key], total_size);

cleanup:

    if (g_ocall_buffer_pool.find(buffer_key) != g_ocall_buffer_pool.end())
    {
        free(g_ocall_buffer_pool[buffer_key]);
        g_ocall_buffer_pool.erase(buffer_key);
    }

    return spacex_status;
}

/**
 * @description: Recall validate meaningful files
 */
void ocall_recall_validate_file()
{
    Validator::get_instance()->validate_file();
}

/**
 * @description: Recall validate srd
 */
void ocall_recall_validate_srd()
{
    Validator::get_instance()->validate_srd();
}

/**
 * @description: Change sealed file info from old type to new type
 * @param cid (in) -> File root cid
 * @param old_type (in) -> Old file type
 * @param new_type (in) -> New file type
 */
void ocall_change_file_type(const char *cid, const char *old_type, const char *new_type)
{
    EnclaveData::get_instance()->change_file_type(cid, old_type, new_type);
}

/**
 * @description: Delete cid by type
 * @param cid (in) -> File root cid
 * @param type (in) -> File type
 */
void ocall_delete_file_info(const char *cid, const char *type)
{
    EnclaveData::get_instance()->del_file_info(cid, type);
}
