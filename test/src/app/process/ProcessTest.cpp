#include "ProcessTest.h"
#include "WebServer.h"

extern bool initialize_config(void);
extern bool initialize_enclave();
extern bool initialize_components(void);
extern bool start_task(task_func_t func);
extern bool do_upgrade();

extern sgx_enclave_id_t global_eid;
extern Config *p_config;
extern std::map<task_func_t, std::shared_ptr<std::future<void>>> g_tasks_m;
extern spacex::Log *p_log;

extern bool offline_chain_mode;
extern int g_start_server_success;
extern bool g_upgrade_flag;

size_t g_block_height;

/**
 * @description: Main function to start application
 * @return: Start status
 */
int process_run_test()
{
    pid_t worker_pid = getpid();
    sgx_status_t sgx_status = SGX_SUCCESS;
    spacex_status_t spacex_status = SPACEX_SUCCESS;
    int return_status = 1;
    int check_interval = 15;
    int upgrade_timeout = 5 * REPORT_SLOT * BLOCK_INTERVAL;
    int upgrade_tryout = upgrade_timeout / check_interval;
    int entry_tryout = 3;
    size_t srd_task = 0;
    long srd_real_change = 0;
    std::string srd_task_str;
    EnclaveData *ed = EnclaveData::get_instance();
    spacex::DataBase *db = NULL;
    p_log->info("WorkerPID = %d\n", worker_pid);

    // Init conifigure
    if (!initialize_config())
    {
        p_log->err("Init configuration failed!\n");
        return_status = -1;
        goto cleanup;
    }

    // There are three startup mode: upgrade, restore and normal
    // Upgrade mode will communicate with old version for data transferring.
    // Resotre mode will restore enclave data from database.
    // Normal mode will start up a new enclave.
    // ----- Upgrade mode ----- //
    if (g_upgrade_flag)
    {
        // Check and do upgrade
        do_upgrade();
        p_log->info("Upgrade from old version successfully!\n");
    }
    else
    {
        // Init related components
        if (!initialize_components())
        {
            p_log->err("Init component failed!\n");
            return_status = -1;
            goto cleanup;
        }

entry_network_flag:
        // Init enclave
        if (!initialize_enclave())
        {
            p_log->err("Init enclave failed!\n");
            return_status = -1;
            goto cleanup;
        }
        p_log->info("Worker global eid: %d\n", global_eid);

        // Start enclave
        if (SGX_SUCCESS != Ecall_restore_metadata(global_eid, &spacex_status) || SPACEX_SUCCESS != spacex_status)
        {
            // ----- Normal startup ----- //
            // Restore data failed
            p_log->info("Starting a new enclave...(code:%lx)\n", spacex_status);

            // Generate ecc key pair
            if (SGX_SUCCESS != Ecall_gen_key_pair(global_eid, &sgx_status, p_config->chain_account_id.c_str(), p_config->chain_account_id.size())
                    || SGX_SUCCESS != sgx_status)
            {
                p_log->err("Generate key pair failed!\n");
                return_status = -1;
                goto cleanup;
            }
            p_log->info("Generate key pair successfully!\n");

            // Get id info
            if (SGX_SUCCESS != Ecall_id_get_info(global_eid))
            {
                p_log->err("Get id info from enclave failed!\n");
                return_status = -1;
                goto cleanup;
            }

            
            // Entry network
            if (!offline_chain_mode)
            {
                spacex_status = entry_network();
                if (SPACEX_SUCCESS != spacex_status)
                {
                    if (SPACEX_INIT_QUOTE_FAILED == spacex_status && entry_tryout > 0)
                    {
                        entry_tryout--;
                        sgx_destroy_enclave(global_eid);
                        global_eid = 0;
                        sleep(60);
                        goto entry_network_flag;
                    }
                    goto cleanup;
                    return_status = -1;
                }
            }
        }
        else
        {
            // ----- Restore data successfully ----- //
            // Compare spacex account it in configure file and recovered file
            if (SGX_SUCCESS != Ecall_cmp_chain_account_id(global_eid, &spacex_status,
                    p_config->chain_account_id.c_str(), p_config->chain_account_id.size())
                || SPACEX_SUCCESS != spacex_status)
            {
                p_log->err("Configure chain account id doesn't equal to recovered one!\n");
                return_status = -1;
                goto cleanup;
            }

            p_log->info("Workload information:\n%s\n", ed->gen_workload_str().c_str());
            p_log->info("Restore enclave data successfully, sworker is running now.\n");
        }
    }
    db = spacex::DataBase::get_instance();

    // Get srd remaining task
    if (SPACEX_SUCCESS == db->get(WL_SRD_REMAINING_TASK, srd_task_str))
    {
        std::stringstream sstream(srd_task_str);
        size_t srd_task_remain = 0;
        sstream >> srd_task_remain;
        srd_task = std::max(srd_task, srd_task_remain);
    }

    // Restore or add srd task
    if (srd_task > 0)
    {
        p_log->info("Detect %ldGB srd task, will execute later.\n", srd_task);
        if (SGX_SUCCESS != (sgx_status = Ecall_change_srd_task(global_eid, &spacex_status, srd_task, &srd_real_change)))
        {
            p_log->err("Set srd change failed!Invoke SGX api failed! Error code:%lx\n", sgx_status);
        }
        else
        {
            switch (spacex_status)
            {
            case SPACEX_SUCCESS:
                p_log->info("Add srd task successfully! %ldG has been added, will be executed later.\n", srd_real_change);
                break;
            case SPACEX_SRD_NUMBER_EXCEED:
                p_log->warn("Add srd task failed!Srd number has reached the upper limit! Real srd task is %ldG.\n", srd_real_change);
                break;
            default:
                p_log->info("Unexpected error has occurred!\n");
            }
        }
    }

    // Construct uuid to disk path map
    ed->construct_uuid_disk_path_map();

    // Start http service
    if (!start_task(start_webservice))
    {
        p_log->err("Start web service failed!\n");
        goto cleanup;
    }

    // Check block height and post report to chain
    //start_task(work_report_loop);

    // Start thread to check srd reserved
    //start_task(srd_check_reserved);

    // Main validate loop
    //start_task(main_loop);

    // Check loop
    while (true)
    {
        // Deal with upgrade
        if (UPGRADE_STATUS_PROCESS == ed->get_upgrade_status())
        {
            if (EnclaveQueue::get_instance()->get_upgrade_ecalls_num() == 0)
            {
                ed->set_upgrade_status(UPGRADE_STATUS_END);
            }
        }
        if (UPGRADE_STATUS_END == ed->get_upgrade_status())
        {
            p_log->info("Start generating upgrade data...\n");
            if (SGX_SUCCESS != (sgx_status = Ecall_gen_upgrade_data_test(global_eid, &spacex_status, 
                            g_block_height+REPORT_SLOT+REPORT_INTERVAL_BLCOK_NUMBER_LOWER_LIMIT)))
            {
                p_log->err("Generate upgrade metadata failed! Invoke SGX API failed! Error code:%lx. Try next turn!\n", sgx_status);
            }
            else if (SPACEX_SUCCESS != spacex_status)
            {
                p_log->warn("Generate upgrade metadata failed! Code:%lx. Try next turn!\n", spacex_status);
            }
            else
            {
                p_log->info("Generate upgrade metadata successfully!\n");
                ed->set_upgrade_status(UPGRADE_STATUS_COMPLETE);
            }
        }

        // Upgrade tryout
        if (UPGRADE_STATUS_NONE != ed->get_upgrade_status())
        {
            if (--upgrade_tryout < 0)
            {
                p_log->err("Upgrade timeout!Current version will restore work!\n");
                ed->set_upgrade_status(UPGRADE_STATUS_NONE);
                upgrade_tryout = upgrade_timeout / check_interval;
                // Restore related work
                //if (!restore_tasks())
                //{
                //    p_log->err("Restore tasks failed! Will exist...\n");
                //    goto cleanup;
                //}
            }
        }

        // Sleep and check exit flag
        if (!sleep_interval(check_interval, [&ed](void) {
            if (UPGRADE_STATUS_EXIT == ed->get_upgrade_status())
            {
                // Wait tasks end
                bool has_task_running = false;
                for (auto task : g_tasks_m)	
                {
                    if(task.first == start_webservice)
                    {
                        continue;
                    }
                    if (task.second->wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    {
                        has_task_running = true;
                    }
                }
                return has_task_running;
            }
            return true;
        })) { goto cleanup; }
    }

cleanup:
    // Release database
    p_log->info("Release database for exit...\n");
    if (db != NULL)
    {
        delete db;
    }

    // Stop web service
    p_log->info("Kill web service for exit...\n");
    stop_webservice();

    // Destroy enclave
    // TODO: Fix me, why destory enclave leads to coredump
    p_log->info("Destroy enclave for exit...\n");
    sgx_destroy_enclave(global_eid);

    return return_status;
}
