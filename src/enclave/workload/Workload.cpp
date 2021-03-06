#include "Workload.h"

sgx_thread_mutex_t g_workload_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t g_report_flag_mutex = SGX_THREAD_MUTEX_INITIALIZER;
sgx_thread_mutex_t g_upgrade_status_mutex = SGX_THREAD_MUTEX_INITIALIZER;

Workload *Workload::workload = NULL;

/**
 * @description: Single instance class function to get instance
 * @return: Workload instance
 */
Workload *Workload::get_instance()
{
    if (Workload::workload == NULL)
    {
        sgx_thread_mutex_lock(&g_workload_mutex);
        if (Workload::workload == NULL)
        {
            Workload::workload = new Workload();
        }
        sgx_thread_mutex_unlock(&g_workload_mutex);
    }

    return Workload::workload;
}

/**
 * @description: Initialize workload
 */
Workload::Workload()
{
    this->report_files = true;
    for (auto item : g_file_spec_status)
    {
        this->wl_file_spec[item.second]["num"] = 0;
        this->wl_file_spec[item.second]["size"] = 0;
    }
}

/**
 * @description: Destructor
 */
Workload::~Workload()
{
    for (auto g_hash : this->srd_hashs)
    {
        if (g_hash != NULL)
            free(g_hash);
    }
    this->srd_hashs.clear();
}

/**
 * @description: Clean up work report data
 */
void Workload::clean_srd()
{
    for (auto g_hash : this->srd_hashs)
    {
        if (g_hash != NULL)
            free(g_hash);
    }
    this->srd_hashs.clear();

    // Clean srd info json
    this->srd_info_json = json::Object();
    std::string srd_info_str = this->srd_info_json.dump();
    ocall_set_srd_info(reinterpret_cast<const uint8_t *>(srd_info_str.c_str()), srd_info_str.size());
}

/**
 * @description: Generate workload info
 * @param status -> Pointer to result status
 * @return: Workload info in json format
 */
json::JSON Workload::gen_workload_info(spacex_status_t *status)
{
    *status = SPACEX_SUCCESS;

    // Generate srd information
    sgx_sha256_hash_t srd_root;
    json::JSON ans;
    SafeLock sl_srd(this->srd_mutex);
    sl_srd.lock();
    if (this->srd_hashs.size() == 0)
    {
        memset(&srd_root, 0, sizeof(sgx_sha256_hash_t));
        ans[WL_SRD_ROOT_HASH] = reinterpret_cast<uint8_t *>(&srd_root);
    }
    else
    {
        size_t srds_data_sz = this->srd_hashs.size() * SRD_LENGTH;
        uint8_t *srds_data = (uint8_t *)enc_malloc(srds_data_sz);
        if (srds_data == NULL)
        {
            log_err("Generate srd info failed due to malloc failed!\n");
            *status = SPACEX_MALLOC_FAILED;
            return ans;
        }
        Defer Def_srds_data([&srds_data](void) { free(srds_data); });
        memset(srds_data, 0, srds_data_sz);
        for (size_t i = 0; i < this->srd_hashs.size(); i++)
        {
            memcpy(srds_data + i * SRD_LENGTH, this->srd_hashs[i], SRD_LENGTH);
        }
        ans[WL_SRD_SPACE] = this->srd_hashs.size() * 1024 * 1024 * 1024;
        sgx_sha256_msg(srds_data, (uint32_t)srds_data_sz, &srd_root);
        ans[WL_SRD_ROOT_HASH] = reinterpret_cast<uint8_t *>(&srd_root);
    }
    sl_srd.unlock();

    // Generate file information
    sgx_sha256_hash_t file_root;
    SafeLock sl_file(this->file_mutex);
    sl_file.lock();
    if (this->sealed_files.size() == 0)
    {
        memset(&file_root, 0, sizeof(sgx_sha256_hash_t));
        ans[WL_FILE_ROOT_HASH] = reinterpret_cast<uint8_t *>(&file_root);
    }
    else
    {
        std::vector<uint8_t> file_buffer;
        for (size_t i = 0; i < this->sealed_files.size(); i++)
        {
            // Do not calculate pending status file
            if (FILE_STATUS_PENDING == this->sealed_files[i][FILE_STATUS].get_char(CURRENT_STATUS))
            {
                continue;
            }
            *status = vector_end_insert(file_buffer, this->sealed_files[i][FILE_HASH].ToBytes(), HASH_LENGTH);
            if (SPACEX_SUCCESS != *status)
            {
                return ans;
            }
        }
        sgx_sha256_msg(file_buffer.data(), file_buffer.size(), &file_root);
        ans[WL_FILE_ROOT_HASH] = reinterpret_cast<uint8_t *>(&file_root);
    }
    sl_file.unlock();

    return ans;
}

