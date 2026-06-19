#include "stdio.h"

int main(int argc, char** argv) {
    char s[64];
    printf("Enter your name: ");
    scanf("%s", s);
    printf("Your name is %s\n", s);
    return 0;
}
