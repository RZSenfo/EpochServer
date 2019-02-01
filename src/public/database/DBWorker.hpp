#pragma once

#ifndef __DATABASE_WORKER_HPP__
#define __DATABASE_WORKER_HPP__

#include <vector>
#include <thread>
#include <mutex>
#include <shared_mutex>

#include <database/DBConfig.hpp>
#include <database/DBConnector.hpp>
#include <database/MySQLConnector.hpp>
#include <database/RedisConnector.hpp>
#include <database/SQLiteConnector.hpp>
#include <main.hpp>

/**
* Type of returned values from the database
*
* NOTE: The actual database result is always a string
* a bool is provided for calls like exists, set (success of execution), etc
* a double is provided for ttl
*
* to cast any string into a valid arma value, simply wrap the received string into brackets "[" "]" and parseSimpleArray it
* \example
*   std::string result;
*   result = "[" + result + "]"
*   game_value gv_result = intercept::sqf::parse_simple_array(result);
*   
* I did it this way to keep the database as simple as possible
**/
typedef std::variant<std::string, bool, float, std::pair<std::string, float>> DBReturn;

/**
* Type/Variant of the callback - string: missionnamespace variable, code: actual function code, lambda: use of dbworker as API (internal)
* if you need external args in lambda, just bind them
**/
typedef std::variant<std::string, intercept::types::code, std::function<void(const DBReturn&)> > DBCallback;

/**
* Statement execution type
* POLL - result will be inserted into result array
* CALLBACK - if provided, a callback is executed as soon as the results are there
* SYNC - none of the above is attempted, just the std::future is given back
**/
enum DBExecutionType {
    ASYNC_POLL,
    ASYNC_CALLBACK,
    SYNC
};

/**
* Container for the database statement options
**/
struct DBStatementOptions {
    DBExecutionType type;
    DBCallback callbackFnc;
    std::optional<game_value> callbackArg;
};

/*
 *  DBWorker - Interface between database connectors and the arma engine 
 *
 */
class DBWorker {
private:

    /**
    * \brief Worker connections
    *
    * Now this is a bit ugly, but its the easiest way
    *
    **/

    /*!< pointers to the db connectors */
    std::vector<std::pair<std::thread::id, std::shared_ptr<DBConnector> > > dbConnectors;
    std::atomic<unsigned int> dbConnectorUID = 0;
    size_t dbConnectorsCount = 0;

    /**
    * Database results (only for polling)
    **/
    
    /*!< unique id returned for polling results */
    std::atomic<unsigned long> currentId = 0;
    
    /*!< results storage */
    std::vector<std::pair< unsigned long, std::shared_future<DBReturn> > > results; 
    
    /*!< mutex for results storage */
    std::shared_mutex resultsMutex;

    /**
    * Settings
    **/
    
    /*!< determines whether or not this worker is able to execute sql queries */
    bool isSqlDB = false;
    
    /*!< database connection details */
    DBConfig dbConfig;

    /**
      *   \brief Internal method for handling callbacks
      *
      *   Handles callbacks if needed. Executed after results are fetched.
      *
      *   \param statement DBStatementOptions Options for the executed statement
      *   \param result DBReturn Result of the Database call
      *
      **/
    void callbackResultIfNeeded(const DBStatementOptions& statement, const DBReturn& result) {
        if (statement.type == DBExecutionType::ASYNC_CALLBACK) {
            if (statement.callbackFnc.index() == 0 || statement.callbackFnc.index() == 1) {
                // string to function name or code
                intercept::client::invoker_lock lock;
                intercept::types::code code;
                if (statement.callbackFnc.index() == 0) {
                    code = intercept::sqf::get_variable(intercept::sqf::mission_namespace(), std::get<std::string>(statement.callbackFnc));
                }
                else {
                    code = std::get<intercept::types::code>(statement.callbackFnc);
                }
                if (!code.is_nil() && code.type_enum() == intercept::types::GameDataType::CODE) {
                    intercept::sqf::call(code, auto_array<game_value>({
                        game_value(result.index() == 0 ? std::get<std::string>(result) : std::to_string(result.index() == 1 ? std::get<bool>(result) : std::get<double>(result))),
                        statement.callbackArg.has_value() ? statement.callbackArg.value() : game_value()
                    }));
                }
            }
            else {
                // lambda function
                auto fnc = std::get< std::function<void(const DBReturn&)> >(statement.callbackFnc);
                fnc(result);
            }
        }
    }