/**
 * @description: Serialize workload for sealing
 * @return: Serialized workload
 */
json::JSON Workload::serialize_srd()
{
    sgx_thread_mutex_lock(&this->srd_mutex);
    std::vector<uint8_t *> srd_hashs;
    srd_hashs.insert(srd_hashs.end(), this->srd_hashs.begin(), this->srd_hashs.end());
    sgx_thread_mutex_unlock(&this->srd_mutex);

    // Get srd buffer
    json::JSON srd_buffer = json::Array();
    for (size_t i = 0; i < srd_hashs.size(); i++)
    {
        srd_buffer.append(hexstring_safe(srd_hashs[i], SRD_LENGTH));
    }

    return srd_buffer;
}

/**
 * @description: Serialize workload for sealing
 * @param status -> Pointer to result status
 * @return: Serialized workload
 */
json::JSON Workload::get_upgrade_srd_info(spacex_status_t *status)
{
    SafeLock sl(this->srd_mutex);
    sl.lock();

    *status = SPACEX_SUCCESS;
    json::JSON srd_buffer;

    // Get srd root hash
    sgx_sha256_hash_t srd_root;
    if (this->srd_hashs.size() == 0)
    {
        memset(&srd_root, 0, sizeof(sgx_sha256_hash_t));
    }
    else
    {
        size_t srds_data_sz = this->srd_hashs.size() * SRD_LENGTH;
        uint8_t *srds_data = (uint8_t *)enc_malloc(srds_data_sz);
        if (srds_data == NULL)
        {
            log_err("Generate srd info failed due to malloc failed!\n");
            *status = SPACEX_MALLOC_FAILED;
            return srd_buffer;
        }
        Defer Def_srds_data([&srds_data](void) { free(srds_data); });
        memset(srds_data, 0, srds_data_sz);
        for (size_t i = 0; i < this->srd_hashs.size(); i++)
        {
            memcpy(srds_data + i * SRD_LENGTH, this->srd_hashs[i], SRD_LENGTH);
        }
        sgx_sha256_msg(srds_data, (uint32_t)srds_data_sz, &srd_root);
    }
    srd_buffer[WL_SRD_ROOT_HASH] = (uint8_t *)&srd_root;
    log_debug("Generate srd root hash successfully!\n");

    // Get srd buffer
    srd_buffer[WL_SRD] = json::Array();
    for (size_t i = 0; i < this->srd_hashs.size(); i++)
    {
        srd_buffer[WL_SRD].append(hexstring_safe(this->srd_hashs[i], SRD_LENGTH));
    }
    
    return srd_buffer;
}

/**
 * @description: Serialize file for sealing
 * @return: Serialized result
 */
json::JSON Workload::serialize_file()
{
    sgx_thread_mutex_lock(&this->file_mutex);
    std::vector<json::JSON> tmp_sealed_files;
    tmp_sealed_files.insert(tmp_sealed_files.end(), this->sealed_files.begin(), this->sealed_files.end());
    sgx_thread_mutex_unlock(&this->file_mutex);

    // Get file buffer
    json::JSON file_buffer = json::Array();
    for (size_t i = 0; i < tmp_sealed_files.size(); i++)
    {
        if (FILE_STATUS_PENDING == tmp_sealed_files[i][FILE_STATUS].get_char(CURRENT_STATUS))
        {
            json::JSON file;
            file[FILE_CID] = tmp_sealed_files[i][FILE_CID].ToString();
            file[FILE_STATUS] = tmp_sealed_files[i][FILE_STATUS].ToString();
            tmp_sealed_files[i] = file;
        }
        file_buffer.append(tmp_sealed_files[i]);
    }

    return file_buffer;
}

/**
 * @description: Serialize file for sealing
 * @param status -> Pointer to result status
 * @return: Serialized result
 */
