#include "pageReplace.h"
#include <stdio.h>

int main() {
    long physic_memory[MAX_PHY_PAGE] = {0};
    long n;
    while (~scanf("%ld", &n)) {
        pageReplace(physic_memory, n);
        printf("[ ");
        for (int i = 0; i < MAX_PHY_PAGE; i++)
            printf("%d ", physic_memory[i]);
        puts("]");
    }
}
