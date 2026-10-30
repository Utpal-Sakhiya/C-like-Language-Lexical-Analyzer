# C-Like Language Lexical Analyzer with Error Reporting

Compiler Design Mini Project. This is a hand-written lexical analyzer (scanner) for a C-like language, written in C++17. It breaks source code into tokens, builds a symbol table, and reports lexical errors gcc-style, giving the line, column, an underlined source line, and a hint for each one.

## Build & Run

**Linux / macOS**
```
make          # builds ./lexer
./lexer       # interactive menu
```

**Windows (MinGW g++)**: double-click `build.bat`, or run:
```
g++ -std=c++17 -O2 -o lexer.exe src\main.cpp src\lexer.cpp src\report.cpp
lexer.exe
```

**Command-line mode**
```
./lexer samples/program_with_errors.c              # full report on screen
./lexer myprog.c -o report.txt                     # also save plain-text report
./lexer myprog.c --no-color
```
The program exits with code 1 if lexical errors were found and 0 otherwise.

## Features
- Token classes: keywords, identifiers, integer constants (decimal, octal, hex, with U/L suffixes), float constants (fractions, exponents, F/L suffixes), char constants, string literals, operators (longest match, e.g. `<<=`), punctuators, and preprocessor directives
- Skips `//` and `/* */` comments and counts them
- Symbol table showing each identifier's first occurrence, its number of uses, and the lines it appears on
- Error recovery: after an error the lexer keeps scanning, so it reports all errors in a single run
- Interactive interface with views for the token stream, symbol table, errors, statistics bar chart, and an annotated source listing, plus export to a file

## Error Codes
| Code | Meaning |
|------|---------|
| E101 | Invalid character (`@`, `$`, `` ` ``, curly quotes, control chars) |
| E102 | Unterminated string literal |
| E103 | Unterminated character constant |
| E104 | Unterminated block comment |
| E105 | Malformed number: `2ndValue`, `3.14.15`, `0789`, `0x`, `1.5e` |
| E106 | Invalid escape sequence (`\q`, `\x` with no digits) |
| E107 | Empty character constant `''` |
| E108 | Identifier longer than 31 characters |
| E109 | Multi-character constant `'abc'` |
| E110 | Integer constant too large |
| E111 | Unknown preprocessor directive (`#defne`) |
| W201 | Unmatched / mismatched closing bracket |
| W202 | Opening bracket never closed |
| W203 | `/*` inside a block comment |

## Project Structure
```
src/lexer.h, lexer.cpp     Lexer: token types, scanning, error detection
src/report.h, report.cpp   Output formatting (tables, caret diagnostics, stats)
src/main.cpp               Menu-driven interface + command-line mode
src/samples.h              Built-in demo programs
samples/                   valid_program.c, program_with_errors.c
```

## Suggested Demo Flow (evaluation)
1. Choose **[3]** to run the valid program. It passes, and you can show the token stream and the symbol table.
2. Choose **[4]** to run the error program. It shows 15 errors and 2 warnings, each with a caret and a hint.
3. Choose **[2]** and type a line live, such as `int x = 5 @ 3;`, to show the error being caught immediately.
4. Choose **[7]** to export the report as a text file you can include in the report's screenshots section.
