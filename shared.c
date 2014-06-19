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
void *Q_malloc(size_t size)
{
	void *p = malloc(size);

	if (!p)
		Sys_Error ("Q_malloc: Not enough memory free; check disk space\n");

	return p;
}

void *Q_calloc(size_t n, size_t size)
{
	void *p = calloc(n, size);

	if (!p)
		Sys_Error ("Q_calloc: Not enough memory free; check disk space\n");

	return p;
}

void *Q_realloc(void *p, size_t newsize)
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

		// Get rid of undefined unicode characters.
		if (((*s >= 0) && (*s <= 31)) || ((*s >= 127) && (*s <= 159)))
		{
			*s = '_';
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
#endif // LINUX & WIN32

size_t strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	// Find the end of dst and adjust bytes left but don't go past end.
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));

	while (*s != '\0')
	{
		if (n != 1) 
		{
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));       /* count does not include NUL */
}

#ifdef _WIN32
int snprintf(char *buffer, size_t count, char const *format, ...)
{
	int ret;
	va_list argptr;
	if (!count) return 0;
	va_start(argptr, format);
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	va_end(argptr);
	return ret;
}

/*
int vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	int ret;
	if (!count) return 0;
	ret = _vsnprintf(buffer, count, format, argptr);
	buffer[count - 1] = 0;
	return ret;
}
*/
#endif // _WIN32

// A Case-insensitive strstr.
char *strstri(const char *text, const char *find)
{
	char *s = (char *)text;
	size_t findlen = strlen(find);

	// Empty substring, return input (like strstr).
	if (findlen == 0)
	{
		return s;
	}

	while (*s)
	{
		// Check if we can find the substring.
		if (!strncasecmp(s, find, findlen))
		{
			return s;
		}

		s++;
	}

	return NULL;
}

// Append an extension to a path.
void COM_ForceExtensionEx(char *path, char *extension, size_t path_size)
{
	char *src;

	if (path_size < 1 || path_size <= strlen (extension))	// To small buffer, can't even theoreticaly append extension.
		Sys_Error("COM_ForceExtensionEx: internall error"); // This is looks like a bug, be fatal then.

	src = path + strlen (path) - strlen (extension);
	if (src >= path && !strcmp (src, extension))
		return; // Seems we alredy have this extension

	strlcat(path, extension, path_size);

	src = path + strlen (path) - strlen (extension);
	if (src >= path && !strcmp (src, extension))
		return; // Seems we succeeded.

	Sys_PrintError("Failed to force extension %s for %s\n", extension, path);
}

qbool COM_ReadFile(const char *filename, byte **data, long *filelen)
{
	long len = 0;
	FILE *f = NULL;

	Sys_PrintDebug(1, "Opening %s\n", filename);

	f = fopen(filename, "rb");

	if (filelen)
	{
		(*filelen) = -1;
	}

	if (f && data)
	{
		size_t num_read = 0;

		// Get the file size.
		fseek(f, 0, SEEK_END);
		len = ftell(f);
		rewind(f);

		if (len < 0)
		{
			Sys_PrintError("File is empty: %s\n", filename);
			fclose(f);
			return false;
		}

		(*data) = Q_malloc(sizeof(byte) * len);

		num_read = fread((*data), 1, len, f);

		fclose(f);

		if (!feof(f) && (num_read != (size_t)len))
		{
			// We failed to read the entire file.
			return false;
		}
	}
	else
	{
		Sys_PrintError("Failed to open file: %s\n", filename);
		return false;
	}

	if (filelen)
	{
		(*filelen) = len;
	}

	return true;
}

char		com_token[MAX_COM_TOKEN];
static int	com_argc;
static char	**com_argv;

