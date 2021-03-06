#ifndef _SPACEX_FILE_UTILS_H_
#define _SPACEX_FILE_UTILS_H_

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <vector>
#include <stack>
#include <unordered_map>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <errno.h>
#include <sys/types.h>
#include <experimental/filesystem>
#include <exception>

#include "Config.h"
#include "DataBase.h"

std::vector<std::string> get_files_under_path(const std::string &path);
std::vector<std::string> get_folders_under_path(const std::string &path);
int rm_dir(const std::string &dir_full_path);
int rm(const std::string &path);
size_t get_free_space_under_directory(const std::string &path);
spacex_status_t rename_dir(const std::string &old_path, const std::string &new_path);
spacex_status_t create_directory(const std::string &path);
std::vector<std::string> get_sub_folders_and_files(const char *path);
spacex_status_t get_file(const char *path, uint8_t **p_data, size_t *data_size);
long get_file_size(const char *path, store_type_t type);
bool is_file_exist(const char *path, store_type_t type);
std::string get_real_path_by_type(const char *path, store_type_t type);
spacex_status_t save_file(const char *path, const uint8_t *data, size_t data_size);
spacex_status_t save_file_ex(const char *path, const uint8_t *data, size_t data_size, mode_t mode, save_file_type_t type);
size_t get_total_space_under_dir_k(const std::string &path);
size_t get_total_space_under_dir_m(const std::string &path);
size_t get_total_space_under_dir_g(const std::string &path);
size_t get_avail_space_under_dir_k(const std::string &path);
size_t get_avail_space_under_dir_m(const std::string &path);
size_t get_avail_space_under_dir_g(const std::string &path);
size_t get_file_or_folder_size(const std::string &path);
size_t get_file_size_by_cid(const std::string &cid);

#endif /* !_SPACEX_FILE_UTILS_H_ */
