#include <windows.h>
#include <windef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mmsystem.h>
#include <limits.h>
#include "strptime.h"
#include "maindef.h"
#include <signal.h>

static double pfreq;
static qbool hwtimer;

void Sys_InitDoubleTime (void) 
{
	__int64 freq;

	if (QueryPerformanceFrequency((LARGE_INTEGER *)&freq) && freq > 0) 
	{
		// Hardware timer available
		pfreq = (double)freq;
		hwtimer = true;
	} 
	else 
	{
		// Make sure the timer is high precision, otherwise NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

double Sys_DoubleTime (void) 
{
	__int64 pcount;
	static __int64 startcount;
	static DWORD starttime;
	static qbool first = true;
	DWORD now;

	if (hwtimer) 
	{
		QueryPerformanceCounter ((LARGE_INTEGER *)&pcount);
		if (first) 
		{
			first = false;
			startcount = pcount;
			return 0.0;
		}
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime();

	if (first) 
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // Wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;
}



