#ifndef _SPACEX_PROCESS_H_
#define _SPACEX_PROCESS_H_

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <map>
#include <fstream>
#include <future>
#include <chrono>

#include <sgx_key_exchange.h>
#include <sgx_report.h>
#include <sgx_uae_launch.h>
#include <sgx_uae_epid.h>
#include <sgx_uae_quote_ex.h>
#include <sgx_error.h>
#include <sgx_eid.h>
#include <sgx_urts.h>
#include <sgx_capable.h>

#include "SgxSupport.h"
#include "ECalls.h"
#include "Config.h"
#include "FormatUtils.h"
#include "Common.h"
#include "Resource.h"
#include "FileUtils.h"
#include "Log.h"
#include "WorkReport.h"
#include "Srd.h"
#include "DataBase.h"
#include "EntryNetwork.h"
#include "Chain.h"

typedef void (*task_func_t)(void);

int process_run();

#endif /* !_SPACEX_PROCESS_H_ */
