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

static void NetMsg_Parser_Parse_svc_updatestat(void)
{
	int stat = MSG_ReadByte();
	int value = MSG_ReadByte();
	// TODO : Update stats.
}

static void NetMsg_Parser_Parse_svc_setview(void)
{
}

static void NetMsg_Parser_Parse_svc_sound(void)
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
}

static void NetMsg_Parser_Parse_svc_print(void)
{
	int level = MSG_ReadByte();
	char *str = MSG_ReadString();
}

static void NetMsg_Parser_Parse_svc_stufftext(void)
{
	char *str = MSG_ReadString();
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

static void NetMsg_Parser_Parse_svc_serverdata(void)
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
}

static void NetMsg_Parser_Parse_svc_lightstyle(void)
{
	MSG_ReadByte();		// Lightstyle count.
	MSG_ReadString();	// Lightstyle string.
}

static void NetMsg_Parser_Parse_svc_updatefrags(mvd_info_t *mvd)
{
	int pnum = MSG_ReadByte();					// Player.
	mvd->players[pnum].frags = MSG_ReadShort();	// Frags.
}

static void NetMsg_Parser_Parse_svc_stopsound(void)
{
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
}

static void NetMsg_Parser_Parse_svc_setpause(void)
{
}

static void NetMsg_Parser_Parse_svc_centerprint(void)
{
	char *str = MSG_ReadString();
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

static void NetMsg_Parser_Parse_svc_updateentertime(void)
{
	MSG_ReadByte();		// Player.
	MSG_ReadFloat();	// Time.
}

static void NetMsg_Parser_Parse_svc_updatestatlong(void)
{
	int stat = MSG_ReadByte();
	int value = MSG_ReadLong();
	// TODO : Update stats.
}

static void NetMsg_Parser_Parse_svc_muzzleflash(void)
{
	MSG_ReadShort(); // Playernum.
}

static void NetMsg_Parser_Parse_svc_updateuserinfo(mvd_info_t *mvd)
{
	char *str;
	int pnum = MSG_ReadByte ();					// Player.
	mvd->players[pnum].userid = MSG_ReadLong (); // Userid.

	str = MSG_ReadString(); // Userinfo string.

	if (!str[0])
	{
		return;
	}

	Q_free(mvd->players[pnum].userinfo);
	mvd->players[pnum].userinfo = Q_strdup(str);

	// TODO : Update name and such from the userinfo.
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

static void NetMsg_Parser_Parse_svc_soundlist(void)
{
	char *str;
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
}

static void NetMsg_Parser_Parse_svc_packetentities(mvd_info_t *mvd)
{
	NetMsg_Parser_ParsePacketEntities(mvd, false);
}

static void NetMsg_Parser_Parse_svc_deltapacketentities(mvd_info_t *mvd)
{
	NetMsg_Parser_ParsePacketEntities(mvd, true);
}

static void NetMsg_Parser_Parse_svc_maxspeed(void)
{
	MSG_ReadFloat();
}

static void NetMsg_Parser_Parse_svc_entgravity(void)
{
}

static void NetMsg_Parser_Parse_svc_setinfo(void)
{
	int slot	= MSG_ReadByte();
	char *key	= MSG_ReadString();
	char *value = MSG_ReadString();
	
	// TODO : Save name and such.
}

static void NetMsg_Parser_Parse_svc_serverinfo(void)
{
	char *key = MSG_ReadString();
	char *value = MSG_ReadString();

	// TODO : Parse server info.
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
				NetMsg_Parser_Parse_svc_print();
				break;
			}
			case svc_centerprint :
			{
				NetMsg_Parser_Parse_svc_centerprint();
				break;
			}
			case svc_stufftext :
			{
				NetMsg_Parser_Parse_svc_stufftext();
				break;
			}
			case svc_damage :
			{
				NetMsg_Parser_Parse_svc_damage();
				break;
			}
			case svc_serverdata :
			{
				NetMsg_Parser_Parse_svc_serverdata();
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
				NetMsg_Parser_Parse_svc_soundlist();
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
				NetMsg_Parser_Parse_svc_updateentertime();
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
				NetMsg_Parser_Parse_svc_serverinfo();
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
				NetMsg_Parser_Parse_svc_updatestatlong();
				break;
			}
			case svc_updatestat :
			{
				NetMsg_Parser_Parse_svc_updatestat();
				break;
			}
			case svc_sound :
			{
				NetMsg_Parser_Parse_svc_sound();
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
				NetMsg_Parser_Parse_svc_setinfo();
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
				NetMsg_Parser_Parse_svc_maxspeed();
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


