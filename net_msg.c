#include <string.h>
#include "maindef.h"
#include "endian.h"
#include "qw_protocol.h"
#include "net_msg.h"

sizebuf_t net_message;
int msg_readcount;
qbool msg_badread;

int outgoing_sequence;	// Last outgoing packet sequence number.
int incoming_sequence;	// Last incoming packet sequence number.

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

int MSG_GetReadCount(void)
{
	return msg_readcount;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int	c;

	if (msg_readcount + 1 > net_message.cursize) 
	{
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte (void)
{
	int	c;

	if (msg_readcount + 1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort (void)
{
	int	c;

	if (msg_readcount + 2 > net_message.cursize) 
	{
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount] + (net_message.data[msg_readcount + 1] << 8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void)
{
	int	c;

	if (msg_readcount + 4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
		+ (net_message.data[msg_readcount + 1] << 8)
		+ (net_message.data[msg_readcount + 2] << 16)
		+ (net_message.data[msg_readcount + 3] << 24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void)
{
	union 
	{
		byte b[4];
		float f;
		int	l;
	} dat;

	dat.b[0] = net_message.data[msg_readcount];
	dat.b[1] = net_message.data[msg_readcount + 1];
	dat.b[2] = net_message.data[msg_readcount + 2];
	dat.b[3] = net_message.data[msg_readcount + 3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (void)
{
	static char string[2048];
	unsigned int l;
	int c;

	l = 0;
	do {
		c = MSG_ReadByte ();

		if (c == 255) // skip these to avoid security problems
			continue; // with old clients and servers

		if (c == -1 || c == 0) // msg_badread or end of string
			break;

		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

char *MSG_ReadStringLine(void)
{
	static char	string[2048];
	unsigned int l;
	int c;

	l = 0;
	do 
	{
		c = MSG_ReadByte ();
		if (c == 255)
			continue;
		if (c == -1 || c == 0 || c == '\n')
			break;
		string[l] = c;
		l++;
	} while (l < sizeof(string) - 1);

	string[l] = 0;

	return string;
}

float MSG_ReadCoord (void)
{
	return MSG_ReadShort() * (1.0f / 8);
}

float MSG_ReadAngle (void)
{
	return MSG_ReadChar() * (360.0f / 256);
}

float MSG_ReadAngle16 (void)
{
	return MSG_ReadShort() * (360.0f / 65536);
}

void MSG_ReadDeltaUsercmd(usercmd_t *from, usercmd_t *move, int protoversion)
{
	int bits;

	memcpy (move, from, sizeof(*move));

	bits = MSG_ReadByte ();

	if (protoversion == 26) 
	{
		// read current angles
		if (bits & CM_ANGLE1)
			move->angles[0] = MSG_ReadAngle16 ();
		move->angles[1] = MSG_ReadAngle16 (); // always sent
		if (bits & CM_ANGLE3)
			move->angles[2] = MSG_ReadAngle16 ();

		// read movement
		if (bits & CM_FORWARD)
			move->forwardmove = MSG_ReadChar() << 3;
		if (bits & CM_SIDE)
			move->sidemove = MSG_ReadChar() << 3;
		if (bits & CM_UP)
			move->upmove = MSG_ReadChar() << 3;
	} 
	else 
	{
		// read current angles
		if (bits & CM_ANGLE1)
			move->angles[0] = MSG_ReadAngle16 ();
		if (bits & CM_ANGLE2)
			move->angles[1] = MSG_ReadAngle16 ();
		if (bits & CM_ANGLE3)
			move->angles[2] = MSG_ReadAngle16 ();

		// read movement
		if (bits & CM_FORWARD)
			move->forwardmove = MSG_ReadShort ();
		if (bits & CM_SIDE)
			move->sidemove = MSG_ReadShort ();
		if (bits & CM_UP)
			move->upmove = MSG_ReadShort ();
	}

	// read buttons
	if (bits & CM_BUTTONS)
		move->buttons = MSG_ReadByte ();

	if (bits & CM_IMPULSE)
		move->impulse = MSG_ReadByte ();

	// read time to run command
	if (protoversion == 26) 
	{
		if (bits & CM_MSEC)
			move->msec = MSG_ReadByte ();
	}
	else 
	{
		move->msec = MSG_ReadByte (); // always sent
	}
}
