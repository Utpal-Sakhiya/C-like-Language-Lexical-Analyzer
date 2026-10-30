/* Sample program: student marks and factorial
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
