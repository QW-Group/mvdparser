#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "maindef.h"
#include "netmsg_parser.h"
#include "net_msg.h"
#include "stats.h"
#include "endian.h"
#include "mvd_parser.h"

//
// Initializes all buffers and structs before parsing an MVD.
//
static void MVD_Parser_Init(mvd_info_t *mvd)
{
	int x;

	memset(mvd, 0, sizeof(*mvd));

	for (x = 0; x < (sizeof(mvd->players) / sizeof(*mvd->players)); x++)
	{
		mvd->players[x].ping_lowest = 999;
		mvd->players[x].pl_lowest = 999;
		mvd->players[x].spectator = 1;
	}
}

#define GREEN_ARMOR_BIT			13
#define GREEN_ARMOR				1
#define YELLOW_ARMOR			2
#define RED_ARMOR				3
#define ARMOR_FLAG_BYNUM(num)	(IT_ARMOR##num)

#define HAS_ARMOR(player, num)	((player)->stats[STAT_ITEMS] & (1 << (GREEN_ARMOR_BIT - 1 + bound(GREEN_ARMOR, num, RED_ARMOR))))

char *Armor_Name(int num)
{
	switch (num)
	{
		case GREEN_ARMOR :	return "GA";
		case YELLOW_ARMOR :	return "YA";
		case RED_ARMOR :	return "RA";
	}

	return NULL;
}

char *Weapon_Name(int num)
{
	switch (num) 
	{
			case IT_AXE:				return "AXE";
			case IT_SHOTGUN:			return "SG";
			case IT_SUPER_SHOTGUN:		return "SSG";
			case IT_NAILGUN:			return "NG";
			case IT_SUPER_NAILGUN:		return "SNG";
			case IT_GRENADE_LAUNCHER:	return "GL";
			case IT_ROCKET_LAUNCHER:	return "RL";
			case IT_LIGHTNING:			return "LG";
			default:					return NULL;
	}
}
/*
#define MAX_LOG_MESSAGE_LENGTH	256

static void MVD_Parser_ClearEvents(mvd_info_t *mvd)
{
	log_event_t *tmp = NULL;
	log_event_t *it = mvd->log_events_tail;

	while (it)
	{
		Q_free(it->message);

		tmp = it->next;
		Q_free(it);
		it = tmp;
	}
}*/

static char *LogVar_name(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].name;
}

static char *LogVar_team(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].team;
}

static char *LogVar_userinfo(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return mvd->players[player_num].userinfo;
}

static char *LogVar_playernum(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", player_num);
}

static char *LogVar_frame(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].frame);
}

static char *LogVar_userid(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].userid);
}

static char *LogVar_frags(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].frags);
}

static char *LogVar_spectator(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].spectator);
}

static char *LogVar_health(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_HEALTH]);
}

static char *LogVar_armor(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ARMOR]);
}

static char *LogVar_activeweapon(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ACTIVEWEAPON]);
}

static char *LogVar_shells(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_SHELLS]);
}

static char *LogVar_nails(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_NAILS]);
}

static char *LogVar_rockets(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_ROCKETS]);
}

static char *LogVar_cells(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].stats[STAT_CELLS]);
}

static char *LogVar_quad(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_QUAD));
}

static char *LogVar_pent(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_INVULNERABILITY));
}

static char *LogVar_ring(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_ITEMS] & IT_INVISIBILITY));
}

static char *LogVar_sg(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SHOTGUN));
}

static char *LogVar_ssg(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SUPER_SHOTGUN));
}

static char *LogVar_ng(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_NAILGUN));
}

static char *LogVar_sng(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_SUPER_NAILGUN));
}

static char *LogVar_gl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_GRENADE_LAUNCHER));
}

static char *LogVar_rl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_ROCKET_LAUNCHER));
}

static char *LogVar_lg(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", (qbool)(mvd->players[player_num].stats[STAT_WEAPON] & IT_LIGHTNING));
}

static char *LogVar_pl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl);
}

static char *LogVar_avgpl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_average / mvd->players[player_num].pl_count);
}

static char *LogVar_maxpl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_highest);
}

static char *LogVar_minpl(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].pl_lowest);
}

static char *LogVar_ping(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping);
}

static char *LogVar_avgping(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_average / mvd->players[player_num].ping_count);
}

static char *LogVar_maxping(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_highest);
}

static char *LogVar_minping(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].ping_lowest);
}

static char *LogVar_gacount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[0]);
}

static char *LogVar_yacount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[1]);
}

