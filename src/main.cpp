// =====================================================================
//  main.cpp  -  Interactive console interface for the lexical analyzer
//
//  Interactive mode :  ./lexer
//  Command-line mode:  ./lexer program.c [-o report.txt] [--no-color]
// =====================================================================
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  // windows.h declares a TokenType enumerator and an ERROR macro that
  // clash with lexer.h, so rename/undefine them around the include.
  #define TokenType WinTokenType
  #include <windows.h>
  #undef TokenType
  #undef ERROR
  #include <fcntl.h>
  #include <io.h>
  #define IS_TTY(f) _isatty(_fileno(f))
#else
  #include <unistd.h>
  #define IS_TTY(f) isatty(fileno(f))
#endif

#include "lexer.h"
#include "report.h"
#include "samples.h"

namespace {

bool        g_color     = true;
bool        g_hasResult = false;
LexResult   g_result;
std::string g_sourceName;

// ---------------------------------------------------------------------
//  Console helpers
// ---------------------------------------------------------------------
void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode) ||
        !SetConsoleMode(h, mode | 0x0004 /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */))
        g_color = false;
#endif
    if (!IS_TTY(stdout)) g_color = false;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Reads one line; on end-of-input returns "0" so every menu exits cleanly.
std::string ask(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string s;
    if (!std::getline(std::cin, s)) { std::cout << "\n"; return "0"; }
    return trim(s);
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string stripQuotes(std::string s) {
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front())
        s = s.substr(1, s.size() - 2);
    return s;
}

std::string col(const char* code) { return g_color ? code : ""; }

void banner() {
    std::cout << col("\033[1m\033[36m")
              << "\n  +============================================================+\n"
              << "  |     C-LIKE LANGUAGE LEXICAL ANALYZER WITH ERROR REPORTING    |\n"
              << "  |            Compiler Design  -  Mini Project                  |\n"
              << "  +============================================================+"
              << col("\033[0m") << "\n";
}

// ---------------------------------------------------------------------
//  Export the last result as a plain-text report
// ---------------------------------------------------------------------
bool exportReport(const std::string& path) {
    std::ofstream out(path);
    if (!out) return false;
    std::time_t now = std::time(nullptr);
    char stamp[64];
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    out << "C-Like Language Lexical Analyzer - Analysis Report\n"
        << "Generated: " << stamp << "\n";
    report::printFull(out, g_result, g_sourceName, false);
    return static_cast<bool>(out);
}

void exportPrompt() {
    if (!g_hasResult) { std::cout << "  Nothing to export yet. Analyze some code first.\n"; return; }
    std::string path = stripQuotes(ask("  Report file name [lexer_report.txt]: "));
    if (path.empty() || path == "0") path = "lexer_report.txt";
    if (exportReport(path)) std::cout << col("\033[32m") << "  Report saved to " << path << col("\033[0m") << "\n";
    else                    std::cout << col("\033[31m") << "  Could not write " << path << col("\033[0m") << "\n";
}

// ---------------------------------------------------------------------
//  Menu shown after every analysis
// ---------------------------------------------------------------------
void resultMenu() {
    while (true) {
        std::cout << "\n  " << col("\033[1m") << "VIEW RESULTS" << col("\033[0m") << "  (" << g_sourceName << ")\n"
                  << "   [1] Token stream          [5] Source listing\n"
                  << "   [2] Symbol table          [6] Full report\n"
                  << "   [3] Errors & warnings     [7] Export report to file\n"
                  << "   [4] Token statistics      [0] Back to main menu\n";
        std::string choice = ask("  Choose: ");

        if      (choice == "1") report::printTokens(std::cout, g_result, g_color);
        else if (choice == "2") report::printSymbols(std::cout, g_result, g_color);
        else if (choice == "3") report::printDiagnostics(std::cout, g_result, g_color);
        else if (choice == "4") report::printStatistics(std::cout, g_result, g_color);
        else if (choice == "5") report::printSource(std::cout, g_result, g_color);
        else if (choice == "6") report::printFull(std::cout, g_result, g_sourceName, g_color);
        else if (choice == "7") exportPrompt();
        else if (choice == "0") return;
        else std::cout << "  Please enter a number from the menu.\n";
    }
}

void analyze(const std::string& code, const std::string& name) {
    Lexer lexer(code);
    g_result     = lexer.analyze();
    g_sourceName = name;
    g_hasResult  = true;

    report::printSummary(std::cout, g_result, name, g_color);
    report::printDiagnostics(std::cout, g_result, g_color);
    resultMenu();
}

