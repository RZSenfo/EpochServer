
#include <database/SQLiteConnector.hpp>

SQLiteConnector::SQLiteConnector(const DBConfig& config) {
    this->config = config;


    SQLiteCon_Detail::DbHolderRef ref;
    try {

        std::lock_guard<std::mutex> lock(SQLiteCon_Detail::dbHolderRefsMutex);
        if (SQLiteCon_Detail::dbHolderRefs.find(config.dbname) == SQLiteCon_Detail::dbHolderRefs.end()) {
            SQLiteCon_Detail::DbHolderRef holder = std::make_shared< SQLiteCon_Detail::SQLiteDBHolder >();
            holder->SQLiteDB = std::make_shared<SQLite::Database>(config.dbname + ".db3", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            SQLiteCon_Detail::dbHolderRefs.insert({ config.dbname, holder });
            ref = holder;
        }
        else {
            ref = SQLiteCon_Detail::dbHolderRefs.at(config.dbname);
        }

    }
    catch (SQLite::Exception& e) {
        WARNING(std::string("Error during SQLLite Setup: ") + e.what());
    }

    if (!ref || !ref->SQLiteDB) {
        throw std::runtime_error("Error creating the sqlite db");
    }

    this->holderRef = ref;

    std::lock_guard<std::mutex> lock(ref->SQLiteDBMutex);
    auto dbRef = ref->SQLiteDB;
    
    bool exists;
    try {
        exists = dbRef->tableExists(this->defaultKeyValTableName);
    }
    catch (SQLite::Exception& e) {
        exists = false;
    }
    
    if (!exists) {
        INFO("Selecting schema failed, trying to create it..");
        
        try {
            auto res = dbRef->exec("CREATE TABLE ? (\
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
std::vector< std::string > SQLiteConnector::keys(const std::string& prefix) {

    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "SELECT key FROM ? WHERE key LIKE ? AND (ttl IS NULL OR ttl > strftime('%s','now'))");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, prefix + "%");

        std::vector< std::string > ret;

        while (query.executeStep()) {
            ret.emplace_back(query.getColumn(0).getString());
        }

        return ret;
    }
    catch (SQLite::Exception& e) {
        return {};
    }
}

std::string SQLiteConnector::get(const std::string& key) {
    
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");
    
    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "SELECT value FROM ? WHERE key=? AND (ttl IS NULL OR ttl > strftime('%s','now'))");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);

        query.executeStep();

        return query.getColumn(0);
    }
    catch (SQLite::Exception& e) {
        return "";
    }
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
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "SELECT value, ttl, strftime('%s','now') FROM ? WHERE key=? AND (ttl IS NULL OR ttl > strftime('%s','now'))");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);

        query.executeStep();

        return { query.getColumn(0), static_cast<long int>(query.getColumn(1)) - static_cast<long int>(query.getColumn(2)) };

    }
    catch (SQLite::Exception& e) {
        return { "", -1 };
    }
}

bool SQLiteConnector::exists(const std::string& key) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "SELECT value FROM ? WHERE key=? AND (ttl IS NULL OR ttl > strftime('%s','now'))");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);

        return query.executeStep();
    }
    catch (SQLite::Exception& e) {
        return false;
    }
}

/**
*  DB SET / SETEX
*  Key
**/
bool SQLiteConnector::set(const std::string& key, const std::string& value) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "INSERT INTO ? (key,value) VALUES (?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);
        query.bind(3, value);

        return query.exec() != 0;
    }
    catch (SQLite::Exception& e) {
        return false;
    }
}

bool SQLiteConnector::setEx(const std::string& key, int ttl, const std::string& value) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "INSERT INTO ? (key,value,ttl) VALUES (?,?, strftime('%s','now') + ?) ON CONFLICT(key) DO UPDATE SET value=excluded.value, ttl=excluded.ttl");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);
        query.bind(3, value);
        query.bind(4, ttl);

        return query.exec() != 0;
    }
    catch (SQLite::Exception& e) {
        return false;
    }
}

bool SQLiteConnector::expire(const std::string& key, int ttl) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "UPDATE ? SET ttl=strftime('%s','now')+? WHERE key=?");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);
        query.bind(3, ttl);

        return query.exec() != 0;
    }
    catch (SQLite::Exception& e) {
        return false;
    }
}

/**
*  DB DEL
*  Key
**/
bool SQLiteConnector::del(const std::string& key) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "DELETE FROM ? WHERE key=?");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);

        return query.exec() != 0;
    }
    catch (SQLite::Exception& e) {
        return false;
    }
}

/**
*  DB PING
**/
std::string SQLiteConnector::ping() {
    return std::to_string(this->holderRef && this->holderRef->SQLiteDB);
}

/**
*  DB TTL
*  Key
**/
int SQLiteConnector::ttl(const std::string& key) {
    if (!this->holderRef || !this->holderRef->SQLiteDB) throw std::runtime_error("SQLite DB undefined");

    std::lock_guard<std::mutex> lock(this->holderRef->SQLiteDBMutex);

    try {

        SQLite::Statement query(*holderRef->SQLiteDB, "SELECT ttl, strftime('%s','now') FROM ? WHERE key=? AND (ttl IS NULL OR ttl > strftime('%s','now'))");
        query.bind(1, this->defaultKeyValTableName);
        query.bind(2, key);

        query.executeStep();

        return static_cast<long int>(query.getColumn(0)) - static_cast<long int >(query.getColumn(1));

    }
    catch (SQLite::Exception& e) {
        return -1;
    }
}