    /**
      *   \brief Internal method to insert future into result storage
      *
      *   Handles insertion into result storage
      *
      *   \param statement DBStatementOptions Options for the executed statement
      *   \param result std::shared_future<DBReturn> future to the result of the Database call
      *
      **/
    unsigned long insertFutureIfNeeded(const DBStatementOptions& statement, const std::shared_future<DBReturn>& fut) {
        if (statement.type == DBExecutionType::ASYNC_POLL) {
            std::unique_lock<std::shared_mutex> lock(this->resultsMutex);
            unsigned long id = this->currentId++;
            this->results.emplace_back(std::pair<unsigned long, std::shared_future<DBReturn> >(id, fut));
        }
        return -1;
    }

    /**
      *   \brief Internal function wrapper for the parseSimpleArray recursion
      *
      *   Parses a string to an array (same restrictions as: https://community.bistudio.com/wiki/parseSimpleArray)
      *
      *   \param statement DBStatementOptions Options for the executed statement
      *   \param result std::shared_future<DBReturn> future to the result of the Database call
      *
      **/
    auto_array<game_value> parseSimpleArray(const std::string& in) {
        size_t idx = 0;
        return __parseSimpleArray(in, 0, idx);
    }

    auto_array<game_value> __parseSimpleArray(const std::string& in, size_t begin_idx, size_t& finished_index) {
        
        auto_array<game_value> out = {};

        // it has to be at least the opening bracket and the closing bracket
        if ((in.length() - begin_idx) >= 2 && *(in.begin() + begin_idx) == '[') {

            size_t current_index = 1;
            auto current = in.begin() + current_index;

            while (true) {
                
                if (*current == ']') {
                    // subarray end
                    finished_index = current_index + 1;
                    return out;
                }
                
                if (*current == ',') {
                    // next element
                    ++current_index; // after ,
                    current = in.begin() + current_index;
                }
                
                // cases: subarray, string, number, bool
                if (*current == '[') {
                    size_t done = 0;
                    out.emplace_back(__parseSimpleArray(in, current_index, done));

                    current_index = current_index + done;
                    current = in.begin() + current_index;

                }
                else if (*current == '"') {

                    // "" x "",
                    // find end
                    auto search_idx = current_index + 2;
                    auto idx = in.find('"', search_idx);

                    out.emplace_back(std::string(in.begin() + search_idx, in.begin() + idx));

                    current_index = idx + 2; // after ""
                    current = in.begin() + current_index;

                }
                else if ((*current >= '0' && *current <= '9') || *current == '.') {
                    auto idx = in.find_first_not_of("1234567890.",current_index) - 1;

                    out.emplace_back(std::stof(std::string(current, in.begin() + idx)));

                    current_index = idx;
                    current = in.begin() + current_index;
                }
                else if (*current == 't' || *current == 'f') {
                    out.emplace_back(*current == 't');
                    auto idx = in.find_first_not_of("truefals", current_index) - 1;
                    current_index = idx;
                    current = in.begin() + current_index;
                }
                else {
                    // unexpected char found
                    return {};
                }
            }
        }
        else {
            // malformed input: not an array
            return {};
        }
    }

    /**
      *   \brief Internal function for lockless db connector assignment
      *
      *   \throws std::runtime_exception if all connectors are assigned and anew one is requested
      **/
    std::shared_ptr<DBConnector> getConnector() {
        auto tid = std::this_thread::get_id();
        for (auto& x : this->dbConnectors) {
            if (x.first == tid) {
                return x.second;
            }
        }
        auto newID = this->dbConnectorUID++;
        if (newID >= this->dbConnectorsCount) {
            throw std::runtime_error("Unexpected amout of threads tried to get database connectors");
        }
        std::shared_ptr<DBConnector> connector;
        try {
            switch (dbConfig.dbType) {
            case DBType::MY_SQL: {
                connector = std::make_shared<MySQLConnector>(this->dbConfig);
                break;
            }
            case DBType::SQLITE: {
                connector = std::make_shared<SQLiteConnector>(this->dbConfig);
                break;
            }
            case DBType::REDIS: {
                connector = std::make_shared<RedisConnector>(this->dbConfig);
                break;
            }
            default: {
                WARNING("Unknown Database type");
                break;
            }
            };
        }
        catch (const std::runtime_error& e) {
            WARNING(std::string("Runtime Error during connector creation:") + e.what());
            throw std::runtime_error("Could not create database connector");
        }
        if (!connector) {
            WARNING("Database connector could not be created");
            throw std::runtime_error("Database connector could not be created");
            return;
        }

        this->dbConnectors[newID].second = connector;
        this->dbConnectors[newID].first = tid;
        return connector;
    }

public:

