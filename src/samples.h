// Built-in demo programs (identical to the files in samples/)
#ifndef SAMPLES_H
#define SAMPLES_H

static const char* const SAMPLE_VALID = R"SAMPLE(/* Sample program: student marks and factorial
   Demonstrates keywords, literals, operators and comments */
#include <stdio.h>
#define MAX_SIZE 10

typedef struct {
    char name[20];
    float marks;
} Student;

int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);   // recursive call
}

int main(void) {
    int values[MAX_SIZE] = {1, 2, 3, 4, 5};
    unsigned long total = 0UL;
    double avg = 0.0, scale = 2.5e-1;
    char grade = 'A', newline = '\n';
    int mask = 0x1F, perms = 0755;

    for (int i = 0; i < 5; i++) {
        total += values[i];
        values[i] <<= 1;
    }
    avg = (double) total / 5;
    if (avg >= 3.0 && grade != 'F')
        printf("Average: %.2f\tScaled: %f\n", avg, avg * scale);
    printf("5! = %d, mask & perms = %d\n", factorial(5), mask & perms);
    return 0;
}
)SAMPLE";

static const char* const SAMPLE_ERRORS = R"SAMPLE(#include <stdio.h>
#defne LIMIT 100

int main() {
    int count = 10;
    int 2ndValue = 5;              // identifier starts with a digit
    float ratio = 3.14.15;         // too many decimal points
    int octal = 0789;              // 8 and 9 are not octal digits
    int hex = 0x;                  // hex literal with no digits
    double big = 1.5e;             // exponent without digits
    char empty = '';               // empty character constant
    char many = 'abc';             // multi-character constant
    char *msg = "Hello\q World";   // unknown escape sequence
    int price = 100$;              // '$' is not a valid character
    int email@domain = 1;          // '@' is not a valid character
    int thisIdentifierNameIsWayTooLongForOurLanguage = 0;
    unsigned long huge = 99999999999999999999999;
    printf("Unterminated string);
    if (count > 0) {
        count--;
    }}
    /* this comment is never closed
    return 0;
}
)SAMPLE";

#endif // SAMPLES_H
