// FMDRVDEF.H
#ifndef _FMDRVDEF_H_
#define _FMDRVDEF_H_
#include <stdarg.h>
#include <stdio.h>

#define TRUE                                    1
#define FALSE                                   0

#define HIBYTE(x) (((unsigned) (x) >> 8) & 0xff)
#define LOBYTE(x) (((unsigned)(x)) & 0xff)
#define REGPAK unsigned _es, unsigned _ds, unsigned _di, unsigned _si,\
               unsigned _bp, unsigned _sp, unsigned _bx, unsigned _dx,\
               unsigned _cx, unsigned _ax, unsigned _ip,unsigned  _cs,\
               unsigned _flags

typedef unsigned char       bool;
typedef char                int8;
typedef short               int16;
typedef long                int32;

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned long       uint32;

#endif // _FMDRVDEF_H_
