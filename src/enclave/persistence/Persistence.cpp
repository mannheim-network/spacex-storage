#include "Persistence.h"

using namespace std;

void inner_ocall_persist_get(spacex_status_t* spacex_status, const char *key, uint8_t **value, size_t *value_len);

/**
 * @description: Add value by key
 * @param key -> Pointer to key
 * @param value -> Pointer to value
 * @param value_len -> value length
 * @return: Add status
 */
spacex_status_t persist_add(std::string key, const uint8_t *value, size_t value_len)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    sgx_sealed_data_t *p_sealed_data = NULL;
    size_t sealed_data_size = 0;
    spacex_status = seal_data_mrenclave(value, value_len, &p_sealed_data, &sealed_data_size);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return spacex_status;
    }

    ocall_persist_add(&spacex_status, key.c_str(), (uint8_t *)p_sealed_data, sealed_data_size);
    free(p_sealed_data);

    return spacex_status;
}

/**
 * @description: Delete value by key
 * @param key -> Pointer to key
 * @return: Delete status
 */
spacex_status_t persist_del(std::string key)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    ocall_persist_del(&spacex_status, key.c_str());

    return spacex_status;
}

/**
 * @description: Update value by key
 * @param key -> Pointer to key
 * @param value -> Pointer to value
 * @param value_len -> value length
 * @return: Update status
 */
spacex_status_t persist_set(std::string key, const uint8_t *value, size_t value_len)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    sgx_sealed_data_t *p_sealed_data = NULL;
    size_t sealed_data_size = 0;
    spacex_status = seal_data_mrenclave(value, value_len, &p_sealed_data, &sealed_data_size);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return spacex_status;
    }
    uint8_t *p_sealed_data_u = (uint8_t *)p_sealed_data;

    uint8_t *store_buf = NULL;
    size_t offset = 0;
    if (sealed_data_size > BOUNDARY_SIZE_THRESHOLD)
    {
        // Data size larger than default size
        uint32_t part_size = 0;
        while (sealed_data_size > offset)
        {
            part_size = std::min((uint32_t)(sealed_data_size - offset), (uint32_t)BOUNDARY_SIZE_THRESHOLD);
            ocall_persist_set(&spacex_status, key.c_str(), p_sealed_data_u + offset, part_size, 
                    sealed_data_size, &store_buf, offset);
            if (SPACEX_SUCCESS != spacex_status)
            {
                log_err("Store part data to DB failed!\n");
                goto cleanup;
            }
            offset += part_size;
        }
    }
    else
    {
        // Set new data
        ocall_persist_set(&spacex_status, key.c_str(), p_sealed_data_u, sealed_data_size,
                sealed_data_size, &store_buf, offset);
    }


cleanup:
    if (p_sealed_data != NULL)
    {
        free(p_sealed_data);
    }

    return spacex_status;
}

/**
 * @description: Update value by key without seal
 * @param key -> Pointer to key
 * @param value -> Pointer to value
 * @param value_len -> value length
 * @return: Update status
 */
spacex_status_t persist_set_unsafe(std::string key, const uint8_t *value, size_t value_len)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;

    uint8_t *store_buf = NULL;
    size_t offset = 0;
    if (value_len > BOUNDARY_SIZE_THRESHOLD)
    {
        // Data size larger than default size
        uint32_t part_size = 0;
        while (value_len > offset)
        {
            part_size = std::min((uint32_t)(value_len - offset), (uint32_t)BOUNDARY_SIZE_THRESHOLD);
            ocall_persist_set(&spacex_status, key.c_str(), value + offset, part_size, 
                    value_len, &store_buf, offset);
            if (SPACEX_SUCCESS != spacex_status)
            {
                log_err("Store part data to DB failed!\n");
                return spacex_status;
            }
            offset += part_size;
        }
    }
    else
    {
        // Set new data
        ocall_persist_set(&spacex_status, key.c_str(), value, value_len, 
                value_len, &store_buf, offset);
    }

    return spacex_status;
}

/**
 * @description: Get value by key from ocall
 * @param spacex_status -> status
 * @param key -> Pointer to key
 * @param value -> Pointer points to value
 * @param value_len -> Pointer to value length
 */
void inner_ocall_persist_get(spacex_status_t* spacex_status, const char *key, uint8_t **value, size_t *value_len)
{
    uint8_t *tmp_value = NULL;
    size_t tmp_value_len = 0;

    ocall_persist_get(spacex_status, key, &tmp_value, &tmp_value_len);
    if (SPACEX_SUCCESS != *spacex_status)
    {
        *value = NULL;
        *value_len = 0;
        return;
    }

    *value = (uint8_t*)enc_malloc(tmp_value_len);
    if(*value == NULL)
    {
        ocall_free_outer_buffer(spacex_status, &tmp_value);
        if (SPACEX_SUCCESS != *spacex_status)
        {
            return;
        }

        *spacex_status = SPACEX_MALLOC_FAILED;
        return;
    }

    memset(*value, 0, tmp_value_len);
    memcpy(*value, tmp_value, tmp_value_len);
    *value_len = tmp_value_len;

    ocall_free_outer_buffer(spacex_status, &tmp_value);
    if (SPACEX_SUCCESS != *spacex_status)
    {
        free(*value);
        *value = NULL;
        *value_len = 0;
        return;
    }
}

/**
 * @description: Get value by key
 * @param key -> Pointer to key
 * @param value -> Pointer points to value
 * @param value_len -> Pointer to value length
 * @return: Get status
 */
spacex_status_t persist_get(std::string key, uint8_t **value, size_t *value_len)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    sgx_status_t sgx_status = SGX_SUCCESS;
    size_t sealed_data_size = 0;
    sgx_sealed_data_t *p_sealed_data = NULL;

    // Get sealed data
    inner_ocall_persist_get(&spacex_status, key.c_str(), (uint8_t **)&p_sealed_data, &sealed_data_size);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return SPACEX_PERSIST_GET_FAILED;
    }

    // Get unsealed data
    uint32_t unsealed_data_size = sgx_get_encrypt_txt_len(p_sealed_data);
    uint8_t *p_unsealed_data = (uint8_t*)enc_malloc(unsealed_data_size);
    if (p_unsealed_data == NULL)
    {
        log_err("Malloc memory failed!\n");
        goto cleanup;
    }
    memset(p_unsealed_data, 0, unsealed_data_size);
    sgx_status = sgx_unseal_data(p_sealed_data, NULL, NULL,
            p_unsealed_data, &unsealed_data_size);
    if (SGX_SUCCESS != sgx_status)
    {
        log_err("Unseal data failed!Error code:%lx\n", sgx_status);
        spacex_status = SPACEX_UNSEAL_DATA_FAILED;
        free(p_unsealed_data);
        goto cleanup;
    }

    *value = p_unsealed_data;
    *value_len = unsealed_data_size;

cleanup:
    free(p_sealed_data);

    return spacex_status;
}

/**
 * @description: Get value by key
 * @param key -> Pointer to key
 * @param value -> Pointer points to value
 * @param value_len -> Pointer to value length
 * @return: Get status
 */
spacex_status_t persist_get_unsafe(std::string key, uint8_t **value, size_t *value_len)
{
    spacex_status_t spacex_status = SPACEX_SUCCESS;

    inner_ocall_persist_get(&spacex_status, key.c_str(), value, value_len);
    if (SPACEX_SUCCESS != spacex_status)
    {
        return SPACEX_PERSIST_GET_FAILED;
    }

    return spacex_status;
}
