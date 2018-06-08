#include "MySQLConnector.hpp"

#include <sstream>
#include <ctime>

thread_local bool is_mainthread = false;

//SQL Injection checks
//if you want to inject stuff from SQF you will need to break out the string for the table, key or value
//tables and key are escaped in `, value in '
//these never pop up in sqf by default, ' especially isnt wanted because it breaks parseSimpleArray
//so we are safe if we exclude these chars from said strings
#define CHECK_KEY(x) if (x.find('`') != std::string::npos /*|| x.find('´') != std::string::npos*/) return SQF::RET_FAIL();
#define CHECK_VALUE(x) if (x.find('\'') != std::string::npos) return SQF::RET_FAIL();

MySQLConnector::MySQLConnector() {
    
}

MySQLConnector::~MySQLConnector() {
    
}

bool beginsWith(const std::string& s1, const std::string& s2) {
    if (s1.size() < s2.size()) return false;
    for (size_t i = 0; i < s2.size(); i++) {
        if (s1[i] != s2[i]) return false;
    }
    return true;
}

/*
Possible Mysql errors in this case:

select db:
unknown databse -> solution: try to create the database and reselect, exit if that still fails

connect:
simply fails -> exit

select:
Unknown Table -> create table (maybe even async) but return empty

You have an error in your SQL syntax -> return async

insert:

delete:

update:


*/

bool transformKey(const std::string& _in, std::string& _table, std::string& _key) {
    auto splitIdx = _in.find_last_of(':');
    if (splitIdx == std::string::npos) return false;

    _table = _in.substr(0, splitIdx);
    for (auto& _x : _table) {
        if (_x == ':') {
            _x = '_';
        }
    }
    _key = _in.substr(splitIdx + 1);

    return true;
}

inline std::string makeCreateTable(const std::string& tableName) {
    return ("CREATE TABLE `" + tableName + "` (\
        `key` BIGINT(255) UNSIGNED NOT NULL,\
        `value` LONGTEXT NULL,\
        `TTL` TIMESTAMP NULL DEFAULT NULL,\
        PRIMARY KEY(`key`),\
        UNIQUE INDEX `UNIQUE` (`key`)\
        )\
        ENGINE = InnoDB");
}

bool MySQLConnector::createTable(MYSQL * con, const std::string& tablename) {

    std::string execQry = makeCreateTable(tablename);

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        return true;
    }
    else {
        this->config.logger->log("Could not create table: " + std::string(mysql_error(con)));
        return false;
    }

};

bool MySQLConnector::init(EpochlibConfigDB _config) {

    this->config = _config;

    is_mainthread = true;

    mysql_library_init(0, NULL, NULL);
    this->mysql = mysql_init(NULL);

    //connect
    if (!mysql_real_connect(
        this->mysql,
        this->config.ip.c_str(),
        this->config.user.c_str(),
        this->config.password.c_str(),
        NULL,
        this->config.port,
        NULL,
        0
    )) {
        return false;
    }

    //SELECT DB
    if (mysql_select_db(this->mysql, this->config.dbIndex.c_str())) {
        
        //COULD NOT SELECT
        this->config.logger->log("Could not select database in first try");

        //TRY TO CREATE DB
        std::string createQry = "CREATE DATABASE " + this->config.dbIndex;
        if (mysql_query(mysql, createQry.c_str())) {
            this->config.logger->log("Could not create database");
            //COULD NOT CREATE
            mysql_close(mysql);
            return false;
        }
        else {
            //CREATED
            this->config.logger->log("Database created");
        }

        //SELECTDB
        if (mysql_select_db(mysql, this->config.dbIndex.c_str())) {
            //COULD NOT SELECT
            this->config.logger->log("Could not select database in second try");
            mysql_close(mysql);
            return false;
        }
        else {
            //SELECTED
        }
    }

    this->config.logger->log("Database selected!");

    return true;
}

std::string MySQLConnector::getRange(const std::string& _key, const std::string& _value, const std::string& _value2) {
    return get(_key);
}

MYSQL * MySQLConnector::setupCon() {
    //setup connection
    if (is_mainthread) {
        return this->mysql;
    }
    else {
        MYSQL * con = mysql_init(NULL);
        //connect
        if (!mysql_real_connect(
            con,
            this->config.ip.c_str(),
            this->config.user.c_str(),
            this->config.password.c_str(),
            this->config.dbIndex.c_str(),
            this->config.port,
            NULL,
            0
        )) {
            return nullptr;
        }
        else {
            return con;
        }
    }
}