json::JSON Workload::get_upgrade_file_info(spacex_status_t *status)
{
    SafeLock sl(this->file_mutex);
    sl.lock();

    *status = SPACEX_SUCCESS;
    json::JSON file_buffer;
    // Generate file information
    sgx_sha256_hash_t file_root;
    if (this->sealed_files.size() == 0)
    {
        memset(&file_root, 0, sizeof(sgx_sha256_hash_t));
    }
    else
    {
        std::vector<uint8_t> file_hashs;
        for (size_t i = 0; i < this->sealed_files.size(); i++)
        {
            if (FILE_STATUS_PENDING == this->sealed_files[i][FILE_STATUS].get_char(CURRENT_STATUS))
            {
                continue;
            }
            if (SPACEX_SUCCESS != (*status = vector_end_insert(file_hashs, this->sealed_files[i][FILE_HASH].ToBytes(), HASH_LENGTH)))
            {
                return file_buffer;
            }
        }
        sgx_sha256_msg(file_hashs.data(), file_hashs.size(), &file_root);
    }
    file_buffer[WL_FILE_ROOT_HASH] = (uint8_t *)&file_root;
    log_debug("Generate file root hash successfully!\n");

    // Get file buffer
    file_buffer[WL_FILES] = json::Array();
    for (size_t i = 0; i < this->sealed_files.size(); i++)
    {
        if (FILE_STATUS_PENDING == this->sealed_files[i][FILE_STATUS].get_char(CURRENT_STATUS))
        {
            continue;
        }
        file_buffer[WL_FILES].append(this->sealed_files[i]);
    }

    return file_buffer;
}

/**
 * @description: Restore workload from serialized workload
 * @param g_hashs -> G hashs json data
 * @return: Restore status
 */
spacex_status_t Workload::restore_srd(json::JSON &g_hashs)
{
    if (g_hashs.JSONType() != json::JSON::Class::Array)
        return SPACEX_BAD_SEAL_DATA;

    if (g_hashs.size() == 0)
        return SPACEX_SUCCESS;

    spacex_status_t spacex_status = SPACEX_SUCCESS;
    this->clean_srd();
    
    // Restore srd_hashs
    for (auto it : g_hashs.ArrayRange())
    {
        std::string hex_g_hash = it.ToString();
        uint8_t *g_hash = hex_string_to_bytes(hex_g_hash.c_str(), hex_g_hash.size());
        if (g_hash == NULL)
        {
            this->clean_srd();
            return SPACEX_UNEXPECTED_ERROR;
        }
        this->srd_hashs.push_back(g_hash);

        // Restore srd detail
        std::string uuid = hex_g_hash.substr(0, UUID_LENGTH * 2);
        this->srd_info_json[WL_SRD_DETAIL][uuid].AddNum(1);
    }

    // Restore srd info
    this->srd_info_json[WL_SRD_COMPLETE] = this->srd_hashs.size();

    // Restore srd info
    std::string srd_info_str = this->get_srd_info().dump();
    ocall_set_srd_info(reinterpret_cast<const uint8_t *>(srd_info_str.c_str()), srd_info_str.size());

    return spacex_status;
}

/**
 * @description: Clean file related data
 */
void Workload::clean_file()
{
    // Clean sealed files
    this->sealed_files.clear();

    // Clean workload spec info
    this->wl_file_spec = json::JSON();

    // Clean file info
    safe_ocall_store2(OCALL_FILE_INFO_ALL, reinterpret_cast<const uint8_t *>("{}"), 2);
}

/**
 * @description: Restore file from json
 * @param file_json -> File json
 * @return: Restore file result
 */
spacex_status_t Workload::restore_file(json::JSON &file_json)
{
    if (file_json.JSONType() != json::JSON::Class::Array)
        return SPACEX_BAD_SEAL_DATA;

    if (file_json.size() == 0)
        return SPACEX_SUCCESS;

    this->sealed_files.clear();
    for (int i = 0; i < file_json.size(); i++)
    {
        char s = file_json[i][FILE_STATUS].get_char(CURRENT_STATUS);
        if (FILE_STATUS_PENDING == s)
        {
            spacex_status_t del_ret = SPACEX_SUCCESS;
            std::string cid = file_json[i][FILE_CID].ToString();
            // Delete file directory
            ocall_delete_ipfs_file(&del_ret, cid.c_str());
            // Delete file tree structure
            persist_del(cid);
        }
        else
        {
            this->sealed_files.push_back(file_json[i]);
            // Restore workload spec info
            this->set_file_spec(s, file_json[i][FILE_SIZE].ToInt());
        }
    }

    // Restore file information
    this->restore_file_info();

    return SPACEX_SUCCESS;
}

/**
 * @description: Set report file flag
 * @param flag -> Report flag
 */
void Workload::set_report_file_flag(bool flag)
{
    sgx_thread_mutex_lock(&g_report_flag_mutex);
    this->report_files = flag;
    sgx_thread_mutex_unlock(&g_report_flag_mutex);
}

/**
 * @description: Get report flag
 * @return: Report flag
 */
bool Workload::get_report_file_flag()
{
    SafeLock sl(g_report_flag_mutex);
    sl.lock();
    return this->report_files;
}

