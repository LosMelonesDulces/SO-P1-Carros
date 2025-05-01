#include "cethreads.h"

int i = 2;
int fact = 1;
int n = 10;

void factorial() {
    while (i <= n) {
        fact *= i;
        printf("Factorial(%d) = %d\n", i, fact);
        i++;
        CEmutex_unlock();
        sleep(1);
        CEThread_yield();
    }
}

void restart() {
    CEmutex_trylock();
    i = 2;
    fact = 1;
    n = 10;
    printf("i: %d  fact: %d  n: %d\n", i, fact, n);
    CEmutex_unlock();
}

int main(){
    initCEThreads();
    CEmutex_init();

    srand(time(0));

    int ID = CEThread_create(&factorial, NULL);
    int ID2 = CEThread_create(&factorial, NULL);
    int ID3 = CEThread_create(&restart, NULL);
    CEThread_wait(ID2);

    CEThread_wait();

    return 0;
}