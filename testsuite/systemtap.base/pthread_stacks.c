#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>


static pthread_once_t printed_p = PTHREAD_ONCE_INIT;
void print_it () 
{
  int rc;
  pthread_attr_t foo;
  size_t size;
  rc = pthread_getattr_np (pthread_self(), & foo);
  assert (rc == 0);
  rc = pthread_attr_getstacksize(&foo, &size);
  assert (rc == 0);
  printf ("stacksize=%u\n", size);
  rc = pthread_attr_destroy (&foo);
  assert (rc == 0);
}

void *tfunc(void *arg)
{
  /* Choose some random thread to print stack size */
  (void) pthread_once(&printed_p, &print_it);
  sleep (4);
  return NULL;
}

 
int
main(int argc, char **argv)
{
    pthread_t thr;
    pthread_attr_t attr;
    int numthreads;
    int stacksize;
    int rc;

    if (argc != 3) {
	fprintf(stderr, "Usage: %s numthreads stacksize|0\n", argv[0]);
	return -1;
    }

    numthreads = atoi(argv[1]);
    stacksize = atoi(argv[2]);

    rc = pthread_attr_init(&attr);
    assert (rc == 0);

    if (stacksize > 0) {
      rc = pthread_attr_setstacksize(&attr, (size_t) stacksize);
      assert (rc == 0);
    }

    while (numthreads--) {
      rc = pthread_create(&thr, (stacksize == 0 ? NULL : &attr), tfunc, NULL);
      assert (rc == 0);
    }

    rc = pthread_attr_destroy(&attr);
    assert (rc == 0);

    return 0;
}

