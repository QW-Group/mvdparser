#include <string.h>
#include <time.h>
#include "endian.h"
#include "maindef.h"
#include "mvd_parser.h"
#include "net_msg.h"
#include "netmsg_parser.h"
#include "frag_parser.h"
#include "logger.h"

#ifdef WIN32
#include "strptime.h"
#endif // WIN32

// ========================================================================================
// HELPER FUNCTITONS
// ========================================================================================

// Searches the string for the given key and returns the associated value, or an empty string.
static char *Info_ValueForKey(char *s, char *key)
{
	char pkey[512];
	static char value[4][512];	// Use two buffers so compares work without stomping on each other.
	static int	valueindex = 0;
	char *o = NULL;

	valueindex = (valueindex + 1) % 4;
	
	if (*s == '\\')
		s++;

	while (1) 
	{
		o = pkey;
		
		while (*s != '\\') 
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey(char *s, char *key) 
{
	char *start, pkey[512], value[512], *o;

	if (strstr(key, "\\")) 
	{
		Sys_PrintError("Info_RemoveKey: Can't use a key with a \\\n");
		return;
	}

	while (1) 
	{
		start = s;
	
		if (*s == '\\')
			s++;
		o = pkey;
		
		while (*s != '\\') 
		{
			if (!*s)
				return;
			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s) 
		{
			if (!*s)
				return;
			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp(key, pkey))
		{
			strcpy(start, s);	// Remove this part.
			return;
		}

		if (!*s)
			return;
	}
}

void Info_RemovePrefixedKeys (char *start, char prefix) 
{
	char *s, pkey[512], value[512], *o;

	s = start;

	while (1) 
	{
		if (*s == '\\')
			s++;

		o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		
		*o = 0;
		s++;

		o = value;
		
		while (*s != '\\' && *s) 
		{
			if (!*s)
				return;
			*o++ = *s++;
		}

		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey(start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey(char *s, char *key, char *value, int maxsize) 
{
	char new[1024], *v;

	if (strstr(key, "\\") || strstr(value, "\\")) 
	{
		Sys_PrintError("Info_SetValueForStarKey: Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) 
	{
		Sys_PrintError("Info_SetValueForStarKey: Can't use keys or values with a \"\n");
		return;
	}

	if ((strlen(key) >= MAX_INFO_KEY) || (strlen(value) >= MAX_INFO_KEY))
	{
		Sys_PrintError("Info_SetValueForStarKey: Keys and values must be < %i characters.\n", MAX_INFO_KEY);
		return;
	}

	// This next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key)))
	{
		// Key exists, make sure we have enough room for new value, if we don't, don't change it!
		if ((int)(strlen(value) - strlen(v) + strlen(s)) >= maxsize) 
		{
			Sys_PrintError("Info_SetValueForStarKey: Info string length exceeded\n");
			return;
		}
	}

	Info_RemoveKey(s, key);
	
	if (!value || !strlen(value))
		return;

	snprintf(new, sizeof (new), "\\%s\\%s", key, value);

	if ((int)(strlen(new) + strlen(s)) >= maxsize) 
	{
		Sys_PrintError("Info_SetValueForStarKey: Info string length exceeded\n");
		return;
	}

	/*
	// only copy ascii values
	s += strlen(s);
	v = new;
#ifndef CLIENTONLY
	if (!sv_highchars.value) 
	{
		while (*v) 
		{
			c = (unsigned char)*v++;

			if (c == ('\\'|128))
				continue;

			c &= 127;
	
			if (c >= 32)
				*s++ = c;
		}
	} 
	else
#endif
	{
		while (*v) 
		{
			c = (unsigned char)*v++;
			if (c > 13)
				*s++ = c;
		}
		*s = 0;
	}*/
}

void Info_SetValueForKey(char *s, char *key, char *value, int maxsize) 
{
	if (key[0] == '*') 
	{
		Sys_PrintError("Info_SetValueForKey: Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

__inline qbool check_stat(int item, int player_stat, int value)
{
	return ((value & item) && !(player_stat & item));
}

static void Stat_CalculateShotsFired(players_t *cp, int stat, int value)
{
	switch (stat)
	{
		default :
		{
			break;
		}
		case STAT_SHELLS :
		{
			if (cp->stats[STAT_ACTIVEWEAPON] == IT_SHOTGUN)
			{
				cp->weapon_shots[1] += cp->stats[STAT_SHELLS] - value;
			}

			if (cp->stats[STAT_ACTIVEWEAPON] == IT_SUPER_SHOTGUN)
			{
				cp->weapon_shots[2] += cp->stats[STAT_SHELLS] - value;
			}
			
			break;
		}
		case STAT_NAILS :
		{
			if (cp->stats[STAT_ACTIVEWEAPON] == IT_NAILGUN)
			{
				cp->weapon_shots[3] += cp->stats[STAT_NAILS] - value;
			}

			if (cp->stats[STAT_ACTIVEWEAPON] == IT_SUPER_NAILGUN)
			{
				cp->weapon_shots[4] += cp->stats[STAT_NAILS] - value;
			}

			break;
		}
		case STAT_ROCKETS :
		{
			if (cp->stats[STAT_ACTIVEWEAPON] == IT_GRENADE_LAUNCHER)
			{
				cp->weapon_shots[5] += cp->stats[STAT_ROCKETS] - value;
			}

			if (cp->stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
			{
				cp->weapon_shots[6] += cp->stats[STAT_ROCKETS] - value;
			}
		}
		case STAT_CELLS :
		{
			if (cp->stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
			{
				cp->weapon_shots[7] += cp->stats[STAT_CELLS] - value;
			}
		}
	}
}

static void Stat_CheckItemPickup(players_t *cp, int stat, int value)
{
	// Check which bits of the item stat bitmask have changed.

	if (check_stat(IT_ARMOR1, cp->stats[stat], value))
	{
		cp->armor_count[0]++;
	}

	if (check_stat(IT_ARMOR2, cp->stats[stat], value))
	{
		cp->armor_count[1]++;
	}

	if (check_stat(IT_ARMOR3, cp->stats[stat], value))
	{
		cp->armor_count[2]++;
	}

	if (check_stat(IT_INVISIBILITY, cp->stats[stat], value))
	{
		cp->ring_count++;
	}
	
	if (check_stat(IT_QUAD, cp->stats[stat], value))
	{
		cp->quad_count++;
	}

	if (check_stat(IT_INVULNERABILITY, cp->stats[stat], value))
	{
		cp->pent_count++;
	}

	if (check_stat(IT_SUPER_SHOTGUN, cp->stats[stat], value))
	{
		cp->weapon_count[0]++;
	}

	if (check_stat(IT_NAILGUN, cp->stats[stat], value))
	{
		cp->weapon_count[1]++;
	}
	
	if (check_stat(IT_SUPER_NAILGUN, cp->stats[stat], value))
	{
		cp->weapon_count[2]++;
	}

	if (check_stat(IT_GRENADE_LAUNCHER, cp->stats[stat], value))
	{
		cp->weapon_count[3]++;
	}

	if (check_stat(IT_ROCKET_LAUNCHER, cp->stats[stat], value))
	{
		cp->weapon_count[4]++;
	}
	
	if (check_stat(IT_LIGHTNING, cp->stats[stat], value))
	{
		cp->weapon_count[5]++;
	}

	if (check_stat(IT_SUPERHEALTH, cp->stats[stat], value))
	{
		cp->megahealth_count++;
	}
}

static void Stat_Death(mvd_info_t *mvd, players_t *cp)
{
	if (cp->stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING)
	{
		cp->weapon_drops[5]++;
		cp->last_dropped_weapon = LG_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
	}
	else if (cp->stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER)
	{
		cp->weapon_drops[4]++;
		cp->last_dropped_weapon = RL_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
    }
	else if (cp->stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER)
    {
		cp->weapon_drops[3]++;
		cp->last_dropped_weapon = GL_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
    }
	else if (cp->stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN)
	{
		cp->weapon_drops[2]++;
		cp->last_dropped_weapon = SNG_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
    }
	else if (cp->stats[STAT_ACTIVEWEAPON] & IT_NAILGUN)
	{
		cp->weapon_drops[1]++;
		cp->last_dropped_weapon = NG_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
	}
	else if (cp->stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN)
	{
		cp->weapon_drops[0]++;
		cp->last_dropped_weapon = SSG_NUM;
		Log_Event(&logger, mvd, LOG_WEAPONDROP, cp->pnum);
	}
}

static void Stat_HealthLoss(players_t *cp, int stat, int value)
{
	// TODO : Hmmmmm, what about when picking up health boxes???

	if (cp->stats[STAT_ITEMS] & IT_ARMOR1)
	{
		cp->damage_health[0] += cp->stats[stat] - value;
	}
	else if (cp->stats[STAT_ITEMS] & IT_ARMOR2)
	{
		cp->damage_health[1] += cp->stats[stat] - value;
	}
	else if (cp->stats[STAT_ITEMS] & IT_ARMOR3)
	{
		cp->damage_health[2] += cp->stats[stat] - value;
	}
	else if (!(cp->stats[STAT_ITEMS] & IT_ARMOR3) && !(cp->stats[STAT_ITEMS] & IT_ARMOR2) && !(cp->stats[STAT_ITEMS] & IT_ARMOR1))
	{
		cp->damage_health[3] += cp->stats[stat] - value;
	}
}

static void Stat_ArmorDamage(players_t *cp, int stat, int value)
{
	if (cp->stats[STAT_ITEMS] & IT_ARMOR1)
	{
		cp->damage_armor[0] += cp->stats[stat] - value;
	}
	else if (cp->stats[STAT_ITEMS] & IT_ARMOR2)
	{
		cp->damage_armor[1] += cp->stats[stat] - value;
	}
	else if (cp->stats[STAT_ITEMS] & IT_ARMOR3)
	{
		cp->damage_armor[2] += cp->stats[stat] - value;
	}
}

static void SetStat(mvd_info_t *mvd, int stat, int value)
{
	// In this case the net message doesn't contain what player the message
	// is directed to, so we have to use the MVDs lastto for this.
	players_t *cp = &mvd->players[mvd->lastto];

	// Only gather stats during a match.
	if (!mvd->serverinfo.match_started)
    {
		cp->stats[stat] = value;
		return;
	}

	// We got less of this stat than before, if it's shells, 
	// the current player just shot a weapon.
	if (cp->stats[stat] > value)
    {
		Stat_CalculateShotsFired(cp, stat, value);
	}

	// Something was picked up.
	if (stat == STAT_ITEMS)
	{
		Stat_CheckItemPickup(cp, stat, value);
	}

	if (stat == STAT_HEALTH)
	{
		// The player just died, check what was dropped, and log a death event.
		if (value <= 0 )
		{
			Stat_Death(mvd, cp);
		}

		if (cp->stats[stat] > value)
		{
			Stat_HealthLoss(cp, stat, value);
		}
    }

	if (stat == STAT_ARMOR)
	{
		if (cp->stats[stat] > value)
		{
			Stat_ArmorDamage(cp, stat, value);
		}
	}

	// Save the new value.
	cp->stats[stat] = value;
}

static void NetMsg_Parser_ParsePacketEntities(mvd_info_t *mvd, qbool delta)
{
	byte from;
	int bits;

	if (delta)
	{
		from = MSG_ReadByte();

		if ((outgoing_sequence - incoming_sequence - 1) >= UPDATE_MASK)
		{
			return;
		}
	}

	while (true)
	{
		bits = MSG_ReadShort();

		if (msg_badread)
		{
			// Something didn't parse right...
			Sys_PrintError("NetMsg_Parser_ParsePacketEntities: msg_badread in packetentities.\n");
			return;
		}

		if (!bits)
		{
			break;
		}

		bits &= ~0x1FF; // Strip the first 9 bits.

		// Read any more bits.
		if (bits & U_MOREBITS)
		{
			bits |= MSG_ReadByte();
		}

		if (bits & U_MODEL)
			MSG_ReadByte();
		if (bits & U_FRAME)
			MSG_ReadByte();
		if (bits & U_COLORMAP)
			MSG_ReadByte();
		if (bits & U_SKIN)
			MSG_ReadByte();
		if (bits & U_EFFECTS)
			MSG_ReadByte();
		if (bits & U_ORIGIN1)
			MSG_ReadCoord();
		if (bits & U_ORIGIN2)
			MSG_ReadCoord();
		if (bits & U_ORIGIN3)
			MSG_ReadCoord();
		if (bits & U_ANGLE1)
			MSG_ReadAngle();
		if (bits & U_ANGLE2)
			MSG_ReadAngle();
		if (bits & U_ANGLE3)
			MSG_ReadAngle();
	}
}

// ========================================================================================
// NET MESSAGE FUNCTITONS
// ========================================================================================

static void NetMsg_Parser_Parse_svc_updatestat(mvd_info_t *mvd)
{
	int stat = MSG_ReadByte();
	int value = MSG_ReadByte();
	
	SetStat(mvd, stat, value);
}

static void NetMsg_Parser_Parse_svc_setview(void)
{
}

static void NetMsg_Parser_Parse_svc_sound(mvd_info_t *mvd)
{
	int i;
	int soundnum;
	vec3_t loc;
	frame_info_t *fi = &mvd->frame_info;
	int channel = MSG_ReadShort();

	if (channel & SND_VOLUME)
	{
		MSG_ReadByte();
	}

	if (channel & SND_ATTENUATION)
	{
		MSG_ReadByte();
	}

	soundnum = MSG_ReadByte();
	
	// Sound location.
	for (i = 0; i < 3; i++)
	{
		loc[i]= MSG_ReadCoord();
	}

	// Only parse sounds when match has started.
	if (mvd->serverinfo.match_started && mvd->sndlist[soundnum])
	{
		// TODO : Hrm, why would a sound that's played not be in the soundlist?!?

		if (!strcmp("items/r_item1.wav", mvd->sndlist[soundnum]))
		{
			fi->healthinfo[fi->healthcount].type = 1;
			VectorCopy(loc, fi->healthinfo[fi->healthcount].origin);
			fi->healthcount++;
		}
		else if (!strcmp("items/health1.wav", mvd->sndlist[soundnum]))
		{
			fi->healthinfo[fi->healthcount].type = 2;
			VectorCopy(loc, fi->healthinfo[fi->healthcount].origin);
			fi->healthcount++;
		}
		else if (!strcmp("items/r_item2.wav", mvd->sndlist[soundnum]))
		{
			fi->healthinfo[fi->healthcount].type = 3;
			VectorCopy(loc, fi->healthinfo[fi->healthcount].origin);
			fi->healthcount++;
		}
		else if (!strcmp("player/plyrjmp8.wav", mvd->sndlist[soundnum]))
		{
			VectorCopy(loc, fi->jumpinfo[fi->jumpcount]);
			fi->jumpcount++;
		}
	}

	Sys_PrintDebug(5, "svc_sound: %s (x: %g y: %g z: %g)\n", mvd->sndlist[soundnum], loc[0], loc[1], loc[2]);
}

static void NetMsg_Parser_Parse_svc_print(mvd_info_t *mvd)
{
	int level = MSG_ReadByte();
	char str[MAX_INFO_STRING];

	strlcpy(str, MSG_ReadString(), sizeof(str)); 

	// TODO : Check for frag messages spread over several svc_prints older mods/servers does this crap :(

	Sys_PrintDebug(5, "svc_print: (%s) RAW: %s\n", print_strings[level], str);
	Sys_PrintDebug(1, "svc_print: (%s) %s\n", print_strings[level], Sys_RedToWhite(str));
	
	// Parse frags.
	Frags_Parse(mvd, str, level);

	if (level == PRINT_HIGH)
	{
		if (!strncmp(str, "matchdate:", 10))
		{
			// KTX
			struct tm t;

			// matchdate: Fri Nov 23, 16:33:46 2007
			// matchdate: 2007-11-23 17:12:44 CET
			// TODO: Parse timezone.
			if (strptime(str, "matchdate: %a %b %d, %X %Y", &t) || strptime(str, "matchdate: %Y-%m-%d %X ", &t))
			{
				mvd->match_start_date_full	= t;
				mvd->match_start_year		= t.tm_year + 1900;
				mvd->match_start_month		= t.tm_mon + 1;
				mvd->match_start_date		= t.tm_mday;
				mvd->match_start_hour		= t.tm_hour;
				mvd->match_start_minute		= t.tm_min;
			}
		}
		else if (!strncmp(str, "matchkey:", 9))
		{
			// KTPro
			struct tm t;
			char *s = str;

			// matchkey: 177-2006-3-19:23-27-20
			while (*s && (*s != '-'))
			{
				s++;
			}

			if (strptime(s, "-%Y-%m-%d:%H-%M", &t))
			{
				mvd->match_start_date_full	= t;
				mvd->match_start_year		= t.tm_year + 1900;
				mvd->match_start_month		= t.tm_mon + 1;
				mvd->match_start_date		= t.tm_mday;
				mvd->match_start_hour		= t.tm_hour;
				mvd->match_start_minute		= t.tm_min;
			}
		}
		else if (!strncmp(str, "The match is over", 17))
		{
			int i;

			mvd->serverinfo.match_ended = true;
			Log_Event(&logger, mvd, LOG_MATCHEND, -1);

			// HACK :(
			for (i = 0; i < MAX_PLAYERS; i++)
			{
				if (!PLAYER_ISVALID(&mvd->players[i]))
				{
					continue;
				}

				Log_Event(&logger, mvd, LOG_MATCHEND_ALL, i);
			}
		}
		else if (strstr(str, "overtime follows"))
		{
			char *s = str;
			while (*s && (*s < '0') && (*s > '9'))
			{
				s++;
			}

			mvd->serverinfo.overtime_minutes = atoi(str + 1);
		}
		else if (!strncmp(str, "time over, the game is a draw", 29))
		{
			mvd->serverinfo.match_overtime = true;
		}
		else if (strstr(str, "left the game with"))
		{
			// A player left the game in the middle of a match, we need to keep track
			// of the ghost player that is created straight after this event.
			mvd->serverinfo.player_timed_out = true;
			mvd->serverinfo.player_timout_frame	= mvd->frame_count;
			Sys_PrintDebug(1, "Warning: Player timed out!");
		}
	}
}

static void NetMsg_Parser_ParseServerInfo(mvd_info_t *mvd)
{
	char *tmp = NULL;
	char *tmp2 = NULL;

	mvd->serverinfo.deathmatch		= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "deathmatch"));
	mvd->serverinfo.fpd				= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "fpd"));
	mvd->serverinfo.fraglimit		= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "fraglimit"));
	mvd->serverinfo.timelimit		= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "timelimit"));
	mvd->serverinfo.teamplay		= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "teamplay"));
	mvd->serverinfo.maxclients		= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "maxclients"));
	mvd->serverinfo.maxspectators	= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "maxspectators"));
	mvd->serverinfo.maxfps			= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "maxfps"));
	mvd->serverinfo.zext			= atoi(Info_ValueForKey(mvd->serverinfo.serverinfo, "*z_ext"));

	tmp = Info_ValueForKey(mvd->serverinfo.serverinfo, "hostname");
	if (strcmp(tmp, ""))
	{
		strlcpy(mvd->serverinfo.hostname, tmp, sizeof(mvd->serverinfo.hostname));
	}

	// Look for mod.
	if (strcmp(tmp = Info_ValueForKey(mvd->serverinfo.serverinfo, "kmod"), ""))
	{
		snprintf(mvd->serverinfo.mod, sizeof(mvd->serverinfo.mod), "KTPro %s build %s", tmp, Info_ValueForKey(mvd->serverinfo.serverinfo, "kbuild"));
	}
	else if (strcmp(tmp = Info_ValueForKey(mvd->serverinfo.serverinfo, "xmod"), ""))
	{
		snprintf(mvd->serverinfo.mod, sizeof(mvd->serverinfo.mod), "KTX %s build %s", tmp, Info_ValueForKey(mvd->serverinfo.serverinfo, "xbuild"));
	}

	strlcpy(mvd->serverinfo.mapname, Info_ValueForKey(mvd->serverinfo.serverinfo, "map"), sizeof(mvd->serverinfo.mapname));
	strlcpy(mvd->serverinfo.serverversion, Info_ValueForKey(mvd->serverinfo.serverinfo, "*version"), sizeof(mvd->serverinfo.serverversion));
	strlcpy(mvd->serverinfo.status, Info_ValueForKey(mvd->serverinfo.serverinfo, "status"), sizeof(mvd->serverinfo.status));
}

