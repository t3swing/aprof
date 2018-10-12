#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

int count = 0;
int cost = 0;

void compute3()
{
    usleep(1);
}

void compute2()
{
    count++;
    while (++cost % 100 * 10000);
}

void compute1()
{
    count++;
    while (++cost % 1000 * 10000);
}

void thread1()
{
    printf("thread1 pid:%ld \n",syscall(SYS_gettid));
    while (count < 100000 * 10000)
    {
        compute1();
        compute2();
        compute3();
    }
}

int main()
{
    pthread_t tid;
    
    pthread_create(&tid, NULL, (void *)thread1, NULL);

    while(1)
    {
        usleep(10*1000);
    }

    printf("test exit\n");
    return 0;
}
