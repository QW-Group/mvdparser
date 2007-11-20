
#ifndef __MAINDEF_H__
#define __MAINDEF_H__

#include "stdlib.h"

#define MAX_PLAYERS				32

#define	MAX_INFO_KEY			64
#define	MAX_INFO_STRING			384
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

// Typedefs.
typedef unsigned char byte;
typedef float vec_t;
typedef vec_t vec3_t[3];
typedef vec_t vec5_t[5];
typedef enum {false, true} qbool;

typedef struct cmdline_params_s
{
	int debug;
	char **mvd_files;
	int mvd_files_count;
} cmdline_params_t;

extern cmdline_params_t cmdargs;

typedef struct sizebuf_s 
{
	qbool	allowoverflow;	// If false, do a Sys_Error
	qbool	overflowed;		// Set to true if the buffer size failed
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

// Math macros.
#define VectorCopy(a,b)			((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorSubtract(a,b,c)	((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])

// Memory management.
#define Q_free(ptr) if(ptr) { free(ptr); ptr = NULL; }
void *Q_malloc (size_t size);
void *Q_calloc (size_t n, size_t size);
void *Q_realloc (void *p, size_t newsize);
char *Q_strdup (const char *src);

void Sys_InitDoubleTime (void);
double Sys_DoubleTime (void);

char *Sys_RedToWhite(char *txt);

void Sys_Print(char *format, ...);
void Sys_PrintDebug(int debuglevel, char *format, ...);
void Sys_PrintError(char *format, ...);
#define Sys_Error(format, ...) {Sys_PrintError(format, ##__VA_ARGS__); exit(1);}

size_t strlcpy(char *dst, const char *src, size_t siz);

#endif // __MAINDEF_H__