static void NetMsg_Parser_Parse_svc_stufftext(mvd_info_t *mvd)
{
	char *stufftext = MSG_ReadString();

	if (strstr(stufftext, "fullserverinfo"))
	{
		// Get rid of "fullserverinfo".
		while (*stufftext && (*stufftext != '\\'))
		{
			stufftext++;
		}

		strlcpy(mvd->serverinfo.serverinfo, stufftext, sizeof(mvd->serverinfo.serverinfo));
		NetMsg_Parser_ParseServerInfo(mvd);
	}
}

static void NetMsg_Parser_Parse_svc_setangle(void)
{
	int i;
	MSG_ReadByte(); // Player number.

	// View angles.
	for (i = 0; i < 3; i++)
	{
		MSG_ReadAngle();
	}
}

static void NetMsg_Parser_Parse_svc_serverdata(mvd_info_t *mvd)
{
	mvd->serverinfo.protocol_version	= MSG_ReadLong();
	mvd->serverinfo.servercount			= MSG_ReadLong();

	// Gamedir.
	strlcpy(mvd->serverinfo.gamedir, MSG_ReadString(), sizeof(mvd->serverinfo.gamedir));
	
	mvd->serverinfo.demotime = MSG_ReadFloat();

	// Map name.
	strlcpy(mvd->serverinfo.mapname, MSG_ReadString(), sizeof(mvd->serverinfo.mapname));

	// Movement vars.
	mvd->serverinfo.movevars.gravity			= MSG_ReadFloat();
	mvd->serverinfo.movevars.stopspeed          = MSG_ReadFloat();
	mvd->serverinfo.movevars.maxspeed			= MSG_ReadFloat();
	mvd->serverinfo.movevars.spectatormaxspeed  = MSG_ReadFloat();
	mvd->serverinfo.movevars.accelerate         = MSG_ReadFloat();
	mvd->serverinfo.movevars.airaccelerate      = MSG_ReadFloat();
	mvd->serverinfo.movevars.wateraccelerate    = MSG_ReadFloat();
	mvd->serverinfo.movevars.friction           = MSG_ReadFloat();
	mvd->serverinfo.movevars.waterfriction      = MSG_ReadFloat();
	mvd->serverinfo.movevars.entgravity	        = MSG_ReadFloat();

	Sys_PrintDebug(1, "sys_serverdata:\n");
	Sys_PrintDebug(1, "Protocol version: %i\n", mvd->serverinfo.protocol_version);
	Sys_PrintDebug(1, "Servercount: %i\n", mvd->serverinfo.servercount);
	Sys_PrintDebug(1, "Gamedir: %s\n", mvd->serverinfo.gamedir);
	Sys_PrintDebug(1, "Demotime: %g\n", mvd->serverinfo.demotime);
	Sys_PrintDebug(1, "Levelname: %s\n", mvd->serverinfo.mapname);
	Sys_PrintDebug(1, "Gravity: %g\n", mvd->serverinfo.movevars.gravity);
	Sys_PrintDebug(1, "Stopspeed: %g\n", mvd->serverinfo.movevars.stopspeed);
	Sys_PrintDebug(1, "Maxspeed: %g\n", mvd->serverinfo.movevars.maxspeed);
	Sys_PrintDebug(1, "Spectator max speed: %g\n", mvd->serverinfo.movevars.spectatormaxspeed);
	Sys_PrintDebug(1, "Accelerate: %g\n", mvd->serverinfo.movevars.accelerate);
	Sys_PrintDebug(1, "Air accelerate: %g\n", mvd->serverinfo.movevars.airaccelerate);
	Sys_PrintDebug(1, "Water accelerate: %g\n", mvd->serverinfo.movevars.wateraccelerate);
	Sys_PrintDebug(1, "Friction: %g\n", mvd->serverinfo.movevars.friction);
	Sys_PrintDebug(1, "Water friction: %g\n", mvd->serverinfo.movevars.waterfriction);
	Sys_PrintDebug(1, "Entity gravity: %g\n", mvd->serverinfo.movevars.entgravity);
}

