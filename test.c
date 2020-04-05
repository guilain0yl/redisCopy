#include<stdio.h>
#include"atomicvar.h"

#define TEST(var,count) do{\
    printf("%s",&var);\
}while(0)

int main(int args,char **argv)
{
    TEST("1",1);
    return 0;
}