/**
 * @description: Set srd remaining task
 * @param num -> Srd remaining task number
 */
void Workload::set_srd_remaining_task(long num)
{
    sgx_thread_mutex_lock(&this->srd_info_mutex);
    this->srd_info_json[WL_SRD_REMAINING_TASK] = num;
    sgx_thread_mutex_unlock(&this->srd_info_mutex);
}

/**
 * @description: Set srd info
 * @param uuid -> Disk path uuid
 * @param change -> Change number
 */
void Workload::set_srd_info(const char *uuid, long change)
{
    std::string uuid_str(uuid);
    sgx_thread_mutex_lock(&this->srd_info_mutex);
    this->srd_info_json[WL_SRD_COMPLETE].AddNum(change);
    if (this->srd_info_json[WL_SRD_COMPLETE].ToInt() <= 0)
    {
        this->srd_info_json[WL_SRD_COMPLETE] = 0;
    }
    this->srd_info_json[WL_SRD_DETAIL][uuid_str].AddNum(change);
    sgx_thread_mutex_unlock(&this->srd_info_mutex);
}

/**
 * @description: Get srd info
 * @return: Return srd info json
 */
json::JSON Workload::get_srd_info()
{
    sgx_thread_mutex_lock(&this->srd_info_mutex);
    if (this->srd_info_json.size() <= 0)
    {
        this->srd_info_json = json::Object();
    }
    json::JSON srd_info = this->srd_info_json;
    sgx_thread_mutex_unlock(&this->srd_info_mutex);

    return srd_info;
}

/**
 * @description: Set upgrade flag
 * @param pub_key -> Previous version's public key
 */
void Workload::set_upgrade(sgx_ec256_public_t pub_key)
{
    this->upgrade = true;
    memcpy(&this->pre_pub_key, &pub_key, sizeof(sgx_ec256_public_t));
}

/**
 * @description: Unset upgrade
 */
void Workload::unset_upgrade()
{
    this->upgrade = false;
}

/**
 * @description: Restore previous public key
 * @param meta -> Reference to metadata
 * @return: Restore result
 */
spacex_status_t Workload::restore_pre_pub_key(json::JSON &meta)
{
    if (!meta.hasKey(ID_PRE_PUB_KEY))
    {
        return SPACEX_SUCCESS;
    }

    sgx_ec256_public_t pre_pub_key;
    std::string pre_pub_key_str = meta[ID_PRE_PUB_KEY].ToString();
    uint8_t *pre_pub_key_u = hex_string_to_bytes(pre_pub_key_str.c_str(), pre_pub_key_str.size());
    if (pre_pub_key_u == NULL)
    {
        return SPACEX_UNEXPECTED_ERROR;
    }
    memcpy(&pre_pub_key, pre_pub_key_u, sizeof(sgx_ec256_public_t));
    free(pre_pub_key_u);
    this->set_upgrade(pre_pub_key);

    return SPACEX_SUCCESS;
}

/**
 * @description: Get upgrade flag
 * @return: Upgrade flag
 */
bool Workload::is_upgrade()
{
    return this->upgrade;
}

/**
 * @description: Set is_upgrading flag
 * @param status -> Enclave upgrade status
 */
void Workload::set_upgrade_status(enc_upgrade_status_t status)
{
    sgx_thread_mutex_lock(&g_upgrade_status_mutex);
    this->upgrade_status = status;
    switch (status)
    {
        case ENC_UPGRADE_STATUS_NONE:
            log_info("Set upgrade status:ENC_UPGRADE_STATUS_NONE\n");
            break;
        case ENC_UPGRADE_STATUS_PROCESS:
            log_info("Set upgrade status:ENC_UPGRADE_STATUS_PROCESS\n");
            break;
        case ENC_UPGRADE_STATUS_SUCCESS:
            log_info("Set upgrade status:ENC_UPGRADE_STATUS_SUCCESS\n");
            break;
        default:
            log_info("Unknown upgrade status!\n");
    }
    sgx_thread_mutex_unlock(&g_upgrade_status_mutex);
}

/**
 * @description: Get is_upgrading flag
 * @return: Enclave upgrade status
 */
enc_upgrade_status_t Workload::get_upgrade_status()
{
    SafeLock sl(g_upgrade_status_mutex);
    sl.lock();
    return this->upgrade_status;
}

/**
 * @description: Handle workreport result
 */
