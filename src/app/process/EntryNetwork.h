#ifndef _SPACEX_ENTRY_NETWORK_H_
#define _SPACEX_ENTRY_NETWORK_H_

#include <string>
#include <stdlib.h>

#include <sgx_eid.h>
#include <sgx_uae_launch.h>
#include <sgx_uae_epid.h>
#include <sgx_uae_quote_ex.h>

#include "Common.h"
#include "Config.h"
#include "Log.h"
#include "SgxSupport.h"
#include "FormatUtils.h"
#include "CrustStatus.h"
#include "EnclaveData.h"
#include "HttpClient.h"

#define OPT_ISSET(x, y) x &y
#define _rdrand64_step(x) ({ unsigned char err; asm volatile("rdrand %0; setc %1":"=r"(*x), "=qm"(err)); err; })
#define OPT_PSE 0x01
#define OPT_NONCE 0x02
#define OPT_LINK 0x04
#define OPT_PUBKEY 0x08

spacex_status_t entry_network();

#endif /* !_SPACEX_ENTRY_NETWORK_H_ */
