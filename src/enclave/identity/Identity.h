#ifndef _SPACEX_IDENTITY_H_
#define _SPACEX_IDENTITY_H_

#include <string>
#include <map>
#include <set>
#include <vector>

#include <sgx_utils.h>
#include <sgx_tkey_exchange.h>
#include <sgx_tcrypto.h>
#include <sgx_uae_launch.h>
#include <sgx_uae_epid.h>
#include <sgx_uae_quote_ex.h>
#include <sgx_ecp_types.h>
#include <sgx_report.h>
#include "sgx_thread.h"
#include "sgx_spinlock.h"
#include "tSgxSSL_api.h"

#include "Workload.h"
#include "Report.h"
#include "Enclave_t.h"
#include "IASReport.h"
#include "EUtils.h"
#include "Persistence.h"
#include "Parameter.h"
#include "Defer.h"

#define PSE_RETRIES    5    /* Arbitrary. Not too long, not too short. */

enum metadata_op_e
{
    ID_APPEND,
    ID_UPDATE
};

using namespace std;

extern sgx_thread_mutex_t g_metadata_mutex;

string url_decode(string str);
int cert_load_size (X509 **cert, const char *pemdata, size_t sz);
int cert_load (X509 **cert, const char *pemdata);
STACK_OF(X509) * cert_stack_build (X509 **certs);
int cert_verify (X509_STORE *store, STACK_OF(X509) *chain);
void cert_stack_free (STACK_OF(X509) *chain);
int sha256_verify(const unsigned char *msg, size_t mlen, unsigned char *sig,
    size_t sigsz, EVP_PKEY *pkey, int *result);
X509_STORE * cert_init_ca(X509 *cert);

spacex_status_t id_verify_and_upload_identity(char ** IASReport, size_t size);
sgx_status_t id_gen_key_pair(const char *account_id, size_t len);
sgx_status_t id_get_quote_report(sgx_report_t *report, sgx_target_info_t *target_info);
sgx_status_t id_gen_sgx_measurement();
spacex_status_t id_cmp_chain_account_id(const char *account_id, size_t len);
void id_get_info();

spacex_status_t id_store_metadata();
spacex_status_t id_restore_metadata();
spacex_status_t id_gen_upgrade_data(size_t block_height);
spacex_status_t id_restore_from_upgrade(const uint8_t *data, size_t data_size);

#endif
