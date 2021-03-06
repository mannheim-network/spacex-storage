#ifndef _SPACEX_REPORT_TEST_H_
#define _SPACEX_REPORT_TEST_H_

#include <string>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/cmac.h>
#include <openssl/conf.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>

#include "Workload.h"
#include "Report.h"
#include "EUtils.h"

spacex_status_t gen_and_upload_work_report_test(const char *block_hash, size_t block_height, long wait_time, bool is_upgrading, bool locked =true);

#endif /* !_SPACEX_REPORT_TEST_H_ */
