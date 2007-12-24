
#ifndef __MAINDEF_H__
#define __MAINDEF_H__

#include <stdlib.h>

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define MAX_PLAYERS				32

#define	MAX_INFO_KEY			64
#define	MAX_INFO_STRING			384
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

#define	MAX_QPATH			64		// max length of a quake game pathname
#define	MAX_OSPATH			128		// max length of a filesystem pathname

#define PLAYER_ISVALID(player) ((player)->name && !(player)->spectator && !(player)->is_ghost)

#define GREEN_ARMOR_BIT			13
#define GREEN_ARMOR				1
#define YELLOW_ARMOR			2
#define RED_ARMOR				3
#define ARMOR_FLAG_BYNUM(num)	(IT_ARMOR##num)

#define HAS_ARMOR(player, num)	((player)->stats[STAT_ITEMS] & (1 << (GREEN_ARMOR_BIT - 1 + bound(GREEN_ARMOR, num, RED_ARMOR))))

#define clamp(a,b,c) (a = min(max(a, b), c))
#define bound(a,b,c) ((a) >= (c) ? (a) : (b) < (a) ? (a) : (b) > (c) ? (c) : (b))
#define Q_rint(x) ((x) > 0 ? (int) ((x) + 0.5) : (int) ((x) - 0.5))

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
	char *template_file;
	char *frag_file;
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
void *Q_malloc(size_t size);
void *Q_calloc(size_t n, size_t size);
void *Q_realloc(void *p, size_t newsize);
char *Q_strdup(const char *src);
float Q_atof(const char *str);

#ifdef _WIN32
#define strcasecmp(s1, s2)	_stricmp((s1), (s2))
#define strncasecmp(s1, s2, n)	_strnicmp((s1), (s2), (n))
#endif // _WIN32

char *strstri(const char *text, const char *find); // A Case-insensitive strstr.

void Sys_InitDoubleTime(void);
double Sys_DoubleTime(void);

char *Sys_RedToWhite(char *txt);

void Sys_Print(char *format, ...);
void Sys_PrintDebug(int debuglevel, char *format, ...);
void Sys_PrintError(char *format, ...);
#define Sys_Error(format, ...) {Sys_PrintError(format, ##__VA_ARGS__); exit(1);}

#if defined(__linux__) || defined(_WIN32)
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#ifdef _WIN32
int snprintf(char *buffer, size_t count, char const *format, ...);
//int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
#endif

// Append an extension to a path.
void COM_ForceExtensionEx(char *path, char *extension, size_t path_size);

qbool COM_ReadFile(const char *filename, byte **data, long *filelen);

#define MAX_COM_TOKEN	1024
#define MAX_ARGS		80

typedef struct tokenizecontext_s
{
	int		cmd_argc;						// Arguments count
	char	*cmd_argv[MAX_ARGS];			// Links to argv_buf[]
	char	argv_buf[MAX_COM_TOKEN];		// Here we store data for *cmd_argv[]
	char	cmd_args[MAX_COM_TOKEN * 2];	// Here we store original of what we parse, from argv(1) to argv(argc() - 1)
	char	text[MAX_COM_TOKEN];			// This is used/overwrite each time we using Cmd_MakeArgs()
} tokenizecontext_t;

// Parses the given string into command line tokens.
void Cmd_TokenizeString(char *text);

int Cmd_Argc(void);
char *Cmd_Argv(int arg);
char *Cmd_Args(void);

unsigned long Com_HashKey(const char *str);

char *va(char *format, ...);

#endif // __MAINDEF_H__
