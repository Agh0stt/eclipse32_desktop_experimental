#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: ekuecho <number>\n");
        return 1;
    }
    int n = atoi(argv[1]);
    printf("%d\n", n + 2);
    return 0;
}