static char *LogVar_racount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].armor_count[2]);
}

static char *LogVar_ssgcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[0]);
}

static char *LogVar_ngcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[1]);
}

static char *LogVar_sngcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[2]);
}

static char *LogVar_glcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[3]);
}

static char *LogVar_rlcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[4]);
}

static char *LogVar_lgcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_count[5]);
}

static char *LogVar_ssgdrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[0]);
}

static char *LogVar_ngdrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[1]);
}

static char *LogVar_sngdrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[2]);
}

static char *LogVar_gldrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[3]);
}

static char *LogVar_rldrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[4]);
}

static char *LogVar_lgdrop(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_drops[5]);
}

static char *LogVar_sgshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[1]);
}

static char *LogVar_ssgshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[2]);
}

static char *LogVar_ngshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[3]);
}

static char *LogVar_sngshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[4]);
}

static char *LogVar_glshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[5]);
}

static char *LogVar_rlshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[6]);
}

static char *LogVar_lgshots(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].weapon_shots[7]);
}

static char *LogVar_deaths(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].death_count);
}

static char *LogVar_mhcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].megahealth_count);
}

static char *LogVar_quadcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].quad_count);
}

static char *LogVar_ringcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].ring_count);
}

static char *LogVar_pentcount(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].pent_count);
}

static char *LogVar_avgspeed(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].acc_average_speed / mvd->players[player_num].speed_frame_count);
}

static char *LogVar_maxspeed(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].speed_highest);
}

static char *LogVar_posx(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[0]);
}

static char *LogVar_posy(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[1]);
}

static char *LogVar_posz(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].origin[2]);
}

static char *LogVar_pitch(const mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[0]);
}

static char *LogVar_yaw(const mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[1]);
}

static char *LogVar_roll(const mvd_info_t *mvd, const char *varname, int player_num)
{
	// TODO : Is this the correct angle?
	return va("%g", mvd->players[player_num].viewangles[2]);
}

static char *LogVar_distancemoved(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%g", mvd->players[player_num].distance_moved);
}

static char *LogVar_topcolor(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].topcolor);
}

static char *LogVar_bottomcolor(const mvd_info_t *mvd, const char *varname, int player_num)
{
	return va("%i", mvd->players[player_num].bottomcolor);
}

typedef char * (* logvar_func)(const mvd_info_t *mvd, const char *varname, int player_num);

typedef struct logvar_s
{
	char			*name;
	unsigned long	hash;
	logvar_func		func;
	struct logvar_s	*next;
} logvar_t;

#define LOGVAR_DEFINE(name)	{#name, 0, LogVar_##name}

logvar_t logvar_list[] =
{
	LOGVAR_DEFINE(name),
	LOGVAR_DEFINE(team),
	LOGVAR_DEFINE(userinfo),
	LOGVAR_DEFINE(playernum),
	LOGVAR_DEFINE(frame),
	LOGVAR_DEFINE(userid),
	LOGVAR_DEFINE(frags),
	LOGVAR_DEFINE(spectator),
	LOGVAR_DEFINE(health),
	LOGVAR_DEFINE(armor),
	LOGVAR_DEFINE(activeweapon),
	LOGVAR_DEFINE(shells),
	LOGVAR_DEFINE(nails),
	LOGVAR_DEFINE(rockets),
	LOGVAR_DEFINE(cells),
	LOGVAR_DEFINE(quad),
	LOGVAR_DEFINE(pent),
	LOGVAR_DEFINE(ring),
	LOGVAR_DEFINE(sg),
	LOGVAR_DEFINE(ssg),
	LOGVAR_DEFINE(ng),
	LOGVAR_DEFINE(sng),
	LOGVAR_DEFINE(gl),
	LOGVAR_DEFINE(rl),
	LOGVAR_DEFINE(lg),
	LOGVAR_DEFINE(pl),
	LOGVAR_DEFINE(avgpl),
	LOGVAR_DEFINE(maxpl),
	LOGVAR_DEFINE(minpl),
	LOGVAR_DEFINE(ping),
	LOGVAR_DEFINE(avgping),
	LOGVAR_DEFINE(maxping),
	LOGVAR_DEFINE(minping),
	LOGVAR_DEFINE(gacount),
	LOGVAR_DEFINE(yacount),
	LOGVAR_DEFINE(racount),
	LOGVAR_DEFINE(ssgcount),
	LOGVAR_DEFINE(ngcount),
	LOGVAR_DEFINE(sngcount),
	LOGVAR_DEFINE(glcount),
	LOGVAR_DEFINE(rlcount),
	LOGVAR_DEFINE(lgcount),
	LOGVAR_DEFINE(ssgdrop),
	LOGVAR_DEFINE(ngdrop),
	LOGVAR_DEFINE(sngdrop),
	LOGVAR_DEFINE(gldrop),
	LOGVAR_DEFINE(rldrop),
	LOGVAR_DEFINE(lgdrop),
	LOGVAR_DEFINE(sgshots),
	LOGVAR_DEFINE(ssgshots),
	LOGVAR_DEFINE(ngshots),
	LOGVAR_DEFINE(sngshots),
	LOGVAR_DEFINE(glshots),
	LOGVAR_DEFINE(rlshots),
	LOGVAR_DEFINE(lgshots),
	LOGVAR_DEFINE(deaths),
	LOGVAR_DEFINE(mhcount),
	LOGVAR_DEFINE(quadcount),
	LOGVAR_DEFINE(ringcount),
	LOGVAR_DEFINE(pentcount),
	LOGVAR_DEFINE(avgspeed),
	LOGVAR_DEFINE(maxspeed),
	LOGVAR_DEFINE(posx),
	LOGVAR_DEFINE(posy),
	LOGVAR_DEFINE(posz),
	LOGVAR_DEFINE(pitch),
	LOGVAR_DEFINE(yaw),
	LOGVAR_DEFINE(distancemoved),
	LOGVAR_DEFINE(topcolor),
	LOGVAR_DEFINE(bottomcolor)
};

