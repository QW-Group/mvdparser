
#ifndef __MVD_PARSER_H__
#define __MVD_PARSER_H__

//
// Reads and parses an MVD demo.
// mvdbuf = The demo buffer to read from.
// filelen = The length of the demo in bytes.
//
qbool MVD_Parser_StartParse(char *demoname, byte *mvdbuf, long filelen);

void LogVarHashTable_Init(void);

#endif // __MVD_PARSER_H__
