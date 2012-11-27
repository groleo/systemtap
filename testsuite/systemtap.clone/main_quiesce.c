#include <pthread.h>
#include <stdio.h>

void spool_write_script(int jobid) __attribute__ ((noinline));

void spool_write_script(int jobid)
{
    printf("sleeping... %d\n", 1 + (jobid % 2));
    sleep(1 + (jobid % 2));
}

void *mythread(void *unused)
{
    int i;
    for (i = 0; i < 30; i++)
	spool_write_script(i);
    return NULL;
}

int main()
{
    pthread_t tid;

    pthread_create(&tid, NULL, mythread, NULL);

    pthread_join(tid, NULL);
}
