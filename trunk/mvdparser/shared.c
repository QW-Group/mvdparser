#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "maindef.h"

//
// Q_malloc
//
// Use it instead of malloc so that if memory allocation fails,
// the program exits with a message saying there's not enough memory
// instead of crashing after trying to use a NULL pointer
//
void *Q_malloc (size_t size)
{
	void *p = malloc(size);

	if (!p)
		Sys_Error ("Q_malloc: Not enough memory free; check disk space\n");

#ifndef _DEBUG
	memset(p, 0, size);
#endif

	return p;
}

void *Q_calloc (size_t n, size_t size)
{
	void *p = calloc(n, size);

	if (!p)
		Sys_Error ("Q_calloc: Not enough memory free; check disk space\n");

	return p;
}

void *Q_realloc (void *p, size_t newsize)
{
	if(!(p = realloc(p, newsize)))
		Sys_Error ("Q_realloc: Not enough memory free; check disk space\n");

	return p;
}

char *Q_strdup (const char *src)
{
	char *p = strdup(src);

	if (!p)
		Sys_Error ("Q_strdup: Not enough memory free; check disk space\n");
	return p;
}

char *Sys_RedToWhite(char *txt)
{
	byte *s = txt;

	if (!s)
	{
		return NULL;
	}

	while (*s)
	{
		if ((*s >= 18) && (*s <= 27))
		{
			*s += 30;
		}
		else if (*s >= 146 && *s <= 155)
		{
			*s -= 98; // Yellow numbers.
		}
		else
		{
			*s &= ~128;	// Remove the highest bit in the byte (which makes the text red).
		}

		// Get rid of bell chars (ASCII 7), printf will beep otherwise :)
		if (*s == (char)0x7)
		{
			*s = ' ';
		}

		s++;
	}

	return txt;
}

void Sys_PrintError(char *format, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, format);
	vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	printf(string, argptr);
}

void Sys_Print(char *format, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, format);
	vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	//fprintf(stdout, string, argptr);
	printf(string, argptr);
}

void Sys_PrintDebug(int debuglevel, char *format, ...)
{
	va_list argptr;
	char string[1024];

	if (debuglevel > cmdargs.debug)
	{
		return;
	}

	va_start(argptr, format);
	vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	printf(string, argptr);
}

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	// Copy as many bytes as will fit.
	if (n != 0 && --n != 0) 
	{
		do 
		{
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	// Not enough room in dst, add NULL and traverse rest of src.
	if (n == 0) 
	{
		if (siz != 0)
			*d = '\0';		// NULL-terminate dst.

		while (*s++)
			;
	}

	return(s - src - 1);	// Count does not include NULL.
}
#endif


