
#include <database/SQLiteConnector.hpp>

SQLiteConnector::SQLiteConnector(const DBConfig& config) {
    this->config = config;

    std::lock_guard<std::mutex> lock(SQLiteConnector_Detail::SQLiteDBMutex);

    try {
        if (!SQLiteConnector_Detail::SQLiteDB) {
            SQLiteConnector_Detail::SQLiteDB = std::make_shared<SQLite::Database>(config.dbname+".db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
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
