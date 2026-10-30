// =====================================================================
//  report.cpp  -  Pretty-printing of tokens, symbols and diagnostics
// =====================================================================
#include "report.h"

#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <set>
#include <vector>

namespace {

const char* RESET   = "\033[0m";
const char* BOLD    = "\033[1m";
const char* DIM     = "\033[2m";
const char* RED     = "\033[31m";
const char* GREEN   = "\033[32m";
const char* YELLOW  = "\033[33m";
const char* BLUE    = "\033[34m";
const char* MAGENTA = "\033[35m";
const char* CYAN    = "\033[36m";

// Returns the escape code only when colors are enabled.
class Paint {
public:
    explicit Paint(bool on) : on_(on) {}
    std::string operator()(const char* code) const { return on_ ? code : ""; }
private:
    bool on_;
};

std::string pad(const std::string& s, size_t width) {
    return s.size() >= width ? s + " " : s + std::string(width - s.size(), ' ');
}

std::string visible(const std::string& s, size_t max) {
    std::string out;
    for (char c : s) {
        if (c == '\n')      out += "\\n";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    if (out.size() > max) out = out.substr(0, max - 3) + "...";
    return out;
}

const char* tokenColor(TokenType t) {
    switch (t) {
        case TokenType::KEYWORD:         return MAGENTA;
        case TokenType::IDENTIFIER:      return CYAN;
        case TokenType::INTEGER_LITERAL:
        case TokenType::FLOAT_LITERAL:   return YELLOW;
        case TokenType::CHAR_LITERAL:
        case TokenType::STRING_LITERAL:  return GREEN;
        case TokenType::PREPROCESSOR:    return BLUE;
        case TokenType::INVALID:         return RED;
        default:                         return "";
    }
}

void heading(std::ostream& out, const std::string& title, const Paint& p) {
    size_t fill = title.size() < 62 ? 62 - title.size() : 2;
    out << "\n" << p(BOLD) << p(CYAN) << "==[ " << title << " ]" << std::string(fill, '=')
        << p(RESET) << "\n";
}

int countedLines(const LexResult& r) {
    int n = static_cast<int>(r.sourceLines.size());
    if (n > 1 && r.sourceLines.back().empty()) --n;   // trailing newline
    return n;
}

int realTokenCount(const LexResult& r) {
    return static_cast<int>(std::count_if(r.tokens.begin(), r.tokens.end(),
        [](const Token& t) { return t.type != TokenType::END_OF_FILE; }));
}

} // namespace

namespace report {

// ---------------------------------------------------------------------
void printSummary(std::ostream& out, const LexResult& r, const std::string& name, bool color) {
    Paint p(color);
    heading(out, "ANALYSIS SUMMARY", p);
    int errors = r.errorCount(), warnings = r.warningCount();

    out << "  Source file        : " << name << "\n"
        << "  Lines scanned      : " << countedLines(r) << "\n"
        << "  Tokens found       : " << realTokenCount(r) << "\n"
        << "  Unique identifiers : " << r.symbols.size() << "\n"
        << "  Comments skipped   : " << r.commentCount << "\n"
        << "  Errors             : " << (errors ? p(RED) : p(GREEN)) << errors << p(RESET) << "\n"
        << "  Warnings           : " << (warnings ? p(YELLOW) : p(GREEN)) << warnings << p(RESET) << "\n"
        << "  Result             : ";
    if (errors == 0)
        out << p(BOLD) << p(GREEN) << "PASSED - source is lexically valid" << p(RESET) << "\n";
    else
        out << p(BOLD) << p(RED) << "FAILED - " << errors << " lexical error(s) found" << p(RESET) << "\n";
}

// ---------------------------------------------------------------------
void printTokens(std::ostream& out, const LexResult& r, bool color) {
    Paint p(color);
    heading(out, "TOKEN STREAM", p);
    out << p(BOLD) << "  " << pad("#", 6) << pad("LINE:COL", 11) << pad("TOKEN TYPE", 17)
        << "LEXEME" << p(RESET) << "\n";
    out << "  " << std::string(6 + 11 + 17 + 34, '-') << "\n";

    int i = 0;
    for (const Token& t : r.tokens) {
        if (t.type == TokenType::END_OF_FILE) continue;
        std::string where = std::to_string(t.line) + ":" + std::to_string(t.column);
        out << "  " << pad(std::to_string(++i), 6) << pad(where, 11)
            << p(tokenColor(t.type)) << pad(tokenTypeName(t.type), 17) << p(RESET)
            << visible(t.lexeme, 40) << "\n";
    }
    out << "  " << std::string(6 + 11 + 17 + 34, '-') << "\n"
        << "  " << i << " token(s)\n";
}

// ---------------------------------------------------------------------
void printSymbols(std::ostream& out, const LexResult& r, bool color) {
    Paint p(color);
    heading(out, "SYMBOL TABLE (identifiers)", p);
    if (r.symbols.empty()) { out << "  (no identifiers)\n"; return; }

    out << p(BOLD) << "  " << pad("#", 5) << pad("IDENTIFIER", 26) << pad("FIRST SEEN", 12)
        << pad("COUNT", 7) << "LINES" << p(RESET) << "\n";
    out << "  " << std::string(5 + 26 + 12 + 7 + 20, '-') << "\n";

    int i = 0;
    for (const SymbolEntry& s : r.symbols) {
        std::string lines;
        for (size_t k = 0; k < s.lines.size(); ++k) {
            if (k) lines += ", ";
            lines += std::to_string(s.lines[k]);
        }
        out << "  " << pad(std::to_string(++i), 5) << p(CYAN) << pad(visible(s.name, 25), 26) << p(RESET)
            << pad(std::to_string(s.firstLine) + ":" + std::to_string(s.firstColumn), 12)
            << pad(std::to_string(s.occurrences), 7) << visible(lines, 30) << "\n";
    }
}

// ---------------------------------------------------------------------
void printDiagnostics(std::ostream& out, const LexResult& r, bool color) {
    Paint p(color);
    heading(out, "ERRORS & WARNINGS", p);
    if (r.diagnostics.empty()) {
        out << "  " << p(GREEN) << "[OK] No lexical errors or warnings found." << p(RESET) << "\n";
        return;
    }

    for (const Diagnostic& d : r.diagnostics) {
        bool isError = d.severity == Severity::ERROR;
        const char* tint = isError ? RED : YELLOW;

        out << "\n  " << p(BOLD) << p(tint) << (isError ? "error" : "warning") << "[" << d.code << "]"
            << p(RESET) << p(BOLD) << " line " << d.line << ", col " << d.column << ": "
            << p(RESET) << d.message << "\n";

        if (d.line >= 1 && d.line <= static_cast<int>(r.sourceLines.size())) {
            const std::string& text = r.sourceLines[d.line - 1];
            out << "  " << p(DIM) << std::setw(5) << d.line << " | " << p(RESET) << text << "\n";

            // Build the caret line, keeping tabs so the marker lines up.
            std::string prefix;
            size_t b = 0;
            int chars = 0;
            while (b < text.size() && chars < d.column - 1) {
                unsigned char c = static_cast<unsigned char>(text[b]);
                if ((c & 0xC0) != 0x80) { prefix += (c == '\t' ? '\t' : ' '); ++chars; }
                ++b;
            }
            int remaining = 0;
            for (size_t k = b; k < text.size(); ++k)
                if ((static_cast<unsigned char>(text[k]) & 0xC0) != 0x80) ++remaining;
            int len = std::max(1, std::min(d.length, std::max(remaining, 1)));

            out << "  " << p(DIM) << "      | " << p(RESET) << prefix << p(BOLD) << p(tint)
                << "^" << std::string(len - 1, '~') << p(RESET) << "\n";
        }
        if (!d.hint.empty())
            out << "  " << p(CYAN) << "  hint: " << p(RESET) << d.hint << "\n";
    }
    out << "\n  " << r.errorCount() << " error(s), " << r.warningCount() << " warning(s)\n";
}

// ---------------------------------------------------------------------
void printStatistics(std::ostream& out, const LexResult& r, bool color) {
    Paint p(color);
    heading(out, "TOKEN STATISTICS", p);

    const TokenType order[] = {
        TokenType::KEYWORD, TokenType::IDENTIFIER, TokenType::INTEGER_LITERAL,
        TokenType::FLOAT_LITERAL, TokenType::CHAR_LITERAL, TokenType::STRING_LITERAL,
        TokenType::OPERATOR, TokenType::PUNCTUATOR, TokenType::PREPROCESSOR, TokenType::INVALID
    };
    std::vector<int> counts;
    int maxCount = 1;
    for (TokenType t : order) {
        int n = static_cast<int>(std::count_if(r.tokens.begin(), r.tokens.end(),
            [t](const Token& tok) { return tok.type == t; }));
        counts.push_back(n);
        maxCount = std::max(maxCount, n);
    }
    for (size_t i = 0; i < counts.size(); ++i) {
        int bar = counts[i] == 0 ? 0 : std::max(1, counts[i] * 36 / maxCount);
        out << "  " << pad(tokenTypeName(order[i]), 17) << std::setw(5) << counts[i] << "  "
            << p(tokenColor(order[i])) << std::string(bar, '#') << p(RESET) << "\n";
    }
    out << "  " << std::string(17 + 5, '-') << "\n"
        << "  " << pad("TOTAL", 17) << std::setw(5) << realTokenCount(r) << "\n";
}

// ---------------------------------------------------------------------
void printSource(std::ostream& out, const LexResult& r, bool color) {
    Paint p(color);
    heading(out, "SOURCE LISTING  ('>' marks lines with problems)", p);
    std::set<int> errorLines, warnLines;
    for (const Diagnostic& d : r.diagnostics)
        (d.severity == Severity::ERROR ? errorLines : warnLines).insert(d.line);

    int n = countedLines(r);
    for (int i = 1; i <= n; ++i) {
        std::string mark = " ";
        const char* tint = "";
        if (errorLines.count(i))     { mark = ">"; tint = RED; }
        else if (warnLines.count(i)) { mark = ">"; tint = YELLOW; }
        out << "  " << p(tint) << mark << p(RESET) << p(DIM) << std::setw(4) << i << " | "
            << p(RESET) << r.sourceLines[i - 1] << "\n";
    }
}

// ---------------------------------------------------------------------
void printFull(std::ostream& out, const LexResult& r, const std::string& name, bool color) {
    printSummary(out, r, name, color);
    printSource(out, r, color);
    printTokens(out, r, color);
    printSymbols(out, r, color);
    printStatistics(out, r, color);
    printDiagnostics(out, r, color);
}

// ---------------------------------------------------------------------
void printReference(std::ostream& out, bool color) {
    Paint p(color);
    heading(out, "LANGUAGE REFERENCE", p);
    out << p(BOLD) << "  Keywords" << p(RESET) << "\n"
        << "    auto break bool case char const continue default do double else enum\n"
        << "    extern false float for goto if inline int long register restrict return\n"
        << "    short signed sizeof static struct switch true typedef union unsigned\n"
        << "    void volatile while\n\n"
        << p(BOLD) << "  Operators" << p(RESET) << "\n"
        << "    <<= >>=  ++ -- -> += -= *= /= %= &= |= ^= == != <= >= && || << >>\n"
        << "    + - * / % = < > ! & | ^ ~ ? : .\n\n"
        << p(BOLD) << "  Punctuators" << p(RESET) << "\n"
        << "    ( ) { } [ ] ; , ...\n\n"
        << p(BOLD) << "  Literals" << p(RESET) << "\n"
        << "    Integer : 42  0755 (octal)  0x1F (hex)  10U  20L  30UL  40LL\n"
        << "    Float   : 3.14  .5  2.  6.02e23  1.5E-3  2.0f\n"
        << "    Char    : 'a'  '\\n'  '\\x41'  '\\101'\n"
        << "    String  : \"Hello, World\\n\"\n\n"
        << p(BOLD) << "  Identifiers" << p(RESET) << "\n"
        << "    Start with a letter or '_', then letters, digits or '_'. Max "
        << Lexer::MAX_IDENTIFIER_LENGTH << " characters.\n\n"
        << p(BOLD) << "  Comments & directives" << p(RESET) << "\n"
        << "    // line comment     /* block comment */     #include, #define, ... (whole line)\n";

    heading(out, "ERROR CODES", p);
    struct Row { const char* code; const char* text; };
    const Row rows[] = {
        {"E101", "Invalid character in source (e.g. @ $ ` or non-ASCII)"},
        {"E102", "Unterminated string literal"},
        {"E103", "Unterminated character constant"},
        {"E104", "Unterminated block comment"},
        {"E105", "Malformed number (bad suffix, 2+ decimal points, bad octal, empty exponent/hex)"},
        {"E106", "Invalid escape sequence inside a literal"},
        {"E107", "Empty character constant ''"},
        {"E108", "Identifier longer than the maximum length"},
        {"E109", "Multi-character character constant"},
        {"E110", "Integer constant too large"},
        {"E111", "Unknown preprocessor directive"},
        {"W201", "Unmatched or mismatched closing bracket"},
        {"W202", "Opening bracket never closed"},
        {"W203", "'/*' inside a block comment (comments do not nest)"},
    };
    for (const Row& row : rows) {
        bool isErr = row.code[0] == 'E';
        out << "    " << p(isErr ? RED : YELLOW) << row.code << p(RESET) << "  " << row.text << "\n";
    }
}

// ---------------------------------------------------------------------
//  JSON output (consumed by the web UI in ui/)
// ---------------------------------------------------------------------
namespace {

std::string jsonString(const std::string& s) {
    std::string out = "\"";
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        if      (ch == '"')  out += "\\\"";
        else if (ch == '\\') out += "\\\\";
        else if (ch == '\n') out += "\\n";
        else if (ch == '\t') out += "\\t";
        else if (c < 0x20 || c == 0x7F) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04X", c);
            out += buf;
        }
        else out += ch;
    }
    return out + "\"";
}

} // namespace

