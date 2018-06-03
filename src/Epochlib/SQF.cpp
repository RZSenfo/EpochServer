#include "SQF.hpp"

SQF::SQF() {

}

SQF::~SQF() {

}

void SQF::push_str(const char *_string, int _flag) {
	if (_string != NULL) {
		std::stringstream string;
		char quoteChar = '"';
		if (_flag == 1) {
			quoteChar = '\'';
		}
		string << quoteChar << _string << quoteChar;
		this->arrayStack.push_back(string.str());
	}
	else {
		this->arrayStack.push_back("nil");
	}
}
void SQF::push_str(const char *_string) {
	this->push_str(_string, 0);
}

void SQF::push_number(long long int _number) {
	this->arrayStack.emplace_back(std::to_string(_number));
}
void SQF::push_number(const char *_number, size_t _numberSize) {
	if (_numberSize > 0) {
		std::string string;
		string.assign(_number, _numberSize);
		this->arrayStack.push_back(string);
	}
	else {
		this->arrayStack.emplace_back("nil");
	}
}

void SQF::push_array(const char *_string) {
	if (_string != NULL) {
		this->arrayStack.push_back(_string);
	}
	else {
		this->arrayStack.push_back("nil");
	}
}
void SQF::push_array(const std::string& _string) {
	this->push_array(_string.c_str());
}

std::string SQF::toArray() {
	std::stringstream arrayStream;
	arrayStream << "[";

    auto it = this->arrayStack.begin();
    if (it != this->arrayStack.end()) {
        arrayStream << *it;
        it++;
    }

    for (; it != this->arrayStack.end(); it++) {
		arrayStream << "," << *it;
	}

	arrayStream << "]";

	return arrayStream.str();
}