void Workload::handle_report_result()
{
    bool handled = false;
    // Set file status by report result
    sgx_thread_mutex_lock(&this->file_mutex);
    for (auto cid : this->reported_files_idx)
    {
        size_t pos = 0;
        if (this->is_file_dup_nolock(cid, pos))
        {
            auto status = &this->sealed_files[pos][FILE_STATUS];
            status->set_char(ORIGIN_STATUS, status->get_char(WAITING_STATUS));
            handled = true;
        }
    }
    this->reported_files_idx.clear();
    sgx_thread_mutex_unlock(&this->file_mutex);

    // Store changed metadata
    if (handled)
    {
        id_store_metadata();
    }
}

/**
 * @description: Check if can report workreport
 * @param block_height -> Block height
 * @return: Check status
 */
spacex_status_t Workload::can_report_work(size_t block_height)
{
    if (block_height == 0 || block_height < REPORT_SLOT + this->get_report_height() + REPORT_INTERVAL_BLCOK_NUMBER_LOWER_LIMIT)
    {
        return SPACEX_UPGRADE_BLOCK_EXPIRE;
    }

    if (!this->report_has_validated_proof())
    {
        return SPACEX_UPGRADE_NO_VALIDATE;
    }

    if (this->get_restart_flag())
    {
        return SPACEX_UPGRADE_RESTART;
    }

    return SPACEX_SUCCESS;
}

/**
 * @description: Set workload spec information
 * @param file_status -> Workload spec
 * @param change -> Spec information change
 */
void Workload::set_file_spec(char file_status, long long change)
{
    if (g_file_spec_status.find(file_status) != g_file_spec_status.end())
    {
        sgx_thread_mutex_lock(&wl_file_spec_mutex);
        std::string ws_name = g_file_spec_status[file_status];
        this->wl_file_spec[ws_name]["num"].AddNum(change > 0 ? 1 : -1);
        this->wl_file_spec[ws_name]["size"].AddNum(change);
        sgx_thread_mutex_unlock(&wl_file_spec_mutex);
    }
}

/**
 * @description: Get workload spec info reference
 * @return: Const reference to wl_file_spec
 */
const json::JSON &Workload::get_file_spec()
{
    SafeLock sl(wl_file_spec_mutex);
    sl.lock();
    return this->wl_file_spec;
}

/**
 * @description: Set chain account id
 * @param account_id -> Chain account id
 */
void Workload::set_account_id(std::string account_id)
{
    this->account_id = account_id;
}

/**
 * @description: Get chain account id
 * @return: Chain account id
 */
std::string Workload::get_account_id()
{
    return this->account_id;
}

/**
 * @description: Can get key pair or not
 * @return: Get result
 */
bool Workload::try_get_key_pair()
{
    return this->is_set_key_pair;
}

/**
 * @description: Get public key
 * @return: Const reference to public key
 */
const sgx_ec256_public_t &Workload::get_pub_key()
{
    return this->id_key_pair.pub_key;
}

/**
 * @description: Get private key
 * @return: Const reference to private key
 */
const sgx_ec256_private_t &Workload::get_pri_key()
{
    return this->id_key_pair.pri_key;
}

/**
 * @description: Set identity key pair
 * @param id_key_pair -> Identity key pair
 */
void Workload::set_key_pair(ecc_key_pair id_key_pair)
{
    memcpy(&this->id_key_pair.pub_key, &id_key_pair.pub_key, sizeof(sgx_ec256_public_t));
    memcpy(&this->id_key_pair.pri_key, &id_key_pair.pri_key, sizeof(sgx_ec256_private_t));
    this->is_set_key_pair = true;
}

/**
 * @description: Unset id key pair
 */
void Workload::unset_key_pair()
{
    this->is_set_key_pair = false;
}

/**
 * @description: Get identity key pair
 * @return: Const reference to identity key pair
 */
const ecc_key_pair &Workload::get_key_pair()
{
    return this->id_key_pair;
}

/**
 * @description: Set MR enclave
 * @param mr -> MR enclave
 */
void Workload::set_mr_enclave(sgx_measurement_t mr)
{
    memcpy(&this->mr_enclave, &mr, sizeof(sgx_measurement_t));
}

/**
 * @description: Get MR enclave
 * @return: Const reference to MR enclave
 */
const sgx_measurement_t &Workload::get_mr_enclave()
{
    return this->mr_enclave;
}

/**
 * @description: Set report height
 * @param height -> Report height
 */
void Workload::set_report_height(size_t height)
{
    sgx_thread_mutex_lock(&this->report_height_mutex);
    this->report_height = height;
    sgx_thread_mutex_unlock(&this->report_height_mutex);
}

/**
 * @description: Get report height
 * @return: Report height
 */
size_t Workload::get_report_height()
{
    SafeLock sl(this->report_height_mutex);
    sl.lock();
    return this->report_height;
}

