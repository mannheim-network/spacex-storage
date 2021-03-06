#ifndef _SPACEX_WORKLOAD_H_
#define _SPACEX_WORKLOAD_H_

#include <utility>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <unordered_map>
#include <set>

#include "sgx_trts.h"
#include "sgx_thread.h"

#include "EUtils.h"
#include "Enclave_t.h"
#include "Persistence.h"
#include "Json.h"
#include "Identity.h"
#include "Srd.h"
#include "Parameter.h"

#define VALIDATE_PROOF_MAX_NUM 2

std::map<char, std::string> file_status_2_name = {
    {FILE_STATUS_PENDING, FILE_TYPE_PENDING},
    {FILE_STATUS_UNVERIFIED, FILE_TYPE_UNVERIFIED},
    {FILE_STATUS_VALID, FILE_TYPE_VALID},
    {FILE_STATUS_LOST, FILE_TYPE_LOST},
    {FILE_STATUS_DELETED, FILE_TYPE_DELETED},
};

// Show information
std::map<char, std::string> g_file_spec_status = {
    {FILE_STATUS_PENDING, FILE_TYPE_PENDING},
    {FILE_STATUS_VALID, FILE_TYPE_VALID},
    {FILE_STATUS_LOST, FILE_TYPE_LOST},
};

class Workload
{
public:
    std::vector<uint8_t*> srd_hashs; // used to store all G srd file collection' hashs
    std::vector<json::JSON> sealed_files; // Files have been added into checked queue
    std::set<std::string> reported_files_idx; // File indexes reported this turn of workreport
    sgx_ec256_public_t pre_pub_key; // Old version's public key
    std::unordered_map<std::string, json::JSON> pending_files_um; // Pending files
    
    // Basic
    static Workload *workload;
    static Workload *get_instance();
    ~Workload();
    void set_srd_remaining_task(long num);
    void set_srd_info(const char *uuid, long change);
    json::JSON get_srd_info();
    json::JSON gen_workload_info(spacex_status_t *status);
    spacex_status_t restore_pre_pub_key(json::JSON &meta);
    void clean_all();

    // For persistence
    json::JSON serialize_srd();
    json::JSON serialize_file();
    json::JSON get_upgrade_srd_info(spacex_status_t *status);
    json::JSON get_upgrade_file_info(spacex_status_t *status);
    spacex_status_t restore_srd(json::JSON &g_hashs);
    spacex_status_t restore_file(json::JSON &file_json);
    spacex_status_t restore_file_info();

    // For report
    void report_add_validated_srd_proof();
    void report_add_validated_file_proof();
    void report_reset_validated_proof();
    bool report_has_validated_proof();
    void set_report_file_flag(bool flag);
    bool get_report_file_flag();
    void set_restart_flag();
    void reduce_restart_flag();
    bool get_restart_flag();
    void handle_report_result();
    spacex_status_t can_report_work(size_t block_height);

    // For upgrade
    void set_upgrade(sgx_ec256_public_t pub_key);
    void unset_upgrade();
    bool is_upgrade();
    void set_upgrade_status(enc_upgrade_status_t status);
    enc_upgrade_status_t get_upgrade_status();

    // For workload spec
    void set_file_spec(char file_status, long long change);
    const json::JSON &get_file_spec();

