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

void Sys_PrintError(char *format, ...)
{
	va_list argptr;
	char string[1024];

	va_start(argptr, format);
	vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	fprintf(stderr, string, argptr);
}

