
#include <database/SQLiteConnector.hpp>

SQLiteConnector::SQLiteConnector(const DBConfig& config) {
    this->config = config;

    std::lock_guard<std::mutex> lock(SQLiteConnector_Detail::db_holder_refs_mutex);

    SQLiteConnector_Detail::db_holder_ref ref;
    try {

        if (SQLiteConnector_Detail::db_holder_refs.find(config.dbname) == SQLiteConnector_Detail::db_holder_refs.end()) {
            auto holder = std::make_shared< SQLiteConnector_Detail::SQLiteDBHolder >();
            holder->SQLiteDB = std::make_shared<SQLite::Database>(config.dbname + ".db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            SQLiteConnector_Detail::db_holder_refs.insert(config.dbname, ref);
            ref = holder;
        }
        else {
            ref = SQLiteConnector_Detail::db_holder_refs.at(config.dbname);
        }

    }
    catch (SQLite::Exception& e) {
        throw std::runtime_error(std::string("Error creating the sqlite db: ") + e.getErrorStr());
    }

    bool exists;
    try {
        exists = SQLiteConnector_Detail::SQLiteDB->tableExists(this->defaultKeyValTableName);
    }
    catch (SQLite::Exception& e) {
        exists = false;
    }
    
    if (!exists) {
        INFO("Selecting schema failed, trying to create it..");
        
        try {
            auto res = SQLiteConnector_Detail::SQLiteDB->exec("CREATE TABLE ? (\
                `key` TEXT NOT NULL PRIMARY KEY UNIQUE,\
                `value` TEXT NOT NULL,\
                `TTL` INT NULL DEFAULT NULL,\
            )");
            INFO("Table checked!");
            INFO("Database ready!");
        }
        catch (SQLite::Exception& e) {
            throw std::runtime_error("Failed to create the key value table");
        }
         
    }
    else {
        throw std::runtime_error("Could not create to the sqlite database file");
    }
}

SQLiteConnector::~SQLiteConnector() {

}

/**
*  DB GET
*  Key
**/
std::string SQLiteConnector::get(const std::string& key) {
    std::string execQry = "SELECT `value` FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > strftime('%s','now'))";
}

std::string SQLiteConnector::getRange(const std::string& key, unsigned int from, unsigned int to) {
    if (from > to) {
        std::swap(from, to);
    }

    auto x = this->get(key);

    if (x.size() < to) {
        return (from >= (x.size() - 1)) ? "" : std::string(x.begin() + from, x.end());
    }
    else {
        return (from >= (x.size() - 1)) ? "" : std::string(x.begin() + from, x.begin() + to);
    }
}

std::pair<std::string, int> SQLiteConnector::getWithTtl(const std::string& key) {
    std::string execQry = "SELECT `value`, `ttl`, strftime('%s','now') FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > strftime('%s','now'))";
}

bool SQLiteConnector::exists(const std::string& key) {

}

/**
*  DB SET / SETEX
*  Key
**/
bool SQLiteConnector::set(const std::string& key, const std::string& value) {

}

bool SQLiteConnector::setEx(const std::string& key, int ttl, const std::string& value) {

}

bool SQLiteConnector::expire(const std::string& key, int ttl) {

}

/**
*  DB DEL
*  Key
**/
bool SQLiteConnector::del(const std::string& key) {

}

/**
*  DB PING
**/
std::string SQLiteConnector::ping() {

}

/**
*  DB TTL
*  Key
**/
int SQLiteConnector::ttl(const std::string& key) {

}