#define LOGVARS_LIST_SIZE		(sizeof(logvar_list) / sizeof(logvar_t))
#define LOGVARS_HASHTABLE_SIZE	512

static logvar_t *logvar_hashtable[LOGVARS_HASHTABLE_SIZE];

logvar_t *LogVarHashTable_GetValue(const logvar_t **hashtable, const char *varname)
{
	unsigned long varhash = Com_HashKey(varname); 
	logvar_t *it = (logvar_t *)hashtable[varhash % LOGVARS_HASHTABLE_SIZE];

	while (it && (it->hash != varhash))
	{
		it = it->next;
	}

	return it;
}

void LogVarHashTable_AddValue(logvar_t **hashtable, logvar_t *logvar)
{
	logvar_t *it = NULL;
	unsigned long varhash;

	logvar->hash = Com_HashKey(logvar->name);
	varhash = logvar->hash % LOGVARS_HASHTABLE_SIZE;
	it = hashtable[varhash];

	if (!it)
	{
		hashtable[varhash] = logvar;
	}
	else
	{
		while (it->next)
		{
			it = it->next;
		}

		it->next = logvar;
	}
}

char *LogVarValueAsString(const mvd_info_t *mvd, const char *varname, int player_num)
{
	logvar_t *logvar = LogVarHashTable_GetValue(logvar_hashtable, varname);

	#ifdef _DEBUG
	if (strcasecmp(varname, logvar->name))
	{
		Sys_Error("LogVarValueAsString: %s != %s\n", varname, logvar->name);
	}
	#endif // _DEBUG

	if (!logvar->func)
	{
		Sys_Error("LogVarValueAsString: The log variable function for %s is NULL\n", varname);
	}

	return logvar->func(mvd, logvar->name, player_num);
}

void LogVarHashTable_Init(void)
{
	int i;

	for (i = 0; i < LOGVARS_LIST_SIZE; i++)
	{
		LogVarHashTable_AddValue(logvar_hashtable, &logvar_list[i]);
	}
}

void LogVarHashTable_Test(mvd_info_t *mvd)
{
	int i;
	int pnum;
	unsigned long hash;
	logvar_t *lv;

	for (i = 0; i < LOGVARS_LIST_SIZE; i++)
	{
		lv = LogVarHashTable_GetValue(logvar_hashtable, logvar_list[i].name);
		hash = Com_HashKey(logvar_list[i].name);

		for (pnum = 0; pnum < MAX_PLAYERS; pnum++)
		{
			if (PLAYER_ISVALID(&mvd->players[pnum]))
			{
				Sys_Print("%s = %s\n", lv->name, lv->func(mvd, lv->name, pnum));
			}
		}

		if (hash != lv->hash)
		{
			Sys_PrintError("%s (%i) != %s (%i)\n", logvar_list[i].name, hash, lv->name, lv->hash);
		}
	}
}

