#include "cethreads.h"

int i = 2;
int fact = 10;
int n = 10;
int mutex0 = 0;

void factorial() {
    while (i <= n) {
        CEmutex_lock(&mutex0);
        fact *= i;
        printf("Factorial(%d) = %d\n", i, fact);
        i++;
        CEmutex_unlock(&mutex0);
        sleep(1);
        CEThread_yield();
    }
}

void restart() {
    CEmutex_lock(&mutex0);
    i = 2;
    fact = 1;
    n = 10;
    printf("i: %d  fact: %d  n: %d\n", i, fact, n);
    CEmutex_unlock(&mutex0);
}

int main(){
    initCEThreads();
    //CEmutex_lock(mutex0);

    srand(time(0));

    int ID = CEThread_create(&factorial, NULL);
    int ID2 = CEThread_create(&factorial, NULL);
    int ID3 = CEThread_create(&restart, NULL);
    CEThread_wait(ID2);

    CEThread_wait();

    return 0;
}