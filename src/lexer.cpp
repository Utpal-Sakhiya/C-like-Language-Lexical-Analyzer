// =====================================================================
//  lexer.cpp  -  Implementation of the C-like lexical analyzer
// =====================================================================
#include "lexer.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>

namespace {

const std::unordered_set<std::string> KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", "bool", "true", "false"
};

const std::unordered_set<std::string> DIRECTIVES = {
    "include", "define", "undef", "if", "ifdef", "ifndef", "elif",
    "else", "endif", "error", "warning", "pragma", "line"
};

// Longest match first: 3-char, then 2-char, then single-char operators.
const char* const OPERATORS_3[] = { "<<=", ">>=" };
const char* const OPERATORS_2[] = {
    "++", "--", "->", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "==", "!=", "<=", ">=", "&&", "||", "<<", ">>"
};
const std::string OPERATOR_CHARS   = "+-*/%=<>!&|^~?:.";
const std::string PUNCTUATOR_CHARS = "(){}[];,";

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
bool isDigit(char c)      { return c >= '0' && c <= '9'; }
bool isOctal(char c)      { return c >= '0' && c <= '7'; }
bool isHex(char c)        { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

std::string toLower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool validNumberSuffix(const std::string& suffix, bool isFloat) {
    std::string s = toLower(suffix);
    if (isFloat) return s == "f" || s == "l";
    static const std::unordered_set<std::string> ok = {
        "u", "l", "ul", "lu", "ll", "ull", "llu"
    };
    return ok.count(s) > 0;
}

std::string shorten(const std::string& s, size_t max = 24) {
    return s.size() <= max ? s : s.substr(0, max - 3) + "...";
}

} // namespace

// ---------------------------------------------------------------------
const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::KEYWORD:         return "KEYWORD";
        case TokenType::IDENTIFIER:      return "IDENTIFIER";
        case TokenType::INTEGER_LITERAL: return "INTEGER_CONST";
        case TokenType::FLOAT_LITERAL:   return "FLOAT_CONST";
        case TokenType::CHAR_LITERAL:    return "CHAR_CONST";
        case TokenType::STRING_LITERAL:  return "STRING_LITERAL";
        case TokenType::OPERATOR:        return "OPERATOR";
        case TokenType::PUNCTUATOR:      return "PUNCTUATOR";
        case TokenType::PREPROCESSOR:    return "PREPROCESSOR";
        case TokenType::INVALID:         return "INVALID";
        case TokenType::END_OF_FILE:     return "EOF";
    }
    return "UNKNOWN";
}

int LexResult::errorCount() const {
    return static_cast<int>(std::count_if(diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& d) { return d.severity == Severity::ERROR; }));
}

int LexResult::warningCount() const {
    return static_cast<int>(diagnostics.size()) - errorCount();
}

// ---------------------------------------------------------------------
//  Construction: normalise line endings and keep a copy of each line
//  so that error messages can show the offending source line.
// ---------------------------------------------------------------------
Lexer::Lexer(std::string source) : src(std::move(source)) {
    src.erase(std::remove(src.begin(), src.end(), '\r'), src.end());
    std::string current;
    for (char ch : src) {
        if (ch == '\n') { result.sourceLines.push_back(current); current.clear(); }
        else current += ch;
    }
    result.sourceLines.push_back(current);
}

// ---------------------------------------------------------------------
//  Character helpers
// ---------------------------------------------------------------------
bool Lexer::atEnd() const { return pos >= src.size(); }

char Lexer::peek(size_t ahead) const {
    return pos + ahead < src.size() ? src[pos + ahead] : '\0';
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') { ++line; col = 1; }
    else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++col;   // UTF-8 aware
    return c;
}

bool Lexer::onlyWhitespaceBeforeOnLine() const {
    for (size_t i = pos; i > 0; --i) {
        char p = src[i - 1];
        if (p == '\n') return true;
        if (p != ' ' && p != '\t') return false;
    }
    return true;
}

// ---------------------------------------------------------------------
//  Bookkeeping
// ---------------------------------------------------------------------
void Lexer::addToken(TokenType type, const std::string& lexeme, int l, int c) {
    result.tokens.push_back({type, lexeme, l, c});
}

