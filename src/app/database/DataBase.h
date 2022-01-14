#ifndef _SPACEX_DATABASE_H_
#define _SPACEX_DATABASE_H_

#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "CrustStatus.h"
#include "Config.h"
#include "Log.h"
#include "FileUtils.h"

namespace spacex
{

class DataBase
{
public:
    ~DataBase();
    static DataBase *get_instance();
    spacex_status_t add(std::string key, std::string value);
    spacex_status_t del(std::string key);
    spacex_status_t set(std::string key, std::string value);
    spacex_status_t get(std::string key, std::string &value);

private:
    static DataBase *database;
    DataBase() {}
    DataBase(const DataBase &);
    DataBase& operator = (const DataBase &);
    leveldb::DB *db = NULL;
    leveldb::WriteOptions write_opt;
};

} // namespace spacex

#endif /* !_SPACEX_DATABASE_H_ */