static void MVD_Parser_LogEvent(mvd_info_t *mvd, log_eventtype_t type, vec3_t loc)
{
	log_event_t *e = NULL;
	log_event_t *tmp = NULL;

	e->time = mvd->demotime;
	e->type = type;

	switch (type)
	{
		case BASIC :
		{
			e = (log_event_t *)Q_malloc(sizeof(*e));
		}
		case DEATH :
		default :
		{
			return;
		}
	}

	Sys_PrintDebug(2, "MVD_Parser_LogEvent: \n");

	// Empty list.
	if (!mvd->log_events_tail)
	{
		mvd->log_events_tail = e;
		mvd->log_events_head = e;
	}

	// Leave the tail be so we have somewhere to start from
	// and add the new ones to the head instead.
	if (mvd->log_events_head)
	{
		tmp = mvd->log_events_head;
		tmp->next = e;
		mvd->log_events_head = e;
	}
}

static void MVD_Parser_StatsGather(mvd_info_t *mvd)
{
	int i;
	int j;
	int armor;
	players_t *cf = NULL;	// Current frames player info.
	players_t *lf = NULL;	// Last frames player info.
	float time_since_last_frame = (mvd->demotime - mvd->old_demotime);

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		cf = &mvd->players[i];
		lf = &mvd->lf_players[i];

		if (!PLAYER_ISVALID(cf))
		{
			continue;
		}

		// Did the players armor increase?
		if (cf->stats[STAT_ARMOR] > lf->stats[STAT_ARMOR])
		{
			for (armor = GREEN_ARMOR; armor <= RED_ARMOR; armor++)
			{
				// If we didn't get a new armor, save the amount we've wasted.
				if (HAS_ARMOR(cf, armor) && HAS_ARMOR(lf, armor))
				{
					// TODO : Uhm, is this correct?
					cf->armor_count[armor]++;
					cf->armor_wasted[armor] += lf->stats[STAT_ARMOR];
				}
			}
		}

		if ((cf->stats[STAT_HEALTH] <= 0) && (lf->stats[STAT_HEALTH] > 0))
		{
			cf->death_count++;
			// TODO : Log death event.
		}

		if ((cf->stats[STAT_HEALTH] >= 100) 
			&& (cf->stats[STAT_HEALTH] > lf->stats[STAT_HEALTH]) 
			&& (lf->stats[STAT_ITEMS] & IT_SUPERHEALTH))
		{
			cf->megahealth_count++;
		}

		// If this player teamkilled someone, find out who it was :s
		if (cf->teamkill_flag)
		{
			int j;
			players_t *p = NULL;	// Player from current frame.
			players_t *lp = NULL;	// Player from last frame.

			for (j = 0; j < MAX_PLAYERS; j++)
			{
				p = &mvd->players[j];
				lp = &mvd->lf_players[j];
				
				// Find out if the player is on the same team and just died
				if (PLAYER_ISVALID(p) 
					&& !strcmp(p->team, cf->team)		// Is the player on the same team?								
					&& (lp->stats[STAT_HEALTH] > 0)		// Did the player just die?
					&& (cf->stats[STAT_HEALTH] <= 0))	
				{
					mvd->fragstats[cf->pnum].tkills[j]++;
					mvd->fragstats[j].tdeaths[cf->pnum]++;
					cf->teamkill_flag = false;
					break;
				}
			}
		}
	}

	/*
	if (mvd->frame_info.jumpcount)
	{
		int j;
		double distance;
		double t_dist;

		for (j = 0; j < mvd->frame_info.jumpcount; j++)
		{
			distance = 1000000.0;

			for (i = 0; i < MAX_PLAYERS; i++)
			{
				cf = &mvd->players[i];
				lf = &mvd->lf_players[i];

				if (!PLAYER_ISVALID(cf)
				{
					continue;
				}

				TODO : Add jump crap stuff.
			}
		}
	}
	*/

	for (i = 0; i < MAX_PLAYERS; i++)
	{
		cf = &mvd->players[i];
		lf = &mvd->lf_players[i];

		for (armor = GREEN_ARMOR; armor <= RED_ARMOR; armor++)
		{
			if (lf->armor_count[armor - 1] < cf->armor_count[armor - 1])
			{
				char *armorname = Armor_Name(armor);
				// TODO : Log picking up an armor.
			}
		}

		if (lf->pent_count < cf->pent_count)
		{
			// TODO : Log picking up pent.
		}

		if (lf->quad_count < cf->quad_count)
		{
			// TODO : Log picking up quad.
		}

		if (lf->ring_count < cf->ring_count)
		{
			// TODO : Log picking up ring.
		}

		// Weapons.
		for (j = 0; j <= 5; j++)
		{
			if (lf->weapon_count[j] < cf->weapon_count[j])
			{
				char *weapon_name = Weapon_Name(j);
				// TODO : Log weapon pickup event.
			}
		}

		// Calculate player speed if alive.
		if ((lf->stats[STAT_HEALTH] > 0) && (time_since_last_frame > 0))
		{
			vec3_t dv;
			float distance;
			float speed;

			// Get the distance vector between the current and last frames origin of the player.
			VectorSubtract(cf->origin, lf->origin, dv);

			// Calculate the distance moved.
			distance = (float)sqrt(pow(dv[0], 2) + pow(dv[1], 2) + pow(dv[2], 2));

			cf->distance_moved += distance;

			speed = distance / time_since_last_frame;

			// 0 = Don't include standing still when calculating speed. Or should we? :)
			if (speed > 0)
			{
				cf->acc_average_speed += speed;
				cf->speed_frame_count++;

				if (speed > cf->speed_highest)
				{
					cf->speed_highest = speed;
				}
			}
		}
	}
}

