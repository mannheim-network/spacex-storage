#include "ApiHandlerTest.h"
#include "ECalls.h"
#include "ECallsTest.h"

extern size_t g_block_height;
extern sgx_enclave_id_t global_eid;

/**
 * @description: Start rest service
 * @return: Start status
 */
json::JSON http_handler_test(UrlEndPoint urlendpoint, json::JSON req)
{
    std::string req_route = req["target"].ToString();
    std::string cur_path;
    json::JSON res_json;
    std::string method = req["method"].ToString();
    EnclaveData *ed = EnclaveData::get_instance();
    EnclaveDataTest *edTest = EnclaveDataTest::get_instance();
    spacex::Log *p_log = spacex::Log::get_instance();
    std::string req_body = req["body"].ToString();
    remove_char(req_body, '\\');

    // ----- Respond to GET request ----- //
    if(method.compare("GET") == 0)
    {
        cur_path = urlendpoint.base + "/upgrade/start";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            int ret_code = 200;
            std::string ret_info;
            if (UPGRADE_STATUS_NONE != ed->get_upgrade_status()
                    && UPGRADE_STATUS_STOP_WORKREPORT != ed->get_upgrade_status())
            {
                res_json[HTTP_STATUS_CODE] = 300;
                res_json[HTTP_MESSAGE] = "Another upgrading is still running!";
                goto getcleanup;
            }

            // Block current work-report for a while
            ed->set_upgrade_status(UPGRADE_STATUS_STOP_WORKREPORT);

            sgx_status_t sgx_status = SGX_SUCCESS;
            spacex_status_t spacex_status = SPACEX_SUCCESS;
            if (SGX_SUCCESS != (sgx_status = Ecall_enable_upgrade(global_eid, &spacex_status, g_block_height+REPORT_SLOT+REPORT_INTERVAL_BLCOK_NUMBER_LOWER_LIMIT)))
            {
                ret_info = "Invoke SGX API failed!";
                ret_code = 401;
                p_log->err("%sError code:%lx\n", ret_info.c_str(), sgx_status);
            }
            else
            {
                if (SPACEX_SUCCESS == spacex_status)
                {
                    ret_info = "Receive upgrade inform successfully!";
                    ret_code = 200;
                    // Set upgrade status
                    ed->set_upgrade_status(UPGRADE_STATUS_PROCESS);
                    // Give current tasks some time to go into enclave queue.
                    sleep(10);
                    p_log->info("%s\n", ret_info.c_str());
                }
                else
                {
                    switch (spacex_status)
                    {
                        case SPACEX_UPGRADE_BLOCK_EXPIRE:
                            ret_info = "Block expired!Wait for next slot.";
                            ret_code = 402;
                            break;
                        case SPACEX_UPGRADE_NO_VALIDATE:
                            ret_info = "Necessary validation not completed!";
                            ret_code = 403;
                            break;
                        case SPACEX_UPGRADE_RESTART:
                            ret_info = "Cannot report due to restart!Wait for next slot.";
                            ret_code = 404;
                            break;
                        case SPACEX_UPGRADE_NO_FILE:
                            ret_info = "Cannot get files for check!Please check ipfs!";
                            ret_code = 405;
                            break;
                        default:
                            ret_info = "Unknown error.";
                            ret_code = 406;
                    }
                    g_block_height += REPORT_SLOT;
                }
            }
            res_json[HTTP_STATUS_CODE] = ret_code;
            res_json[HTTP_MESSAGE] = ret_info;

            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/report/work";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            spacex_status_t spacex_status = SPACEX_SUCCESS;
            uint8_t *hash_u = (uint8_t *)malloc(32);
            int tmp_int;
            int ret_code = 400;
            std::string ret_info;
            for (uint32_t i = 0; i < 32 / sizeof(tmp_int); i++)
            {
                tmp_int = rand();
                memcpy(hash_u + i * sizeof(tmp_int), &tmp_int, sizeof(tmp_int));
            }
            std::string block_hash = hexstring_safe(hash_u, 32);
            g_block_height += REPORT_SLOT;
            free(hash_u);
            if (SGX_SUCCESS != Ecall_gen_and_upload_work_report_test(global_eid, &spacex_status,
                    block_hash.c_str(), g_block_height + REPORT_SLOT))
            {
                res_json[HTTP_STATUS_CODE] = 400;
                ret_info = "Get signed work report failed!";
                p_log->err("%s\n", ret_info.c_str());
            }
            else
            {
                if (SPACEX_SUCCESS == spacex_status)
                {
                    // Send signed validation report to spacex chain
                    p_log->info("Send work report successfully!\n");
                    std::string work_str = EnclaveData::get_instance()->get_workreport();
                    res_json[HTTP_MESSAGE] = work_str;
                    res_json[HTTP_STATUS_CODE] = 200;
                    goto getcleanup;
                }
                if (spacex_status == SPACEX_BLOCK_HEIGHT_EXPIRED)
                {
                    ret_code = 401;
                    ret_info = "Block height expired.";
                    p_log->info("%s\n", ret_info.c_str());
                }
                else if (spacex_status == SPACEX_FIRST_WORK_REPORT_AFTER_REPORT)
                {
                    ret_code = 402;
                    ret_info = "Can't generate work report for the first time after restart";
                    p_log->info("%s\n", ret_info.c_str());
                }
                else
                {
                    ret_code = 403;
                    ret_info = "Get signed validation report failed!";
                    p_log->err("%s Error code: %x\n", ret_info.c_str(), spacex_status);
                }
            }
            res_json[HTTP_STATUS_CODE] = ret_code;
            res_json[HTTP_MESSAGE] = ret_info;
            goto getcleanup;
        }

        // --- Srd change API --- //
        cur_path = urlendpoint.base + "/report/result";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            res_json[HTTP_STATUS_CODE] = 200;
            std::string ret_info;
            // Confirm new file
            report_add_callback();
            ret_info = "Reporting result task has beening added!";
            res_json[HTTP_MESSAGE] = ret_info;

            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/validate/add_proof";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_add_validate_proof(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "validate add proof.";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/validate/srd";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_validate_srd_test(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "validate srd.";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/validate/srd_bench";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_validate_srd_bench(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "validate srd.";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/validate/file";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_validate_file_test(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "validate file.";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/validate/file_bench";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_validate_file_bench(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "validate file bench.";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/store_metadata";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_store_metadata(global_eid);
            res_json[HTTP_STATUS_CODE] = 200;
            res_json[HTTP_MESSAGE] = "store metadata";
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/test/add_file";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            long file_num = req_json["file_num"].ToInt();
            Ecall_test_add_file(global_eid, file_num);
            res_json[HTTP_MESSAGE] = "Add file successfully!";
            res_json[HTTP_STATUS_CODE] = 200;
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/test/delete_file";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            uint32_t file_num = req_json["file_num"].ToInt();
            Ecall_test_delete_file(global_eid, file_num);
            res_json[HTTP_MESSAGE] = "Delete file successfully!";
            res_json[HTTP_STATUS_CODE] = 200;
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/test/delete_file_unsafe";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            uint32_t file_num = req_json["file_num"].ToInt();
            Ecall_test_delete_file_unsafe(global_eid, file_num);
            EnclaveData::get_instance()->restore_file_info(reinterpret_cast<const uint8_t *>("{}"), 2);
            res_json[HTTP_MESSAGE] = "Delete file successfully!";
            res_json[HTTP_STATUS_CODE] = 200;
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/clean_file";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            Ecall_clean_file(global_eid);
            res_json[HTTP_MESSAGE] = "Clean file successfully!";
            res_json[HTTP_STATUS_CODE] = 200;
            goto getcleanup;
        }

        cur_path = urlendpoint.base + "/file_info";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            std::string hash = req_json["hash"].ToString();
            spacex_status_t spacex_status = SPACEX_SUCCESS;
            Ecall_get_file_info(global_eid, &spacex_status, hash.c_str());
            if (SPACEX_SUCCESS == spacex_status)
            {
                res_json[HTTP_MESSAGE] = edTest->get_file_info();
                res_json[HTTP_STATUS_CODE] = 200;
            }
            else
            {
                res_json[HTTP_MESSAGE] = "";
                res_json[HTTP_STATUS_CODE] = 400;
            }
        }

    getcleanup:

        return res_json;
    }


    // ----- Respond to POST request ----- //
    if(method.compare("POST") == 0)
    {
        cur_path = urlendpoint.base + "/storage/delete_sync";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            std::string ret_info;
            int ret_code = 400;
            // Delete file
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            std::string cid = req_json["cid"].ToString();
            // Check cid
            if (cid.size() != CID_LENGTH)
            {
                ret_info = "Delete file failed! Invalid cid:" + cid;
                p_log->err("%s\n", ret_info.c_str());
                ret_code = 400;
                res_json[HTTP_STATUS_CODE] = ret_code;
                res_json[HTTP_MESSAGE] = ret_info;
                goto postcleanup;
            }
            
            sgx_status_t sgx_status = SGX_SUCCESS;
            spacex_status_t spacex_status = SPACEX_SUCCESS;
            ret_info = "Deleting file failed!";
            if (SGX_SUCCESS != (sgx_status = Ecall_delete_file(global_eid, &spacex_status, cid.c_str())))
            {
                ret_code = 401;
                ret_info = "Delete file failed!";
                p_log->err("Delete file(%s) failed!Invoke SGX API failed!Error code:%lx\n", cid.c_str(), sgx_status);
            }
            else if (SPACEX_SUCCESS != spacex_status)
            {
                ret_code = 402;
                ret_info = "Delete file failed!";
                p_log->err("Delete file(%s) failed!Error code:%lx\n", cid.c_str(), spacex_status);
            }
            else
            {
                EnclaveData::get_instance()->del_file_info(cid);
                p_log->info("Delete file(%s) successfully!\n", cid.c_str());
                res_json[HTTP_STATUS_CODE] = 200;
                res_json[HTTP_MESSAGE] = "Deleting file successfully!";
                goto postcleanup;
            }

            res_json[HTTP_STATUS_CODE] = ret_code;
            res_json[HTTP_MESSAGE] = ret_info;

            goto postcleanup;
        }

        cur_path = urlendpoint.base + "/srd/set_change";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            int ret_code = 400;
            std::string ret_info;
            // Check input parameters
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            long change_srd_num = req_json["change"].ToInt();

            if (change_srd_num == 0)
            {
                p_log->info("Invalid change\n");
                ret_info = "Invalid change";
                ret_code = 400;
            }
            else
            {
                // Start changing srd
                if (SGX_SUCCESS != Ecall_srd_change_test(global_eid, change_srd_num, false))
                {
                    ret_info = "Change srd failed!";
                    ret_code = 401;
                }
                else
                {
                    ret_info = "Change srd successfully!";
                    ret_code = 200;
                }
                p_log->info("%s\n", ret_info.c_str());
            }
            res_json[HTTP_STATUS_CODE] = ret_code;
            res_json[HTTP_MESSAGE] = ret_info;
            goto postcleanup;
        }

        cur_path = urlendpoint.base + "/srd/change_real";
        if (req_route.size() == cur_path.size() && req_route.compare(cur_path) == 0)
        {
            int ret_code = 400;
            std::string ret_info;
            // Check input parameters
            json::JSON req_json = json::JSON::Load_unsafe(req_body);
            long change_srd_num = req_json["change"].ToInt();
            spacex_status_t spacex_status = SPACEX_SUCCESS;

            if (change_srd_num == 0)
            {
                p_log->info("Invalid change\n");
                ret_info = "Invalid change";
                ret_code = 400;
            }
            else
            {
                // Start changing srd
                if (SGX_SUCCESS == Ecall_srd_change_test(global_eid, change_srd_num, true)
                        && SPACEX_SUCCESS == spacex_status)
                {
                    ret_info = "Change srd successfully!";
                    ret_code = 200;
                }
                else
                {
                    ret_info = "Set srd change failed!";
                    ret_code = 400;
                }
            }
            res_json[HTTP_STATUS_CODE] = ret_code;
            res_json[HTTP_MESSAGE] = ret_info;
            goto postcleanup;
        }


    postcleanup:
        return res_json;
    }

    return res_json;
}
