// =====================================================================
//  report.h  -  Formatting the analyzer's output (tables, errors, stats)
// =====================================================================
#ifndef REPORT_H
#define REPORT_H

#include <ostream>
#include <string>

#include "lexer.h"

namespace report {

void printSummary    (std::ostream& out, const LexResult& r, const std::string& sourceName, bool color);
void printTokens     (std::ostream& out, const LexResult& r, bool color);
void printSymbols    (std::ostream& out, const LexResult& r, bool color);
void printDiagnostics(std::ostream& out, const LexResult& r, bool color);
void printStatistics (std::ostream& out, const LexResult& r, bool color);
void printSource     (std::ostream& out, const LexResult& r, bool color);
void printFull       (std::ostream& out, const LexResult& r, const std::string& sourceName, bool color);
void printReference  (std::ostream& out, bool color);
void printJson       (std::ostream& out, const LexResult& r);   // machine-readable, used by the web UI

} // namespace report

#endif // REPORT_H
