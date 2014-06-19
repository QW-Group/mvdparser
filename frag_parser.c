
#include <string.h>
#include <ctype.h>
#include "maindef.h"
#include "qw_protocol.h"
#include "mvd_parser.h"
#include "logger.h"

#define FUH_FRAGFILE_VERSION_1_00	"1.00"			// For compatibility with fuh.
#define FRAGFILE_VERSION_1_00		"ezquake-1.00"	// Fuh suggest such format.

#define MAX_WEAPON_CLASSES			64
#define MAX_FRAG_DEFINITIONS		512
#define MAX_FRAGMSG_LENGTH			256

typedef enum msgtype_s {
	mt_fragged,
	mt_frags,
	mt_tkills,
	mt_tkilled,

	mt_death,
	mt_suicide,
	mt_frag,
	mt_tkill,
	mt_tkilled_unk,
	mt_flagtouch,
	mt_flagdrop,
	mt_flagcap
} msgtype_t;

typedef struct wclass_s 
{
	char	*keyword;
	char	*name;
	char	*shortname;
} wclass_t;

typedef struct fragmsg_s 
{
	char		*msg1;
	char		*msg2;
	msgtype_t	type;
	int			wclass_index;
} fragmsg_t;

typedef struct fragdef_s 
{
	qbool		active;
	qbool		flagalerts;

	int			version;
	char		*gamedir;

	char		*title;
	char		*description;
	char		*author;
	char		*email;
	char		*webpage;

	fragmsg_t	*fragmsgs[MAX_FRAG_DEFINITIONS];
	fragmsg_t	msgdata[MAX_FRAG_DEFINITIONS];
	int			num_fragmsgs;
} fragdef_t;

static fragdef_t fragdefs;
static wclass_t wclasses[MAX_WEAPON_CLASSES];
static int num_wclasses;

static int fragmsg1_indexes[26];

char *stat_string[] = 
{
	"STAT_HEALTH",
	"STAT_FRAGS",
	"STAT_WEAPON",
	"STAT_AMMO",
	"STAT_ARMOR",
	"STAT_WEAPONFRAME",
	"STAT_SHELLS",
	"STAT_NAILS",
	"STAT_ROCKETS",
	"STAT_CELLS",
	"STAT_ACTIVEWEAPON",
	"STAT_TOTALSECRETS",
	"STAT_TOTALMONSTERS",
	"STAT_SECRETS",
	"STAT_MONSTERS",
	"STAT_ITEMS",
	"STAT_VIEWHEIGHT",
	"STAT_TIME",
	"STAT_18",
	"STAT_19",
	"STAT_20",
	"STAT_21",
	"STAT_22",
	"STAT_23",
	"STAT_24",
	"STAT_25",
	"STAT_26",
	"STAT_27",
	"STAT_28",
	"STAT_29",
	"STAT_30",
	"STAT_31",
	"STAT_32"
};

#define ISLOWER(c)	(c >= 'a' && c <= 'z')

int Compare_FragMsg(const void *p1, const void *p2) 
{
	unsigned char a, b;
	char *s, *s1, *s2;

	s1 = (*((fragmsg_t **) p1))->msg1;
	s2 = (*((fragmsg_t **) p2))->msg1;

	for (s = s1; *s && isspace(*s & 127); s++)
		;
	a = tolower(*s & 127);

	for (s = s2; *s && isspace(*s & 127); s++)
		;
	b = tolower(*s & 127);

	if (ISLOWER(a) && ISLOWER(b)) 
	{
		return (a != b) ? a - b : -1 * strcasecmp(s1, s2);
	}
	else 
	{
		return ISLOWER(a) ? 1 : (ISLOWER(b) ? -1 : ((a != b) ? (a - b) : -1 * strcasecmp(s1, s2)));
	}
}

