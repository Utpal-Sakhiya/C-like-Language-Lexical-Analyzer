CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
SRC       = src/main.cpp src/lexer.cpp src/report.cpp
HDR       = src/lexer.h src/report.h src/samples.h
TARGET    = lexer

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

demo: $(TARGET)
	-./$(TARGET) samples/program_with_errors.c

clean:
	rm -f $(TARGET) $(TARGET).exe lexer_report.txt
