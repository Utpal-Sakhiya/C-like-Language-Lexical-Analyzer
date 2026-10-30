#include <stdio.h>
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
