#include <database/DBManager.hpp>

WorkerCacheRef DBManager::__getDbWorkerCache(const std::string& name) {
    for (auto& x : this->dbWorkerCaches) {
        if (x.first == name) {
            return x.second;
        }
    }
    throw std::runtime_error("No worker cache found for name: " + name);
}

WorkerRef DBManager::__getDbWorker(const std::string& name) {
    for (auto& x : this->dbWorkers) {
        if (x.first == name) {
            return x.second;
        }
    }
    throw std::runtime_error("No worker found for name: " + name);
}

DBManager::DBManager(const rapidjson::Value& cons) {
    for (auto& itr = cons.MemberBegin(); itr != cons.MemberEnd(); itr++ ) {

        std::string name = itr->name.GetString();
        auto config = itr->value.GetObject();

        if (config.HasMember("enable") && !config["enable"].GetBool()) {
            continue;
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
                map.insert({ x, std::get< std::pair<std::string,int> >( worker->getWithTtl<DBExecutionType::SYNC>(std::move(x)) ) });
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