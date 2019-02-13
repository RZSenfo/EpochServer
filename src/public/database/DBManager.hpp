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

    WorkerCacheRef __getDbWorkerCache(const std::string& name);
    WorkerRef __getDbWorker(const std::string& name);

public:
    DBManager(const rapidjson::Value& cons);

    DBManager() = delete;
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;
    DBManager(DBManager&&) = delete;
    DBManager& operator=(DBManager&&) = delete;

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
