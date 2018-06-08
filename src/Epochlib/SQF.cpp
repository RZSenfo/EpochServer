#include "SQF.hpp"

SQF::SQF() {

}

SQF::~SQF() {

}

void SQF::push_empty_str() {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    this->arrayStack.append("\"\"");
}

void SQF::push_nil() {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    this->arrayStack.append("nil");
}

void SQF::push_str(const std::string& _string, int _flag) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    
    if (!_string.empty()) {
        auto quoteChar = _flag ? "'" : "\"";
        this->arrayStack.append(quoteChar).append(_string).append(quoteChar);
    }
    else {
        this->arrayStack.append("nil");
    }
}

void SQF::push_str(const char *_string, int _flag) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    if (_string != NULL) {
        auto quoteChar = _flag ? "'" : "\"";
        this->arrayStack.append(quoteChar).append(_string).append(quoteChar);
    }
    else {
        this->arrayStack.append("nil");
    }
}

void SQF::push_number(long long int _number) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    this->arrayStack.append(std::to_string(_number));
}

void SQF::push_number(const char *_number, size_t _numberSize) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    if (_numberSize > 0) {
        this->arrayStack.append(std::string().assign(_number, _numberSize));
    }
    else {
        this->arrayStack.append("nil");
    }
}

void SQF::push_array(const char *_string) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    
    if (_string != NULL) {
        this->arrayStack.append(_string);
    }
    else {
        this->arrayStack.append("nil");
    }
}

void SQF::push_array(const std::string& _string) {
    
    if (this->arrayStack.size() != 1) {
        this->arrayStack.append(",");
    }
    
    
    if (!_string.empty()) {
        this->arrayStack.append(_string);
    }
    else {
        this->arrayStack.append("nil");
    }
}

std::string SQF::toArray() {
    return arrayStack.append("]");
}