#ifndef __DB_CONNECTOR_H
#define __DB_CONNECTOR_H


#include "defines.hpp"
#include "SQF.hpp"

class DBConnector
{
protected:
    SQF _DBExecToSQF(const EpochlibDBExecute& dbExecute, SQF_RETURN_TYPE forcedReturnType) {
        SQF returnSQF;

        returnSQF.push_number(dbExecute.success ? SQF_RETURN_STATUS::SUCCESS : SQF_RETURN_STATUS::FAIL);
        if (!dbExecute.message.empty()) {
            if (dbExecute.message.at(0) == '[') {
                returnSQF.push_array(dbExecute.message);
            }
            else {
                returnSQF.push_str(dbExecute.message.c_str());
            }
        }
        else if (forcedReturnType != SQF_RETURN_TYPE::NOT_SET) {
            if (forcedReturnType == SQF_RETURN_TYPE::STRING) {
                returnSQF.push_str("");
            }
            else if (forcedReturnType == SQF_RETURN_TYPE::ARRAY) {
                returnSQF.push_array("[]");
            }
        }

        return returnSQF;
    }

public:

    /*
     * DB INIT
     */
    virtual bool init(EpochlibConfigDB Config) = 0;

    /*
    *  DB GET
    *  Key
    */
    virtual std::string get(const std::string& key) = 0;
    virtual std::string getRange(const std::string& key, const std::string& value, const std::string& value2) = 0;
    virtual std::string getTtl(const std::string& key) = 0;
    virtual std::string getbit(const std::string& key, const std::string& value) = 0;
    virtual std::string exists(const std::string& key) = 0;

    /*
    *  DB SET / SETEX
    */
    virtual std::string set(const std::string& key, const std::string& value) = 0;
    virtual std::string setex(const std::string& key, const std::string& ttl, const std::string& value) = 0;
    virtual std::string expire(const std::string& key, const std::string& ttl) = 0;
    virtual std::string setbit(const std::string& key, const std::string& bitidx, const std::string& value) = 0;

    /*
    *  DB DEL
    *  Key
    */
    virtual std::string del(const std::string& key) = 0;

    /*
    *  DB PING
    */
    virtual std::string ping() = 0;

    /*
    *  DB LPOP with a given prefix
    */
    virtual std::string lpopWithPrefix(const std::string& Prefix, const std::string& Key) = 0;

    /*
    *  DB TTL
    *  Key
    */
    virtual std::string ttl(const std::string& Key) = 0;

    /*
    *  DB LOG
    */
    virtual std::string log(const std::string& Key, const std::string& Value) = 0;

    /*
    *  Increase bancount
    */
    virtual std::string increaseBancount() = 0;
};
#endif // !__DB_CONNECTOR_H