static void NetMsg_Parser_Parse_svc_lightstyle(void)
{
	MSG_ReadByte();		// Lightstyle count.
	MSG_ReadString();	// Lightstyle string.
}

static void NetMsg_Parser_Parse_svc_updatefrags(mvd_info_t *mvd)
{
	int pnum = MSG_ReadByte();					// Player.
	Sys_PrintDebug(2, "svc_updatefrags: Pnum = %i Userid = %i Name = %s Prev = %i ", pnum, mvd->players[pnum].userid, mvd->players[pnum].name, mvd->players[pnum].frags);
	mvd->players[pnum].frags = MSG_ReadShort();	// Frags.
	Sys_PrintDebug(2, "New = %i\n", mvd->players[pnum].frags);
}

static void NetMsg_Parser_Parse_svc_stopsound(void)
{
	MSG_ReadShort();
}

static void NetMsg_Parser_Parse_svc_damage(void)
{
	int i;
	vec3_t from;
	int armor = MSG_ReadByte();
	int blood = MSG_ReadByte();
	
	for (i = 0; i < 3; i++)
	{
		from[i] = MSG_ReadCoord();
	}
}

static void NetMsg_Parser_Parse_svc_spawnstatic(void)
{
	int i;

	MSG_ReadByte(); // Model index.
	MSG_ReadByte(); // Frame.
	MSG_ReadByte(); // Colormap.
	MSG_ReadByte(); // Skinnum.

	// Origin and angles.
	for (i = 0; i < 3; i++)
	{
		MSG_ReadCoord();
		MSG_ReadAngle();
	}
}

