/* COVERAGE: alarm nanosleep pause */
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <signal.h>

static void
sigrt_act_handler(int signo, siginfo_t *info, void *context)
{
}

int main()
{
  struct timespec rem, t = {0,789};
  struct sigaction sigrt_act;
  memset(&sigrt_act, 0, sizeof(sigrt_act));
  sigrt_act.sa_handler = (void *)sigrt_act_handler;
  sigaction(SIGALRM, &sigrt_act, NULL);

  alarm(1);
#if defined(__ia64__) || defined(__arm__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,1.000000\], XXXX) = 0
#else
  //staptest// alarm (1) = 0
#endif

  pause();
#if defined(__ia64__)
  //staptest// rt_sigsuspend () =
#else
  //staptest// pause () =
#endif

  alarm(0);
#if defined(__ia64__) || defined(__arm__)
  //staptest// setitimer (ITIMER_REAL, \[0.000000,0.000000\], XXXX) = 0
#else
  //staptest// alarm (0) = 0
#endif

  sleep(1);
  //staptest// nanosleep (\[1.000000000\], XXXX) = 0

  usleep(1234);
  //staptest// nanosleep (\[0.001234000\], 0x[0]+) = 0

  nanosleep(&t, &rem); 
  //staptest// nanosleep (\[0.000000789\], XXXX) = 0

  nanosleep(&t, NULL); 
  //staptest// nanosleep (\[0.000000789\], 0x[0]+) = 0

  return 0;
}