void MySQLConnector::closeCon(MYSQL * con) {
    if (!is_mainthread) {
        mysql_close(con);
    }
}

bool MySQLConnector::__get(const std::string& _table, const std::string& _key, std::string& _result) {
    
    
    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return false;
    }
    bool _success = false;

    std::string execQry = "SELECT value FROM `" + _table + "` WHERE `key`='" + _key + "' AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {

        MYSQL_RES * _mresult = NULL;
        _mresult = mysql_store_result(con);

        if (!_mresult) {

            //handle empty result
            _success = true;

        }
        else {

            //num cols
            unsigned int fieldcount = mysql_num_fields(_mresult);

            //num rows
            unsigned long long int rowcount = mysql_num_rows(_mresult);

            if (!fieldcount || !rowcount) {

                //handle empty result
                _success = true;

            }
            else {

                //we only gonna fetch one row (there shouldnt be multiple)
                //MYSQL_FIELD * fields = mysql_fetch_fields(_result); //TODO should we add type checking for the key?..
                MYSQL_ROW row = mysql_fetch_row(_mresult);

                _success = true;
                auto _pos = row[0];
                if (_pos == NULL) {
                    //NULL == nil
                    _result = "nil";
                }
                else {
                    _result = _pos;
                }

            }

            mysql_free_result(_mresult);

        }

    }
    else {
        this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
    }

    closeCon(con);
    return _success;
}

std::string MySQLConnector::get(const std::string& _key) {

    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    std::string result;
    auto _success = __get(table,key,result);

    if(!_success) {
        //handle the error (TODO might need to create the table)
        return SQF::RET_FAIL();
    }
    else {

        SQF returnSqf;
        returnSqf.push_number(SQF_RETURN_STATUS::SUCCESS);
        if (result.empty() || result[0] != '[') {
            returnSqf.push_str(result);
        }
        else {
            returnSqf.push_array(result);
        }
        return returnSqf.toArray();
    }


}

std::string MySQLConnector::getTtl(const std::string& _key) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "SELECT `value`, UNIX_TIMESTAMP(`ttl`), UNIX_TIMESTAMP(CURRENT_TIMESTAMP()) FROM `" + table + "` WHERE `key`='" + key + "' AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    SQF returnSQF;

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {

        MYSQL_RES * _result = NULL;
        _result = mysql_store_result(con);

        if (!_result) {

            //handle empty result
            returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);
            returnSQF.push_number(-2);


        }
        else {

            //num cols
            unsigned int fieldcount = mysql_num_fields(_result);

            //num rows
            unsigned long long int rowcount = mysql_num_rows(_result);

            if (fieldcount != 3 || !rowcount) {

                //handle empty result
                returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);
                returnSQF.push_number(-2);

            }
            else {

                //we only gonna fetch one row (there shouldnt be multiple)
                //MYSQL_FIELD * fields = mysql_fetch_fields(_result); //TODO should we add type checking for the key?..
                MYSQL_ROW row = mysql_fetch_row(_result);

                returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);

                auto _val = row[0];
                auto _ttl = row[1];
                auto _cts = row[2];
                if (_ttl == NULL || _cts == NULL) {
                    returnSQF.push_number(-1);
                }
                else {
                    try {
                        returnSQF.push_number(std::atoll(_ttl) - std::atoll(_cts));
                    }
                    catch (...) {
                        returnSQF.push_number(-1);
                    }
                }

                if (_val == NULL || _val[0] != '[') {
                    returnSQF.push_str(_val);
                }
                else {
                    returnSQF.push_array(_val);
                }

            }

            mysql_free_result(_result);

        }

    }
    else {
        this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
        returnSQF.push_number(SQF_RETURN_STATUS::FAIL);
    }

    closeCon(con);
    return returnSQF.toArray();
}

std::string MySQLConnector::set(const std::string& _key, const std::string& _value) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);
    
    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);
    CHECK_VALUE(_value);
    
    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "INSERT INTO `" + table + "`(`key`,`value`)VALUES('" + key + "', '" + _value + "') ON DUPLICATE KEY UPDATE value = VALUES(value)";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        closeCon(con);
        return SQF::RET_SUCCESS();
    }
    else {

        //try to create table and retry
        createTable(con, table);

        if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
            closeCon(con);
            return SQF::RET_SUCCESS();
        }
        else {
            //DIdnt work again
            this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
            closeCon(con);
            return SQF::RET_FAIL();
        }

    }

}

