#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "maindef.h"
#include "net_msg.h"
#include "stats.h"
#include "endian.h"
#include "mvd_parser.h"

typedef struct h_info_s
{
	int type;
	vec3_t  origin;
} h_info_t;

typedef struct frame_info_s
{
	int			healthcount;
	h_info_t	healthinfo[32];
	int			jumpcount;
	vec3_t		jumpinfo[32];
} frame_info_t;

typedef struct players_s
{
	#ifdef __CONSISTENCY_CHECK__
	char	name[64];
	char	userinfo[512];
	char	team[64];
	#else
	char	*name;
	char	*userinfo;
	char	*team;
	#endif

	int		pnum;
	int		frame;
	int		userid;
	int		frags;
	int		spectator;
	int		stats[MAX_CL_STATS];
	float	pl;
	int		pl_count;
	float	pl_average;
	float	pl_highest;
	float	pl_lowest;
	float	ping;
	int		ping_count;
	float	ping_average;
	float	ping_highest;
	float	ping_lowest;
	int		armor_count[3];
	int		weapon_count[7];
	int		weapon_drops[7];
	int		weapon_shots[9];
	
	int		armor_took_flag;
	float	weapon_time[7];
	int		death_count;
	int		megahealth_count;
	
	int		quad_count;
	int		ring_count;
	int		pent_count;

	int		armor_wasted[3];
	int		health_count[3];
	int		health_wasted;
	int		jump_count;
	int		damage_health[4];
	int		damage_armor[3];
	int		weaponframe;
	int		speed_frame_count;
	float	acc_average_speed;
	float	speed_highest;
	vec3_t	origin;

	int		mysql_id;

	qbool	teamkill_flag;
} players_t;

typedef struct fragstats_s
{
	int		kills[32][64];
	int		deaths[32][64];
	int		wdeaths[64];
	int		wkills[64];
	int		tkills[64];
	int		tdeaths[64];
	int		total_frags;
	int		flag_touches;
	int		flag_dropped;
	int		flag_captured;
	int		suicides;
	int		teamkills;
} fragstats_t ;

typedef struct server_s
{
	char    *serverinfo;
	qbool    countdown;
	qbool    match_started;
} server_t;

typedef struct mvd_info_s
{
	byte			*demo_ptr;					// A pointer to the current position in the demo.
	byte			*demo_start;				// A pointer to the start of the demo.
	long			demo_size;					// The size of the demo in bytes.
	float			demotime;					// The current time in the demo.
	int				lastto;						// The player number/bitmask of the last player/players a demo message was directed at.
	int				lasttype;					// The type of the last demo message.

	frame_info_t	frame_info;					// Some info on the current frame.
	players_t		players[MAX_PLAYERS];		// Current frame playerinfo
	players_t		lf_players[MAX_PLAYERS];	// Last    frame playerinfo
	players_t		ltr_players[MAX_PLAYERS];	// Only used for debugging with -t
	fragstats_t		fragstats[MAX_PLAYERS];
	char			*sndlist[1024];
	server_t		serverinfo;
} mvd_info_t;

void MVD_Parser_LogEvent(int i, int type,char *string)
{
	// TODO : Log events.
}

void MVD_Parser_ParseServerinfo(char *key, char *value)
{
	if (!strcmp(key,"status"))
	{
		if (strcmp(value,"Countdown"))
		{
			// TODO : Fix this.
			//serverinfo.match_started = true;
		}
	}
}

void debug_printf(char *str, ...)
{
	va_list argptr;

	if (!cmdargs.debug)
		return;

	va_start(argptr, str);
	vprintf(str, argptr);
	va_end(argptr);
}

void MVD_Parser_ProcessStufftext(char *stufftext)
{

	if (strstr(stufftext,"fullserverinfo"))
	{
		//l_printf("fullserverinfo recieved\n %s \n", stufftext);
		//serverinfo.serverinfo = strdup(stufftext);
		// TODO : Fix this.
	}
}

//
// Initializes all buffers and structs before parsing an MVD.
//
static void MVD_Parser_Init(mvd_info_t *mvd)
{
	int x;

	memset(mvd, 0, sizeof(*mvd));

	for (x = 0; x < sizeof(*mvd->players); x++)
	{
		mvd->players[x].ping_lowest = 999;
		mvd->players[x].pl_lowest = 999;
		mvd->players[x].spectator = 1;
	}
}

//
// Reads a specified amount of bytes from the demo.
//
static void MVD_Parser_DemoRead(mvd_info_t *mvd, void *dest, unsigned int need, qbool peek)
{
	if ((long)((mvd->demo_ptr - mvd->demo_start) + need) > mvd->demo_size)
	{
		Sys_Error("MVD_Parser_DemoRead: Unexpected end of demo.\n");
	}

	memcpy(dest, mvd->demo_ptr, need);

	if (!peek)
	{
		mvd->demo_ptr += need;
	}
}

//
// Reads a new MVD frame.
//
static void MVD_Parser_ReadFrame(mvd_info_t *mvd)
{
	int i;
	byte mvd_time;
	byte message_type;
	byte c;

	// Check if we need to get more messages for now and if so read it
	// from the demo file and pass it on to the net channel.
	while (true)
	{
		// Read MVD time.
		MVD_Parser_DemoRead(mvd, &mvd_time, 1, false);

		mvd->demotime += (mvd_time * 0.001f);

		// Get the msg type.
		MVD_Parser_DemoRead(mvd, &c, 1, false);
		message_type = (c & 7);

		// QWD Only.
		if (message_type == dem_cmd)
		{
			// TODO : Fail! This should only appear in QWDs
			continue;
		}

		// MVD Only. These message types tells to which players the message is directed to.
		if (message_type >= dem_multiple && message_type <= dem_all)
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
					MVD_Parser_DemoRead(mvd, &i, sizeof(i), false);
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
					Sys_Error("MVD_Parser_ReadFrame: Unknown command type.\n");
					return;
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
			MVD_Parser_DemoRead(mvd, &net_message.cursize, 4, false);
			net_message.cursize = LittleLong(net_message.cursize);

			// The message was too big, stop playback.
			if (net_message.cursize > net_message.maxsize)
			{
				Sys_Error("MVD_Parser_ReadFrame: net_message.cursize > net_message.maxsize");
			}

			// Read the net message from the demo.
			net_message.data = Q_malloc(sizeof(byte) * net_message.cursize);
			MVD_Parser_DemoRead(mvd, net_message.data, net_message.cursize, false);

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

			return; // We just read something.
		}

		// Gets the sequence numbers for the netchan at the start of the demo.
		if (message_type == dem_set)
		{
			MVD_Parser_DemoRead(mvd, &i, sizeof(i), false);
			outgoing_sequence = LittleLong(i);

			MVD_Parser_DemoRead(mvd, &i, sizeof(i), false);
			incoming_sequence = LittleLong(i);

			continue;
		}

		// We should never get this far if the demo is ok.
		Sys_Error("MVD_Parser_ReadFrame: Corrupted demo.\n");
		return;
	}
}

//
// Reads and parses an MVD demo.
// mvdbuf = The demo buffer to read from.
// filelen = The length of the demo in bytes.
//
void MVD_Parser_StartParse(byte *mvdbuf, long filelen)
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
		MVD_Parser_ReadFrame(&mvd);

		// If a net message was read, parse it.
		if (net_message.data)
		{
			// TODO : Parse the server message.

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
}



