#include <database/MySQLConnector.hpp>

#include <sstream>
#include <ctime>

MySQLConnector::MySQLConnector(const DBConfig& config) {
    this->config = config;

    if (MySQLConnector_Detail::is_first_connection) {
        MySQLConnector_Detail::is_first_connection = false;
        mysql_library_init(0, NULL, NULL);
    }

    //CONNECT
    mariadb::account_ref acc = mariadb::account::create(
        config.ip,
        config.user,
        config.password,
        "",
        config.port
    );
    this->con = mariadb::connection::create(acc);
    
    if (this->con->connect()) {
        if (!this->con->set_schema(config.dbname)) {
            INFO("Selecting schema failed, trying to create it..");
            auto statement = con->create_statement("CREATE DATABASE ?;");
            statement->set_string(0, config.dbname);
            auto res = statement->query();
            
            if (!res || res->error_no() != 0) {
                throw std::runtime_error("Failed to create the database");
            }
            else {
                if (!this->con->set_schema(config.dbname)) {
                    throw std::runtime_error("Failed to create the database");
                }
            }
        }
    }
    else {
        throw std::runtime_error("Could not connect to the database server");
    }

    // throws before if not connected
    INFO("Database selected! Checking table...");
    if (this->__createKeyValueTable(this->defaultKeyValTableName)) {
        INFO("Table checked!");
        INFO("Database ready!");
    }
    else {
        throw std::runtime_error("Could not connect to the database server");
    }

}

MySQLConnector::~MySQLConnector() {}

bool MySQLConnector::__createKeyValueTable(const std::string& tableName) {

    std::string queryExists = "SELECT *\
        FROM information_schema.tables\
        WHERE table_schema = ?\
        AND table_name = ?\
        LIMIT 1;";

    auto statement = con->create_statement(queryExists);
    statement->set_string(0, this->config.dbname);
    statement->set_string(1, tableName);
    auto res = statement->query();

    if (!res || res->error_no() != 0) {
        
        std::string queryCreate = "CREATE TABLE ? (\
                `key` BIGINT(255) UNSIGNED NOT NULL,\
                `value` LONGTEXT NULL,\
                `TTL` TIMESTAMP NULL DEFAULT NULL,\
                PRIMARY KEY(`key`),\
                UNIQUE INDEX `UNIQUE` (`key`)\
            )\
            ENGINE = InnoDB";

        statement = con->create_statement(queryCreate);
        statement->set_string(0, tableName);
        res = statement->query();
        
        if (!res || res->error_no() != 0) {
            WARNING("Could not create table: " + res->error());
            return false;
        }
        else {
            INFO("Created key value table..");
            return true;
        }
    }
    else {
        return true;
    }
};

std::string MySQLConnector::get(const std::string& _key) {
    std::string execQry = "SELECT `value` FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);

    auto res = statement->query();
    if (!res || res->error_no != 0 || res->row_count == 0) {
        if (extendedLogging) WARNING("Call failed: " + (res ? res->error() : "empty result"));
        return "";
    }
    else {
        return res->get_string(0);
    }
}

std::string MySQLConnector::getRange(const std::string& _key, unsigned int from, unsigned int to) {
    
    if (from > to) {
        std::swap(from, to);
    }
    
    auto x = get(_key);

    if (x.size() < to) {
        return (from >= (x.size()-1)) ? "" : std::string(x.begin() + from, x.end());
    }
    else {
        return (from >= (x.size() - 1)) ? "" : std::string(x.begin() + from, x.begin() + to);
    }
}

std::pair<std::string, int> MySQLConnector::getWithTtl(const std::string& _key) {
    
    std::string execQry = "SELECT `value`, UNIX_TIMESTAMP(`ttl`), UNIX_TIMESTAMP(CURRENT_TIMESTAMP()) FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);
    auto res = statement->query();
    if (!res || res->error_no != 0 || res->row_count == 0) {
        if (extendedLogging) WARNING("Call failed: " + (res ? res->error() : "empty result"));
        return { "", 0 };
    }
    else {
        return { res->get_string(0), res->get_signed64(1) - res->get_signed64(2) };
    }
    
}

bool MySQLConnector::set(const std::string& _key, const std::string& _value) {
    
    std::string execQry = "INSERT INTO ? (`key`,`value`) VALUES (?,?) ON DUPLICATE KEY UPDATE value = VALUES(value)";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);
    statement->set_string(2, _value);
    return statement->execute();

}

bool MySQLConnector::setEx(const std::string& _key, int _ttl, const std::string& _value) {
    
    std::string execQry = "INSERT INTO ? (`key`,`value`,`ttl`)VALUES(?,?,DATE_ADD(NOW(),INTERVAL ? SECOND)) ON DUPLICATE KEY UPDATE value = VALUES(value), ttl = VALUES(ttl)";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);
    statement->set_string(2, _value);
    statement->set_signed32(3, _ttl);
    return statement->execute();
}

bool MySQLConnector::expire(const std::string& _key, int _ttl) {
    
    std::string execQry = "UPDATE ? SET `ttl`=DATE_ADD(NOW(),INTERVAL ? SECOND)) WHERE `key`=?";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_unsigned32(1, _ttl);
    statement->set_string(2, _key);
    return statement->execute();
}

bool MySQLConnector::exists(const std::string& _key) {
    
    std::string execQry = "SELECT `value` FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);
    auto res = statement->query();
    if (!res || res->error_no != 0 || res->row_count == 0) {
        return false;
    }
    else {
        return true;
    }

}

bool MySQLConnector::del(const std::string& _key) {
    
    std::string execQry = "DELETE FROM ? WHERE `key`=?";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->config.dbname);
    statement->set_string(1, this->defaultKeyValTableName);
    return statement->execute();

}

std::string MySQLConnector::ping() {
    
    return std::to_string(con->connected());

}

int MySQLConnector::ttl(const std::string& _key) {
    
    std::string execQry = "SELECT UNIX_TIMESTAMP(`ttl`), UNIX_TIMESTAMP(CURRENT_TIMESTAMP()) FROM ? WHERE `key`=? AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    auto statement = con->create_statement(execQry);
    statement->set_string(0, this->defaultKeyValTableName);
    statement->set_string(1, _key);
    auto res = statement->query();
    if (!res || res->error_no != 0 || res->row_count == 0) {
        if (extendedLogging) WARNING("Call failed: " + (res ? res->error() : "empty result"));
        return -1;
    }
    else {
        return res->get_signed64(0) - res->get_signed64(1);
    }

}

