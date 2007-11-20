#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "maindef.h"
#include "netmsg_parser.h"
#include "net_msg.h"
#include "stats.h"
#include "endian.h"
#include "mvd_parser.h"

void debug_printf(char *str, ...)
{
	va_list argptr;

	if (!cmdargs.debug)
		return;

	va_start(argptr, str);
	vprintf(str, argptr);
	va_end(argptr);
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