static void NetMsg_Parser_Parse_svc_spawnbaseline(void)
{
	int i;

	MSG_ReadShort();	// Entity.
	MSG_ReadByte();		// Modelindex.
	MSG_ReadByte();		// Frame.
	MSG_ReadByte();		// Colormap.
	MSG_ReadByte();		// Skinnum.

	for (i = 0; i < 3; i++)
	{
		MSG_ReadCoord();	// Origin.
		MSG_ReadAngle();	// Angles.
	}
}

static void NetMsg_Parser_Parse_svc_temp_entity(void)
{
	int i;
	int type = MSG_ReadByte();

	if (type == TE_GUNSHOT || type == TE_BLOOD)
	{
		MSG_ReadByte();
	}

	if (type == TE_LIGHTNING1 || type == TE_LIGHTNING2 || type ==  TE_LIGHTNING3)
	{
		MSG_ReadShort(); // Entity number.

		// Start position (the other position is the end of the beam).
		MSG_ReadCoord();
		MSG_ReadCoord();
		MSG_ReadCoord();
	}

	// Position.
	for (i = 0; i < 3; i++)
	{
		MSG_ReadCoord();
	}
}

static void NetMsg_Parser_Parse_svc_setpause(void)
{
}

static void NetMsg_Parser_Parse_svc_centerprint(void)
{
	char *str = MSG_ReadString();

	Sys_PrintDebug(5, "svc_centerprint: RAW: %s\n", str);
	Sys_PrintDebug(1, "svc_centerprint: %s\n", Sys_RedToWhite(str));
}