std::string MySQLConnector::setex(const std::string& _key, const std::string& _ttl, const std::string& _value) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);
    CHECK_VALUE(_value);


    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "INSERT INTO `" + table + "`(`key`,`value`,`ttl`)VALUES('" + key + "','" + _value + "',DATE_ADD(NOW(),INTERVAL "+ _ttl +" SECOND)) ON DUPLICATE KEY UPDATE value = VALUES(value), ttl = VALUES(ttl)";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        closeCon(con);
        return SQF::RET_SUCCESS();
    }
    else {
        
        //try to create table and retry
        createTable(con, table);

        if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
            closeCon(con);
            return SQF::RET_SUCCESS();
        }
        else {
            //DIdnt work again
            this->config.logger->log("Call failed: " + std::string(mysql_error(con)));

            closeCon(con);
            return SQF::RET_FAIL();
        }

    }

}

std::string MySQLConnector::expire(const std::string& _key, const std::string& _ttl) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }
    
    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);
    CHECK_VALUE(_ttl);
    /* TODO CHECK if thats needed
    try {
        long long save_var = std::stoll(_ttl);
    }
    catch (...) {
        return SQF::RET_FAIL();
    }
    */
    
    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "INSERT INTO `" + table + "`(`key`,`value`,`ttl`)VALUES('" + key + "','',DATE_ADD(NOW(),INTERVAL " + _ttl + " SECOND)) ON DUPLICATE KEY UPDATE ttl = VALUES(ttl)";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        closeCon(con);
        return SQF::RET_SUCCESS();
    }
    else {
        
        this->config.logger->log("Call failed: " + std::string(mysql_error(con)) + "\n Trying to create table.");

        //try to create table and retry
        if (createTable(con, table)) {

            if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
                closeCon(con);
                return SQF::RET_SUCCESS();
            }
            else {
                //Didnt work again
                this->config.logger->log("Call failed: " + std::string(mysql_error(con)));

                closeCon(con);
                return SQF::RET_FAIL();
            }
        }

        return SQF::RET_FAIL();
    }
}

std::string MySQLConnector::setbit(const std::string& _key, const std::string& _bitindex, const std::string& _value) {
    
    if (_value.empty()) {
        this->config.logger->log("SetBIT had empty value");
        return SQF::RET_FAIL();
    }

    int bitIndex = -1;
    try {
        bitIndex = std::stoi(_bitindex);
    }
    catch (...) {}
    
    if (bitIndex < 0) {
        this->config.logger->log("BitIndex was less than 0");
        return SQF::RET_FAIL();
    }

    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);
    CHECK_VALUE(_value);

    std::string result;
    auto _success = __get(table, key, result);

    std::string bitstring;
    if (_success) {
        bitstring = result;
    }

    if (bitstring.size() > bitIndex) {
        bitstring[bitIndex] = _value[0];
    }
    else {
        size_t numberOfZerosToFill = bitIndex - bitstring.size();
        for (size_t i = 0; i < numberOfZerosToFill; i++) {
            bitstring += '0';
        }
        bitstring += _value[0];
    }

    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }


    std::string execQry = "INSERT INTO `" + table + "`(`key`,`value`)VALUES('" + key + "', '" + bitstring + "') ON DUPLICATE KEY UPDATE value = VALUES(value)";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        closeCon(con);
        return SQF::RET_SUCCESS();
    }
    else {
       
        //try to create table and retry
        createTable(con, table);

        if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
            closeCon(con);
            return SQF::RET_SUCCESS();
        }
        else {
            //DIdnt work again
            this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
            closeCon(con);
            return SQF::RET_FAIL();
        }

    }

}

std::string MySQLConnector::getbit(const std::string& _key, const std::string& _value) {
    
    int bitIndex = -1;

    try {
        bitIndex = std::stoi(_value);
    }
    catch (...) {};

    if (bitIndex < 0) {
        this->config.logger->log("Get Bit Index was negative");
        return SQF::RET_FAIL();
    }

    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    std::string result;
    auto _success = __get(table, key, result);

    if (_success) {
        SQF returnSQF;
        returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);
        returnSQF.push_str(result.size() > bitIndex ? (std::string({ result[bitIndex] })) : "0");
        return returnSQF.toArray();
    }
    else {
        return SQF::RET_FAIL();
    }

}

