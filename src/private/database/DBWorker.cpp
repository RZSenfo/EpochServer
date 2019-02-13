
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

#include <epochserver/epochserver.hpp>

void DBWorker::callbackResultIfNeeded(
    const DBReturn& result,
    const std::optional<DBCallback>& fnc,
    const std::optional<DBCallbackArg>& args
) {
    if (!fnc.has_value()) return;

    if (fnc->index() == 0) {
        // string
        SQFCallBackHandle cbh;
        cbh.function = std::get<std::string>(fnc.value());
        
        if (args) {
            if (args->index() == 0) {
                cbh.extraArg = std::get<std::string>(args.value());
            }
        }
        else {
            cbh.extraArg = "[]";
        }
        cbh.functionIsCode = !cbh.function.empty() && cbh.function[0] == '{';
        
        std::stringstream buffer;
        switch (result.index()) {
            // string
            case 0: {
                buffer << "\"" << std::get<std::string>(result) << "\"";
                break;
            }
            //bool
            case 1: {
                buffer << std::to_string(std::get<bool>(result));
                break;
            }
            //int
            case 2: {
                buffer << std::to_string(std::get<int>(result));
                break;
            }
            // string,int
            case 3: {
                buffer << "[\"" << std::get<std::string>(result) << "\"," << std::to_string(std::get<int>(result)) << "]";
                break;
            }
            // vector string
            case 4: {
                auto& vec = std::get< std::vector<std::string> >(result);
                buffer << "[";
                for (size_t i = 0; i < vec.size(); ++i) {
                    buffer << "\"" << vec[i] << "\"" << ((i == (vec.size() - 1)) ? "" : ",");
                }
                buffer << "]";
                break;
            }
        };
        cbh.result = buffer.str();

        server->insertCallback(cbh);
    }
    else if (fnc->index() == 1) {
        // lambda function
        auto cbfnc = std::get< std::function<void(const DBReturn&)> >(fnc.value()); 
        if (cbfnc) {
            cbfnc(result);
        }
    }
#ifdef WITH_INTERCEPT
    else if (fnc->index() == 2) {
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
#endif // WITH_INTERCEPT

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
