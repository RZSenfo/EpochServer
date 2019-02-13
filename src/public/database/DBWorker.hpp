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
typedef std::variant<
    std::string,    // value
    bool,           // success/failure
    int,            // ttl
    std::pair<std::string, int>, // value, ttl
    std::vector<std::string> // keys
> DBReturn;

/**
* Type/Variant of the callback:
*    string: missionnamespace variable or string of code
*    code: actual function code
*    lambda: use of dbworker as API (internal)
* if you need external args in lambda, just bind them (they have to be copyable tho)
**/
typedef std::variant<std::string, intercept::types::code, std::function<void(const DBReturn&)> > DBCallback;
typedef std::variant<std::string, intercept::types::game_value > DBCallbackArg;

typedef std::shared_ptr<DBConnector> DBConRef;

/**
* Statement execution type
* ASYNC_CALLBACK - if provided, a callback is executed as soon as the results are there, otherweise its fire and forget
* ASYNC_FUTURE - just the std::future is given back
* ASYNC_POLL - returns a id for the request. the result can be polled with this id
**/
enum class DBExecutionType {
    ASYNC_CALLBACK,
    ASYNC_FUTURE,
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
    std::vector<std::pair<std::thread::id, DBConRef > > dbConnectors;
    std::atomic<unsigned int> dbConnectorUID = 0;
    size_t dbConnectorsCount = 0;

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
    void callbackResultIfNeeded(
        const DBReturn& result,
        const std::optional<DBCallback>& fnc,
        const std::optional<DBCallbackArg>& args
    );

    /**
      *   \brief Internal function for lockless db connector assignment
      *
      *   \throws std::runtime_exception if all connectors are assigned and anew one is requested
      **/
    DBConRef getConnector();

    template<typename E>
    inline std::function<DBReturn()> getFncWrapper(
        E&& errorValue,
        std::function<DBReturn(const DBConRef& ref)>&& f
    ) {
        return [this, errorValue = std::move(errorValue), fnc = std::move(f)](){
            try {
                auto& db = this->getConnector();
                DBReturn result = fnc(db);
                return result;
            }
            catch (std::exception& e) {
                return static_cast<DBReturn>(errorValue);
            }
        };
    }

