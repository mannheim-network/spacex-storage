#ifndef _SPACEX_SRD_TEST_H_
#define _SPACEX_SRD_TEST_H_

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

#include "sgx_trts.h"
#include "sgx_thread.h"

#include "Workload.h"
#include "EUtils.h"
#include "PathHelper.h"
#include "SafeLock.h"
#include "Parameter.h"

void srd_change_test(long change, bool real);
size_t srd_decrease_test(size_t change);
spacex_status_t srd_increase_test(const char *uuid);

#endif /* !_SPACEX_SRD_TEST_H_ */