static void NetMsg_Parser_Parse_svc_killedmonster(void)
{
}

static void NetMsg_Parser_Parse_svc_foundsecret(void)
{
	// Do nothing.
}

static void NetMsg_Parser_Parse_svc_spawnstaticsound(void)
{
	int i;

	for (i = 0; i < 3; i++)
	{
		MSG_ReadCoord();	// Location.
	}

	MSG_ReadByte();			// Number.
	MSG_ReadByte();			// Volume.
	MSG_ReadByte();			// Attentuion.
}

static void NetMsg_Parser_Parse_svc_intermission(void)
{
	int i;
				
	// Position.
	for (i = 0; i < 3; i++)
	{
		MSG_ReadCoord();
	}

	// View angle.
	for (i = 0; i < 3; i++)
	{
		MSG_ReadAngle();
	}
}

static void NetMsg_Parser_Parse_svc_finale(void)
{
}

static void NetMsg_Parser_Parse_svc_cdtrack(void)
{
	MSG_ReadByte();
}

static void NetMsg_Parser_Parse_svc_sellscreen(void)
{
}

static void NetMsg_Parser_Parse_svc_smallkick(void)
{
	// Nothing.
}

static void NetMsg_Parser_Parse_svc_bigkick(void)
{
	// Nothing.
}