    // For identity
    void set_account_id(std::string account_id);
    std::string get_account_id();
    // Key pair
    bool try_get_key_pair();
    const sgx_ec256_public_t& get_pub_key();
    const sgx_ec256_private_t& get_pri_key();
    void set_key_pair(ecc_key_pair id_key_pair);
    void unset_key_pair();
    const ecc_key_pair& get_key_pair();
    // MR enclave
    void set_mr_enclave(sgx_measurement_t mr);
    const sgx_measurement_t& get_mr_enclave();
    // Report height
    void set_report_height(size_t height);
    size_t get_report_height();
    // Srd related
    void clean_srd();
    bool add_srd_to_deleted_buffer(uint32_t index);
    template <class InputIterator>
    void add_srd_to_deleted_buffer(InputIterator begin, InputIterator end)
    {
        sgx_thread_mutex_lock(&this->srd_del_idx_mutex);
        this->srd_del_idx_s.insert(begin, end);
        sgx_thread_mutex_unlock(&this->srd_del_idx_mutex);
    }
    template <class InputContainer>
    long delete_srd_meta(InputContainer &indexes)
    {
        if (indexes.size() == 0)
        {
            return 0;
        }

        long del_num = 0;
    
        for (auto rit = indexes.rbegin(); rit != indexes.rend(); rit++)
        {
            if (*rit < this->srd_hashs.size())
            {
                uint8_t *hash = this->srd_hashs[*rit];
                if (hash != NULL)
                {
                    std::string uuid = hexstring_safe(hash, UUID_LENGTH);
                    this->set_srd_info(uuid.c_str(), -1);
                    free(hash);
                }
                this->srd_hashs.erase(this->srd_hashs.begin() + *rit);
                del_num++;
            }
        }

        // Update srd info
        std::string srd_info_str = this->get_srd_info().dump();
        ocall_set_srd_info(reinterpret_cast<const uint8_t *>(srd_info_str.c_str()), srd_info_str.size());

        return del_num;
    }
    bool is_srd_in_deleted_buffer(uint32_t index);
    void deal_deleted_srd();
    void deal_deleted_srd_nolock();
    void _deal_deleted_srd(bool locked);
    // File related
    void clean_file();
    bool add_to_deleted_file_buffer(std::string cid);
    bool is_in_deleted_file_buffer(std::string cid);
    void recover_from_deleted_file_buffer(std::string cid);
    void deal_deleted_file();
    bool is_file_dup_nolock(std::string cid);
    bool is_file_dup_nolock(std::string cid, size_t &pos);
    void add_file_nolock(json::JSON file);
    void add_file_nolock(json::JSON file, size_t pos);
    void del_file_nolock(std::string cid);
    void del_file_nolock(size_t pos);
    spacex_status_t recover_illegal_file(const uint8_t *data, size_t data_size);

#ifdef _SPACEX_TEST_FLAG_
    void clean_wl_file_spec()
    {
        sgx_thread_mutex_lock(&wl_file_spec_mutex);
        this->wl_file_spec = json::JSON();
        sgx_thread_mutex_unlock(&wl_file_spec_mutex);
    }
#endif

    sgx_thread_mutex_t ocall_wr_mutex = SGX_THREAD_MUTEX_INITIALIZER; // Workreport mutex
    sgx_thread_mutex_t ocall_wl_mutex = SGX_THREAD_MUTEX_INITIALIZER; // Workload mutex
    sgx_thread_mutex_t ocall_upgrade_mutex = SGX_THREAD_MUTEX_INITIALIZER; // Upgrade mutex
    sgx_thread_mutex_t srd_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    sgx_thread_mutex_t file_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    sgx_thread_mutex_t pending_files_um_mutex = SGX_THREAD_MUTEX_INITIALIZER;

private:
    Workload();

    std::string account_id; // Chain account id
    ecc_key_pair id_key_pair; // Identity key pair
    bool is_set_key_pair = false; // Check if key pair has been generated
    sgx_measurement_t mr_enclave; // Enclave code measurement
    size_t report_height = 0; // Identity report height, Used to check current block head out-of-date
    sgx_thread_mutex_t report_height_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    int restart_flag = 0;// Used to indicate whether it is the first report after restart

    int validated_srd_proof = 0; // Generating workreport will decrease this value, while validating will increase it
    sgx_thread_mutex_t validated_srd_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    int validated_file_proof = 0; // Generating workreport will decrease this value, while validating will increase it
    sgx_thread_mutex_t validated_file_mutex = SGX_THREAD_MUTEX_INITIALIZER;

    bool report_files; // True indicates reporting files this turn, false means not report
    json::JSON srd_info_json; // Srd info
    sgx_thread_mutex_t srd_info_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    bool upgrade = false; // True indicates workreport should contain previous public key
    enc_upgrade_status_t upgrade_status = ENC_UPGRADE_STATUS_NONE; // Initial value indicates no upgrade
    json::JSON wl_file_spec; // For workload statistics
    sgx_thread_mutex_t wl_file_spec_mutex = SGX_THREAD_MUTEX_INITIALIZER;
    // Deleted srd index in metadata while the value indicates whether
    // this srd metadata has been deleted by other thread
    std::set<uint32_t> srd_del_idx_s;
    sgx_thread_mutex_t srd_del_idx_mutex = SGX_THREAD_MUTEX_INITIALIZER; // Deleted srd mutex
    // file_del_cid_s stores deleted file cid, if this file has been validated to lost, ignore this message
    std::set<std::string> file_del_cid_s;
    sgx_thread_mutex_t file_del_idx_mutex = SGX_THREAD_MUTEX_INITIALIZER; // Deleted srd mutex
};

#endif /* !_SPACEX_WORKLOAD_H_ */
