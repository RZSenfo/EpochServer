
#include <database/DBWorker.hpp>

DBWorker::DBWorker(const DBConfig& dbConfig) {
    this->dbConfig = dbConfig;
    this->isSqlDB = dbConfig.dbType == DBType::MY_SQL;

    // Threadpool threads + current
    this->dbConnectorsCount = threadpool->getPoolSize() + 1;
    this->dbConnectors.reserve(this->dbConnectorsCount);
    for (size_t i = 0; i < this->dbConnectorsCount; ++i) {
        this->dbConnectors.emplace_back(
            std::pair < std::thread::id, DBConRef >(
                std::thread::id(), nullptr
                )
        );
    }

    this->getConnector();
}

DBWorker::~DBWorker() {
}

void DBWorker::callbackResultIfNeeded(
    const DBReturn& result,
    const std::optional<DBCallback>& fnc,
    const std::optional<DBCallbackArg>& args
) {
    if (!fnc.has_value()) return;

    if (fnc->index() == 1) {
        // arma script code
        intercept::client::invoker_lock lock;
        intercept::sqf::call(
            std::get<intercept::types::code>(*fnc)
            /*
            ,
            auto_array<game_value>({
            game_value(result.index() == 0 ? std::get<std::string>(result) : std::to_string(result.index() == 1 ? std::get<bool>(result) : std::get<int>(result))),
            args.has_value() ? *args : game_value()
            })
            */
        );
    }
    else {
        // lambda function
        std::get< std::function<void(const DBReturn&)> >(fnc.value())(result);
    }
}

DBConRef DBWorker::getConnector() {
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
    DBConRef connector;
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
    }

    this->dbConnectors[newID].second = connector;
    this->dbConnectors[newID].first = tid;
    return connector;
}

template<typename E>
std::function<DBReturn()> DBWorker::getFncWrapper(E&& errorValue, std::function<DBReturn(const DBConRef& ref)>&& f) {
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
std::function<void()> DBWorker::getFncWrapper(E&& errorValue, std::function<DBReturn(const DBConRef& ref)>&& fnc,
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