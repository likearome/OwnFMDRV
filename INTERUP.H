// INTERUP.H
#ifndef _INTERUP_H_
#define _INTERUP_H_
#include "FMDRVDEF.H"

// PlayStep에 적용된 비트는 2 뿐이므로 4개 이상 늘어나지 않도록 주의한다.
enum PlayStep
{
	PLAYSTEP_PREPARE = 0,
	PLAYSTEP_OPEN = 0,
	PLAYSTEP_MAIN,
	PLAYSTEP_END,
	PLAYSTEP_MAX
};

#define INTERRUPT_SUCCESS					(int8)(0)
#define INTERRUPT_FAIL						(int8)(-1)

#define INTERRUPT_NOCHAIN					(int8)(0)
#define INTERRUPT_NEXTCHAIN					(int8)(1)

#define NEWSTACKSZ							0x200	// 512 bytes

#define GLOB_DATOFF_PREV_FMDRV_HANDLER_SEG		0
#define GLOB_DATOFF_PREV_FMDRV_HANDLER_OFF		2
#define GLOB_DATOFF_PREV_TICK_HANDLER_SEG		4
#define GLOB_DATOFF_PREV_TICK_HANDLER_OFF		6
#define GLOB_DATOFF_PREV_RTC_HANDLER_SEG		8
#define GLOB_DATOFF_PREV_RTC_HANDLER_OFF		10
#define GLOB_DATOFF_PREV_DOS_HANDLER_SEG		12
#define GLOB_DATOFF_PREV_DOS_HANDLER_OFF		14
#define GLOB_DATOFF_PSPSEG						16

struct tsrshareddata {
	/*offset*/
	/*  0 */ unsigned short prev_fmdrv_handler_seg; /* seg:off of the previous 66 (FMDRV) handler */
	/*  2 */ unsigned short prev_fmdrv_handler_off;
	/*  4 */ unsigned short prev_tick_handler_seg; /* seg:off of the previous 08 (TICK) handler */
	/*  6 */ unsigned short prev_tick_handler_off;
	/*  8 */ unsigned short prev_rtc_handler_seg; /* seg:off of the previous 70 (RTC) handler */
	/* 10 */ unsigned short prev_rtc_handler_off;
	/* 12 */ unsigned short prev_dos_handler_seg; /* seg:off of the previous 21 (DOS) handler */
	/* 14 */ unsigned short prev_dos_handler_off;

	/* 16 */ unsigned short pspseg;    /* segment of the program's PSP block */
};

extern struct tsrshareddata globData;

extern void begtextend(void);

extern int8 StartTSR(void);
extern int8 IsTSRInstalled(void);
extern int8 UnloadTSR(void);

#endif // _INTERUP_H_