void Lexer::diag(Severity sev, const std::string& code, const std::string& msg,
                 const std::string& hint, int l, int c, int len) {
    result.diagnostics.push_back({sev, code, msg, hint, l, c, std::max(1, len)});
}

void Lexer::recordSymbol(const std::string& name, int l, int c) {
    auto it = symbolIndex.find(name);
    if (it == symbolIndex.end()) {
        symbolIndex[name] = result.symbols.size();
        result.symbols.push_back({name, l, c, 1, {l}});
    } else {
        SymbolEntry& e = result.symbols[it->second];
        ++e.occurrences;
        if (e.lines.back() != l) e.lines.push_back(l);
    }
}

void Lexer::trackBracket(char ch, int l, int c) {
    if (ch == '(' || ch == '{' || ch == '[') {
        bracketStack.push_back({ch, l, c});
        return;
    }
    if (ch != ')' && ch != '}' && ch != ']') return;

    char expectedOpen = (ch == ')') ? '(' : (ch == '}') ? '{' : '[';
    if (bracketStack.empty()) {
        diag(Severity::WARNING, "W201",
             std::string("Unmatched '") + ch + "' has no opening bracket",
             std::string("Remove this '") + ch + "' or add the matching '" + expectedOpen + "' before it.",
             l, c, 1);
    } else if (bracketStack.back().ch != expectedOpen) {
        const OpenBracket& top = bracketStack.back();
        diag(Severity::WARNING, "W201",
             std::string("Mismatched '") + ch + "': the innermost open bracket is '" + top.ch +
                 "' from line " + std::to_string(top.line) + ", col " + std::to_string(top.column),
             "Check that every bracket is closed in the reverse order it was opened.",
             l, c, 1);
        bracketStack.pop_back();
    } else {
        bracketStack.pop_back();
    }
}

// ---------------------------------------------------------------------
//  Main driver loop
// ---------------------------------------------------------------------
LexResult Lexer::analyze() {
    while (!atEnd()) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f') { advance(); continue; }
        if (c == '\\' && peek(1) == '\n') { advance(); advance(); continue; }   // line splice

        if (c == '/' && peek(1) == '/') { skipLineComment();  continue; }
        if (c == '/' && peek(1) == '*') { skipBlockComment(); continue; }
        if (c == '#' && onlyWhitespaceBeforeOnLine()) { scanPreprocessor(); continue; }

        if (isIdentStart(c))                        { scanIdentifierOrKeyword(); continue; }
        if (isDigit(c) || (c == '.' && isDigit(peek(1)))) { scanNumber(); continue; }
        if (c == '"')                               { scanString(); continue; }
        if (c == '\'')                              { scanChar();   continue; }
        if (scanOperatorOrPunctuator())             continue;

        scanInvalid();   // panic-mode recovery: report and skip one character
    }

    for (const OpenBracket& b : bracketStack) {
        char close = (b.ch == '(') ? ')' : (b.ch == '{') ? '}' : ']';
        diag(Severity::WARNING, "W202",
             std::string("'") + b.ch + "' is never closed",
             std::string("Add a matching '") + close + "' later in the program.",
             b.line, b.column, 1);
    }

    addToken(TokenType::END_OF_FILE, "<EOF>", line, col);

    std::stable_sort(result.diagnostics.begin(), result.diagnostics.end(),
        [](const Diagnostic& a, const Diagnostic& b) {
            return a.line != b.line ? a.line < b.line : a.column < b.column;
        });
    return result;
}

// ---------------------------------------------------------------------
//  Comments
// ---------------------------------------------------------------------
void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') advance();
    ++result.commentCount;
}

void Lexer::skipBlockComment() {
    int sl = line, sc = col;
    advance(); advance();                     // consume "/*"
    while (!atEnd()) {
        if (peek() == '*' && peek(1) == '/') {
            advance(); advance();
            ++result.commentCount;
            return;
        }
        if (peek() == '/' && peek(1) == '*') {
            diag(Severity::WARNING, "W203", "'/*' found inside a block comment",
                 "Block comments do not nest; the first '*/' ends the whole comment.",
                 line, col, 2);
            advance(); advance();
            continue;
        }
        advance();
    }
    diag(Severity::ERROR, "E104", "Unterminated block comment",
         "Close the comment with '*/'. Everything after this point was treated as a comment.",
         sl, sc, 2);
}