void printJson(std::ostream& out, const LexResult& r) {
    out << "{\"errors\":" << r.errorCount()
        << ",\"warnings\":" << r.warningCount()
        << ",\"comments\":" << r.commentCount;

    out << ",\"tokens\":[";
    for (size_t i = 0; i < r.tokens.size(); ++i) {
        const Token& t = r.tokens[i];
        out << (i ? "," : "") << "{\"type\":" << jsonString(tokenTypeName(t.type))
            << ",\"lexeme\":" << jsonString(t.lexeme)
            << ",\"line\":" << t.line << ",\"column\":" << t.column << "}";
    }

    out << "],\"diagnostics\":[";
    for (size_t i = 0; i < r.diagnostics.size(); ++i) {
        const Diagnostic& d = r.diagnostics[i];
        out << (i ? "," : "")
            << "{\"severity\":\"" << (d.severity == Severity::ERROR ? "error" : "warning") << "\""
            << ",\"code\":" << jsonString(d.code)
            << ",\"message\":" << jsonString(d.message)
            << ",\"hint\":" << jsonString(d.hint)
            << ",\"line\":" << d.line << ",\"column\":" << d.column
            << ",\"length\":" << d.length << "}";
    }

    out << "],\"symbols\":[";
    for (size_t i = 0; i < r.symbols.size(); ++i) {
        const SymbolEntry& s = r.symbols[i];
        out << (i ? "," : "") << "{\"name\":" << jsonString(s.name)
            << ",\"firstLine\":" << s.firstLine << ",\"firstColumn\":" << s.firstColumn
            << ",\"occurrences\":" << s.occurrences << ",\"lines\":[";
        for (size_t j = 0; j < s.lines.size(); ++j) out << (j ? "," : "") << s.lines[j];
        out << "]}";
    }
    out << "]}\n";
}

} // namespace report
