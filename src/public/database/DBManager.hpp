#pragma once


#ifndef __DB_MANAGER_HPP__
#define __DB_MANAGER_HPP__

#include <main.hpp>
#include <database/DBWorker.hpp>

#undef GetObject
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/istreamwrapper.h>


typedef std::unordered_map<std::string, std::pair<std::string, int> > DBEntryMap;
typedef std::pair< std::shared_mutex, DBEntryMap > WorkerCache;
typedef std::shared_ptr<WorkerCache> WorkerCacheRef;
typedef std::shared_ptr<DBWorker> WorkerRef;

class DBManager {

private:

    std::vector< std::pair< std::string, WorkerRef > > dbWorkers;
    std::vector< std::pair< std::string, WorkerCacheRef > > dbWorkerCaches;

    WorkerCacheRef __getDbWorkerCache(const std::string& name) {
        for (auto& x : this->dbWorkerCaches) {
            if (x.first == name) {
                return x.second;
            }
        }
        throw std::runtime_error("No worker cache found for name: " + name);
    }

    WorkerRef __getDbWorker(const std::string& name) {
        for (auto& x : this->dbWorkers) {
            if (x.first == name) {
                return x.second;
            }
        }
        throw std::runtime_error("No worker found for name: " + name);
    }

public:
    DBManager(const rapidjson::Value& cons) {
        for (auto& itr = cons.MemberBegin(); itr != cons.MemberEnd(); itr++ ) {
            
            std::string name = itr->name.GetString();
            auto config = itr->value.GetObject();
            
            if (config.HasMember("enable") && !config["enable"].GetBool()) {
                return;
            }

            DBConfig dbConf;

            if (!config.HasMember("type")) throw std::runtime_error("Undefined connection value: \"type\" in " + name);
            std::string type = config["type"].GetString();


            if (utils::iequals(type, "mysql")) {

                if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

                dbConf.dbType = DBType::MY_SQL;
                dbConf.ip = config.HasMember("ip") ? config["ip"].GetString() : "127.0.0.1";
                dbConf.password = config.HasMember("password") ? config["password"].GetString() : "";
                dbConf.user = config.HasMember("username") ? config["username"].GetString() : "root";;
                dbConf.port = config.HasMember("port") ? config["port"].GetInt() : 3306;
                dbConf.dbname = config["database"].GetString();
            }
            else if (utils::iequals(type, "redis")) {

                if (!config.HasMember("password")) throw std::runtime_error("Undefined connection value: \"password\" in " + name);
                if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

                dbConf.dbType = DBType::REDIS;
                dbConf.ip = config.HasMember("ip") ? config["ip"].GetString() : "127.0.0.1";
                dbConf.port = config.HasMember("port") ? config["port"].GetInt() : 6379;
                dbConf.password = config.HasMember("password") ? config["password"].GetString() : "";
            }
            else if (utils::iequals(type, "sqlite")) {

                if (!config.HasMember("database")) throw std::runtime_error("Undefined connection value: \"database\" in " + name);

                dbConf.dbType = DBType::SQLITE;
                dbConf.dbname = config["database"].GetString();
            }
            else {
                throw std::runtime_error("Unknown database type: \"" + type + "\" in " + name);
            }

            dbConf.connectionName = name;

            if (config.HasMember("statements") && config["statements"].IsObject()) {
                for (auto itr = config["statements"].MemberBegin(); itr != config["statements"].MemberEnd(); ++itr) {
                    std::string statementName = itr->name.GetString();
                    auto statementBody = itr->value.GetObject();

                    if (!statementBody.HasMember("query")) throw std::runtime_error("Undefined statement value: \"query\" in " + name + "." + statementName);
                    if (!statementBody.HasMember("params")) throw std::runtime_error("Undefined statement value: \"params\" in " + name + "." + statementName);
                    if (!statementBody.HasMember("result")) throw std::runtime_error("Undefined statement value: \"result\" in " + name + "." + statementName);

                    DBSQLStatementTemplate statement;
                    statement.statementName = statementName;
                    statement.query = statementBody["query"].GetString();
                    statement.isInsert = statementBody.HasMember("isinsert") && statementBody["isinsert"].GetBool();

                    auto paramslist = statementBody["params"].GetArray();
                    statement.params.reserve(paramslist.Size());
                    for (auto itr = paramslist.begin(); itr != paramslist.end(); ++itr) {
                        auto typeStr = itr->GetString();
                        if (typeStr == "number") {
                            statement.params.emplace_back(DBSQLStatementParamType::DB_NUMBER);
                        }
                        else if (typeStr == "array") {
                            statement.params.emplace_back(DBSQLStatementParamType::DB_ARRAY);
                        }
                        else if (typeStr == "bool") {
                            statement.params.emplace_back(DBSQLStatementParamType::DB_BOOL);
                        }
                        else if (typeStr == "string") {
                            statement.params.emplace_back(DBSQLStatementParamType::DB_STRING);
                        }
                        else {
                            throw std::runtime_error("Unknown type in params types of " + statementName);
                        }
                    }

                    auto resultslist = statementBody["results"].GetArray();
                    statement.result.reserve(resultslist.Size());
                    for (auto itr = resultslist.begin(); itr != resultslist.end(); ++itr) {
                        auto typeStr = itr->GetString();
                        if (typeStr == "number") {
                            statement.result.emplace_back(DBSQLStatementParamType::DB_NUMBER);
                        }
                        else if (typeStr == "array") {
                            statement.result.emplace_back(DBSQLStatementParamType::DB_ARRAY);
                        }
                        else if (typeStr == "bool") {
                            statement.result.emplace_back(DBSQLStatementParamType::DB_BOOL);
                        }
                        else if (typeStr == "string") {
                            statement.result.emplace_back(DBSQLStatementParamType::DB_STRING);
                        }
                        else {
                            throw std::runtime_error("Unknown type in result types of " + statementName);
                        }
                    }

                    dbConf.statements.emplace_back(statement);
                }
            }

            auto worker = std::make_shared<DBWorker>(dbConf);
            this->dbWorkers.emplace_back(
                std::pair< std::string, WorkerRef >({ name, worker })
            );

            if (!config.HasMember("disableCache") || !config["disableCache"].GetBool()) {
                
                std::vector<std::string> keys = std::get< std::vector<std::string> >( worker->keys<DBExecutionType::SYNC>("") );
                DBEntryMap map;
                map.reserve(std::min(keys.size() * 2, 1000u));

                for (auto& x : keys) {
                    map.insert({ x, std::get< std::pair<std::string,int> >( worker->getWithTtl<DBExecutionType::SYNC>(x) ) });
                }

                auto cacheref = std::make_shared<WorkerCache>();
                cacheref->second = std::move(map);
                
                this->dbWorkerCaches.emplace_back(
                    std::pair< std::string, WorkerCacheRef >(name, cacheref)
                );
            }
            else {
                this->dbWorkerCaches.emplace_back(
                    std::pair< std::string, WorkerCacheRef >(name, nullptr)
                );
            }

        }
    }