// ---------------------------------------------------------------------
//  Preprocessor directive: whole line becomes one token
// ---------------------------------------------------------------------
void Lexer::scanPreprocessor() {
    int sl = line, sc = col;
    std::string lexeme;
    while (!atEnd()) {
        if (peek() == '\\' && peek(1) == '\n') { advance(); advance(); lexeme += ' '; continue; }
        if (peek() == '\n') break;
        if (peek() == '/' && peek(1) == '/') break;
        lexeme += advance();
    }
    while (!lexeme.empty() && (lexeme.back() == ' ' || lexeme.back() == '\t')) lexeme.pop_back();

    // Validate the directive name
    size_t i = 1;
    while (i < lexeme.size() && (lexeme[i] == ' ' || lexeme[i] == '\t')) ++i;
    size_t nameStart = i;
    while (i < lexeme.size() && isIdentChar(lexeme[i])) ++i;
    std::string name = lexeme.substr(nameStart, i - nameStart);

    if (!name.empty() && DIRECTIVES.count(name) == 0) {
        diag(Severity::ERROR, "E111", "Unknown preprocessor directive '#" + name + "'",
             "Valid directives: #include #define #undef #if #ifdef #ifndef #elif #else #endif #pragma #error",
             sl, sc + static_cast<int>(nameStart) - 1, static_cast<int>(name.size()) + 1);
        addToken(TokenType::INVALID, lexeme, sl, sc);
        return;
    }
    addToken(TokenType::PREPROCESSOR, lexeme, sl, sc);
}

// ---------------------------------------------------------------------
//  Identifiers and keywords
// ---------------------------------------------------------------------
void Lexer::scanIdentifierOrKeyword() {
    int sl = line, sc = col;
    std::string lexeme;
    while (!atEnd() && isIdentChar(peek())) lexeme += advance();

    if (KEYWORDS.count(lexeme)) {
        addToken(TokenType::KEYWORD, lexeme, sl, sc);
        return;
    }
    if (static_cast<int>(lexeme.size()) > MAX_IDENTIFIER_LENGTH) {
        diag(Severity::ERROR, "E108",
             "Identifier '" + shorten(lexeme) + "' is " + std::to_string(lexeme.size()) +
                 " characters long (limit is " + std::to_string(MAX_IDENTIFIER_LENGTH) + ")",
             "Use a shorter, meaningful name.", sl, sc, static_cast<int>(lexeme.size()));
    }
    addToken(TokenType::IDENTIFIER, lexeme, sl, sc);
    recordSymbol(lexeme, sl, sc);
}

