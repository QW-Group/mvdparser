
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#ifndef __FreeBSD__
#include <linux/rtc.h>
#endif
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sched.h>
#include <errno.h>
#include <dirent.h>

/* needed for RTC timer */
#define RTC_RATE 1024.00
static int rtc_fd;  /* file descriptor for rtc device */

void Sys_InitDoubleTime(void)
{
}

double Sys_DoubleTime(void)
{
    // RTC timer vars.
    unsigned long curticks = 0;
    struct pollfd pfd;
    static unsigned long totalticks;

    // Old timer vars.
    struct timeval tp;
    struct timezone tzp;
    static int secbase;

    if (rtc_fd)
	{  /* rtc timer is enabled */
		pfd.fd = rtc_fd;
		pfd.events = POLLIN | POLLERR;
again:
		if (poll(&pfd, 1, 100000) < 0)
		{
			if ((errno = EINTR))
			{
				// Happens with gdb or signal exiting.
				goto again;
			}
			Sys_Error("Poll call on RTC timer failed!\n");
		}
		read(rtc_fd, &curticks, sizeof(curticks));
		curticks = curticks >> 8; // knock out info byte.
		totalticks += curticks;
		return totalticks / RTC_RATE;
    }
	else
	{
		// Old timer.
		gettimeofday(&tp, &tzp);

		if (!secbase)
		{
			secbase = tp.tv_sec;
			return tp.tv_usec / 1000000.0;
		}

		return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
    }
}