// ---------------------------------------------------------------------
//  Main menu actions
// ---------------------------------------------------------------------
void analyzeFile() {
    std::string path = stripQuotes(ask("  Path to source file (e.g. samples/valid_program.c): "));
    if (path.empty() || path == "0") return;
    std::string code;
    if (!readFile(path, code)) {
        std::cout << col("\033[31m") << "  Cannot open '" << path << "'." << col("\033[0m") << "\n";
        return;
    }
    analyze(code, path);
}

void analyzeTyped() {
    std::cout << "\n  Type or paste your code below.\n"
              << "  Finish with a line containing only " << col("\033[1m") << "END" << col("\033[0m") << ".\n\n";
    std::string code, lineText;
    int n = 1;
    while (true) {
        std::cout << col("\033[2m") << "  " << (n < 10 ? " " : "") << n << " | " << col("\033[0m") << std::flush;
        if (!std::getline(std::cin, lineText)) break;
        if (trim(lineText) == "END") break;
        code += lineText + "\n";
        ++n;
    }
    if (trim(code).empty()) { std::cout << "  No code entered.\n"; return; }
    analyze(code, "<typed input>");
}

} // namespace

// =====================================================================
int main(int argc, char* argv[]) {
    setupConsole();

    // ---------------- Command-line mode ----------------
    if (argc > 1) {
        std::string file, outFile;
        bool json = false;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--no-color")                   g_color = false;
            else if (a == "--json")                  json = true;
            else if (a == "-o" && i + 1 < argc)      outFile = argv[++i];
            else if (a == "-h" || a == "--help") {
                std::cout << "Usage: " << argv[0] << " [source-file] [-o report.txt] [--no-color] [--json]\n"
                          << "Use '-' as the source file to read from standard input.\n"
                          << "Run with no arguments for the interactive menu.\n";
                return 0;
            }
            else file = a;
        }
        if (file.empty()) { std::cerr << "No source file given.\n"; return 2; }

        std::string code;
        if (file == "-") {
#ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
#endif
            std::ostringstream ss;
            ss << std::cin.rdbuf();
            code = ss.str();
            file = "<stdin>";
        } else if (!readFile(file, code)) { std::cerr << "Cannot open '" << file << "'\n"; return 2; }

        if (json) {                       // machine-readable output for the web UI
            LexResult r = Lexer(code).analyze();
            report::printJson(std::cout, r);
            return r.errorCount() > 0 ? 1 : 0;
        }

        g_result = Lexer(code).analyze();
        g_sourceName = file;
        g_hasResult = true;
        report::printFull(std::cout, g_result, file, g_color);

        if (!outFile.empty()) {
            if (exportReport(outFile)) std::cout << "\nReport written to " << outFile << "\n";
            else { std::cerr << "Cannot write '" << outFile << "'\n"; return 2; }
        }
        return g_result.errorCount() > 0 ? 1 : 0;
    }

    // ---------------- Interactive mode ----------------
    banner();
    while (true) {
        std::cout << "\n  " << col("\033[1m") << "MAIN MENU" << col("\033[0m") << "\n"
                  << "   [1] Analyze a source file\n"
                  << "   [2] Type / paste code to analyze\n"
                  << "   [3] Demo: valid C program\n"
                  << "   [4] Demo: program with lexical errors\n"
                  << "   [5] Re-open last analysis\n"
                  << "   [6] Language reference & error codes\n"
                  << "   [7] Colored output: " << (g_color ? "ON" : "OFF") << "\n"
                  << "   [0] Exit\n";
        std::string choice = ask("  Choose: ");

        if      (choice == "1") analyzeFile();
        else if (choice == "2") analyzeTyped();
        else if (choice == "3") analyze(SAMPLE_VALID, "demo: valid_program.c");
        else if (choice == "4") analyze(SAMPLE_ERRORS, "demo: program_with_errors.c");
        else if (choice == "5") {
            if (g_hasResult) resultMenu();
            else std::cout << "  No analysis yet.\n";
        }
        else if (choice == "6") report::printReference(std::cout, g_color);
        else if (choice == "7") g_color = !g_color;
        else if (choice == "0") { std::cout << "  Goodbye!\n"; return 0; }
        else std::cout << "  Please enter a number from the menu.\n";
    }
}