// ---------------------------------------------------------------------
//  Numeric literals: decimal, octal, hexadecimal, floating point,
//  exponents and U/L/F suffixes.
// ---------------------------------------------------------------------
void Lexer::scanNumber() {
    int sl = line, sc = col;
    std::string lexeme, problem, hint;
    bool isFloat = false, isHexLit = false;
    auto take = [&]() { lexeme += advance(); };

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        isHexLit = true;
        take(); take();
        int digits = 0;
        while (!atEnd() && isHex(peek())) { take(); ++digits; }
        if (digits == 0) {
            problem = "Hexadecimal literal '" + lexeme + "' has no digits";
            hint    = "Write at least one hex digit after 0x, e.g. 0x1F.";
        }
    } else {
        while (!atEnd() && isDigit(peek())) take();

        if (peek() == '.') {
            isFloat = true;
            take();
            while (!atEnd() && isDigit(peek())) take();
            if (peek() == '.' && isDigit(peek(1))) {
                while (!atEnd() && (isDigit(peek()) || peek() == '.')) take();
                problem = "Too many decimal points in number '" + lexeme + "'";
                hint    = "A numeric literal can contain at most one decimal point.";
            }
        }
        if (problem.empty() && (peek() == 'e' || peek() == 'E')) {
            isFloat = true;
            take();
            if (peek() == '+' || peek() == '-') take();
            if (!isDigit(peek())) {
                problem = "Exponent in '" + lexeme + "' has no digits";
                hint    = "Follow 'e' with digits, e.g. 1.5e10 or 2e-3.";
            }
            while (!atEnd() && isDigit(peek())) take();
        }
        if (problem.empty() && !isFloat && lexeme.size() > 1 && lexeme[0] == '0') {
            for (char d : lexeme) {
                if (!isOctal(d)) {
                    problem = std::string("Invalid digit '") + d + "' in octal literal '" + lexeme + "'";
                    hint    = "Numbers starting with 0 are octal (digits 0-7 only). "
                              "Remove the leading 0 for a decimal number.";
                    break;
                }
            }
        }
    }

    // Suffix / trailing identifier characters (e.g. 10UL, 3.0f, 2ndValue)
    const std::string body = lexeme;
    std::string suffix;
    while (!atEnd() && isIdentChar(peek())) { suffix += peek(); take(); }

    if (problem.empty() && !suffix.empty() && !validNumberSuffix(suffix, isFloat)) {
        problem = "Invalid suffix '" + shorten(suffix) + "' on " +
                  (isFloat ? "floating-point" : "integer") + " literal '" + body + "'";
        hint = isIdentStart(suffix[0])
                   ? "Identifiers cannot begin with a digit. If this is a name, start it with a letter or '_'."
                   : "Valid suffixes: U, L, UL, LL, ULL for integers; F, L for floats.";
    }

    // Range check for integer constants
    if (problem.empty() && !isFloat) {
        errno = 0;
        std::strtoull(body.c_str(), nullptr, isHexLit ? 16 : (body.size() > 1 && body[0] == '0' ? 8 : 10));
        if (errno == ERANGE) {
            diag(Severity::ERROR, "E110", "Integer constant '" + shorten(body) + "' is too large",
                 "The largest integer constant is 18446744073709551615 (unsigned long long).",
                 sl, sc, static_cast<int>(lexeme.size()));
            addToken(TokenType::INVALID, lexeme, sl, sc);
            return;
        }
    }

    if (!problem.empty()) {
        diag(Severity::ERROR, "E105", problem, hint, sl, sc, static_cast<int>(lexeme.size()));
        addToken(TokenType::INVALID, lexeme, sl, sc);
        return;
    }
    addToken(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INTEGER_LITERAL, lexeme, sl, sc);
}

// ---------------------------------------------------------------------
//  Escape sequences inside string / character literals.
//  Returns false if the escape is invalid (error already reported).
// ---------------------------------------------------------------------
bool Lexer::scanEscape(std::string& lexeme) {
    int el = line, ec = col;
    lexeme += advance();                        // the backslash
    if (atEnd()) return true;                   // caller reports unterminated literal

    char e = peek();
    if (e == '\n') { advance(); lexeme.pop_back(); return true; }   // line continuation

    if (isOctal(e)) {                           // \0, \12, \177
        for (int i = 0; i < 3 && isOctal(peek()); ++i) lexeme += advance();
        return true;
    }
    if (e == 'x') {                             // \x41
        lexeme += advance();
        if (!isHex(peek())) {
            diag(Severity::ERROR, "E106", "'\\x' escape has no hexadecimal digits",
                 "Write hex digits after \\x, e.g. \\x41 for 'A'.", el, ec, 2);
            return false;
        }
        while (isHex(peek())) lexeme += advance();
        return true;
    }
    if (std::string("ntrabfv\\'\"?").find(e) != std::string::npos) {
        lexeme += advance();
        return true;
    }

    std::string shown(1, e);
    diag(Severity::ERROR, "E106", "Unknown escape sequence '\\" + shown + "'",
         "Valid escapes: \\n \\t \\r \\0 \\\\ \\' \\\" \\a \\b \\f \\v \\? \\xHH \\ooo",
         el, ec, 2);
    if ((static_cast<unsigned char>(e) & 0x80) == 0) lexeme += advance();
    return false;
}

// ---------------------------------------------------------------------
//  String literals
// ---------------------------------------------------------------------
void Lexer::scanString() {
    int sl = line, sc = col;
    std::string lexeme(1, advance());           // opening quote
    while (true) {
        if (atEnd() || peek() == '\n') {
            diag(Severity::ERROR, "E102", "Unterminated string literal",
                 "Add a closing \" before the end of the line.", sl, sc,
                 static_cast<int>(lexeme.size()));
            addToken(TokenType::INVALID, lexeme, sl, sc);
            return;
        }
        char c = peek();
        if (c == '\\') { scanEscape(lexeme); continue; }
        lexeme += advance();
        if (c == '"') break;
    }
    addToken(TokenType::STRING_LITERAL, lexeme, sl, sc);
}

