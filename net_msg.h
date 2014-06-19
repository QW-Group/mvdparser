
#ifndef __NET_MSG_H__
#define __NET_MSG_H__

#include "qw_protocol.h"

extern int msg_readcount;
extern qbool msg_badread;
extern sizebuf_t	net_message;
extern int outgoing_sequence;	// Last outgoing packet sequence number.
extern int incoming_sequence;	// Last incoming packet sequence number.

void MSG_BeginReading (void);
int MSG_GetReadCount(void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);
char *MSG_ReadStringLine (void);
float MSG_ReadCoord (void);
float MSG_ReadAngle (void);
float MSG_ReadAngle16 (void);

#define CM_MSEC	(1 << 7) // same as CM_ANGLE2

void MSG_ReadDeltaUsercmd(usercmd_t *from, usercmd_t *move, int protoversion);

#endif // __NET_MSG_H__
