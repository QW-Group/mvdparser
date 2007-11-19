#include <string.h>
#include "endian.h"
#include "maindef.h"
#include "mvd_parser.h"
#include "net_msg.h"
#include "netmsg_parser.h"

void NetMsg_Parser_LogEvent(int i, int type,char *string)
{
	// TODO : Log events.
}

static void NetMsg_Parser_ProcessStufftext(char *stufftext)
{
	if (strstr(stufftext, "fullserverinfo"))
	{
		//l_printf("fullserverinfo recieved\n %s \n", stufftext);
		//serverinfo.serverinfo = strdup(stufftext);
		// TODO : Fix this.
	}
}

static void NetMsg_Parser_ParseServerinfo(char *key, char *value)
{
	if (!strcmp(key, "status"))
	{
		if (strcmp(value, "Countdown"))
		{
			// TODO : Fix this.
			//serverinfo.match_started = true;
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
				int level = MSG_ReadByte();
				str = MSG_ReadString();

				break;
			}
			case svc_centerprint :
			{
				str = MSG_ReadString();
				break;
			}
			case svc_stufftext :
			{
				str = MSG_ReadString();
				break;
			}
			case svc_damage :
			{
				int i;
				vec3_t from;
				int armor = MSG_ReadByte();
				int blood = MSG_ReadByte();
				
				for (i = 0; i < 3; i++)
				{
					from[i] = MSG_ReadCoord();
				}

				break;
			}
			case svc_serverdata :
			{
				int i = 0;
				int protover	= MSG_ReadLong();
				int servercount = MSG_ReadLong();
				char *gamedir	= MSG_ReadString();
				float demotime	= MSG_ReadFloat();
				char *levelname = MSG_ReadString();

				for (i = 0; i < 10; i++)
				{
					MSG_ReadFloat(); // Move var.
				}

				break;
			}
			case svc_cdtrack :
			{
				MSG_ReadByte();
				break;
			}
			case svc_playerinfo :
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
						mvd->players[num].origin[i] = MSG_ReadCoord();
					}
				}

				for (i = 0; i < 3; i++)
				{
					if (flags & (DF_ANGLES << i))
					{
						MSG_ReadAngle16();
					}
				}

				if (flags & DF_MODEL)
				{
					MSG_ReadByte(); // Modelindex
				}

				if (flags & DF_SKINNUM)
				{
					MSG_ReadByte(); // skinnum
				}

				if (flags & DF_EFFECTS)
				{
					MSG_ReadByte(); // Modelindex
				}

				if (flags & DF_WEAPONFRAME)
				{
					mvd->players[num].weaponframe = MSG_ReadByte(); // Modelindex
				}

				break;
			}
			case svc_modellist :
			{
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

				break;
			}
			case svc_soundlist :
			{
				int soundcount = MSG_ReadByte();

				while (true)
				{
					str = MSG_ReadString();

					if (!str[0])
					{
						break;
					}

					soundcount++;
					// TODO: Save the list of sounds.
				}

				MSG_ReadByte(); // Ignore.

				break;
			}
			case svc_spawnstaticsound :
			{
				int i;

				for (i = 0; i < 3; i++)
				{
					MSG_ReadCoord();	// Location.
				}

				MSG_ReadByte();			// Number.
				MSG_ReadByte();			// Volume.
				MSG_ReadByte();			// Attentuion.

				break;
			}
			case svc_spawnbaseline :
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
				break;
			}
			case svc_updatefrags :
			{
				int pnum = MSG_ReadByte();					// Player.
				mvd->players[pnum].frags = MSG_ReadShort();	// Frags.
				break;
			}
			case svc_updateping :
			{
				int pnum = MSG_ReadByte();					// Player.
				mvd->players[pnum].ping = MSG_ReadShort();	// Ping.
				mvd->players[pnum].ping_average += mvd->players[pnum].ping;
				mvd->players[pnum].ping_count++;

				mvd->players[pnum].ping_highest = max(mvd->players[pnum].ping_highest, mvd->players[pnum].ping);
				mvd->players[pnum].ping_lowest  = min(mvd->players[pnum].ping_lowest, mvd->players[pnum].ping);

				break;
			}
			case svc_updatepl :
			{
				int pnum = MSG_ReadByte();				// Player.
				mvd->players[pnum].pl = MSG_ReadByte();	// Ping.
				mvd->players[pnum].pl_average += mvd->players[pnum].pl;
				mvd->players[pnum].pl_count++;

				mvd->players[pnum].pl_highest = max(mvd->players[pnum].pl_highest, mvd->players[pnum].pl);
				mvd->players[pnum].pl_lowest  = min(mvd->players[pnum].pl_lowest, mvd->players[pnum].pl);
				
				break;
			}
			case svc_updateentertime :
			{
				MSG_ReadByte();		// Player.
				MSG_ReadFloat();	// Time.
				break;
			}
			case svc_updateuserinfo :
			{
				int pnum = MSG_ReadByte ();					// Player.
				mvd->players[pnum].userid = MSG_ReadLong (); // Userid.

				str = MSG_ReadString(); // Userinfo string.

				if (!str[0])
				{
					break;
				}

				Q_free(mvd->players[pnum].userinfo);
				mvd->players[pnum].userinfo = Q_strdup(str);

				// TODO : Update name and such from the userinfo.

				break;
			}
			case svc_lightstyle :
			{
				MSG_ReadByte();		// Lightstyle count.
				MSG_ReadString();	// Lightstyle string.
				break;
			}
			case svc_bad :
			{
				Sys_PrintDebug(1, "Bad packet\n");
				break;
			}
			case svc_serverinfo :
			{
				char *key = MSG_ReadString();
				char *value = MSG_ReadString();

				// TODO : Parse server info.
				break;
			}
			case svc_packetentities :
			{
				NetMsg_Parser_ParsePacketEntities(mvd, false);
				break;
			}
			case svc_deltapacketentities :
			{
				NetMsg_Parser_ParsePacketEntities(mvd, true);
				break;
			}
			case svc_updatestatlong :
			{
				int stat = MSG_ReadByte();
				int value = MSG_ReadLong();
				// TODO : Update stats.
				break;
			}
			case svc_updatestat :
			{
				int stat = MSG_ReadByte();
				int value = MSG_ReadByte();
				// TODO : Update stats.
				break;
			}
			case svc_sound :
			{
				int i;
				int soundnum;
				vec3_t loc;
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
				
				for (i = 0; i < 3; i++)
					loc[i]= MSG_ReadCoord();

				// TODO : Parse the sound types.
				break;
			}
			case svc_temp_entity :
			{
				int i;
				int type = MSG_ReadByte();

				if (type == TE_GUNSHOT || type == TE_BLOOD)
				{
					MSG_ReadByte ();
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

				break;
			}
			case svc_setangle :
			{
				int i;
				MSG_ReadByte(); // Player number.

				// View angles.
				for (i = 0; i < 3; i++)
				{
					MSG_ReadAngle();
				}

				break;
			}
			case svc_setinfo :
			{
				int slot	= MSG_ReadByte();
				char *key	= MSG_ReadString();
				char *value = MSG_ReadString();
				
				// TODO : Save name and such.

				break;
			}
			case svc_muzzleflash :
			{
				MSG_ReadShort(); // Playernum.
				break;
			}
			case svc_smallkick :
			{
				// Nothing.
				break;
			}
			case svc_bigkick :
			{
				// Nothing.
				break;
			}
			case svc_intermission :
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

				break;
			}
			case svc_chokecount :
			{
				MSG_ReadByte();
				break;
			}
			case svc_spawnstatic :
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

				break;
			}
			case svc_foundsecret :
			{
				// Do nothing.
				break;
			}
			case svc_maxspeed :
			{
				MSG_ReadFloat();
				break;
			}
			case svc_nails2 :
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
				break;
			}
		}
	}
}