// ---------------------------------------------------------------------
//  Character literals
// ---------------------------------------------------------------------
void Lexer::scanChar() {
    int sl = line, sc = col;
    std::string lexeme(1, advance());           // opening quote

    if (peek() == '\'') {
        lexeme += advance();
        diag(Severity::ERROR, "E107", "Empty character constant ''",
             "A character constant must hold exactly one character, e.g. 'a' or '\\n'.",
             sl, sc, 2);
        addToken(TokenType::INVALID, lexeme, sl, sc);
        return;
    }

    int count = 0;
    while (true) {
        if (atEnd() || peek() == '\n') {
            diag(Severity::ERROR, "E103", "Unterminated character constant",
                 "Close it with a single quote ('). Use double quotes for text.",
                 sl, sc, static_cast<int>(lexeme.size()));
            addToken(TokenType::INVALID, lexeme, sl, sc);
            return;
        }
        char c = peek();
        if (c == '\'') { lexeme += advance(); break; }
        if (c == '\\') { scanEscape(lexeme); ++count; continue; }
        lexeme += advance();
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
    }

    if (count > 1) {
        diag(Severity::ERROR, "E109", "Multi-character constant " + shorten(lexeme),
             "A char holds a single character. Use a string literal (\"...\") for text.",
             sl, sc, static_cast<int>(lexeme.size()));
        addToken(TokenType::INVALID, lexeme, sl, sc);
        return;
    }
    addToken(TokenType::CHAR_LITERAL, lexeme, sl, sc);
}

// ---------------------------------------------------------------------
//  Operators and punctuators (maximal munch)
// ---------------------------------------------------------------------
bool Lexer::scanOperatorOrPunctuator() {
    int sl = line, sc = col;

    if (src.compare(pos, 3, "...") == 0) {
        advance(); advance(); advance();
        addToken(TokenType::PUNCTUATOR, "...", sl, sc);
        return true;
    }
    for (const char* op : OPERATORS_3) {
        if (src.compare(pos, 3, op) == 0) {
            advance(); advance(); advance();
            addToken(TokenType::OPERATOR, op, sl, sc);
            return true;
        }
    }
    for (const char* op : OPERATORS_2) {
        if (src.compare(pos, 2, op) == 0) {
            advance(); advance();
            addToken(TokenType::OPERATOR, op, sl, sc);
            return true;
        }
    }

    char c = peek();
    if (PUNCTUATOR_CHARS.find(c) != std::string::npos) {
        advance();
        addToken(TokenType::PUNCTUATOR, std::string(1, c), sl, sc);
        trackBracket(c, sl, sc);
        return true;
    }
    if (OPERATOR_CHARS.find(c) != std::string::npos) {
        advance();
        addToken(TokenType::OPERATOR, std::string(1, c), sl, sc);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------
//  Anything else is an illegal character (panic-mode: skip it)
// ---------------------------------------------------------------------
void Lexer::scanInvalid() {
    int sl = line, sc = col;
    unsigned char first = static_cast<unsigned char>(peek());

    int bytes = 1;                              // keep a UTF-8 character together
    if (first >= 0xF0) bytes = 4;
    else if (first >= 0xE0) bytes = 3;
    else if (first >= 0xC0) bytes = 2;

    std::string lexeme;
    for (int i = 0; i < bytes && !atEnd(); ++i) lexeme += advance();

    std::string shown, hint;
    if (first < 0x20 || first == 0x7F) {
        char buf[8];
        std::snprintf(buf, sizeof buf, "0x%02X", first);
        shown = std::string("control character ") + buf;
        hint  = "Remove the hidden control character from the source file.";
    } else if (first >= 0x80) {
        shown = "'" + lexeme + "'";
        hint  = "Non-ASCII characters are only allowed inside strings and comments "
                "(watch out for curly quotes copied from documents).";
    } else {
        shown = "'" + lexeme + "'";
        if (first == '\\') hint = "A backslash is only valid inside literals or at the end of a line.";
        else               hint = "This symbol is not part of the language's character set.";
    }

    diag(Severity::ERROR, "E101", "Invalid character " + shown + " in source", hint, sl, sc, 1);
    addToken(TokenType::INVALID, lexeme, sl, sc);
}
