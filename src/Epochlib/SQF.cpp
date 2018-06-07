#include "SQF.hpp"

SQF::SQF() {

}

SQF::~SQF() {

}

void SQF::push_empty_str() {
    this->arrayStack.emplace_back("\"\"");
}

void SQF::push_nil() {
    this->arrayStack.emplace_back("nil");
}

void SQF::push_str(const std::string& _string, int _flag) {
    if (!_string.empty()) {
        auto quoteChar = _flag ? "'" : "\"";
        this->arrayStack.emplace_back(std::string(quoteChar).append(_string).append(quoteChar));
    }
    else {
        this->arrayStack.emplace_back("nil");
    }
}

void SQF::push_str(const char *_string, int _flag) {
	if (_string != NULL) {
		auto quoteChar = _flag ? "'" : "\"";
		this->arrayStack.emplace_back(std::string(quoteChar).append(_string).append(quoteChar));
	}
	else {
		this->arrayStack.emplace_back("nil");
	}
}

void SQF::push_number(long long int _number) {
	this->arrayStack.emplace_back(std::to_string(_number));
}

void SQF::push_number(const char *_number, size_t _numberSize) {
	if (_numberSize > 0) {
		this->arrayStack.emplace_back(std::string().assign(_number, _numberSize));
	}
	else {
		this->arrayStack.emplace_back("nil");
	}
}

void SQF::push_array(const char *_string) {
	if (_string != NULL) {
		this->arrayStack.emplace_back(_string);
	}
	else {
		this->arrayStack.emplace_back("nil");
	}
}

void SQF::push_array(const std::string& _string) {
    if (!_string.empty()) {
        this->arrayStack.emplace_back(_string);
    }
    else {
        this->arrayStack.emplace_back("nil");
    }
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