static void NetMsg_Parser_Parse_svc_updateping(mvd_info_t *mvd)
{
	int pnum = MSG_ReadByte();							// Player.
	mvd->players[pnum].ping = (float)MSG_ReadShort();	// Ping.
	mvd->players[pnum].ping_average += mvd->players[pnum].ping;
	mvd->players[pnum].ping_count++;

	mvd->players[pnum].ping_highest = max(mvd->players[pnum].ping_highest, mvd->players[pnum].ping);
	mvd->players[pnum].ping_lowest  = min(mvd->players[pnum].ping_lowest, mvd->players[pnum].ping);
}

static void NetMsg_Parser_Parse_svc_updateentertime(mvd_info_t *mvd)
{
	int pnum	= MSG_ReadByte();	// Player.
	float time	= MSG_ReadFloat();	// Time (sent as seconds ago).

	mvd->players[pnum].entertime = mvd->demotime - time;

	// TODO: Hmmm??? wtf is this, gives values like 1019269.8
	Sys_PrintDebug(4, "svc_updateentertime: %s %f\n", Sys_RedToWhite(mvd->players[pnum].name), mvd->players[pnum].entertime);
}

static void NetMsg_Parser_Parse_svc_updatestatlong(mvd_info_t *mvd)
{
	int stat = MSG_ReadByte();
	int value = MSG_ReadLong();
	
	SetStat(mvd, stat, value);
}

static void NetMsg_Parser_Parse_svc_muzzleflash(void)
{
	MSG_ReadShort(); // Playernum.
}

static void NetMsg_Parser_Parse_svc_updateuserinfo(mvd_info_t *mvd)
{
	char *tmp			= NULL;
	char *userinfo		= NULL;
	players_t *player	= NULL;
	int userid			= 0;
	qbool isGhost		= false;

	int pnum = MSG_ReadByte();			// Player.

	player = &mvd->players[pnum];
	userid = MSG_ReadLong();			// Userid.

	// All userid should be > 0 ... KTPro resends userinfo with 0 as userid for all players!
	if (userid != 0)
	{
		player->userid = userid;
	}

	// A player just timed out, this userinfo will be the ghost that is created.
	// HACK: Checking for ghosts this way is somewaht of a hack, but I can't come up with a better solution, don't have enough info :(
	if (mvd->serverinfo.player_timed_out)
	{
		Sys_PrintDebug(1, "svc_updateuserinfo: Saving ghost info\n");
		
		if (mvd->serverinfo.player_timout_frame != mvd->frame_count)
		{
			Sys_PrintDebug(1, "svc_updateuserinfo: WARNING: Ghost userinfo was not sent the same frame as the user left, might label player as a ghost incorrectly!\n");
		}

		mvd->serverinfo.player_timed_out = false;

		player->is_ghost = true;
	}

	userinfo = MSG_ReadString();		// Userinfo string.

	if (!userinfo[0])
	{
		return;
	}

	// Update the userinfo.
	strlcpy(player->userinfo, userinfo, sizeof(player->userinfo));

	// Parse the userinfo.
	{
		// Name.
		strlcpy(player->name_raw, Info_ValueForKey(player->userinfo, "name"), sizeof(player->name));
		strlcpy(player->name, player->name_raw, sizeof(player->name));
		Sys_RedToWhite(player->name);

		// Color.
		player->topcolor	= atoi(Info_ValueForKey(player->userinfo, "topcolor"));
		player->bottomcolor	= atoi(Info_ValueForKey(player->userinfo, "bottomcolor"));
		
		// Team.
		strlcpy(player->team_raw, Info_ValueForKey(player->userinfo, "team"), sizeof(player->team));
		strlcpy(player->team, player->team_raw, sizeof(player->team));
		Sys_RedToWhite(player->team);

		// Spectator.
		player->spectator = (qbool)(Info_ValueForKey(player->userinfo, "*spectator")[0]);

		if (strcmp((tmp = Info_ValueForKey(player->userinfo, "*client")), ""))
		{
			strlcpy(player->client, tmp, sizeof(player->client));
		}
		else if (strcmp((tmp = Info_ValueForKey(player->userinfo, "*FuhQuake")), ""))
		{
			snprintf(player->client, sizeof(player->client), "FuhQuake %s", tmp);
		}
	}

	Sys_PrintDebug(1, "svc_updateuserinfo: Player %i\n", pnum);
	Sys_PrintDebug(1, "%s\n", player->userinfo);
	Sys_PrintDebug(1, "userid      = %i\n", player->userid);
	Sys_PrintDebug(1, "name        = %s\n", player->name);
	Sys_PrintDebug(1, "team        = %s\n", player->team);
	Sys_PrintDebug(1, "topcolor    = %i\n", player->topcolor);
	Sys_PrintDebug(1, "bottomcolor = %i\n", player->bottomcolor);
	Sys_PrintDebug(1, "spectator   = %i\n", player->spectator);
}

static void NetMsg_Parser_Parse_svc_download(void)
{
}