//
// Reads a specified amount of bytes from the demo.
//
static qbool MVD_Parser_DemoRead(mvd_info_t *mvd, void *dest, unsigned int need, qbool peek)
{
	if ((long)((mvd->demo_ptr - mvd->demo_start) + need) > mvd->demo_size)
	{
		Sys_PrintError("MVD_Parser_DemoRead: Unexpected end of demo.\n");
		return false;
	}

	memcpy(dest, mvd->demo_ptr, need);

	if (!peek)
	{
		mvd->demo_ptr += need;
	}

	return true;
}

//
// Reads a new MVD frame.
//
static qbool MVD_Parser_ReadFrame(mvd_info_t *mvd)
{
	int i;
	static int mvd_frame_count = 0;
	byte mvd_time;
	byte message_type;
	byte c;

	// Save the demo time before reading a new frame.
	mvd->old_demotime = mvd->demotime;

	// Check if we need to get more messages for now and if so read it
	// from the demo file and pass it on to the net channel.
	while (true)
	{
		// Read MVD time.
		if (!MVD_Parser_DemoRead(mvd, &mvd_time, 1, false))
		{
			Sys_PrintError("MVD_Parser_ReadFrame: Failed to read demo time\n");
			return false;
		}

		mvd->demotime += (mvd_time * 0.001f);

		Sys_PrintDebug(5, "MVD_Parser_ReadFrame: Time: %g Frame: %i \n", mvd->demotime, mvd->frame_count++);

		// Get the msg type.
		if (!MVD_Parser_DemoRead(mvd, &c, 1, false))
		{
			Sys_PrintError("MVD_Parser_ReadFrame: Failed to read command type\n");
			return false;
		}

		message_type = (c & 7); // We only care about the first 3 bits.

		// QWD Only.
		if (message_type == dem_cmd)
		{
			// Fail! This should only appear in QWDs
			Sys_PrintError("MVD_Parser_ReadFrame: Encountered a dem_cmd command, this should only occur in QWDs\n");
			return false;
		}

		// MVD Only. These message types tells to which players the message is directed to.
		if ((message_type >= dem_multiple) && (message_type <= dem_all))
		{
			switch (message_type)
			{
				case dem_multiple:
				//
				// This message should be sent to more than one player, get which players.
				//
				{
					// Read the player bit mask from the demo and convert to the correct byte order.
					// Each bit in this number represents a player, 32-bits, 32 players.
					if (!MVD_Parser_DemoRead(mvd, &i, sizeof(i), false))
					{
						Sys_PrintError("MVD_Parser_ReadFrame: Failed to read dem_multiple player bitmask\n");
						return false;
					}

					mvd->lastto = LittleLong(i);
					mvd->lasttype = dem_multiple;
					break;
				}
				case dem_stats:
				//
				// The stats for a player has changed. Get the player number of that player.
				//
				case dem_single:
				//
				// Only a single player should receive this message. Get the player number of that player.
				//
				{
					// The first 3 bits contain the message type (so remove that part), the rest contains
					// a 5-bit number which contains the player number of the affected player.
					mvd->lastto = (c >> 3);
					mvd->lasttype = message_type;
					break;
				}
				case dem_all:
				//
				// This message is directed to all players.
				//
				{
					mvd->lastto = 0;
					mvd->lasttype = dem_all;
					break;
				}
				default:
				{
					Sys_PrintError("MVD_Parser_ReadFrame: Unknown command type.\n");
					return false;
				}
			}

			// Fall through to dem_read after we've gotten the affected players.
			message_type = dem_read;
		}

		// Get the next net message from the demo file.
		if (message_type == dem_read)
		{
			// Read the size of next net message in the demo file
			// and convert it into the correct byte order.
			if (!MVD_Parser_DemoRead(mvd, &net_message.cursize, 4, false))
			{
				Sys_PrintError("MVD_Parser_ReadFrame: Failed to read current net_message size in dem_read\n");
				return false;
			}

			net_message.cursize = LittleLong(net_message.cursize);

			// The message was too big, stop playback.
			if (net_message.cursize > net_message.maxsize)
			{
				Sys_PrintError("MVD_Parser_ReadFrame: net_message.cursize > net_message.maxsize\n");
				return false;
			}

			// Read the net message from the demo.
			net_message.data = Q_malloc(sizeof(byte) * net_message.cursize);

			if (!MVD_Parser_DemoRead(mvd, net_message.data, net_message.cursize, false))
			{
				Sys_PrintError("MVD_Parser_ReadFrame: Failed to read net message data of size %i.\n", net_message.cursize);
				return false;
			}

			// Check what the last message type was for MVDs.
			switch(mvd->lasttype)
			{
				case dem_multiple:
				{
					// Only used for chat really, so don't care.
					continue;
				}
				case dem_single:
				{
					break;
				}
			}

			return true; // We just read something.
		}

		// Gets the sequence numbers for the netchan at the start of the demo.
		if (message_type == dem_set)
		{
			// Read outgoing sequence.
			if (!MVD_Parser_DemoRead(mvd, &i, sizeof(i), false))
			{
				Sys_PrintError("MVD_Parser_ReadFrame: Failed to read outgoing sequence number in dem_set.\n");
				return false;
			}

			outgoing_sequence = LittleLong(i);

			// Read incoming sequence.
			if (!MVD_Parser_DemoRead(mvd, &i, sizeof(i), false))
			{
				Sys_PrintError("MVD_Parser_ReadFrame: Failed to read incoming sequence number in dem_set.\n");
				return false;
			}

			incoming_sequence = LittleLong(i);

			continue;
		}

		// We should never get this far if the demo is ok.
		Sys_PrintError("MVD_Parser_ReadFrame: Corrupted demo.\n");
		return false;
	}
}

