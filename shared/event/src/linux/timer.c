/*
 Copyright (c) 2011 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include <sys/timerfd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

static int tfd = -1;

inline int timer_get()
{
  return tfd;
}

int timer_start(int usec)
{
  struct timespec period = {.tv_sec = 0, .tv_nsec = usec * 1000};

  struct itimerspec new_value =
  {
      .it_interval = period,
      .it_value = period,
  };
  
  if(tfd != -1)
  {
    return tfd;
  }

  tfd = timerfd_create(CLOCK_REALTIME, 0);
  if(tfd < 0)
  {
    fprintf(stderr, "timerfd_create");
  }
  else if(timerfd_settime(tfd, 0, &new_value, NULL))
  {
    fprintf(stderr, "timerfd_settime");
    close(tfd);
    tfd = -1;
  }
  return tfd;
}

int timer_close(int unused)
{
  close(tfd);
  tfd = -1;

  return 1;
}

int timer_read(int unused)
{
  uint64_t exp;

  ssize_t res;

  res = read (tfd, &exp, sizeof(exp));

  if (res != sizeof(exp)) {
    fprintf (stderr, "Wrong timer fd read...\n");
  }
  else
  {
    if(exp > 1)
    {
      fprintf (stderr, "Timer fired several times...\n");
    }
    return 1;
  }
  return 0;
}