/**
 * @description: Clean all workload information
 */
void Workload::clean_all()
{
    // Clean srd
    this->clean_srd();

    // Clean file
    this->clean_file();

    // Clean id key pair
    this->unset_key_pair();

    // Clean upgrade related
    this->unset_upgrade();
}

/**
 * @description: Set restart flag
 */
void Workload::set_restart_flag()
{
    this->restart_flag = 1;
}

/**
 * @description: Reduce flag
 */
void Workload::reduce_restart_flag()
{
    this->restart_flag -= 1;
    if (this->restart_flag < 0)
    {
        this->restart_flag = 0;
    }
}

/**
 * @description: Get restart flag
 * @return: Restart flag
 */
bool Workload::get_restart_flag()
{
    return this->restart_flag > 0;
}

/**
 * @description: add validated srd proof
 */
void Workload::report_add_validated_srd_proof()
{
    sgx_thread_mutex_lock(&this->validated_srd_mutex);
    if (this->validated_srd_proof >= VALIDATE_PROOF_MAX_NUM)
    {
        this->validated_srd_proof = VALIDATE_PROOF_MAX_NUM;
    }
    else
    {
        this->validated_srd_proof++;
    }
    sgx_thread_mutex_unlock(&this->validated_srd_mutex);
}

/**
 * @description: add validated file proof
 */
void Workload::report_add_validated_file_proof()
{
    sgx_thread_mutex_lock(&this->validated_file_mutex);
    if (this->validated_file_proof >= VALIDATE_PROOF_MAX_NUM)
    {
        this->validated_file_proof = VALIDATE_PROOF_MAX_NUM;
    }
    else
    {
        this->validated_file_proof++;
    }
    sgx_thread_mutex_unlock(&this->validated_file_mutex);
}

/**
 * @description: Reset validated proof
 */
void Workload::report_reset_validated_proof()
{
    sgx_thread_mutex_lock(&this->validated_srd_mutex);
    this->validated_srd_proof = 0;
    sgx_thread_mutex_unlock(&this->validated_srd_mutex);

    sgx_thread_mutex_lock(&this->validated_file_mutex);
    this->validated_file_proof = 0;
    sgx_thread_mutex_unlock(&this->validated_file_mutex);
}

/**
 * @description: Has validated proof
 * @return: true or false
 */
bool Workload::report_has_validated_proof()
{
    sgx_thread_mutex_lock(&this->validated_srd_mutex);
    bool res = (this->validated_srd_proof > 0);
    sgx_thread_mutex_unlock(&this->validated_srd_mutex);

    sgx_thread_mutex_lock(&this->validated_file_mutex);
    res = (res && this->validated_file_proof > 0);
    sgx_thread_mutex_unlock(&this->validated_file_mutex);

    return res;
}

/**
 * @description: Add deleted srd to buffer
 * @param index -> Srd index in indicated path
 * @return: Add result
 */
bool Workload::add_srd_to_deleted_buffer(uint32_t index)
{
    sgx_thread_mutex_lock(&this->srd_del_idx_mutex);
    auto ret_val = this->srd_del_idx_s.insert(index);
    sgx_thread_mutex_unlock(&this->srd_del_idx_mutex);

    return ret_val.second;
}

/**
 * @description: Has given srd been added to buffer
 * @param index -> Srd index in indicated path
 * @return: Added to deleted buffer or not
 */
bool Workload::is_srd_in_deleted_buffer(uint32_t index)
{
    SafeLock sl(this->srd_del_idx_mutex);
    sl.lock();
    return (this->srd_del_idx_s.find(index) != this->srd_del_idx_s.end());
}

/**
 * @description: Delete invalid srd from metadata
 */
void Workload::deal_deleted_srd()
{
    _deal_deleted_srd(true);
}

/**
 * @description: Delete invalid srd from metadata
 */
void Workload::deal_deleted_srd_nolock()
{
    _deal_deleted_srd(false);
}

/**
 * @description: Delete invalid srd from metadata
 * @param locked -> Lock srd_hashs or not
 */
void Workload::_deal_deleted_srd(bool locked)
{
    // Delete related srd from metadata by mainloop thread
    if (locked)
    {
        sgx_thread_mutex_lock(&this->srd_mutex);
    }

    sgx_thread_mutex_lock(&this->srd_del_idx_mutex);
    std::set<uint32_t> tmp_del_idx_s;
    // Put to be deleted srd to a buffer map and clean the old one
    tmp_del_idx_s.insert(this->srd_del_idx_s.begin(), this->srd_del_idx_s.end());
    this->srd_del_idx_s.clear();
    sgx_thread_mutex_unlock(&this->srd_del_idx_mutex);

    // Delete hashs
    this->delete_srd_meta(tmp_del_idx_s);

    if (locked)
    {
        sgx_thread_mutex_unlock(&this->srd_mutex);
    }
}