//
// Reads and parses an MVD demo.
// mvdbuf = The demo buffer to read from.
// filelen = The length of the demo in bytes.
//
qbool MVD_Parser_StartParse(byte *mvdbuf, long filelen)
{
	mvd_info_t mvd;
	double start_time = Sys_DoubleTime();
	double end_time	= 0.0;

	// Init the mvd struct.
	MVD_Parser_Init(&mvd);

	// Set the demo pointer.
	mvd.demo_start = mvdbuf;
	mvd.demo_ptr = mvdbuf;
	mvd.demo_size = filelen;

	memset(&net_message, 0, sizeof(net_message));
	net_message.maxsize = 8192;

	while (mvd.demo_ptr < (mvd.demo_start + mvd.demo_size))
	{
		// Reset read state.
		memset(&mvd.frame_info, 0, sizeof(mvd.frame_info));
		MSG_BeginReading();

		// Read an MVD frame.
		if (!MVD_Parser_ReadFrame(&mvd))
		{
			Q_free(net_message.data);

			Sys_PrintError("MVD_Parser_StartParse: Failed to parse MVD frame.\n");
			return false;
		}

		// If a net message was read, parse it.
		if (net_message.data)
		{
			// Parse the server message.
			if (!NetMsg_Parser_StartParse(&mvd))
			{
				Sys_PrintError("MVD_Parser_StartParse: Failed to parse server message.\n");
				return false;
			}

			Q_free(net_message.data);
			net_message.cursize = 0;
		}

		// Only gather stats when a match has started.
		if (mvd.serverinfo.match_started)
		{
			// TODO : Gather stats.
		}

		// Save the current player infos for next frame.
		memcpy(mvd.lf_players, mvd.players, sizeof(players_t) * MAX_PLAYERS);
	}

	end_time = Sys_DoubleTime() - start_time;

	Sys_PrintDebug(1, "MVD_Parser_StartParse: Finished parsing in %g seconds.\n", end_time);

	return true;
}



