#ifndef _SPACEX_PERSISTENCE_H_
#define _SPACEX_PERSISTENCE_H_

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "sgx_tseal.h"

#include "EUtils.h"
#include "CrustStatus.h"

#if defined(__cplusplus)
extern "C"
{
#endif

spacex_status_t persist_add(std::string key, const uint8_t *value, size_t value_len);
spacex_status_t persist_del(std::string key);
spacex_status_t persist_set(std::string key, const uint8_t *value, size_t value_len);
spacex_status_t persist_set_unsafe(std::string key, const uint8_t *value, size_t value_len);
spacex_status_t persist_get(std::string key, uint8_t **value, size_t *value_len);
spacex_status_t persist_get_unsafe(std::string key, uint8_t **value, size_t *value_len);

#if defined(__cplusplus)
}
#endif

#endif /* !_SPACEX_PERSISTENCE_H_ */