/**
 * @description: Add file to deleted buffer
 * @param cid -> File content id
 * @return: Added result
 */
bool Workload::add_to_deleted_file_buffer(std::string cid)
{
    sgx_thread_mutex_lock(&this->file_del_idx_mutex);
    auto ret = this->file_del_cid_s.insert(cid);
    sgx_thread_mutex_unlock(&this->file_del_idx_mutex);

    return ret.second;
}

/**
 * @description: Is deleted file in buffer
 * @param cid -> File content id
 * @return: Check result
 */
bool Workload::is_in_deleted_file_buffer(std::string cid)
{
    SafeLock sl(this->file_del_idx_mutex);
    sl.lock();
    return (this->file_del_cid_s.find(cid) != this->file_del_cid_s.end());
}

/**
 * @description: Recover file from deleted buffer
 * @param cid -> File content id
 */
void Workload::recover_from_deleted_file_buffer(std::string cid)
{
    sgx_thread_mutex_lock(&this->file_del_idx_mutex);
    this->file_del_cid_s.erase(cid);
    sgx_thread_mutex_unlock(&this->file_del_idx_mutex);
}

/**
 * @description: Deal with deleted file
 */
void Workload::deal_deleted_file()
{
    // Clear file deleted buffer
    sgx_thread_mutex_lock(&this->file_del_idx_mutex);
    std::set<std::string> tmp_del_cid_s = this->file_del_cid_s;
    this->file_del_cid_s.clear();
    sgx_thread_mutex_unlock(&this->file_del_idx_mutex);

    // Deleted invalid file item
    std::set<std::string> left_del_cid_s = tmp_del_cid_s;
    SafeLock sealed_files_sl(this->file_mutex);
    sealed_files_sl.lock();
    for (auto cid : tmp_del_cid_s)
    {
        size_t pos = 0;
        if (this->is_file_dup_nolock(cid, pos))
        {
            std::string status = this->sealed_files[pos][FILE_STATUS].ToString();
            if ((status[CURRENT_STATUS] == FILE_STATUS_DELETED && status[ORIGIN_STATUS] == FILE_STATUS_DELETED)
                    || (status[CURRENT_STATUS] == FILE_STATUS_DELETED && status[ORIGIN_STATUS] == FILE_STATUS_UNVERIFIED)
                    || (status[CURRENT_STATUS] == FILE_STATUS_DELETED && status[ORIGIN_STATUS] == FILE_STATUS_LOST))
            {
                this->sealed_files.erase(this->sealed_files.begin() + pos);
                left_del_cid_s.erase(cid);
            }
        }
    }
    sealed_files_sl.unlock();

    // Put left file to file deleted set
    sgx_thread_mutex_lock(&this->file_del_idx_mutex);
    this->file_del_cid_s.insert(left_del_cid_s.begin(), left_del_cid_s.end());
    sgx_thread_mutex_unlock(&this->file_del_idx_mutex);
}

/**
 * @description: Is file duplicated, must hold file_mutex before invoked
 * @param cid -> File's content id
 * @return: File duplicated or not
 */
bool Workload::is_file_dup_nolock(std::string cid)
{
    size_t pos = 0;
    return is_file_dup_nolock(cid, pos);
}

/**
 * @description: Is file duplicated, must hold file_mutex before invoked
 * @param cid -> File's content id
 * @param pos -> Duplicated file's position
 * @return: Duplicated or not
 */
bool Workload::is_file_dup_nolock(std::string cid, size_t &pos)
{
    long spos = 0;
    long epos = this->sealed_files.size();
    while (spos <= epos)
    {
        long mpos = (spos + epos) / 2;
        if (mpos >= (long)this->sealed_files.size())
        {
            break;
        }
        int ret = cid.compare(this->sealed_files[mpos][FILE_CID].ToString());
        if (ret > 0)
        {
            spos = mpos + 1;
            pos = std::min(spos, (long)this->sealed_files.size());
        }
        else if (ret < 0)
        {
            pos = mpos;
            epos = mpos - 1;
        }
        else
        {
            pos = mpos;
            return true;
        }
    }

    return false;
}

/**
 * @description: Add sealed file, must hold file_mutex before invoked
 * @param file -> File content
 * @param pos -> Inserted position
 */