std::string MySQLConnector::exists(const std::string& _key) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    std::string result;
    auto _success = __get(table, key, result);

    if (!_success || result.empty()) {
        //handle the error (TODO might need to create the table)
        return SQF::RET_FAIL();
    }
    else {
        return SQF::RET_SUCCESS();
    }

}

std::string MySQLConnector::del(const std::string& _key) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key");
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "DELETE FROM `" + table + "` WHERE `key`='" + key + "'";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        closeCon(con);
        
        //TODO Return Rows affected
        return "[1,\"1\"]";
    }
    else {
        this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
        closeCon(con);
        return SQF::RET_FAIL();
    }

}

std::string MySQLConnector::ping() {
    
    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    if (mysql_ping(con)) {
        closeCon(con);
        return SQF::RET_FAIL();
    }
    else {
        closeCon(con);
        return SQF::RET_SUCCESS();
    }

}

std::string MySQLConnector::lpopWithPrefix(const std::string& _prefix, const std::string& _key) {
    
    //NOT NEEDED
    return SQF::RET_FAIL();

}

std::string MySQLConnector::ttl(const std::string& _key) {
    
    std::string table;
    std::string key;
    bool success = transformKey(_key, table, key);

    if (!success) {
        this->config.logger->log("Could not transfrom key"); 
        return SQF::RET_FAIL();
    }

    //injection checks
    CHECK_KEY(table);
    CHECK_KEY(key);

    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "SELECT UNIX_TIMESTAMP(`ttl`), UNIX_TIMESTAMP(CURRENT_TIMESTAMP()) FROM `" + table + "` WHERE `key`='" + key + "' AND (`ttl` IS NULL OR `ttl` > CURRENT_TIMESTAMP())";

    SQF returnSQF;

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {

        MYSQL_RES * _result = NULL;
        _result = mysql_store_result(con);

        if (!_result) {

            //handle empty result
            returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);
            returnSQF.push_number(-2);

        }
        else {

            //num cols
            unsigned int fieldcount = mysql_num_fields(_result);

            //num rows
            unsigned long long int rowcount = mysql_num_rows(_result);

            if (fieldcount != 2 || !rowcount) {

                //handle empty result
                returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);
                returnSQF.push_number(-2);

            }
            else {

                //we only gonna fetch one row (there shouldnt be multiple)
                //MYSQL_FIELD * fields = mysql_fetch_fields(_result); //TODO should we add type checking for the key?..
                MYSQL_ROW row = mysql_fetch_row(_result);

                returnSQF.push_number(SQF_RETURN_STATUS::SUCCESS);

                auto _ttl = row[0];
                auto _cts = row[1];
                if (_ttl == NULL || _cts == NULL) {
                    returnSQF.push_number(-1);
                }
                else {
                    try {
                        returnSQF.push_number(std::stoll(_ttl) - std::stoll(_cts));
                    }
                    catch (...) {
                        returnSQF.push_number(-1);
                    }
                }

            }

            mysql_free_result(_result);

        }

    }
    else {
        this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
        returnSQF.push_number(SQF_RETURN_STATUS::FAIL);
    }

    closeCon(con);
    return returnSQF.toArray();

}

std::string MySQLConnector::log(const std::string& _key, const std::string& _value) {
    char formatedTime[64];
    time_t t = time(0);
    struct tm * currentTime = localtime(&t);

    strftime(formatedTime, 64, ":%Y%m%d%H%M%S", currentTime);

    return set(_key+formatedTime, _value);

}


std::string MySQLConnector::increaseBancount() {
    
    MYSQL * con = setupCon();
    if (con == nullptr) {
        this->config.logger->log("Could not setup database connection for call");
        return SQF::RET_FAIL();
    }

    std::string execQry = "INSERT INTO `AH`(`key`,`value`)VALUES('bcnt','1') ON DUPLICATE KEY UPDATE value = value + VALUES(value)";

    if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
        return SQF::RET_SUCCESS();
        closeCon(con);
    }
    else {
        
        //try to create table and retry
        createTable(con, "AH");

        if (!mysql_real_query(con, execQry.c_str(), execQry.size())) {
            return SQF::RET_SUCCESS();
            closeCon(con);
        }
        else {
            //DIdnt work again
            this->config.logger->log("Call failed: " + std::string(mysql_error(con)));
            closeCon(con);
            return SQF::RET_FAIL();
        }

    }

}