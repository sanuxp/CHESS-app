#include "chess/PgnImport.hpp"

#include <cctype>
#include <sstream>

#include "chess/Pgn.hpp"

namespace chess {

namespace {

bool isResult(const std::string& t) { return t == "1-0" || t == "0-1" || t == "1/2-1/2" || t == "*"; }

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Parses `[Name "value"]`. Returns false when the line is not a tag.
bool parseTag(const std::string& line, std::string& name, std::string& value) {
    std::string t = trim(line);
    if (t.size() < 5 || t.front() != '[' || t.back() != ']') return false;
    size_t sp = t.find(' ');
    size_t q1 = t.find('"');
    size_t q2 = t.rfind('"');
    if (sp == std::string::npos || q1 == std::string::npos || q2 <= q1) return false;
    name = trim(t.substr(1, sp - 1));
    value.clear();
    for (size_t i = q1 + 1; i < q2; ++i) {
        if (t[i] == '\\' && i + 1 < q2) ++i;
        value += t[i];
    }
    return !name.empty();
}

}  // namespace

PgnImport importPGN(const std::string& text) {
    PgnImport out;

    // Header tags, then everything else is movetext.
    std::istringstream lines(text);
    std::string line, body;
    bool inHeaders = true;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (inHeaders) {
            std::string name, value;
            if (parseTag(line, name, value)) {
                out.headers[name] = value;
                continue;
            }
            if (trim(line).empty()) continue;
            inHeaders = false;
        }
        body += line;
        body += '\n';
    }

    Board start;
    auto fen = out.headers.find("FEN");
    if (fen != out.headers.end()) {
        if (!Board::fromFEN(fen->second, start)) {
            out.error = "Invalid FEN in the PGN tags";
            return out;
        }
    }
    out.game = Game(start);

    auto res = out.headers.find("Result");
    if (res != out.headers.end() && isResult(res->second)) out.result = res->second;

    int variationDepth = 0;
    int moveNumber = start.fullmoveNumber;
    std::string token;

    // Returns false when reading must stop (a result token or an illegal move).
    auto flush = [&]() {
        if (token.empty()) return true;
        std::string t = token;
        token.clear();
        if (variationDepth > 0) return true;
        if (isResult(t)) {
            out.result = t;
            return false;
        }
        if (t[0] == '$') return true;  // NAG
        size_t digits = 0;
        while (digits < t.size() && std::isdigit(static_cast<unsigned char>(t[digits]))) ++digits;
        if (digits > 0 && digits < t.size() && t[digits] == '.') {  // move number: "12." / "12..." / "12.e4"
            size_t dots = digits;
            while (dots < t.size() && t[dots] == '.') ++dots;
            t = t.substr(dots);
        }
        if (t.empty()) return true;
        if (!out.game.makeMoveSAN(t)) {
            out.error = "Illegal or unreadable move \"" + t + "\" (move " + std::to_string(moveNumber) + ")";
            return false;
        }
        if (out.game.board().sideToMove == Color::White) ++moveNumber;
        return true;
    };

    size_t i = 0;
    while (i < body.size()) {
        char ch = body[i];
        if (ch == '{') {
            if (!flush()) break;
            size_t end = body.find('}', i);
            i = end == std::string::npos ? body.size() : end + 1;
        } else if (ch == ';') {
            if (!flush()) break;
            size_t end = body.find('\n', i);
            i = end == std::string::npos ? body.size() : end + 1;
        } else if (ch == '(') {
            if (!flush()) break;
            ++variationDepth;
            ++i;
        } else if (ch == ')') {
            if (!flush()) break;
            if (variationDepth > 0) --variationDepth;
            ++i;
        } else if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!flush()) break;
            ++i;
        } else {
            token += ch;
            ++i;
        }
    }
    if (out.error.empty()) flush();
    return out;
}

}  // namespace chess