    /**
    *  \brief Constructor
    *
    *  Sets up the DBWorker
    *
    *  \throws std::runtime_error if there was an error creating the connector
    *  \param dbConfig configuration object
    **/
    DBWorker(const DBConfig& dbConfig) {
        this->dbConfig = dbConfig;
        this->isSqlDB = dbConfig.dbType == DBType::MY_SQL;

        // Threadpool threads + current
        this->dbConnectorsCount = threadpool->getPoolSize() + 1;
        this->dbConnectors.reserve(this->dbConnectorsCount);
        for (size_t i = 0; i < this->dbConnectorsCount; ++i) {
            this->dbConnectors.emplace_back(
                std::pair < std::thread::id, std::shared_ptr<DBConnector> >(
                    std::thread::id(), nullptr
                )
            );
        }

        this->getConnector();
    }

    ~DBWorker() {
    }

    /**
    *  \brief Checks if the id is known
    *
    *  \param id unsigned long request id
    *  \returns bool true if the id is known/a result is being created
    **/
    bool isResultKnown(unsigned long id) {
        std::shared_lock<std::shared_mutex> lock(this->resultsMutex);
        for (auto& x : this->results) {
            if (x.first == id) {
                return true;
            }
        }
        return false;
    }

    /**
    *  \brief Checks if the result is ready
    *
    *  \param id unsigned long request id
    *  \returns bool true if the result is ready
    **/
    bool isResultReady(unsigned long id) {
        std::shared_lock<std::shared_mutex> lock(this->resultsMutex);
        for (auto& x : this->results) {
            if (x.first == id) {
                return x.second.valid() && x.second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }
        }
        return false;
    }

    /**
    *  \brief Removes result from list and returns it
    *
    *  Removes result from list and returns it if it is known and ready
    *  otherwise throws exception
    *
    *  \throws std::out_of_range if id is unknown or not ready
    *  \param id unsigned long request id
    *  \returns DBReturn
    **/
    DBReturn popResult(unsigned long id) {
        std::unique_lock<std::shared_mutex> lock(this->resultsMutex);
        for (unsigned int i = 0; i < this->results.size(); i++) {
            auto& x = this->results[i];
            if (x.first == id && x.second.valid() && x.second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                this->results.erase(results.begin() + i);
                return x.second.get();                
            }
        }
        throw std::out_of_range("Unknown id");
    }

    /**
    *  \brief DB GET  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    **/
    std::shared_future<DBReturn> get(const DBStatementOptions& statement, const std::string& key, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->get(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB GETRANGE  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    *  \param from unsigned int
    *  \param to unsigned int
    *
    **/
    std::shared_future<DBReturn> getRange(const DBStatementOptions& statement, const std::string& key, unsigned int from, unsigned int to, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, from{ std::move(from) }, to{ std::move(to) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->getRange(key, from, to);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB GETTTL  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    **/
    std::shared_future<DBReturn> getWithTtl(const DBStatementOptions& statement, const std::string& key, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->getWithTtl(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB EXISTS  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    **/
    std::shared_future<DBReturn> exists(const DBStatementOptions& statement, const std::string& key, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->exists(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB SET  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    *  \param value const std::string&
    **/
    std::shared_future<DBReturn> set(const DBStatementOptions& statement, const std::string& key, const std::string& value, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->set(key, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB SETEX  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    *  \param ttl int
    *  \param value const std::string&
    **/
    std::shared_future<DBReturn> setEx(const DBStatementOptions& statement, const std::string& key, int ttl, const std::string& value, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, ttl{ std::move(ttl) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->setEx(key, ttl, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB EXPIRE  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    *  \param value const std::string&
    *  \param ttl int
    **/
    std::shared_future<DBReturn> expire(const DBStatementOptions& statement, const std::string& key, int ttl, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, ttl{ std::move(ttl) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->expire(key, ttl);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB DEL  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    **/
    std::shared_future<DBReturn> del(const DBStatementOptions& statement, const std::string& key, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->del(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB PING
    *
    **/
    std::shared_future<DBReturn> ping(const DBStatementOptions& statement, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->ping();
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB TTL  Args are moved!
    *
    *  \param statement const DBStatementOptions&
    *  \param key const std::string&
    **/
    std::shared_future<DBReturn> ttl(const DBStatementOptions& statement, const std::string& key, unsigned long& id) {
        auto fut = threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto& db = this->getConnector();
                auto result = db->ttl(key);
                this->callbackResultIfNeeded(statement, (float)result);
                return DBReturn((float)result);
            }
        ).share();
        id = this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /**
    *  \brief DB Can execute SQL Query
    *
    **/
    bool canExecuteSQL() { return this->isSqlDB; };

};

#endif