    template<typename E>
    inline std::function<void()> getFncWrapper(
        E&& errorValue,
        std::function<DBReturn(const DBConRef& ref)>&& fnc,
        std::optional<DBCallback>&& callback,
        std::optional<DBCallbackArg>&& args
    ) {
        return [this, errorValue = std::move(errorValue), fnc = std::move(fnc),
            callback = std::move(callback), args = std::move(args)
        ](){
            try {
                auto& db = this->getConnector();
                DBReturn result = fnc(db);
                this->callbackResultIfNeeded(result, callback, args);
            }
            catch (std::exception& e) {}
        };
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
    DBWorker(const DBConfig& dbConfig);

    DBWorker() = delete;
    DBWorker(const DBWorker&) = delete;
    DBWorker& operator=(const DBWorker&) = delete;
    DBWorker(DBWorker&&) = delete;
    DBWorker& operator=(DBWorker&&) = delete;

    ~DBWorker();


#define CREATE_FUNCTION(fncname, defaultreturn, lambda, ...) \
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::ASYNC_FUTURE, std::shared_future<DBReturn> >\
    fncname(__VA_ARGS__) {\
        return threadpool->enqueue(\
            this->getFncWrapper(defaultreturn, lambda);\
        ).share();\
    };\
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::ASYNC_CALLBACK, void >\
    fncname(__VA_ARGS__,\
        std::optional<DBCallback>&& fnc,\
        std::optional<DBCallbackArg>&& args\
    ) {\
        threadpool->fireAndForget(\
            this->getFncWrapper(defaultreturn, lambda, std::move(fnc), std::move(args))\
        );\
    };\
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::SYNC, DBReturn >\
    fncname(__VA_ARGS__) {\
        return (this->getFncWrapper(defaultreturn, lambda))();\
    };

// TODO find a alternative to __VA_OPT__(,) and merge this into CREATE_FUNCTION
#define CREATE_FUNCTION_NO_ARGS(fncname, defaultreturn, lambda) \
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::ASYNC_FUTURE, std::shared_future<DBReturn> >\
    fncname() {\
        return threadpool->enqueue(\
            this->getFncWrapper(defaultreturn, lambda);\
        ).share();\
    };\
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::ASYNC_CALLBACK, void >\
    fncname(\
        std::optional<DBCallback>&& fnc,\
        std::optional<DBCallbackArg>&& args\
    ) {\
        threadpool->enqueue(\
            this->getFncWrapper(defaultreturn, lambda, std::move(fnc), std::move(args))\
        );\
    };\
    template <DBExecutionType T>\
    inline std::enable_if_t<T == DBExecutionType::SYNC, DBReturn >\
    fncname() {\
        return (this->getFncWrapper(defaultreturn, lambda))();\
    };

    /**
    *  \brief DB Key  Args are moved!
    *
    *  \param pattern const std::string&
    **/

    CREATE_FUNCTION(keys, std::vector<std::string>(), [prefix = std::move(prefix)](const DBConRef& ref){ return DBReturn(ref->keys(prefix)); }, std::string&& prefix);

    /**
    *  \brief DB GET  Args are moved!
    *
    *  \param key const std::string&
    **/

    CREATE_FUNCTION(get, "", [key = std::move(key)](const DBConRef& ref){ return ref->get(key); }, std::string&& key);

    /**
    *  \brief DB GETRANGE  Args are moved!
    *
    *  \param key const std::string&
    *  \param from unsigned int
    *  \param to unsigned int
    *
    **/
    CREATE_FUNCTION(getRange, "", ([key = std::move(key), from, to](const DBConRef& ref){ return ref->getRange(key,from,to); }), std::string&& key, unsigned int from, unsigned int to);

    /**
    *  \brief DB GETTTL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_FUNCTION(getWithTtl, (std::pair<std::string, int>("", -1)), [key = std::move(key)](const DBConRef& ref){ return ref->getWithTtl(key); }, std::string&& key);
    
    /**
    *  \brief DB EXISTS  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_FUNCTION(exists, false, [key = std::move(key)](const DBConRef& ref){ return ref->exists(key); }, std::string&& key);
    
    /**
    *  \brief DB SET  Args are moved!
    *
    *  \param key const std::string&
    *  \param value const std::string&
    **/
    CREATE_FUNCTION(set, false, ([key = std::move(key), value = std::move(value)](const DBConRef& ref){ return ref->set(key,value); }), std::string&& key, std::string&& value);
    
    /**
    *  \brief DB SETEX  Args are moved!
    *
    *  \param key const std::string&
    *  \param ttl int
    *  \param value const std::string&
    **/
    CREATE_FUNCTION(setEx, false, ([key = std::move(key), value = std::move(value), ttl](const DBConRef& ref){ return ref->setEx(key, ttl, value); }), std::string&& key, int ttl, std::string&& value);
    

    /**
    *  \brief DB EXPIRE  Args are moved!
    *
    *  \param key const std::string&
    *  \param value const std::string&
    *  \param ttl int
    **/
    CREATE_FUNCTION(expire, false, ([key = std::move(key), ttl](const DBConRef& ref){ return ref->expire(key, ttl); }), std::string&& key, int ttl);
    
    /**
    *  \brief DB DEL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_FUNCTION(del, false, ([key = std::move(key)](const DBConRef& ref){ return ref->del(key); }), std::string&& key);
    
    /**
    *  \brief DB TTL  Args are moved!
    *
    *  \param key const std::string&
    **/
    CREATE_FUNCTION(ttl, -1, ([key = std::move(key)](const DBConRef& ref){ return ref->ttl(key); }), std::string&& key);

    /**
    *  \brief DB PING
    *
    **/
    CREATE_FUNCTION_NO_ARGS(ping, "false", ([](const DBConRef& ref){ return ref->ping(); }));
    
    /**
    *  \brief DB Can execute SQL Query
    *
    **/
    bool canExecuteSQL() { return this->isSqlDB; };

};

#endif
