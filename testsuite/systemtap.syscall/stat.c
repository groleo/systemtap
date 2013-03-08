/* COVERAGE: getcwd fstat stat lstat utime */
/* COVERAGE: fstat64 stat64 lstat64 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

int main()
{
  int fd;
  char cwd[128];
  struct stat sbuf;
  struct utimbuf ubuf;

  getcwd(cwd, 128);
  //staptest// getcwd (XXXX, 128) = NNNN

  fd = creat("foobar",S_IREAD|S_IWRITE);
  //staptest// [[[[open ("foobar", O_WRONLY|O_CREAT[[[[.O_LARGEFILE]]]]?|O_TRUNC!!!!creat ("foobar"]]]], 0600) = NNNN

  fstat(fd, &sbuf);
  //staptest// fstat (NNNN, XXXX) = 0

  close(fd);

  stat("foobar",&sbuf);
  //staptest// stat ("foobar", XXXX) = 0

  lstat("foobar",&sbuf);
  //staptest// lstat ("foobar", XXXX) = 0

  ubuf.actime = 1;
  ubuf.modtime = 1135641600;
  utime("foobar", &ubuf);
#if defined(__ia64__) || defined(__arm__)
  //staptest// utimes ("foobar", \[1.000000\]\[1135641600.000000\]) =
#else
  //staptest// utime ("foobar", \[Thu Jan  1 00:00:01 1970, Tue Dec 27 00:00:00 2005\]) = 0
#endif

  ubuf.actime =  1135690000;
  ubuf.modtime = 1135700000;
  utime("foobar", &ubuf);
#if defined(__ia64__) || defined(__arm__)
  //staptest// utimes ("foobar", \[1135690000.000000\]\[1135700000.000000\]) =
#else
  //staptest// utime ("foobar", \[Tue Dec 27 13:26:40 2005, Tue Dec 27 16:13:20 2005\]) = 0
#endif
  return 0;
}