char *COM_Parse(char *data)
{
	unsigned char c;
	int len;
	int quotes;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

	// Skip whitespace.
	while (true) 
	{
		while (((c = *data) == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
		{
			data++;
		}

		if (c == 0)
			return NULL; // End of file;

		// Skip // comments
		if ((c == '/') && (data[1] == '/'))
		{
			while (*data && *data != '\n')
			{
				data++;
			}
		}
		else
		{
			break;
		}
	}

	// handle quoted strings specially
	if ((c == '\"') || (c == '{')) 
	{
		if (c == '{')
		{
			quotes = 1;
		}
		else
		{
			quotes = -1;
		}

		data++;

		while (true) 
		{
			c = *data;
			data++;
			if (quotes < 0)
			{
				if (c == '\"')
				{
					quotes++;
				}
			}
			else 
			{
				if (c == '}')
				{
					quotes--;
				}
				else if (c == '{')
				{
					quotes++;
				}
			}

			if (!quotes || !c) 
			{
				com_token[len] = 0;
				return c ? data:data-1;
			}

			com_token[len] = c;
			len++;
		}
	}

	// Parse a regular word.
	do 
	{
		com_token[len] = c;
		data++;
		len++;
		
		if (len >= (MAX_COM_TOKEN - 1))
		{
			break;
		}

		c = *data;
	} while (c && (c != ' ') && (c != '\t') && (c != '\n') && (c != '\r'));

	com_token[len] = 0;
	return data;
}

// Parses the given string into command line tokens.
void Cmd_TokenizeStringEx(tokenizecontext_t *ctx, char *text)
{
	int idx = 0, token_len;

	memset(ctx, 0, sizeof(*ctx));

	while (true)
	{
		// Skip whitespace
		while (*text == ' ' || *text == '\t' || *text == '\r')
			text++;

		// a newline separates commands in the buffer
		if (*text == '\n')
			return;

		if (!*text)
			return;

		if (ctx->cmd_argc == 1)
			strlcpy(ctx->cmd_args, text, sizeof(ctx->cmd_args));

		text = COM_Parse(text);

		if (!text)
			return;

		if (ctx->cmd_argc >= MAX_ARGS)
			return;

		token_len = (int)strlen(com_token);

		// ouch ouch, no more space
		if (idx + token_len + 1 > sizeof(ctx->argv_buf))
			return;

		ctx->cmd_argv[ctx->cmd_argc] = ctx->argv_buf + idx;
		strcpy(ctx->cmd_argv[ctx->cmd_argc], com_token);
		ctx->cmd_argc++;

		idx += token_len + 1;
	}
}

static  tokenizecontext_t cmd_tokenizecontext;
static	char	*cmd_null_string = "";

// Parses the given string into command line tokens.
void Cmd_TokenizeString(char *text)
{
	Cmd_TokenizeStringEx(&cmd_tokenizecontext, text);
}

int Cmd_ArgcEx(tokenizecontext_t *ctx)
{
	return ctx->cmd_argc;
}

char *Cmd_ArgvEx(tokenizecontext_t *ctx, int arg)
{
	if (arg >= ctx->cmd_argc || arg < 0)
		return cmd_null_string;

	return ctx->cmd_argv[arg];
}

// Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_ArgsEx(tokenizecontext_t *ctx)
{
	return ctx->cmd_args;
}

int Cmd_Argc(void)
{
	return Cmd_ArgcEx(&cmd_tokenizecontext);
}

char *Cmd_Argv(int arg)
{
	return Cmd_ArgvEx(&cmd_tokenizecontext, arg);
}

// Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_Args(void)
{
	return Cmd_ArgsEx(&cmd_tokenizecontext);
}

float Q_atof(const char *str)
{
	float val;
	int sign, c, decimal, total;

	if (!str) return 0;

	for (; *str && *str <= ' '; str++);

	if (*str == '-') 
	{
		sign = -1;
		str++;
	}
	else 
	{
		if (*str == '+')
			str++;

		sign = 1;
	}

	val = 0;

	// Check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) 
	{
		str += 2;
		while (true) 
		{
			c = *str++;
			if (c >= '0' && c <= '9')
			{
				val = (val * 16) + c - '0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				val = (val * 16) + c - 'a' + 10;
			}
			else if (c >= 'A' && c <= 'F')
			{
				val = (val * 16) + c - 'A' + 10;
			}
			else
			{
				return val * sign;
			}
		}
	}

	// check for character
	if (str[0] == '\'')
		return (float)(sign * str[1]);

	// assume decimal
	decimal = -1;
	total = 0;
	
	while (true) 
	{
		c = *str++;
		if (c == '.') 
		{
			decimal = total;
			continue;
		}

		if (c <'0' || c > '9')
			break;
		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;
	while (total > decimal) 
	{
		val /= 10;
		total--;
	}

	return val * sign;
}

unsigned long Com_HashKey(const char *str)
{
	unsigned long hash = 0;
	int c;

	// the (c&~32) makes it case-insensitive
	// hash function known as sdbm, used in gawk
	while ((c = *str++))
        hash = (c &~ 32) + (hash << 6) + (hash << 16) - hash;

    return hash;
}

char *va(char *format, ...)
{
	va_list argptr;
	static char string[32][2048];
	static int idx = 0;

	idx++;
	if (idx == 32)
		idx = 0;

	va_start(argptr, format);
	vsnprintf(string[idx], sizeof(string[idx]), format, argptr);
	va_end(argptr);

	return string[idx];
}

