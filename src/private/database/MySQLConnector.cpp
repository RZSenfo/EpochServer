#include <database/MySQLConnector.hpp>

#include <sstream>
#include <ctime>

MySQLConnector::MySQLConnector() {}

MySQLConnector::~MySQLConnector() {}

inline std::string makeCreateTable(const std::string& tableName) {
    return 
}

bool MySQLConnector::__createKeyValueTable(const std::string& tableName) {

    std::string query = (
        "CREATE TABLE `" + tableName + "` (\
            `key` BIGINT(255) UNSIGNED NOT NULL,\
            `value` LONGTEXT NULL,\
            `TTL` TIMESTAMP NULL DEFAULT NULL,\
            PRIMARY KEY(`key`),\
            UNIQUE INDEX `UNIQUE` (`key`)\
        )\
        ENGINE = InnoDB"
    );
    
    auto result = con->query(query);
    
    if (result->)
     
    else {
        LOG("Could not create table: " + std::string(mysql_error(con)));
        return false;
    }

};

bool MySQLConnector::init(DBConfig _config) {

    this->config = _config;

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

MY_SQL * MySQLConnector::setupCon() {
    //setup connection
    if (is_mainthread) {
        return this->mysql;
    }
    else {
        MY_SQL * con = mysql_init(NULL);
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