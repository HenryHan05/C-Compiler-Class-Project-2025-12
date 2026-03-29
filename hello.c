#include <stdio.h>
//错误提示
int x;
// int x;//duplicate global

///*常量优化*/

int main(){
    int x;
    int y;

    // a=1;//undefined variable

    x = 5 + 1  ; //删去;提示expected token :59

    y = x + 2;
    printf("%d\n",y);
}
