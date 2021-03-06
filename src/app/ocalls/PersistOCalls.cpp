# include "PersistOCalls.h"

spacex::Log *p_log = spacex::Log::get_instance();

/**
 * @description: Add record to DB
 * @param key (in) -> Pointer to key
 * @param value (in) -> Pointer to value
 * @param value_len -> value length
 * @return: Add status
 */
spacex_status_t ocall_persist_add(const char *key, const uint8_t *value, size_t value_len)
{
    return spacex::DataBase::get_instance()->add(std::string(key), std::string((const char*)value, value_len));
}

/**
 * @description: Delete record from DB
 * @param key (in) -> Pointer to key
 * @return: Delete status
 */
spacex_status_t ocall_persist_del(const char *key)
{
    return spacex::DataBase::get_instance()->del(std::string(key));
}

/**
 * @description: Update record in DB
 * @param key (in) -> Pointer to key
 * @param value (in) -> Pointer to value
 * @param value_len -> value length
 * @param total_size -> Data from enclave total size
 * @param total_buf (in) -> Pointer to total buffer
 * @param offset -> Data offset in total buffer
 * @return: Update status
 */
spacex_status_t ocall_persist_set(const char *key, const uint8_t *value, size_t value_len, 
        size_t total_size, uint8_t **total_buf, size_t offset)
{
    std::string value_str;
    if (value_len < total_size)
    {
        if (*total_buf == NULL)
        {
            *total_buf = (uint8_t *)malloc(total_size);
            memset(*total_buf, 0, total_size);
        }
        memcpy(*total_buf + offset, value, value_len);
        if (offset + value_len < total_size)
        {
            return SPACEX_SUCCESS;
        }
        value_str = std::string((const char *)(*total_buf), total_size);
        free(*total_buf);
    }
    else
    {
        value_str = std::string((const char *)value, value_len);
    }
    return spacex::DataBase::get_instance()->set(std::string(key), value_str);
}

/**
 * @description: Get record from DB
 * @param key (in) -> Pointer to key
 * @param value (out) -> Pointer points to pointer to value
 * @param value_len (out) -> value length
 * @return: Get status
 */
spacex_status_t ocall_persist_get(const char *key, uint8_t **value, size_t *value_len)
{
    std::string val;
    spacex_status_t spacex_status = spacex::DataBase::get_instance()->get(std::string(key), val);
    if (SPACEX_SUCCESS != spacex_status)
    {
        *value_len = 0;
        return spacex_status;
    }
    
    *value_len = val.size();
    *value = (uint8_t*)malloc(*value_len);
    memcpy(*value, val.c_str(), *value_len);

    return spacex_status;
}
