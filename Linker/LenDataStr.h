#pragma once
#include <iostream>
// For outputting a string prefixed with its length.
//
// Makes reading the string faster since we know its length in advance and can
// read exactly that number of bytes + preallocate the space we need. Much
// faster to read than just putting the string in quotes because we don't need
// to search for a quote character.
class LenDataStr {
    std::string data;
    // Should only be used in input operator
    LenDataStr() {}
public:
    explicit LenDataStr(std::string data): data{data} {}
    friend std::ostream &operator<<(std::ostream &out, const LenDataStr &s) {
        out << s.data.size() << ' ' << s.data;
        return out;
    }
    static std::istream &read(std::istream &in, std::string &s) {
        unsigned int size;
        in >> size;
        // Ignore the separator space
        in.ignore();
        // Single allocation to read the exact length
        s.resize(size);
        in.read(&s[0], size);
        return in;
    }
};