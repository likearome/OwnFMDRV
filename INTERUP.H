// INTERUP.H
#ifndef _INTERUP_H_
#define _INTERUP_H_
#include "FMDRVDEF.H"

#define INTERRUPT_SUCCESS       (int8)(0)
#define INTERRUPT_FAIL          (int8)(0xFF)

#define INTERRUPT_NOCHAIN       (int8)(0)
#define INTERRUPT_NEXTCHAIN     (int8)(1)

#define NEWSTACKSZ 0x200	// 512 bytes

/* a few globals useful only for debug messages */
#if DEBUGLEVEL > 0
static unsigned short dbg_xpos = 0;
static unsigned short far* dbg_VGA = (unsigned short far*)(0xB8000000l);
static unsigned char dbg_hexc[16] = "0123456789ABCDEF";
#define dbg_startoffset 80*16
#endif

#define GLOB_DATOFF_PREV_66_HANDLER_SEG		0
#define GLOB_DATOFF_PREV_66_HANDLER_OFF		2
#define GLOB_DATOFF_PREV_1C_HANDLER_SEG		4
#define GLOB_DATOFF_PREV_1C_HANDLER_OFF		6
#define GLOB_DATOFF_PREV_21_HANDLER_SEG		8
#define GLOB_DATOFF_PREV_21_HANDLER_OFF		10
#define GLOB_DATOFF_PSPSEG					12

struct tsrshareddata {
	/*offset*/
	/*  0 */ unsigned short prev_fmdrv_handler_seg; /* seg:off of the previous 66 (FMDRV) handler */
	/*  2 */ unsigned short prev_fmdrv_handler_off; 
	/*  4 */ unsigned short prev_tick_handler_seg; /* seg:off of the previous 1C (CLOCK-TICK) handler */
	/*  6 */ unsigned short prev_tick_handler_off;
	/*  8 */ unsigned short prev_dos_handler_seg; /* seg:off of the previous 21 (DOS) handler */
	/* 10 */ unsigned short prev_dos_handler_off;

	/* 12 */ unsigned short pspseg;    /* segment of the program's PSP block */
};

extern struct tsrshareddata globData;

extern void begtextend(void);

extern int StartTSR(void);
extern int IsTSRInstalled(void);
extern int UnloadTSR(void);

#endif // _INTERUP_H_
