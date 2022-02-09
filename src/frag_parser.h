
#ifndef __FRAG_PARSER_H__
#define __FRAG_PARSER_H__

#include "maindef.h"
#include "qw_protocol.h"
#include "mvd_parser.h"

void Frags_Parse(mvd_info_t *mvd, char *fragmessage, int level);
qbool LoadFragFile(char *filename, qbool quiet);

#endif //__FRAG_PARSER_H__


