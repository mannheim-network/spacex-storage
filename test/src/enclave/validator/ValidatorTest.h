#ifndef _SPACEX_VALIDATOR_TEST_H_
#define _SPACEX_VALIDATOR_TEST_H_

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>

#include "sgx_thread.h"
#include "sgx_trts.h"

#include "MerkleTree.h"
#include "Workload.h"
#include "PathHelper.h"
#include "Persistence.h"
#include "EUtils.h"
#include "Parameter.h"
#include "SafeLock.h"
#include "Storage.h"

void validate_meaningful_file_bench();
void validate_meaningful_file_bench_real();
void validate_srd_bench();
void validate_srd_bench_real();

#endif /* !_SPACEX_VALIDATOR_TEST_H_ */