    /**
    *  \brief Checks if the id is known
    *
    *  \param id unsigned long request id
    *  \returns bool true if the id is known/a result is being created
    **/
    bool isResultKnown(const std::string& workerName, unsigned long id) {
        return this->__getDbWorker(workerName)->isResultKnown(id);
    }

    /**
    *  \brief Checks if the result is ready
    *
    *  \param id unsigned long request id
    *  \returns bool true if the result is ready
    **/
    bool isResultReady(const std::string& workerName, unsigned long id) {
        return this->__getDbWorker(workerName)->isResultReady(id);
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
    DBReturn popResult(const std::string& workerName, unsigned long id) {
        return this->__getDbWorker(workerName)->popResult(id);
    }

    /**
    *  \brief Tries to remove a result from list and to returns it
    *
    *  Removes result from list and returns it if it is possible (known and ready)
    *  otherwise throws exception
    *
    *  \throws std::out_of_range if id is unknown or not ready
    *  \param id unsigned long request id
    *  \returns DBReturn
    **/
    DBReturn tryPopResult(const std::string& workerName, unsigned long id) {
        return this->__getDbWorker(workerName)->tryPopResult(id);
    }

#define DBM_CREATION_HELPER(...) __VA_ARGS__
#define CREATE_DBM_FUNCTION(fncname, fncargtypes, fncargs) \
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_FUTURE, std::shared_future<DBReturn> >::type\
    fncname(const std::string& workerName, fncargtypes) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_FUTURE >(fncargs);\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_CALLBACK, void >::type\
    fncname(const std::string& workerName, fncargtypes,\
        const std::optional<std::variant<std::function<void(const DBReturn&)>, intercept::types::code> >& fnc,\
        const std::optional<game_value>& args\
    ) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_CALLBACK >(fncargs, fnc, args);\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_POLL, unsigned long>::type\
    fncname(const std::string& workerName, fncargtypes) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_POLL >(fncargs);\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::SYNC, DBReturn>::type\
    fncname(const std::string& workerName, fncargtypes) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::SYNC >(fncargs);\
    };

#define CREATE_DBM_FUNCTION_NO_ARGS(fncname) \
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_FUTURE, std::shared_future<DBReturn> >::type\
    fncname(const std::string& workerName) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_FUTURE >();\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_CALLBACK, void >::type\
    fncname(const std::string& workername,\
        const std::optional<std::variant<std::function<void(const DBReturn&)>, intercept::types::code> >& fnc,\
        const std::optional<game_value>& args\
    ) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_CALLBACK >(fnc, args);\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::ASYNC_POLL, unsigned long>::type\
    fncname(const std::string& workerName) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::ASYNC_POLL >();\
    };\
    template <DBExecutionType T>\
    typename std::enable_if<T == DBExecutionType::SYNC, DBReturn>::type\
    fncname(const std::string& workerName) {\
        auto worker = this->__getDbWorker(workerName);\
        return worker->fncname< DBExecutionType::SYNC >();\
    };

    /**
    *  \brief DB keys   Args are moved!
    *
    *  \param pattern const std::string&
    **/

    CREATE_DBM_FUNCTION(keys, const std::string& prefix, prefix);

    /**
    *  \brief DB GET  Args are moved!
    *
    *  \param key const std::string&
    **/

    CREATE_DBM_FUNCTION(get, const std::string& key, key);

    /**
    *  \brief DB GETRANGE  Args are moved!
    *
    *  \param key const std::string&
    *  \param from unsigned int
    *  \param to unsigned int
    *
    **/
    CREATE_DBM_FUNCTION(getRange, DBM_CREATION_HELPER(const std::string& key, unsigned int from, unsigned int to), DBM_CREATION_HELPER(key,from,to));

    /**
    *  \brief DB GETTTL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_DBM_FUNCTION(getWithTtl, const std::string& key, key);

    /**
    *  \brief DB EXISTS  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_DBM_FUNCTION(exists, const std::string& key, key);

    /**
    *  \brief DB SET  Args are moved!
    *
    *  \param key const std::string&
    *  \param value const std::string&
    **/
    CREATE_DBM_FUNCTION(set, DBM_CREATION_HELPER(const std::string& key, const std::string& value), DBM_CREATION_HELPER(key, value));

    /**
    *  \brief DB SETEX  Args are moved!
    *
    *  \param key const std::string&
    *  \param ttl int
    *  \param value const std::string&
    **/
    CREATE_DBM_FUNCTION(setEx, DBM_CREATION_HELPER(const std::string& key, int ttl, const std::string& value), DBM_CREATION_HELPER(key,ttl,value));


    /**
    *  \brief DB EXPIRE  Args are moved!
    *
    *  \param key const std::string&
    *  \param value const std::string&
    *  \param ttl int
    **/
    CREATE_DBM_FUNCTION(expire, DBM_CREATION_HELPER(const std::string& key, int ttl), DBM_CREATION_HELPER(key,ttl));

    /**
    *  \brief DB DEL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_DBM_FUNCTION(del, const std::string& key, key);

    /**
    *  \brief DB TTL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_DBM_FUNCTION(ttl, const std::string& key, key);

    /**
    *  \brief DB PING
    *
    **/
    CREATE_DBM_FUNCTION_NO_ARGS(ping);

    

};

#endif // !__DB_MANAGER_HPP__
