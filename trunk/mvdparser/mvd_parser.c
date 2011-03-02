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
#include "logger.h"

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
			
			// Log a death event.
			Log_Event(&logger, mvd, LOG_DEATH, cf->pnum);
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
				strlcpy(cf->last_pickedup_item, Armor_Name(armor), sizeof(cf->last_pickedup_item));
				Log_Event(&logger, mvd, LOG_ITEMPICKUP, cf->pnum);
			}
		}

		if (lf->pent_count < cf->pent_count)
		{
			strlcpy(cf->last_pickedup_item, "PENT", sizeof(cf->last_pickedup_item));
			Log_Event(&logger, mvd, LOG_ITEMPICKUP, cf->pnum);
		}

		if (lf->quad_count < cf->quad_count)
		{
			strlcpy(cf->last_pickedup_item, "QUAD", sizeof(cf->last_pickedup_item));
			Log_Event(&logger, mvd, LOG_ITEMPICKUP, cf->pnum);
		}

		if (lf->ring_count < cf->ring_count)
		{
			strlcpy(cf->last_pickedup_item, "RING", sizeof(cf->last_pickedup_item));
			Log_Event(&logger, mvd, LOG_ITEMPICKUP, cf->pnum);
		}

		// Weapons.
		for (j = 0; j <= 5; j++)
		{
			if (lf->weapon_count[j] < cf->weapon_count[j])
			{
				strlcpy(cf->last_pickedup_item, WeaponNumToName(j + SSG_NUM), sizeof(cf->last_pickedup_item));
				Log_Event(&logger, mvd, LOG_ITEMPICKUP, cf->pnum);
			}
		}

		// Calculate player speed if alive.
		if ((lf->stats[STAT_HEALTH] > 0) && (time_since_last_frame > 0))
		{
			vec3_t dv;
			float distance;
			float speed;

			// Get the distance vector between the current and last frames origin of the player.
			//VectorSubtract(cf->origin, lf->origin, dv);
			VectorSubtract(cf->origin, cf->prev_origin, dv);

			// Log a movement if the origin has changed since last.
			if ((dv[0] != 0) || (dv[1] != 0) || (dv[2] != 0))
			{
				Log_Event(&logger, mvd, LOG_MOVE, cf->pnum);
			}

			// Calculate the distance moved.
			distance = (float)sqrt(pow(dv[0], 2) + pow(dv[1], 2) + pow(dv[2], 2));

			// If we teleport we'll get really high speeds since we moved so far in a short time, cap this :S
			if (distance < 150)
			{
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
qbool MVD_Parser_StartParse(char *demopath, byte *mvdbuf, long filelen)
{
	mvd_info_t mvd;
	double start_time = Sys_DoubleTime();
	double end_time	= 0.0;

	// Init the mvd struct.
	MVD_Parser_Init(&mvd);

	mvd.demo_name = demopath;

	// Set the demo pointer.
	mvd.demo_start = mvdbuf;
	mvd.demo_ptr = mvdbuf;
	mvd.demo_size = filelen;

	memset(&net_message, 0, sizeof(net_message));
	net_message.maxsize = 8192;

	Log_Event(&logger, &mvd, LOG_DEMOSTART, -1);

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
			MVD_Parser_StatsGather(&mvd);
		}

		// Save the current player infos for next frame.
		memcpy(mvd.lf_players, mvd.players, sizeof(players_t) * MAX_PLAYERS);
	}

	// If we haven't found the match end yet so raise it now.
	if (!mvd.serverinfo.match_ended)
	{
		int i;

		Log_Event(&logger, &mvd, LOG_MATCHEND, -1);

		// HACK :( 
		for (i = 0; i < MAX_PLAYERS; i++)
		{
			if (!PLAYER_ISVALID(&mvd.players[i]))
			{
				continue;
			}

			Log_Event(&logger, &mvd, LOG_MATCHEND_ALL, i);
		}
	}

	Log_Event(&logger, &mvd, LOG_DEMOEND, -1);

	Log_OutputFilesHashTable_Clear(&logger);

	end_time = Sys_DoubleTime() - start_time;

	Sys_PrintDebug(1, "MVD_Parser_StartParse: Finished parsing in %g seconds.\n", end_time);

	return true;
}