static void NetMsg_Parser_Parse_svc_playerinfo(mvd_info_t *mvd)
{
	int i;
	int num		= MSG_ReadByte();
	int flags	= MSG_ReadShort();

	mvd->players[num].frame = MSG_ReadByte();
	mvd->players[num].pnum	= num;

	// flags = MVD_TranslateFlags(flags); // No use for us.

	for (i = 0; i < 3; i++)
	{
		if (flags & (DF_ORIGIN << i))
		{
			mvd->players[num].prev_origin[i] = mvd->players[num].origin[i];
			mvd->players[num].origin[i] = MSG_ReadCoord();
		}
	}

	for (i = 0; i < 3; i++)
	{
		if (flags & (DF_ANGLES << i))
		{
			mvd->players[num].viewangles[i] = MSG_ReadAngle16();
		}
	}

	if (flags & DF_MODEL)
	{
		MSG_ReadByte(); // Modelindex
	}

	if (flags & DF_SKINNUM)
	{
		MSG_ReadByte(); // Skinnum
	}

	if (flags & DF_EFFECTS)
	{
		MSG_ReadByte(); // Modelindex
	}

	if (flags & DF_WEAPONFRAME)
	{
		mvd->players[num].weaponframe = MSG_ReadByte(); // Modelindex
	}
}

static void NetMsg_Parser_Parse_svc_nails(void)
{
}

static void NetMsg_Parser_Parse_svc_chokecount(void)
{
	MSG_ReadByte();
}

static void NetMsg_Parser_Parse_svc_modellist(void)
{
	char *str;
	int model_count = MSG_ReadByte();

	while (true)
	{
		str = MSG_ReadString();

		if (!str[0])
		{
			break;
		}
	}

	MSG_ReadByte(); // Ignore.
}

static void NetMsg_Parser_Parse_svc_soundlist(mvd_info_t *mvd)
{
	char *soundname;
	int soundindex = MSG_ReadByte();

	Sys_PrintDebug(3, "svc_soundlist: Start sound index: %i\n", soundindex);

	while (true)
	{
		soundname = MSG_ReadString();

		if (!soundname[0])
		{
			break;
		}

		Sys_PrintDebug(3, "sound %i: %s\n", soundindex, soundname);

		mvd->sndlist[soundindex] = Q_strdup(soundname); // TODO : Make sure we free these.
		soundindex++;
	}

	MSG_ReadByte(); // Ignore.
}

static void NetMsg_Parser_Parse_svc_packetentities(mvd_info_t *mvd)
{
	NetMsg_Parser_ParsePacketEntities(mvd, false);
}

static void NetMsg_Parser_Parse_svc_deltapacketentities(mvd_info_t *mvd)
{
	NetMsg_Parser_ParsePacketEntities(mvd, true);
}

static void NetMsg_Parser_Parse_svc_maxspeed(mvd_info_t *mvd)
{
	mvd->serverinfo.movevars.maxspeed = MSG_ReadFloat();
}

static void NetMsg_Parser_Parse_svc_entgravity(void)
{
}

static void NetMsg_Parser_Parse_svc_setinfo(mvd_info_t *mvd)
{
	int pnum = MSG_ReadByte();
	char key[MAX_INFO_KEY];
	char value[MAX_INFO_STRING];
	players_t *player = &mvd->players[pnum];

	strlcpy(key, MSG_ReadString(), sizeof(key));
	strlcpy(value, MSG_ReadString(), sizeof(value));

	Sys_PrintDebug(2, "svc_setinfo: player: %i name: %s key: \"%s\" value \"%s\"\n", pnum, player->name, key, value);
	
	if (!strcmp(key, "name"))
	{
		Sys_PrintDebug(1, "svc_setinfo: Player %i renamed from %s to %s\n", pnum, player->name, value);
		strlcpy(player->name, value, sizeof(player->name));
	}
}

static void NetMsg_Parser_Parse_svc_serverinfo(mvd_info_t *mvd)
{
	char key[MAX_INFO_KEY];
	char value[MAX_INFO_STRING];

	strlcpy(key, MSG_ReadString(), sizeof(key));
	strlcpy(value, MSG_ReadString(), sizeof(value));

	Info_SetValueForKey(mvd->serverinfo.serverinfo, key, value, 2 * MAX_INFO_STRING);

	if (!mvd->serverinfo.match_started && !strcmp(key, "status") && strcmp(value, "Countdown"))
	{
		// TODO : Do a better check here maybe?
		// If the status is not countdown, the match has started.
		mvd->serverinfo.match_started = true;
		mvd->match_start_demotime = mvd->demotime;

		Log_Event(&logger, mvd, LOG_MATCHSTART, -1);
	}

	NetMsg_Parser_ParseServerInfo(mvd);
}

static void NetMsg_Parser_Parse_svc_updatepl(mvd_info_t *mvd)
{
	int pnum = MSG_ReadByte();						// Player.
	mvd->players[pnum].pl = (float)MSG_ReadByte();	// Ping.
	mvd->players[pnum].pl_average += mvd->players[pnum].pl;
	mvd->players[pnum].pl_count++;

	mvd->players[pnum].pl_highest = max(mvd->players[pnum].pl_highest, mvd->players[pnum].pl);
	mvd->players[pnum].pl_lowest  = min(mvd->players[pnum].pl_lowest, mvd->players[pnum].pl);
}

static void NetMsg_Parser_Parse_svc_nails2(void)
{
	int i;
	int j;
	int nailcount = MSG_ReadByte();

	for (i = 0; i < nailcount; i++)
	{
		MSG_ReadByte(); // Projectile number.

		// Bits for origin and angles.
		for (j = 0; j < 6; j++)
		{
			MSG_ReadByte();
		}
	}
}

static int MVD_TranslateFlags(int src)
{
	int dst = 0;

	if (src & DF_EFFECTS)
		dst |= PF_EFFECTS;
	if (src & DF_SKINNUM)
		dst |= PF_SKINNUM;
	if (src & DF_DEAD)
		dst |= PF_DEAD;
	if (src & DF_GIB)
		dst |= PF_GIB;
	if (src & DF_WEAPONFRAME)
		dst |= PF_WEAPONFRAME;
	if (src & DF_MODEL)
		dst |= PF_MODEL;

	return dst;
}

