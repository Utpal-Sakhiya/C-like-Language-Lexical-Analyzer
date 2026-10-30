// =====================================================================
//  lexer.h  -  C-Like Language Lexical Analyzer with Error Reporting
//  Compiler Design Mini Project
// =====================================================================
#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------
//  Token categories recognised by the analyzer
// ---------------------------------------------------------------------
enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    CHAR_LITERAL,
    STRING_LITERAL,
    OPERATOR,
    PUNCTUATOR,
    PREPROCESSOR,
    INVALID,        // a lexeme that contains a lexical error
    END_OF_FILE
};

const char* tokenTypeName(TokenType type);

struct Token {
    TokenType   type;
    std::string lexeme;
    int         line;
    int         column;
};

// ---------------------------------------------------------------------
//  Diagnostics (errors and warnings)
// ---------------------------------------------------------------------
enum class Severity { ERROR, WARNING };

struct Diagnostic {
    Severity    severity;
    std::string code;      // e.g. "E102"
    std::string message;   // what went wrong
    std::string hint;      // how to fix it
    int         line;
    int         column;
    int         length;    // number of characters to underline
};

// ---------------------------------------------------------------------
//  Symbol table entry (one per distinct identifier)
// ---------------------------------------------------------------------
struct SymbolEntry {
    std::string      name;
    int              firstLine;
    int              firstColumn;
    int              occurrences;
    std::vector<int> lines;    // distinct lines where it appears
};

// ---------------------------------------------------------------------
//  Everything produced by one run of the lexer
// ---------------------------------------------------------------------
struct LexResult {
    std::vector<Token>       tokens;
    std::vector<Diagnostic>  diagnostics;
    std::vector<SymbolEntry> symbols;
    std::vector<std::string> sourceLines;
    int commentCount = 0;

    int errorCount() const;
    int warningCount() const;
};

// ---------------------------------------------------------------------
//  The lexical analyzer (hand-written, single pass, panic-mode recovery)
// ---------------------------------------------------------------------
class Lexer {
public:
    static constexpr int MAX_IDENTIFIER_LENGTH = 31;

    explicit Lexer(std::string source);
    LexResult analyze();

private:
    struct OpenBracket { char ch; int line; int column; };

    std::string src;
    size_t pos  = 0;
    int    line = 1;
    int    col  = 1;

    LexResult result;
    std::unordered_map<std::string, size_t> symbolIndex;
    std::vector<OpenBracket> bracketStack;

    // character helpers
    bool atEnd() const;
    char peek(size_t ahead = 0) const;
    char advance();
    bool onlyWhitespaceBeforeOnLine() const;

    // bookkeeping
    void addToken(TokenType type, const std::string& lexeme, int l, int c);
    void diag(Severity sev, const std::string& code, const std::string& msg,
              const std::string& hint, int l, int c, int len);
    void recordSymbol(const std::string& name, int l, int c);
    void trackBracket(char ch, int l, int c);

    // scanners (one per token class)
    void skipLineComment();
    void skipBlockComment();
    void scanPreprocessor();
    void scanIdentifierOrKeyword();
    void scanNumber();
    void scanString();
    void scanChar();
    bool scanEscape(std::string& lexeme);
    bool scanOperatorOrPunctuator();
    void scanInvalid();
};

#endif // LEXER_H