static void Build_FragMsg_Indices(void) 
{
	int i, j = -1, c;
	char *s;

	for (i = 0; i < fragdefs.num_fragmsgs; i++) 
	{
		for (s = fragdefs.fragmsgs[i]->msg1; *s && isspace(*s & 127); s++)
			;
		c = tolower(*s & 127);
		if (!ISLOWER(c))
			continue;

		if (c == 'a' + j)
			continue;

		while (++j < c - 'a')
			fragmsg1_indexes[j] = -1;

		fragmsg1_indexes[j] = i;
	}

	while (++j <= 'z' - 'a')
		fragmsg1_indexes[j] = -1;
}

static void InitFragDefs(void) 
{
	int i;

	Q_free(fragdefs.gamedir);
	Q_free(fragdefs.title);
	Q_free(fragdefs.author);
	Q_free(fragdefs.email);
	Q_free(fragdefs.webpage);

	for (i = 0; i < fragdefs.num_fragmsgs; i++) 
	{
		Q_free(fragdefs.msgdata[i].msg1);
		Q_free(fragdefs.msgdata[i].msg2);
		memset(&fragdefs.msgdata[i], 0, sizeof(fragdefs.msgdata[i]));
	}

	for (i = 0; i < num_wclasses; i++) 
	{
		Q_free(wclasses[i].keyword);
		Q_free(wclasses[i].name);
		Q_free(wclasses[i].shortname);
	}

	memset(&fragdefs, 0, sizeof(fragdefs));
	memset(wclasses, 0, sizeof(wclasses));

	wclasses[0].name = Q_strdup("Unknown");
	num_wclasses = 1;
}

#define FRAGS_CHECKARGS(x)																\
	if (c != x) {																		\
		Sys_PrintError("Fragfile warning (line %d): %d tokens expected\n", line, x);	\
		goto nextline;																	\
	}

#define FRAGS_CHECKARGS2(x, y)																	\
	if (c != x && c != y) {																		\
		Sys_PrintError("Fragfile warning (line %d): %d or %d tokens expected\n", line, x, y);	\
		goto nextline;																			\
	}

#define	FRAGS_CHECK_VERSION_DEFINED()																	\
	if (!gotversion) {																					\
		Sys_PrintError("Fragfile error: #FRAGFILE VERSION must be defined at the head of the file\n");	\
		goto finish;																					\
	}

