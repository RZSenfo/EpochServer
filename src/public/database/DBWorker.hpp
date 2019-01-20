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

#include <ThreadPool.hpp>

typedef std::variant<std::string, bool, double> DBReturn;

enum DBStatementType {
    ASYNC_POLL,
    ASYNC_CALLBACK,
    SYNC
};

class DBStatement {
public:
    DBStatementType type;
    std::variant<std::string, intercept::types::code> callbackFnc;
    game_value callbackArg;
};

/*
 *  DBWorker - Interface between database connectors and the arma engine 
 *
 */
class DBWorker {
private:

    /*
     * Worker state
     */
    bool initzialized = false;
    std::unique_ptr<ThreadPool> threadpool;
    std::unique_ptr<DBConnector> db;
    
    /*
     * Database results (only for polling)
     */
    unsigned long currentId = 0;
    std::vector<std::pair< unsigned long, std::shared_future<DBReturn> > > results;
    std::shared_mutex resultsMutex;

    /*
     * Settings
     */
    bool isSqlDB = false;
    DBConfig dbConfig;

    void callbackResultIfNeeded(const DBStatement& statement, const DBReturn& result) {
        if (statement.type == DBStatementType::ASYNC_CALLBACK) {
            // string to function name
            {
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
                        statement.callbackArg
                    }));
                }
            }
        }
    }

    void insertFutureIfNeeded(const DBStatement& statement, const std::shared_future<DBReturn>& fut) {
        if (statement.type == DBStatementType::ASYNC_POLL) {
            std::unique_lock<std::shared_mutex> lock(this->resultsMutex);
            this->results.emplace_back(std::pair<unsigned long, std::shared_future<DBReturn> >(this->currentId++, fut));
        }
    }

public:

    DBWorker() {}

    DBWorker(const DBConfig& dbConfig) {
        this->init(dbConfig);
        this->db.reset();
    }

    ~DBWorker() {
        this->threadpool->stopThreadPool(false);
    }

    bool isResultKnown(unsigned long id) {
        std::shared_lock<std::shared_mutex> lock(this->resultsMutex);
        for (auto& x : this->results) {
            if (x.first == id) {
                return true;
            }
        }
        return false;
    }

    bool isResultReady(unsigned long id) {
        std::shared_lock<std::shared_mutex> lock(this->resultsMutex);
        for (auto& x : this->results) {
            if (x.first == id) {
                return x.second.valid() && x.second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }
        }
        return false;
    }

    std::string getResult(unsigned long id) {
        std::unique_lock<std::shared_mutex> lock(this->resultsMutex);
        for (unsigned int i = 0; i < this->results.size(); i++) {
            auto& x = this->results[i];
            if (x.first == id && x.second.valid() && x.second.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                this->results.erase(results.begin() + i);
                
                auto resultHolder = x.second.get();
                switch (resultHolder.index()) {
                case 0: { // string
                    return std::get<std::string>(resultHolder);
                }
                case 1: { // bool
                    return std::to_string(std::get<bool>(resultHolder));
                }
                case 2: { // double
                    return std::to_string(std::get<double>(resultHolder));
                }
                default: {
                    return "";
                }
                }
            }
        }
        return "";
    }

    bool init(const DBConfig& Config) {

        this->dbConfig = dbConfig;

        if (this->initzialized) {
            WARNING("Database connection: " + Config.connectionName + " is already initialzed");
            return false;
        }
        else {

            this->isSqlDB = Config.dbType == DBType::MY_SQL;

            switch (dbConfig.dbType) {
                case DBType::MY_SQL: {
                    this->db = std::make_unique<MySQLConnector>();
                    break;
                }
                case DBType::SQLITE: {
                    this->db = std::make_unique<SQLiteConnector>();
                    break;
                }
                case DBType::REDIS: {
                    this->db = std::make_unique<RedisConnector>();
                    break;
                }
                default: {
                    WARNING("Unknown Database type");
                    break;
                }
            };

            if (!this->db) {
                WARNING("Database connector could not be created");
                return false;
            }

            if (!this->db->init(dbConfig)) {
                WARNING("Database connector config failed");
                return false;
            }

            // TODO to increase ThreadPool size, wee need to secure the db connection (or have multiple with a mutex each and iterate them round robin like)
            this->threadpool = std::make_unique<ThreadPool>(1);
            
            this->initzialized = true;
        }
    };

    /*
    *  DB GET
    *  Args are moved!
    */
    std::shared_future<DBReturn> get(const DBStatement& statement, const std::string& key) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto result = this->db->get(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> getRange(const DBStatement& statement, const std::string& key, const std::string& from, const std::string& to) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, from{ std::move(from) }, to{ std::move(to) }, statement{ std::move(statement) }](){
            auto result = this->db->getRange(key, from, to);
            this->callbackResultIfNeeded(statement, result);
            return DBReturn(result);
        }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> getTtl(const DBStatement& statement, const std::string& key) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
            auto result = this->db->getTtl(key);
            this->callbackResultIfNeeded(statement, result);
            return DBReturn(result);
        }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> getBit(const DBStatement& statement, const std::string& key, const std::string& value) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, statement{ std::move(statement) }](){
                auto result = this->db->getBit(key, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> exists(const DBStatement& statement, const std::string& key) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto result = this->db->exists(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /*
    *  DB SET / SETEX
    *  Args are moved!
    */
    std::shared_future<DBReturn> set(const DBStatement& statement, const std::string& key, const std::string& value) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, statement{ std::move(statement) }](){
                auto result = this->db->set(key, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> setEx(const DBStatement& statement, const std::string& key, const std::string& ttl, const std::string& value) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, ttl{ std::move(ttl) }, statement{ std::move(statement) }](){
                auto result = this->db->setEx(key, ttl, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> expire(const DBStatement& statement, const std::string& key, const std::string& ttl) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, ttl{ std::move(ttl) }, statement{ std::move(statement) }](){
                auto result = this->db->expire(key, ttl);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };
    std::shared_future<DBReturn> setBit(const DBStatement& statement, const std::string& key, const std::string& bitidx, const std::string& value) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, value{ std::move(value) }, bitidx{ std::move(bitidx) }, statement{ std::move(statement) }](){
                auto result = this->db->setBit(key, bitidx, value);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /*
    *  DB DEL
    *  Args are moved!
    */
    std::shared_future<DBReturn> del(const DBStatement& statement, const std::string& key) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto result = this->db->del(key);
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /*
    *  DB PING
    */
    std::shared_future<DBReturn> ping(const DBStatement& statement) {
        auto fut = this->threadpool->enqueue(
            [this, statement{ std::move(statement) }](){
                auto result = this->db->ping();
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /*
    *  DB TTL
    *  Args are moved!
    */
    std::shared_future<DBReturn> ttl(const DBStatement& statement, const std::string& key) {
        auto fut = this->threadpool->enqueue(
            [this, key{ std::move(key) }, statement{ std::move(statement) }](){
                auto result = (double)(this->db->ttl(key));
                this->callbackResultIfNeeded(statement, result);
                return DBReturn(result);
            }
        ).share();
        this->insertFutureIfNeeded(statement, fut);
        return fut;
    };

    /*
    *  DB Can execute SQL Query
    */
    bool canExecuteSQL() { return this->isSqlDB; };

};

#endif
