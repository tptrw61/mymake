#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include "sys/wait.h"
#endif
int main(void) {
    printf("%d\n", system("./fail.out"));
    printf("%d\n", system("sleep 2;"));
    printf("%d\n", system("./fail.out"));
    int rv = system("sleep 2");
    printf("\n%d\n", WEXITSTATUS(rv));
    rv = system("./fail.out");
    printf("%d\n", WEXITSTATUS(rv));

    return 0;
}