void Workload::add_file_nolock(json::JSON file, size_t pos)
{
    if (pos <= this->sealed_files.size())
    {
        this->sealed_files.insert(this->sealed_files.begin() + pos, file);
    }
}

/**
 * @description: Add sealed file, must hold file_mutex before invoked
 * @param file -> File content
 */
void Workload::add_file_nolock(json::JSON file)
{
    size_t pos = 0;
    if (is_file_dup_nolock(file[FILE_CID].ToString(), pos))
    {
        return;
    }

    this->sealed_files.insert(this->sealed_files.begin() + pos, file);
}

/**
 * @description: Delete sealed file, must hold file_mutex before invoked
 * @param cid -> File content id
 */
void Workload::del_file_nolock(std::string cid)
{
    size_t pos = 0;
    if (this->is_file_dup_nolock(cid, pos))
    {
        this->sealed_files.erase(this->sealed_files.begin() + pos);
    }
}

/**
 * @description: Delete sealed file, must hold file_mutex before invoked
 * @param pos -> Deleted file position
 */
void Workload::del_file_nolock(size_t pos)
{
    if (pos < this->sealed_files.size())
    {
        this->sealed_files.erase(this->sealed_files.begin() + pos);
    }
}

/**
 * @description: Deal with file illegal transfer
 * @param data -> Pointer to recover data
 * @param data_size -> Recover data size
 * @return: Recover status
 */
spacex_status_t Workload::recover_illegal_file(const uint8_t *data, size_t data_size)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    json::JSON commit_file = json::JSON::Load(&spacex_status, data, data_size);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return spacex_status;
    }

    Workload *wl = Workload::get_instance();
    SafeLock sl(wl->file_mutex);
    sl.lock();
    std::vector<std::string> file_tag = {WORKREPORT_FILES_ADDED, WORKREPORT_FILES_DELETED};
    for (auto type : file_tag)
    {
        if (commit_file.hasKey(type))
        {
            if (commit_file[type].JSONType() == json::JSON::Class::Array)
            {
                for (auto file : commit_file[type].ArrayRange())
                {
                    size_t pos = 0;
                    std::string cid = file.ToString();
                    if (wl->is_file_dup_nolock(cid, pos))
                    {
                        if (type.compare(WORKREPORT_FILES_ADDED) == 0)
                        {
                            wl->sealed_files[pos][FILE_STATUS].set_char(ORIGIN_STATUS, FILE_STATUS_VALID);
                        }
                        else
                        {
                            char cur_status = wl->sealed_files[pos][FILE_STATUS].get_char(CURRENT_STATUS);
                            char com_status = (FILE_STATUS_DELETED == cur_status ? FILE_STATUS_DELETED : FILE_STATUS_LOST);
                            wl->sealed_files[pos][FILE_STATUS].set_char(ORIGIN_STATUS, com_status);
                        }
                        log_info("Recover illegal file:'%s' type:'%s' done\n", cid.c_str(), type.c_str());
                    }
                    else
                    {
                        log_warn("Recover illegal: cannot find file:'%s'\n", cid.c_str());
                    }
                }
            }
            else
            {
                log_err("Recover illegal: %s is not an array!\n", type.c_str());
            }
        }
        else
        {
            log_debug("Recover illegal: no %s provided.\n", type.c_str());
        }
    }
    sl.unlock();

    return spacex_status;
}

/**
 * @description: Restore file informatione
 * @return: Restore file information result
 */
spacex_status_t Workload::restore_file_info()
{
    // ----- Construct file information json string ----- //
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    json::JSON file_buffer;
    for (auto it = g_file_spec_status.begin(); it != g_file_spec_status.end(); it++)
    {
        std::string file_type = it->second;
        char s = it->first;
        for (auto file : this->sealed_files)
        {
            if (s != file[FILE_STATUS].get_char(CURRENT_STATUS))
                continue;

            json::JSON info;
            info[file[FILE_CID].ToString()] = "{ \"" FILE_SIZE "\" : " + std::to_string(file[FILE_SIZE].ToInt())
                + " , \"" FILE_SEALED_SIZE "\" : " + std::to_string(file[FILE_SEALED_SIZE].ToInt())
                + " , \"" FILE_CHAIN_BLOCK_NUM "\" : " + std::to_string(file[FILE_CHAIN_BLOCK_NUM].ToInt()) + " }";
            file_buffer[file_type].append(info);
        }
    }

    // Store file information to app
    std::vector<uint8_t> file_data = file_buffer.dump_vector(&spacex_status);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return spacex_status;
    }
    return safe_ocall_store2(OCALL_FILE_INFO_ALL, file_data.data(), file_data.size());
}
