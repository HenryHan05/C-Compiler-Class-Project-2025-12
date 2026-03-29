#include <stdio.h>

///*斐波那契*/
int fibonacci(int n) {
    if (n <= 1) 
        return 1;
    else
        return fibonacci(n-1) + fibonacci(n-2);
}

int main(){
    int i;
    i = 0;
    while (i <= 5) {
        printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
        i ++;
    }
    for (  ;i <= 10;  ) {
        printf("fibonacci(%2d) = %d\n", i, fibonacci(i));
        i ++;
    }
    return 0;
}