qbool NetMsg_Parser_StartParse(mvd_info_t *mvd)
{
	int cmd;
	char *str = NULL;

	if (!net_message.data)
	{
		Sys_PrintError("NetMsg_Parser_StartParse: NULL net message\n");
		return false;
	}

	while (true)
	{
		if (msg_badread) 
		{
			//msg_readcount++;
			Sys_PrintError("CL_ParseServerMessage: Bad server message.\n");
			return false;
		}

		cmd = MSG_ReadByte();

		if (cmd == -1)
		{
			msg_readcount++;
			break;
		}

		switch (cmd)
		{
			default :
			{
				Sys_PrintError("CL_ParseServerMessage: Unknown cmd type.\n");
				return false;
			}
			case svc_nop :
			{
				// Nothingness!
				break;
			}
			case svc_disconnect :
			{
				//LogVarHashTable_Test(mvd);
				Sys_PrintDebug(1, "NetMsg_Parser_StartParse: Disconnected\n");
				return true;
			}
			case nq_svc_time :
			{
				// TODO : Hmm is this needed?
				MSG_ReadFloat();
				break;
			}
			case svc_print :
			{
				NetMsg_Parser_Parse_svc_print(mvd);
				break;
			}
			case svc_centerprint :
			{
				NetMsg_Parser_Parse_svc_centerprint();
				break;
			}
			case svc_stufftext :
			{
				NetMsg_Parser_Parse_svc_stufftext(mvd);
				break;
			}
			case svc_damage :
			{
				NetMsg_Parser_Parse_svc_damage();
				break;
			}
			case svc_serverdata :
			{
				NetMsg_Parser_Parse_svc_serverdata(mvd);
				break;
			}
			case svc_cdtrack :
			{
				NetMsg_Parser_Parse_svc_cdtrack();
				break;
			}
			case svc_playerinfo :
			{
				NetMsg_Parser_Parse_svc_playerinfo(mvd);
				break;
			}
			case svc_modellist :
			{
				NetMsg_Parser_Parse_svc_modellist();
				break;
			}
			case svc_soundlist :
			{
				NetMsg_Parser_Parse_svc_soundlist(mvd);
				break;
			}
			case svc_spawnstaticsound :
			{
				NetMsg_Parser_Parse_svc_spawnstaticsound();
				break;
			}
			case svc_spawnbaseline :
			{
				NetMsg_Parser_Parse_svc_spawnbaseline();
				break;
			}
			case svc_updatefrags :
			{
				NetMsg_Parser_Parse_svc_updatefrags(mvd);
				break;
			}
			case svc_updateping :
			{
				NetMsg_Parser_Parse_svc_updateping(mvd);
				break;
			}
			case svc_updatepl :
			{
				NetMsg_Parser_Parse_svc_updatepl(mvd);				
				break;
			}
			case svc_updateentertime :
			{
				NetMsg_Parser_Parse_svc_updateentertime(mvd);
				break;
			}
			case svc_updateuserinfo :
			{
				NetMsg_Parser_Parse_svc_updateuserinfo(mvd);
				break;
			}
			case svc_lightstyle :
			{
				NetMsg_Parser_Parse_svc_lightstyle();
				break;
			}
			case svc_bad :
			{
				Sys_PrintDebug(1, "Bad packet\n");
				break;
			}
			case svc_serverinfo :
			{
				NetMsg_Parser_Parse_svc_serverinfo(mvd);
				break;
			}
			case svc_packetentities :
			{
				NetMsg_Parser_Parse_svc_packetentities(mvd);
				break;
			}
			case svc_deltapacketentities :
			{
				NetMsg_Parser_Parse_svc_deltapacketentities(mvd);
				break;
			}
			case svc_updatestatlong :
			{
				NetMsg_Parser_Parse_svc_updatestatlong(mvd);
				break;
			}
			case svc_updatestat :
			{
				NetMsg_Parser_Parse_svc_updatestat(mvd);
				break;
			}
			case svc_sound :
			{
				NetMsg_Parser_Parse_svc_sound(mvd);
				break;
			}
			case svc_stopsound :
			{
				NetMsg_Parser_Parse_svc_stopsound();
				break;
			}
			case svc_temp_entity :
			{
				NetMsg_Parser_Parse_svc_temp_entity();
				break;
			}
			case svc_setangle :
			{
				NetMsg_Parser_Parse_svc_setangle();
				break;
			}
			case svc_setinfo :
			{
				NetMsg_Parser_Parse_svc_setinfo(mvd);
				break;
			}
			case svc_muzzleflash :
			{
				NetMsg_Parser_Parse_svc_muzzleflash();
				break;
			}
			case svc_smallkick :
			{
				NetMsg_Parser_Parse_svc_smallkick();
				break;
			}
			case svc_bigkick :
			{
				NetMsg_Parser_Parse_svc_bigkick();
				break;
			}
			case svc_intermission :
			{
				NetMsg_Parser_Parse_svc_intermission();
				break;
			}
			case svc_chokecount :
			{
				NetMsg_Parser_Parse_svc_chokecount();
				break;
			}
			case svc_spawnstatic :
			{
				NetMsg_Parser_Parse_svc_spawnstatic();
				break;
			}
			case svc_foundsecret :
			{
				NetMsg_Parser_Parse_svc_foundsecret();
				break;
			}
			case svc_maxspeed :
			{
				NetMsg_Parser_Parse_svc_maxspeed(mvd);
				break;
			}
			case svc_nails2 :
			{
				NetMsg_Parser_Parse_svc_nails2();
				break;
			}
		}
	}

	return true;
}


