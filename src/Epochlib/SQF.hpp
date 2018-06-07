#ifndef SQF_H
#define SQF_H

#include <vector>
#include <string>
#include <sstream>

enum SQF_RETURN_TYPE {
    NOTHING = 0,
    STRING = 1,
    ARRAY = 2,
    NOT_SET = 3
};

enum SQF_RETURN_STATUS {
    FAIL = 0,
    SUCCESS = 1,
    CONTINUE = 2
};

class SQF {
private:
	std::vector<std::string> arrayStack;

public:
	SQF();
	~SQF();
    void push_empty_str();
    void push_nil();
	void push_str(const char *String, int Flag = 0);
    void push_str(const std::string& String, int Flag = 0);
	void push_number(long long int Number);
	void push_number(const char *Number, size_t NumberSize);
	void push_array(const char *String);
	void push_array(const std::string& String);
	std::string toArray();

    static std::string RET_FAIL() {
        return "[0]";
    }

    static std::string RET_SUCCESS() {
        return "[1]";
    }
};

#endif
