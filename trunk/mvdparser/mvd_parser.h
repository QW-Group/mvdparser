
#ifndef __MVD_PARSER_H__
#define __MVD_PARSER_H__

//
// Reads and parses an MVD demo.
// mvdbuf = The demo buffer to read from.
// filelen = The length of the demo in bytes.
//
void MVD_Parser_StartParse(byte *mvdbuf, long filelen);

#endif // __MVD_PARSER_H__
