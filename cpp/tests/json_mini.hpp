#pragma once

// Tiny JSON reader, just enough for docs/testdata. Supports objects, arrays, strings
// (with \n \t \" \\ \/ escapes), numbers, true/false/null. Throws std::runtime_error on bad input.

#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool boolean = false;
    double number = 0;
    std::string str;
    std::vector<Value> items;                  // Array
    std::vector<std::pair<std::string, Value>> members;  // Object, in file order

    bool has(const std::string& key) const { return find(key) != nullptr; }
    const Value* find(const std::string& key) const {
        for (const auto& m : members) {
            if (m.first == key) return &m.second;
        }
        return nullptr;
    }
    const Value& at(const std::string& key) const {
        const Value* v = find(key);
        if (!v) throw std::runtime_error("missing key: " + key);
        return *v;
    }
    const std::string& asString() const { return str; }
    long long asInt() const { return static_cast<long long>(number); }
};

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    Value parse() {
        Value v = value();
        skipWs();
        if (pos_ != s_.size()) fail("trailing characters");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error("JSON error at offset " + std::to_string(pos_) + ": " + msg);
    }
    void skipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\n' || s_[pos_] == '\r' || s_[pos_] == '\t')) ++pos_;
    }
    char peek() {
        skipWs();
        if (pos_ >= s_.size()) fail("unexpected end");
        return s_[pos_];
    }
    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++pos_;
    }
    bool literal(const char* word) {
        size_t n = std::string(word).size();
        if (s_.compare(pos_, n, word) == 0) {
            pos_ += n;
            return true;
        }
        return false;
    }

    Value value() {
        char c = peek();
        Value v;
        if (c == '{') {
            v.type = Value::Type::Object;
            ++pos_;
            if (peek() == '}') {
                ++pos_;
                return v;
            }
            while (true) {
                std::string key = string();
                expect(':');
                v.members.emplace_back(key, value());
                if (peek() == ',') {
                    ++pos_;
                    continue;
                }
                expect('}');
                return v;
            }
        }
        if (c == '[') {
            v.type = Value::Type::Array;
            ++pos_;
            if (peek() == ']') {
                ++pos_;
                return v;
            }
            while (true) {
                v.items.push_back(value());
                if (peek() == ',') {
                    ++pos_;
                    continue;
                }
                expect(']');
                return v;
            }
        }
        if (c == '"') {
            v.type = Value::Type::String;
            v.str = string();
            return v;
        }
        if (literal("true")) {
            v.type = Value::Type::Bool;
            v.boolean = true;
            return v;
        }
        if (literal("false")) {
            v.type = Value::Type::Bool;
            return v;
        }
        if (literal("null")) return v;
        char* end = nullptr;
        v.number = std::strtod(s_.c_str() + pos_, &end);
        if (end == s_.c_str() + pos_) fail("unexpected character");
        pos_ = static_cast<size_t>(end - s_.c_str());
        v.type = Value::Type::Number;
        return v;
    }

    std::string string() {
        expect('"');
        std::string out;
        while (pos_ < s_.size() && s_[pos_] != '"') {
            char c = s_[pos_++];
            if (c == '\\') {
                if (pos_ >= s_.size()) fail("bad escape");
                char e = s_[pos_++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    default: fail("unsupported escape");
                }
            } else {
                out += c;
            }
        }
        if (pos_ >= s_.size()) fail("unterminated string");
        ++pos_;
        return out;
    }

    const std::string& s_;
    size_t pos_ = 0;
};

inline Value parseFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::stringstream buf;
    buf << in.rdbuf();
    std::string text = buf.str();
    return Parser(text).parse();
}

}  // namespace json