qbool LoadFragFile(char *filename, qbool quiet)
{
	int c;
	int line;
	int i;
	msgtype_t msgtype;
	char save;
	char *buffer = NULL;
	char *start;
	char *end;
	char *token;
	char fragfilename[MAX_OSPATH];
	qbool gotversion = false;
	qbool warned_flagmsg_overflow = false;

	if (!filename)
	{
		return false;
	}

	InitFragDefs();

	strlcpy(fragfilename, filename, sizeof(fragfilename));
	COM_ForceExtensionEx(fragfilename, ".dat", sizeof(fragfilename));

	if (!COM_ReadFile(fragfilename, &buffer, (long *)NULL) || !buffer)
	{
		if (!quiet)
			Sys_Error("Couldn't load fragfile \"%s\"\n", fragfilename);
		return false;
	}

	line = 1;
	start = buffer;
	end = buffer;

	while (true) 
	{
		//end = strstr(end, "\n");
		for ( ; *end && *end != '\n'; end++)
			;

		save = *end;
		*end = 0;
		Cmd_TokenizeString(start);
		*end = save;

		if (!(c = Cmd_Argc()))
			goto nextline;

		if (!strcasecmp(Cmd_Argv(0), "#FRAGFILE")) 
		{
			FRAGS_CHECKARGS(3);
			if (!strcasecmp(Cmd_Argv(1), "VERSION")) 
			{
				if (gotversion) 
				{
					Sys_PrintError("Fragfile error (line %d): VERSION redefined\n", line);
					goto finish;
				}

				token = Cmd_Argv(2);
				if (!strcasecmp(token, FUH_FRAGFILE_VERSION_1_00))
				{
					token = FRAGFILE_VERSION_1_00; // So we compatible with old fuh format, because our format include all fuh features + our
				}

				if (!strcasecmp(token, FRAGFILE_VERSION_1_00) && (token = strchr(token, '-'))) 
				{
					token++; // find float component of string like this "ezquake-x.yz"
					fragdefs.version = (int) (100 * Q_atof(token));
					gotversion = true;
				}
				else 
				{
					Sys_PrintError("Error: fragfile version (\"%s\") unsupported \n", Cmd_Argv(2));
					goto finish;
				}
			} 
			else if (!strcasecmp(Cmd_Argv(1), "GAMEDIR")) 
			{
				FRAGS_CHECK_VERSION_DEFINED();

				if (!strcasecmp(Cmd_Argv(2), "ANY"))
				{
					fragdefs.gamedir = NULL;
				}
				else
				{
					fragdefs.gamedir = Q_strdup(Cmd_Argv(2));
				}
			} 
			else
			{
				FRAGS_CHECK_VERSION_DEFINED();
				Sys_PrintError("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;
			}
		} 
		else if (!strcasecmp(Cmd_Argv(0), "#META"))
		{
			FRAGS_CHECK_VERSION_DEFINED();
			FRAGS_CHECKARGS(3);

			if (!strcasecmp(Cmd_Argv(1), "TITLE")) 
			{
				fragdefs.title = Q_strdup(Cmd_Argv(2));
			} 
			else if (!strcasecmp(Cmd_Argv(1), "AUTHOR")) 
			{
				fragdefs.author = Q_strdup(Cmd_Argv(2));
			} 
			else if (!strcasecmp(Cmd_Argv(1), "DESCRIPTION")) 
			{
				fragdefs.description = Q_strdup(Cmd_Argv(2));
			} 
			else if (!strcasecmp(Cmd_Argv(1), "EMAIL")) 
			{
				fragdefs.email = Q_strdup(Cmd_Argv(2));
			} 
			else if (!strcasecmp(Cmd_Argv(1), "WEBPAGE")) 
			{
				fragdefs.webpage = Q_strdup(Cmd_Argv(2));
			} 
			else 
			{
				Sys_PrintError("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(1));
				goto nextline;
			}
		}
		else if (!strcasecmp(Cmd_Argv(0), "#DEFINE")) 
		{
			FRAGS_CHECK_VERSION_DEFINED();

			if (!strcasecmp(Cmd_Argv(1), "WEAPON_CLASS") || !strcasecmp(Cmd_Argv(1), "WC")) 
			{
				FRAGS_CHECKARGS2(4, 5);
				token = Cmd_Argv(2);
			
				if (!strcasecmp(token, "NOWEAPON") || !strcasecmp(token, "NONE") || !strcasecmp(token, "NULL")) 
				{
					Sys_PrintError("Fragfile warning (line %d): \"%s\" is a reserved keyword\n", line, token);
					goto nextline;
				}
				
				for (i = 1; i < num_wclasses; i++) 
				{
					if (!strcasecmp(token, wclasses[i].keyword)) 
					{
						Sys_PrintError("Fragfile warning (line %d): WEAPON_CLASS \"%s\" already defined\n", line, wclasses[i].keyword);
						goto nextline;
					}
				}
				
				if (num_wclasses == MAX_WEAPON_CLASSES) 
				{
					Sys_PrintError("Fragfile warning (line %d): only %d WEAPON_CLASS's may be #DEFINE'd\n", line, MAX_WEAPON_CLASSES);
					goto nextline;
				}

				wclasses[num_wclasses].keyword = Q_strdup(token);
				
				// VULT DISPLAY KILLS - Modified oh so slighly so it looks neater on the tracker
				wclasses[num_wclasses].name = Q_strdup(Cmd_Argv(3));
				
				if (c == 5)
					wclasses[num_wclasses].name = Q_strdup(Cmd_Argv(4));
				num_wclasses++;
			} 
			else if (!strcasecmp(Cmd_Argv(1), "OBITUARY") || !strcasecmp(Cmd_Argv(1), "OBIT")) 
			{
				if (!strcasecmp(Cmd_Argv(2), "PLAYER_DEATH")) 
				{
					FRAGS_CHECKARGS(5);
					msgtype = mt_death;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "PLAYER_SUICIDE")) 
				{
					FRAGS_CHECKARGS(5);
					msgtype = mt_suicide;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_FRAGS_UNKNOWN"))
				{
					FRAGS_CHECKARGS(5);
					msgtype = mt_frag;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_UNKNOWN")) 
				{
					FRAGS_CHECKARGS(5);
					msgtype = mt_tkill;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_FRAGS_Y")) 
				{
					FRAGS_CHECKARGS2(5, 6);
					msgtype = mt_frags;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_FRAGGED_BY_Y")) 
				{
					FRAGS_CHECKARGS2(5, 6);
					msgtype = mt_fragged;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLS_Y"))
				{
					FRAGS_CHECKARGS2(5, 6);
					msgtype = mt_tkills;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLED_BY_Y"))
				{
					FRAGS_CHECKARGS2(5, 6);
					msgtype = mt_tkilled;
				}
				else if (!strcasecmp(Cmd_Argv(2), "X_TEAMKILLED_UNKNOWN")) 
				{
					FRAGS_CHECKARGS(5);
					msgtype = mt_tkilled_unk;
				} 
				else 
				{
					Sys_PrintError("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(2));
					goto nextline;
				}

				if (fragdefs.num_fragmsgs == MAX_FRAG_DEFINITIONS) 
				{
					if (!warned_flagmsg_overflow) 
					{
						Sys_PrintError("Fragfile warning (line %d): only %d OBITUARY's and FLAG_ALERT's's may be #DEFINE'd\n", line, MAX_FRAG_DEFINITIONS);
					}

					warned_flagmsg_overflow = true;
					goto nextline;
				}

				if (strlen(Cmd_Argv(4)) >= MAX_FRAGMSG_LENGTH || (c == 6 && strlen(Cmd_Argv(5)) >= MAX_FRAGMSG_LENGTH)) 
				{
					Sys_PrintError("Fragfile warning (line %d): Message token cannot exceed %d characters\n", line, MAX_FRAGMSG_LENGTH - 1);
					goto nextline;
				}

				token = Cmd_Argv(3);

				if (!strcasecmp(token, "NOWEAPON") || !strcasecmp(token, "NONE") || !strcasecmp(token, "NULL")) 
				{
					fragdefs.msgdata[fragdefs.num_fragmsgs].wclass_index = 0;
				}
				else 
				{
					for (i = 1; i < num_wclasses; i++) 
					{
						if (!strcasecmp(token, wclasses[i].keyword)) 
						{
							fragdefs.msgdata[fragdefs.num_fragmsgs].wclass_index = i;
							break;
						}
					}

					if (i == num_wclasses)
					{
						Sys_PrintError("Fragfile warning (line %d): \"%s\" is not a defined WEAPON_CLASS\n", line, token);
						goto nextline;
					}
				}

				fragdefs.msgdata[fragdefs.num_fragmsgs].type = msgtype;
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = Q_strdup (Cmd_Argv(4));
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg2 = (c == 6) ? Q_strdup(Cmd_Argv(5)) : NULL;
				fragdefs.num_fragmsgs++;
			} 
			else if (!strcasecmp(Cmd_Argv(1), "FLAG_ALERT") || !strcasecmp(Cmd_Argv(1), "FLAG_MSG")) 
			{
				FRAGS_CHECKARGS(4);

				if (!strcasecmp(Cmd_Argv(2), "X_TOUCHES_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_GETS_FLAG") ||
					!strcasecmp(Cmd_Argv(2), "X_TAKES_FLAG")) 
				{
					msgtype = mt_flagtouch;
				} 
				else if (!strcasecmp(Cmd_Argv(2), "X_DROPS_FLAG") ||
						 !strcasecmp(Cmd_Argv(2), "X_FUMBLES_FLAG") ||
						 !strcasecmp(Cmd_Argv(2), "X_LOSES_FLAG"))
				{
					msgtype = mt_flagdrop;
				}
				else if (!strcasecmp(Cmd_Argv(2), "X_CAPTURES_FLAG") ||
						 !strcasecmp(Cmd_Argv(2), "X_CAPS_FLAG") ||
						 !strcasecmp(Cmd_Argv(2), "X_SCORES")) 
				{
					msgtype = mt_flagcap;
				} 
				else 
				{
					Sys_PrintError("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(2));
					goto nextline;
				}

				if (fragdefs.num_fragmsgs == MAX_FRAG_DEFINITIONS) 
				{
					if (!warned_flagmsg_overflow)
					{
						Sys_PrintError("Fragfile warning (line %d): only %d OBITUARY's and FLAG_ALERT's's may be #DEFINE'd\n", line, MAX_FRAG_DEFINITIONS);
					}
					warned_flagmsg_overflow = true;
					goto nextline;
				}

				fragdefs.msgdata[fragdefs.num_fragmsgs].type = msgtype;
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg1 = Q_strdup(Cmd_Argv(3));
				fragdefs.msgdata[fragdefs.num_fragmsgs].msg2 = NULL;
				fragdefs.num_fragmsgs++;

				fragdefs.flagalerts = true;
			}
		} 
		else 
		{
			FRAGS_CHECK_VERSION_DEFINED();
			Sys_PrintError("Fragfile warning (line %d): unexpected token \"%s\"\n", line, Cmd_Argv(0));
			goto nextline;
		}

nextline:
		if (!*end)
			break;

		start = end = end + 1;
		line++;
	}

	if (fragdefs.num_fragmsgs)
	{
		for (i = 0; i < fragdefs.num_fragmsgs; i++)
		{
			fragdefs.fragmsgs[i] = &fragdefs.msgdata[i];
		}

		qsort(fragdefs.fragmsgs, fragdefs.num_fragmsgs, sizeof(fragmsg_t *), Compare_FragMsg);
		Build_FragMsg_Indices();

		fragdefs.active = true;
		if (!quiet)
			Sys_PrintDebug(1, "Loaded fragfile \"%s\" (%d frag defs)\n", fragfilename, fragdefs.num_fragmsgs);
		goto finish;
	} 
	else 
	{
		if (!quiet)
			Sys_PrintError("Fragfile \"%s\" was empty\n", fragfilename);
		goto finish;
	}

finish:
	Q_free(buffer);
	return true;
}

void Frags_Parse(mvd_info_t *mvd, char *fragmessage, int level)
{
	int i = 0;

	qbool msg_found = false;
	fragmsg_t *mess = NULL;
	players_t *p1 = NULL;
	players_t *p2 = NULL;
	char *name_pos = NULL;

	if ((level != PRINT_HIGH) && (level != PRINT_MEDIUM))
	{
		return;
	}

	Sys_PrintDebug(6, "Frags_Parse: \"%s\"\n", fragmessage);

	// Find the frag message.
	for (i = 0; i < fragdefs.num_fragmsgs; i++)
	{
		mess = &fragdefs.msgdata[i];

		if (mess->msg2)
		{
			if (strstr(fragmessage, mess->msg1) && strstr(fragmessage, mess->msg2))
			{
				msg_found = true;
				break;
			}
		}
		else
		{
			if (strstr(fragmessage, mess->msg1))
			{
				msg_found = true;
				break;
			}
		}
	}

	if (!msg_found)
	{
		Sys_PrintDebug(5, "Frags_Parse: Frag message type not found for %s\n", fragmessage);
		return;
	}

	// Find the players mentioned in the frag message.
	for (i = 0; i < MAX_PLAYERS; i++)
	{
		if (!PLAYER_ISVALID(&mvd->players[i]))
		{
			 continue;
		}

		name_pos = strstr(fragmessage, mvd->players[i].name);

		if (name_pos)
		{
			// We found the name at the start of the string
			// so it's the first player. Otherwise it's 
			// the second player.
			if (name_pos == fragmessage)
			{
				p1 = &mvd->players[i];
			}
			else if (name_pos > fragmessage)
			{
				p2 = &mvd->players[i];
			}
		}
	}

	if (p1)
	{
		Sys_PrintDebug(3, "Frags_Parse: Player 1: %s\n", p1->name);
	}

	if (p2)
	{
		Sys_PrintDebug(3, "Frags_Parse: Player 2: %s\n", p2->name);
	}

	if (!p1 && p2)
	{
		p1 = p2;
	}

	if (p1 && !p2)
	{
		p2 = p1;
	}

	if (!p1 && !p2)
	{
		Sys_PrintError("Frags_Parse: Error found no player for fragmessage \"%s\"\n", fragmessage);
		return;
	}

	switch (mess->type)
	{
		case mt_death :
		{
			Sys_PrintDebug(3, "Frags_Parse: %s was killed by %s\n", p1->name, wclasses[mess->wclass_index].name);
			mvd->fragstats[p1->pnum].wdeaths[mess->wclass_index]++;

			break;
		}
		case mt_suicide :
		{
			mvd->fragstats[p1->pnum].suicides++;
			p1->death_count++;
			Log_Event(&logger, mvd, LOG_DEATH, p1->pnum);			
			break;
		}
		case mt_fragged :
		case mt_frags :
		{
			players_t *killer = (mess->type == mt_fragged) ? p2 : p1;
			players_t *victim = (mess->type == mt_fragged) ? p1 : p2;

			Sys_PrintDebug(3, "Frags_Parse: %s killed %s with %s\n", killer->name, victim->name, wclasses[mess->wclass_index].name);

			//Log_Event(&logger, mvd, 
			break;
		}
		case mt_frag :
		{
			mvd->fragstats[p1->pnum].wkills[mess->wclass_index]++;
			break;
		}
		case mt_tkilled :
        case mt_tkills :
		{
			players_t *killer = (mess->type == mt_tkilled) ? p2 : p1;
			players_t *victim = (mess->type == mt_tkilled) ? p1 : p2;

			mvd->fragstats[killer->pnum].tkills[victim->pnum]++;
			mvd->fragstats[victim->pnum].tdeaths[killer->pnum]++;
			mvd->fragstats[killer->pnum].total_frags--;

			if (mess->type == mt_tkilled)
			{
				Sys_PrintDebug(3, "Frags_Parse: %s teamkilled %s.\n", killer->name, victim->name);
			}
			else
			{
				Sys_PrintDebug(3, "Frags_Parse: %s got teamkilled.\n", killer->name, victim->name);
			}

			break;
		}
		case mt_tkill:
		{
			mvd->fragstats[p1->pnum].teamkills++;
			p1->teamkill_flag = true;
			break;
		}
		case mt_flagtouch:
		{
			mvd->fragstats[p1->pnum].flag_touches++;
			Sys_PrintDebug(3, "Frags_Parse: %s touched flag\n", p1->name);
			break;
		}
		case mt_flagdrop:
		{		
			mvd->fragstats[p1->pnum].flag_dropped++;
			Sys_PrintDebug(3, "Frags_Parse: %s dropped flag\n", p1->name);
			break;
		}
		case mt_flagcap:
		{
			mvd->fragstats[p1->pnum].flag_captured++;
			Sys_PrintDebug(3, "Frags_Parse: %s captured flag\n", p1->name);
			break;
		}
	}
}



