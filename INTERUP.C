#include <dos.h>
#include <bios.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include "FMDRVDEF.H"
#include "DOSUTIL.H"

#define FMDRV_INTERRUPT			0x66
#define TICK_INTERRUPT			0x08    // 0x08은 실제 타이머 인터럽트. TSR은 08을 직접 후킹해야 한다.
#define DOS_INTERRUPT			0x21

#define TICK_SECONDS			60		// 60hz 기준. 60이면 1초대기

#define FMDRV_MARKER_OFFSET		(0x08L)
#define FMDRV_MARKER_SIZE		7

// 문자열은 반드시 _DATA 세그먼트에 저장되기 때문에,
// TSR에서 메모리를 컷할 때 잘려나가버린다. (RDATA에 문자열 저장 불가)
// 그러므로 문자열을 포인터가 아닌! 전역 배열변수에서 저장할 수 있도록 하여야 한다.
#define FMDRV_MARKER			{'O', 'P', 'L', 'D', 'R', 'V', '\0'}
#define SBDRV_MARKER			{'S', 'B', ' ', 'D', 'R', 'V', '\0'}

#define FMDRV_PASSTHROUGH		(0)
#define FMDRV_NO_PASSTHROUGH	(1)

#define DOS_SETINTVEC			0x25
#define DOS_FOPEN				0x3D
#define DOS_FSEEK				0x42
#define DOS_EXEC				0x4B
#define DOS_UNLOADTSR			0xFF

enum FMDRV_COMMAND_TYPE
{
	FMDRV_COMMAND_0 = 0,
	FMDRV_INIT = FMDRV_COMMAND_0,
	FMDRV_COMMAND_1,
	FMDRV_PLAY = FMDRV_COMMAND_1,
	FMDRV_COMMAND_2,
	FMDRV_STOP = FMDRV_COMMAND_2,
	FMDRV_COMMAND_3,
	FMDRV_GETSTATUS = FMDRV_COMMAND_3,
	FMDRV_COMMAND_4,
	FMDRV_SETLOOP = FMDRV_COMMAND_4,
	FMDRV_COMMAND_5,
	FMDRV_COMMAND_6,
	FMDRV_COMMAND_7,
	FMDRV_COMMAND_8,
	FMDRV_COMMAND_9,			
	FMDRV_COMMAND_INSTALL_TIMER_CALLBACK = FMDRV_COMMAND_9,	// 타이머 콜백 함수 등록 (AX:DX)

	// 효과음 재생용 함수
	FMDRV_COMMAND_10 = 0x10,	// 효과음 INIT
	FMDRV_COMMAND_11,			// 효과음 PLAY
	FMDRV_COMMAND_12,			// 효과음 STOP
	FMDRV_COMMAND_13,			// 효과음 GETSTATUS

	// 70~80 명령어는 다음 핸들러가 있을 경우, 다음 핸들러로 체이닝된다.
	FMDRV_COMMAND_80 = 0x80,
	FMDRV_COMMAND_81,

	FMDRV_COMMAND_F0 = 0xF0,
	FMDRV_COMMAND_CHECK_TIMER_1 = FMDRV_COMMAND_F0,		// FMDRV가 후킹한 08번 틱 인터럽트 내부의 1번째 타이머 현재 남은 값(시간)을 리턴해주는 함수
	FMDRV_COMMAND_F1,
	FMDRV_COMMAND_CHECK_TIMER_2 = FMDRV_COMMAND_F1,		// FMDRV가 후킹한 08번 틱 인터럽트 내부의 2번째 타이머 현재 남은 값(시간)을 리턴해주는 함수

	// FMDRV.COM이 처음 램상주될 때, 이미 램상주된 FMDRV.COM이 있으면 0xFE 함수를 호출하는 것으로 보임
	// 내부 타이머 파라미터들을 전달받음
	FMDRV_COMMAND_FE = 0xFE,
	FMDRV_CHECK_TIMER = FMDRV_COMMAND_FE,

	// FMDRV.COM을 종료하기 직전에 호출되는 함수
	FMDRV_COMMAND_FF = 0xFF,
	FMDRV_TERMINATE = FMDRV_COMMAND_FF,
};

typedef struct
{
	uint8 isFMDRVSet : 1;
	uint8 isTickSet : 1;

	uint8 isCDLoop : 1;
	uint8 isCDPlay : 1;
	uint8 isFMPlay : 1;

	uint8 isBufferChangeStyleExist : 1;
	uint8 curPlayStep : 2;
} LogicStatus;

/////////////////////////////////////////
// 이 코드는 메모리에 남는다.
#pragma code_seg(BEGTEXT, CODE)
#pragma data_seg(RESDATA, RDATA)

#include "INTERUP.H"
#include "CDROMDRV.H"
#include "chint.h"

struct tsrshareddata globData;

LogicStatus logicStatus;

// cd info
int8 cddrive = 0;
int8 playMargin = 0;
uint8 orginalCDVolume = 0;

// play info
int8 volatile bufChgFMTrack = -1;
int8 volatile FMPassThrough = 0;

// tick timer
enum TickAlarmType
{
	TICKALARM_STOPMUSIC = 0,
	TICKALARM_CHECKBUSY,
	TICKALARM_MAX
};
uint16 volatile tickAlarm[TICKALARM_MAX] = { 0, };

// 트랙 데이터 정보
int8 isBufferChangeStyleExist;
int8 isBufferChangeStyle[PLAYSTEP_MAX];
char openMusicFileName[20];
char mainMusicFileName[20];
char endMusicFileName[20];
uint16 openMusicFileHandle;
uint16 mainMusicFileHandle;
uint16 endMusicFileHandle;
char openExeFileName[20];
char mainExeFileName[20];
char endExeFileName[20];

CD_TMSF trackMapOpening[MAX_CDAUDIOTRACK + 1];	// 더 줄일 수 있을 지 모르겠는데 항유기 등 특수구조에서는 또 쉽지 않음
CD_TMSF trackMap[MAX_CDAUDIOTRACK + 1];
CD_TMSF trackMapEnding[MAX_CDAUDIOTRACK + 1];	// 더 줄일 수 있을 지 모르겠는데 항유기 등 특수구조에서는 또 쉽지 않음

uint8 fmTrackSongNumOpening = 0;
uint8 fmTrackSongNum = 0;
uint8 fmTrackSongNumEnding = 0;

uint32 fmTrackOffsetOpening[MAX_CDAUDIOTRACK + 1];	// 더 줄일 수 있을 지 모르겠는데 항유기 등 특수구조에서는 또 쉽지 않음
uint32 fmTrackOffset[MAX_CDAUDIOTRACK + 1];
uint32 fmTrackOffsetEnding[MAX_CDAUDIOTRACK + 1];	// 더 줄일 수 있을 지 모르겠는데 항유기 등 특수구조에서는 또 쉽지 않음

////////////////////////////////////////
// Interrupt parameters
uint8 far* dosActiveFlag;
uint8 skipDOSChain = FALSE;
uint8 terminateTSR = FALSE;

const char FMDRV_Marker_Str[FMDRV_MARKER_SIZE] = FMDRV_MARKER;
const char SBDRV_Marker_Str[FMDRV_MARKER_SIZE] = SBDRV_MARKER;

// 유틸리티 함수
// memcpy
static void __declspec(naked) mymemcpy(void far* dst, void far* src, uint16 len) {
	(void)dst;
	(void)src;
	(void)len;
	_asm
	{
		//스택에 레지스터와 플래그 값을 보존한다.
		push ds				// 우선 작업 전에 ds 를 보존한다.
		pushf
		cld                // si/di 증가쪽 플래그로 세팅
		mov ds, dx         // 소스쪽 세그먼트를 DS에 덮어쓴다.
		shr cx, 1          // 2바이트씩 복사(movsw)할 것이므로 복사할 크기를 /2로 나눈다.
		rep movsw          // 2바이트(1워드)씩, dx:si를 es:di에 복사한다.
		adc cx, cx         // cx에 나머지를 구한다.
		rep movsb          // 나머지만큼 1바이트씩 복사(movsb)한다.

		// 보존했던 레지스터/플래그와 ds를 복원한다.
		popf
		pop ds
		ret
	}
}
#pragma aux mymemcpy parm [es di] [dx si] [cx] modify exact [cx di si] nomemory;

//// strlen
//static uint16 __declspec(naked) mystrlen(void far* s) {
//	(void)s;	// es:di
//
//	_asm 
//	{
//		//스택에 레지스터와 플래그 값을 보존한다.
//		pushf
//		cld                // si/di 증가쪽 플래그로 세팅
//		mov al, 0          // Zero 터미네이터 세팅
//		mov cx, 0xFFFF     // CX에 uint16 최대 길이값을 세팅
//		repne scasb        // 문자열에서 Zero 터미네이터(NULL)를 탐색
//		neg cx             // 위 명령에 의해 얻은 CX값을 (-CX - 2)로 계산하면 문자열 길이를 얻는다.
//		dec cx
//		dec cx
//
//		// 보존했던 레지스터/플래그를 복원한다.
//		popf
//		ret
//	}
//}
//#pragma aux mystrlen parm [es di] value [cx] modify exact [ax cx di] nomemory;

static int8 mystrstr(char far* srcFilePtr, char far* dstFilePtr)
{
	char far* srcFilePtrBegin = 0;
	while (*srcFilePtr)
	{
		srcFilePtrBegin = srcFilePtr;
		while (*srcFilePtr && *dstFilePtr && *srcFilePtr == *dstFilePtr)
		{
			srcFilePtr++;
			dstFilePtr++;
		}
		if (!*dstFilePtr)
		{
			return 0;
		}
		srcFilePtr = srcFilePtrBegin + 1;
	}

	return -1;
}

static int mystrcmp(const char far* s, const char far* t)
{
	for (; *s == *t; s++, t++)
		if (*s == '\0')
			return(0);
	return (int)(*s - *t);
}

//static void __declspec(naked) (__interrupt __far* mygetvect(int8 interruptnum))()
//{
//	(void)interruptnum;
//
//	_asm
//	{
//		mov ah, 35h
//		int 21h
//		ret
//	}
//}
//#pragma aux mygetvect parm [al] value [es bx] modify exact [ax es bx] nomemory;
//
//static void __declspec(naked) mysetvect(int8 interruptnum, void (__interrupt __far* vect)())
//{
//	(void)interruptnum;
//	(void)vect;
//
//	_asm
//	{
//		push ds
//		mov ds, cx
//		mov ah, 25h
//		int 21h
//		pop ds
//		ret
//	}
//}
//#pragma aux mysetvect parm [ax cx dx] modify exact [ax] nomemory;

// allocseg로 확보한 세그먼트를 free한다.
static uint16 freeseg(uint16 segm)
{
	uint16 retval = 0;
	_asm
	{
		mov ah, 49h   // 49h 세그먼트 free MS-DOS 2+
		mov es, segm  // 주어진 세그먼트 값을 es에 입력
		int 21h
		jc failed
		xor ax, ax
		failed :
		mov retval, ax
	}
	return retval;
}

////////////////////////////////////////
// 인터럽트 핸들러에서 사용하기 위한 스택
static uint8 FMDRVIntStack[NEWSTACKSZ];
static uint8 DOSIntStack[NEWSTACKSZ];

// 기존 DOS 스택 위치
uint16 globFMDRVIntOldstackSeg;
uint16 globFMDRVIntOldstackOff;
uint16 globDOSIntOldstackSeg;
uint16 globDOSIntOldstackOff;

/* an INTPACK structure used to store registers as set when INT is called */
union INTPACK globFMDRVIntregs;
union INTPACK globTickIntregs;
union INTPACK globDOSIntregs;

inline void FMDRVReturn(int8 highbyte, int8 lowbyte)
{
	globFMDRVIntregs.w.ax = ((highbyte << 8) & 0xFF00) | lowbyte;
}

void ProcessFMDRVInt(void)
{
	static int8 curCDPlayTrack = 0;
	static int8 curFMPlayTrack = 0;

	static uint32 fromSector = 0;
	static uint32 toSector = 0;

	static uint16 stopDelay = 0;
	static uint16 lastStopMusicTickAlarm = 0;

	static int8 loopDelay = FALSE;

	uint8 register command;
	uint8 register data;
	bool diffTrack;
	bool diffSector;

	CD_TMSF* curTrackMap = NULL;

	FMPassThrough = FMDRV_PASSTHROUGH;

	command = globFMDRVIntregs.h.ah;
	data = globFMDRVIntregs.h.al;

	switch (command)
	{
	case FMDRV_PLAY:
		// FMTrack을 저장해둔다.
		curFMPlayTrack = data;
		if (bufChgFMTrack >= 0)
		{
			curFMPlayTrack = bufChgFMTrack;
			//bufChgFMTrack = -1;
		}
		switch (logicStatus.curPlayStep)
		{
		case PLAYSTEP_OPEN: curTrackMap = trackMapOpening; break;
		case PLAYSTEP_MAIN: curTrackMap = trackMap; break;
		case PLAYSTEP_END: curTrackMap = trackMapEnding; break;
		default: curTrackMap = NULL; break;
		}

		if (NULL != curTrackMap)
		{
			diffTrack = (curFMPlayTrack != curTrackMap[curFMPlayTrack].track);
			curCDPlayTrack = curTrackMap[curFMPlayTrack].track;

			diffSector = (fromSector != curTrackMap[curFMPlayTrack].fromSector);
			fromSector = curTrackMap[curFMPlayTrack].fromSector;

			toSector = curTrackMap[curFMPlayTrack].toSector;
		}

		if (curCDPlayTrack > 0 && !diffTrack && !diffSector)
		{
			// 현재 연주중인 곡과 같은 곡일 때에는 계속 연주하게 놔둔다.
			FMPassThrough = FMDRV_NO_PASSTHROUGH;
			FMDRVReturn(curFMPlayTrack, TRUE);
		}
		else if (curCDPlayTrack > 0)
		{
			// 새로운 CD트랙을 연주한다.
			// 이를 위해 변수들을 모두 초기화한다.
			logicStatus.isCDLoop = FALSE;	// 루프 플래그도 끈다.
			logicStatus.isFMPlay = FALSE;	// FM플레이도 끈다.
			FMPassThrough = FMDRV_NO_PASSTHROUGH;

			// FM 사운드를 끈다.
			{
				void (__interrupt __far * oldFmdrvInt)() = _MK_FP(globData.prev_fmdrv_handler_seg, globData.prev_fmdrv_handler_off);
				_asm
				{
					push ax
					mov ax, 0x200	// AH: 0x02 (FMDRV_STOP), AL: 0x00 (지금 즉시 종료)
					pushf
					call dword ptr oldFmdrvInt
					pop ax
				}
			}

			CDAudio_PlaySector(cddrive, fromSector + playMargin, toSector);
			CDAudio_SetVolume(cddrive, orginalCDVolume);
			logicStatus.isCDPlay = TRUE;

			FMDRVReturn(curFMPlayTrack, TRUE);
		}
		else
		{
			// 매핑이 없으므로 FM 사운드를 재생한다.
			if (logicStatus.isCDPlay)
			{
				CDAudio_Stop(cddrive);
				CDAudio_SetVolume(cddrive, 0);
				logicStatus.isCDPlay = FALSE;
				logicStatus.isCDLoop = FALSE;
			}

			curFMPlayTrack = data;
			logicStatus.isFMPlay = TRUE;
			FMPassThrough = FMDRV_PASSTHROUGH;
		}
		break;
	case FMDRV_STOP:
		_asm
		{
			jmp SKIPTSRSIG
			TSRSIG db 'STOP'
			SKIPTSRSIG:
		}
		// 연주 종료
		if (logicStatus.isCDPlay)
		{
			//FMPassThrough = FMDRV_NO_PASSTHROUGH;
			if (0 == data)
			{
				// AL(data)값이 0이면 지금 즉시 종료이다.
				CDAudio_Stop(cddrive);
				CDAudio_SetVolume(cddrive, 0);
				logicStatus.isCDPlay = FALSE;
				logicStatus.isCDLoop = FALSE;
			}
			else
			{
				// data 카운트만큼 Fade-out을 연출하고 종료한다.
				// 틱 타이머를 활성화한다.
				stopDelay = data * 4;
				tickAlarm[TICKALARM_STOPMUSIC] = stopDelay;
				lastStopMusicTickAlarm = tickAlarm[TICKALARM_STOPMUSIC];
			}
			FMDRVReturn(curFMPlayTrack, FALSE);
		}
		break;
	case FMDRV_GETSTATUS:
		// Fade-out 같은 볼륨 조절 등에서 사용되는 곳이다.
		if (logicStatus.isCDPlay)
		{
			if (0 != stopDelay)
			{
				// 노래가 연주중이지만 Stop 싸인은 왔음
				if (0 < tickAlarm[TICKALARM_STOPMUSIC])
				{
					// 볼륨을 서서히 낮춘다.
					if (lastStopMusicTickAlarm != tickAlarm[TICKALARM_STOPMUSIC])
					{
						uint16 newVol = (uint16)orginalCDVolume * tickAlarm[TICKALARM_STOPMUSIC] / stopDelay;
						CDAudio_SetVolume(cddrive, newVol);
						lastStopMusicTickAlarm = tickAlarm[TICKALARM_STOPMUSIC];
					}

					FMPassThrough = FMDRV_NO_PASSTHROUGH;
					FMDRVReturn(curFMPlayTrack, TRUE);	// 노래가 아직 연주중
				}
				else
				{
					CDAudio_Stop(cddrive);
					CDAudio_SetVolume(cddrive, 0);
					stopDelay = 0;
					lastStopMusicTickAlarm = 0;
					logicStatus.isCDPlay = FALSE;
					logicStatus.isCDLoop = FALSE;

					FMPassThrough = FMDRV_NO_PASSTHROUGH;
					FMDRVReturn(curFMPlayTrack, FALSE);	// 노래가 연주중이지 않음
				}
			}
			else
			{
				// Stop 싸인이 오지 않았으며 노래가 정상 연주중임
				FMPassThrough = FMDRV_NO_PASSTHROUGH;
				FMDRVReturn(curFMPlayTrack, TRUE);	// 노래가 아직 연주중
			}
		}
		else if(FALSE == logicStatus.isFMPlay)
		{
			// CD가 연주중이지 않음
			// 우리가 응답하고 FMDRV에는 전달하지 않는다.
			FMPassThrough = FMDRV_NO_PASSTHROUGH;
			FMDRVReturn(curFMPlayTrack, FALSE);	// 노래가 연주중이지 않음
		}
		else
		{
			// FM음악이 연주중이므로 FMDRV에 전달한다.
			// Pass Through
			FMPassThrough = FMDRV_PASSTHROUGH;
		}
		break;
	case FMDRV_SETLOOP:
		if (logicStatus.isCDPlay)
		{
			logicStatus.isCDLoop = data;
			FMPassThrough = FMDRV_NO_PASSTHROUGH;

			FMDRVReturn(curFMPlayTrack, TRUE);
		}
		break;
	case FMDRV_COMMAND_F0:
	case FMDRV_COMMAND_F1:
		// 노래가 시작되면 항상 호출되는 타이머 신호
		// 여기서 노래 루프가 가능할 것으로 예상된다.
		// CD가 바쁘지 않을 때(연주가 멈췄을 때)
		if (logicStatus.isCDPlay && 0 == tickAlarm[TICKALARM_CHECKBUSY])
		{
			tickAlarm[TICKALARM_CHECKBUSY] = TICK_SECONDS;
			if (CDAUDIO_BUSY != CDAudio_CheckCDBusy(cddrive))
			{
				if (logicStatus.isCDLoop)
				{
					if (FALSE == loopDelay)
					{
						// 지정된 시간만큼 한번 더 쉬었다가 연주한다.
						loopDelay = TRUE;
					}
					else
					{
						loopDelay = FALSE;
						// 루프가 설정돼 있다면 다시 연주를 시작해준다.
						CDAudio_PlaySector(cddrive, fromSector + playMargin, toSector);
						CDAudio_SetVolume(cddrive, orginalCDVolume);
					}
				}
				else
				{
					// 루프가 아닐 경우에는 STOP한다.
					CDAudio_Stop(cddrive);
					CDAudio_SetVolume(cddrive, 0);
					logicStatus.isCDLoop = FALSE;
					logicStatus.isCDPlay = FALSE;
				}
			}
		}
		break;
		// FMDRV.COM이 내려가기 시작할 때 호출되는 인터럽트
		// 우리도 미리 준비가 필요하다.
	case FMDRV_TERMINATE:
		if (logicStatus.isCDPlay)
		{
			CDAudio_SetVolume(cddrive, 0);
			CDAudio_Stop(cddrive);
			CDAudio_SetVolume(cddrive, 0xFF);	// CD 기본 볼륨(최대)로 돌려놓는다.
		}
		break;
		// 아래 명령어는 그대로 넘겨준다.
		// 의도치 않은 오동작을 막기 위해 아예 주석처리한다.
	//case FMDRV_INIT:
	//case FMDRV_COMMAND_5:
	//case FMDRV_COMMAND_6:
	//case FMDRV_COMMAND_7:
	//case FMDRV_COMMAND_8:
	//case FMDRV_COMMAND_9:
	//case FMDRV_COMMAND_10:
	//case FMDRV_COMMAND_11:
	//case FMDRV_COMMAND_12:
	//case FMDRV_COMMAND_13:
	//case FMDRV_COMMAND_80:
	//case FMDRV_COMMAND_81:
	//case FMDRV_COMMAND_FE:
	default:
		break;
	}
}

void __interrupt __far MyFMDRVInterrupt(union INTPACK r)
{
	// ### 아래에서 스택 포인터를 바꾸기 때문에,
	// 여기에 로컬 변수(스택할당)을 선언한 뒤에 스택 포인터 바꾸고 난 위치에서 사용하면 안된다.
	// 그리고 DS도 교체한다.
	_asm
	{
		push ax         // AX 보존
		mov ax, ds;

		// CS:glob_newds에 보존된 우리 데이터 세그먼트를 ds에 덮어쓰기
		mov ax, cs:glob_newds
		mov ds, ax

		pop ax          // AX 복원
	}

	switch (r.h.ah)	// COMMAND
	{
	case FMDRV_PLAY:
	case FMDRV_STOP:
	case FMDRV_GETSTATUS:
	case FMDRV_SETLOOP:
	case FMDRV_TERMINATE:
	case FMDRV_COMMAND_F0:
	case FMDRV_COMMAND_F1:
		break;
	default:
		FMPassThrough = FMDRV_PASSTHROUGH;
		goto CHAINTOPREVHANDLER;
	}

	// 파라미터로 입력받은 (실제로는 스택에 저장된) 인터럽트 레지스터 상태값 r을 별도 전역변수에 보존한다.
	// 그래야 이 인터럽트를 실행하고 바뀐 레지스터값이 아닌, 첫 호출자(caller)가 보냈던 인터럽트를 그대로
	// 체인된 다음 인터럽트에 보내줄 수 있다.
	mymemcpy(&globFMDRVIntregs, &r, sizeof(union INTPACK));
	// 스택 포인터를 전역변수 공간으로 이동한다.
	_asm
	{
		cli //인터럽트를 중지하여, 다른 인터럽트가 스택 변환중 문제가 발생하지 않도록 조치한다.
		mov globFMDRVIntOldstackSeg, SS
		mov globFMDRVIntOldstackOff, SP

		// 현재 DS를 스택 세그먼트에 덮어쓴다.
		mov ax, ds
		mov ss, ax

		// 스택 포인터에 newstack의 맨 뒷부분 바로 앞 (+NEWSTACKSZ-2) 위치를 덮어쓴다.
		mov sp, offset FMDRVIntStack + NEWSTACKSZ - 2
		sti // 인터럽트를 재개한다.
	}

	ProcessFMDRVInt();

	// 스택을 원래로 돌린다.
	_asm
	{
		cli //인터럽트를 중지하여, 다른 인터럽트가 스택 변환중 문제가 발생하지 않도록 조치한다.

		// 원래의 스택 포인터를 반환한다.
		mov SS, globFMDRVIntOldstackSeg
		mov SP, globFMDRVIntOldstackOff
		sti // 인터럽트를 재개한다.
	}

	// r의 레지스터를 모두 복원하여 이 값이 함수를 빠져나간 뒤에 정상적으로 세팅될 수 있도록 한다.
	mymemcpy(&r, &globFMDRVIntregs, sizeof(union INTPACK));
CHAINTOPREVHANDLER:
	if (FMDRV_PASSTHROUGH == FMPassThrough)
	{
		_mvchain_intr(MK_FP(globData.prev_fmdrv_handler_seg, globData.prev_fmdrv_handler_off));
	}
}

void __interrupt __far MyTickInterrupt(union INTPACK r)
{
	static uint8 carryDetect = 0;
	(void)r;

	_asm
	{
		push ax         // AX 보존
		mov ax, ds;

		// CS:glob_newds에 보존된 우리 데이터 세그먼트를 ds에 덮어쓰기
		mov ax, cs:glob_newds
		mov ds, ax

		pop ax          // AX 복원
	}

	// 틱 인터럽트에서 할 일은 오로지 틱 카운터 계산 뿐이다.
	// 따라서 다른 인터럽트와 달리 여기서는 스택을 교체하거나 하지 않는다.
#define TICK_DIVIDER	0x35
	if ((uint16)(carryDetect + TICK_DIVIDER) > 0xFF)
	{
		// 캐리 발생
		int i;
		for (i = 0; i < TICKALARM_MAX; ++i)
		{
			if (tickAlarm[i] > 0) tickAlarm[i]--;
		}
	}

	// FMDRV가 커스터마이즈한 틱 주기는 1193181/4096 = 291.304 hz 이다.
	// 이를 256/ = 약7.3으로 나누어서 40hz를 사용한다.
	carryDetect = (carryDetect + TICK_DIVIDER) & 0xFF;

//CHAINTOPREVHANDLER:
	_mvchain_intr(MK_FP(globData.prev_tick_handler_seg, globData.prev_tick_handler_off));
}

void InDOSInt_SetInterruptVector(uint8 intnum, void far* vector)
{
	void (__interrupt __far * oldDOSInt)() = _MK_FP(globData.prev_dos_handler_seg, globData.prev_dos_handler_off);
	uint16 vector_seg = _FP_SEG(vector);
	uint16 vector_off = _FP_OFF(vector);

	_asm
	{
		push ds
		push dx
		push ax

		mov ax, vector_seg
		mov ds, ax
		mov dx, vector_off
		mov ah, DOS_SETINTVEC
		mov al, intnum

		pushf
		call dword ptr oldDOSInt

		pop ax
		pop dx
		pop ds
	}
}

void ProcessDOSInt_SetIntr(uint8 intnum, void far* vector)
{
	char __far* FMDRVMarker = 0;

	int cmpSBOPL2 = -1;
	int cmpFMDRV = -1;

	uint16 curcs = 0;
	_asm
	{
		push ax
		mov ax, cs
		mov curcs, ax
		pop ax
	}

	// 코드 정합성을 위해 추가
	if (FMDRV_INTERRUPT == intnum)
	{
		if (TRUE == logicStatus.isFMDRVSet)
		{
			if (_MK_FP(curcs, _FP_OFF(MyFMDRVInterrupt)) != vector)
			{
				// 여기서는 66번의 Restore를 막지 않고 인터럽트를 체이닝한다.
				// 실제 FMDRV.COM쪽에서 66번의 핸들러의 원래 값을 알고 있기 때문이다.
				// 우리는 66번 핸들러를 교체하지 않는다.
				logicStatus.isFMDRVSet = FALSE;
				if (logicStatus.isCDPlay)
				{
					CDAudio_Stop(cddrive);
					logicStatus.isCDPlay = FALSE;
				}
				terminateTSR = TRUE;
			}
		}
		else
		{
			if (NULL != vector && ((uint32)vector > FMDRV_MARKER_OFFSET))
			{
				FMDRVMarker = (char __far*)((uint32)vector - FMDRV_MARKER_OFFSET);
				cmpFMDRV = mystrcmp(FMDRVMarker, FMDRV_Marker_Str);
				cmpSBOPL2 = mystrcmp(FMDRVMarker, SBDRV_Marker_Str);
				if (0 == cmpFMDRV || 0 == cmpSBOPL2)
				{
					// 실제 66번 핸들러의 설치를 막는다.
					skipDOSChain = TRUE;
					// 해당 위치에 "OPLDRV"가 있으면 Koei의 FMDRV 인터럽트가 세팅된 것이 맞다.
					// TODO: 해당 위치에서 +7한 부분에 1~9까지의 값이 있는지도 원래는 체크해야 한다.

					// FMDRV_INTERRUPT의 핸들러 수정
					globData.prev_fmdrv_handler_seg = _FP_SEG(vector);
					globData.prev_fmdrv_handler_off = _FP_OFF(vector);

					_asm cli;
					InDOSInt_SetInterruptVector(intnum, _MK_FP(curcs, _FP_OFF(MyFMDRVInterrupt)));
					_asm sti; // 인터럽트를 다시 활성화한다.

					logicStatus.isFMDRVSet = TRUE;
				}
			}
		}
	}
	else if (TICK_INTERRUPT == intnum)
	{
		if (TRUE == logicStatus.isTickSet)
		{
			if (_MK_FP(curcs, _FP_OFF(MyTickInterrupt)) != vector)
			{
				// 여기서는 08번의 Restore를 막지 않고 인터럽트를 체이닝한다.
				// 실제 FMDRV.COM쪽에서 08번의 핸들러의 원래 값을 알고 있기 때문이다.
				// 우리는 08번 핸들러를 교체하지 않는다.
				logicStatus.isTickSet = FALSE;
				terminateTSR = TRUE;
			}
		}
		else
		{
			// 반드시 FMDRV_INTERRUPT의 후킹 이후에 후킹되기 때문에
			// FMDRV_INTERRUPT의 후킹여부를 보고 후킹하면 된다.
			if (NULL != vector && TRUE == logicStatus.isFMDRVSet)
			{
				// 실제 08번 핸들러의 설치를 막는다.
				skipDOSChain = TRUE;

				// TICK_INTERRUPT의 핸들러 수정
				globData.prev_tick_handler_seg = _FP_SEG(vector);
				globData.prev_tick_handler_off = _FP_OFF(vector);

				_asm cli;
				InDOSInt_SetInterruptVector(intnum, _MK_FP(curcs, _FP_OFF(MyTickInterrupt)));
				_asm sti; // 인터럽트를 다시 활성화한다.

				logicStatus.isTickSet = TRUE;
			}

		}
	}
	else
	{
		return;
	}
}

void ProcessDOSInt_SetPlayStep(char far* execFileName)
{
	if (!execFileName)
		return;

	// 1. is OpenFile
	if (!mystrstr(execFileName, openExeFileName))
	{
		// OpenFile
		logicStatus.curPlayStep = PLAYSTEP_OPEN;
		//stopAudioCDStep = 1;
		return;
	}
	// 2. is MainFile
	else if (!mystrstr(execFileName, mainExeFileName))
	{
		// MainFile
		logicStatus.curPlayStep = PLAYSTEP_MAIN;
		//stopAudioCDStep = 1;
	}
	// 3. is EndFile
	else if (!mystrstr(execFileName, endExeFileName))
	{
		// EndFile
		logicStatus.curPlayStep = PLAYSTEP_END;
		//stopAudioCDStep = 1;
	}

	return;
}

uint8 TrackIdxBinarySearch(uint32 trackOffset[], uint8 trackNum, uint32 target)
{
	int8 idxLow = 0;
	int8 IdxMid = 0;
	int8 idxHigh = 0;

	if (trackNum < 1)
		return (-1);

	idxHigh = trackNum - 1;
	// 트랙을 바이너리 서치로 찾는다.
	while (idxLow <= idxHigh)
	{
		IdxMid = idxLow + ((idxHigh - idxLow) / 2);
		if (trackOffset[IdxMid + 1] > target)
		{
			idxHigh = IdxMid - 1;
		}
		else if (trackOffset[IdxMid + 1] < target)
		{
			idxLow = IdxMid + 1;
		}
		else
		{
			return (IdxMid + 1);
		}
	}
	return (0);
}

void ProcessDOSInt_BufChgFMTrack(uint16 fileHandle, uint32 localFMTrackOffset)
{
	uint8 trackIdx = 0;

	if (logicStatus.curPlayStep >= PLAYSTEP_MAX)
	{
		// 여기에 도달하면 코드에 문제가 있다는 소리다.
		return;
	}

	if (!isBufferChangeStyle[logicStatus.curPlayStep])
		return;

	switch (logicStatus.curPlayStep)
	{
	case PLAYSTEP_OPEN:
		if (fileHandle == openMusicFileHandle)
		{
			// 트랙을 바이너리 서치로 찾는다.
			trackIdx = TrackIdxBinarySearch(fmTrackOffsetOpening, fmTrackSongNumOpening, localFMTrackOffset);
			if (trackIdx > 0)
				bufChgFMTrack = trackIdx;
			else
				bufChgFMTrack = -1;
		}
		break;
	case PLAYSTEP_MAIN:
		if (fileHandle == mainMusicFileHandle)
		{
			trackIdx = TrackIdxBinarySearch(fmTrackOffset, fmTrackSongNum, localFMTrackOffset);
			if (trackIdx > 0)
				bufChgFMTrack = trackIdx;
			else
				bufChgFMTrack = -1;
		}
		break;
	case PLAYSTEP_END:
		if (fileHandle == endMusicFileHandle)
		{
			trackIdx = TrackIdxBinarySearch(fmTrackOffsetEnding, fmTrackSongNumEnding, localFMTrackOffset);
			if (trackIdx > 0)
				bufChgFMTrack = trackIdx;
			else
				bufChgFMTrack = -1;
		}
		break;
	default:
		break;
	}
	return;
}

uint16 InDOSInt_OpenFile(uint8 flag, char far* filename)
{
	void (__interrupt __far * oldDOSInt)() = _MK_FP(globData.prev_dos_handler_seg, globData.prev_dos_handler_off);
	uint16 filename_seg = _FP_SEG(filename);
	uint16 filename_off = _FP_OFF(filename);

	bool isErr = FALSE;
	uint16 retval = 0;
	_asm
	{
		push ds
		push dx
		push ax

		mov ax, filename_seg
		mov ds, ax
		mov dx, filename_off
		mov ah, 3Dh
		mov al, flag
		pushf
		call dword ptr oldDOSInt
		jnc success
		mov isErr, TRUE

	success :
		mov retval, ax
		pop ax
		pop dx
		pop ds
	}
	if (isErr == TRUE)
	{
		globDOSIntregs.w.flags |= INTR_CF;
		_puthex(retval);
	}
	return retval;
}

void ProcessDOSInt_SetMusicFileHandle(uint8 flag, char far* filename)
{
	if (!filename)
		return;

	// 코드 정합성을 위해 추가
	if (0 != globDOSIntregs.h.al && 2 != globDOSIntregs.h.al)
		return;

	// 1. is OpenMusicFile
	if (isBufferChangeStyle[PLAYSTEP_OPEN] &&
		0 == openMusicFileHandle && !mystrstr(filename, openMusicFileName))
	{
		// OpenMusicFile
		openMusicFileHandle = InDOSInt_OpenFile(flag, filename);
		globDOSIntregs.w.ax = openMusicFileHandle;
		skipDOSChain = TRUE;
		return;
	}
	// 2. is MainMusicFile
	else if (isBufferChangeStyle[PLAYSTEP_MAIN] && 
		0 == mainMusicFileHandle && !mystrstr(filename, mainMusicFileName))
	{
		// MainMusicFile
		mainMusicFileHandle = InDOSInt_OpenFile(flag, filename);
		globDOSIntregs.w.ax = mainMusicFileHandle;
		skipDOSChain = TRUE;
	}
	// 3. is EndMusicFile
	else if (isBufferChangeStyle[PLAYSTEP_END] && 
		0 == endMusicFileHandle && !mystrstr(filename, endMusicFileName))
	{
		// EndMusicFile
		endMusicFileHandle = InDOSInt_OpenFile(flag, filename);
		globDOSIntregs.w.ax = endMusicFileHandle;
		skipDOSChain = TRUE;
	}

	return;
}

void __interrupt __far MyDOSInterrupt(union INTPACK r)
{
	// ### 아래에서 스택 포인터를 바꾸기 때문에,
	// 여기에 로컬 변수(스택할당)을 선언한 뒤에 스택 포인터 바꾸고 난 위치에서 사용하면 안된다.
	// 
	// 인터럽트가 바뀌었는지를 검사하는 정적 문자열을 설정한다.
	// 그리고 DS도 교체한다.
	_asm
	{
		jmp SKIPTSRSIG
		TSRSIG db 'MyDO'
		SKIPTSRSIG:

		push ax         // AX 보존
		mov ax, ds;

		// CS:glob_newds에 보존된 우리 데이터 세그먼트를 ds에 덮어쓰기
		mov ax, cs:glob_newds
		mov ds, ax

		pop ax          // AX 복원
	}

	skipDOSChain = FALSE;

	if (!(TRUE == logicStatus.isBufferChangeStyleExist && (r.h.ah == DOS_FOPEN || r.h.ah == DOS_FSEEK) )
	&& r.h.ah != DOS_EXEC
	&& r.h.ah != DOS_SETINTVEC
	&& r.h.ah != DOS_UNLOADTSR) goto CHAINTOPREVHANDLER;

	// 파라미터로 입력받은 (실제로는 스택에 저장된) 인터럽트 레지스터 상태값 r을 별도 전역변수에 보존한다.
	// 그래야 이 인터럽트를 실행하고 바뀐 레지스터값이 아닌, 첫 호출자(caller)가 보냈던 인터럽트를 그대로
	// 체인된 다음 인터럽트에 보내줄 수 있다.
	mymemcpy(&globDOSIntregs, &r, sizeof(union INTPACK));
	// 스택 포인터를 전역변수 공간으로 이동한다.
	_asm
	{
		cli //인터럽트를 중지하여, 다른 인터럽트가 스택 변환중 문제가 발생하지 않도록 조치한다.
		mov globDOSIntOldstackSeg, SS
		mov globDOSIntOldstackOff, SP

		// 현재 DS를 스택 세그먼트에 덮어쓴다.
		mov ax, ds
		mov ss, ax

		// 스택 포인터에 newstack의 맨 뒷부분 바로 앞 (+NEWSTACKSZ-2) 위치를 덮어쓴다.
		mov sp, offset DOSIntStack + NEWSTACKSZ - 2
		sti // 인터럽트를 재개한다.
	}

	// ***** 여기에 실제로 실행할 기능을 호출한다.
	// 스택이 바뀐 이후이므로 r은 쓸 수 없다. globIntreg를 사용해야 한다.
	switch (globDOSIntregs.h.ah)
	{
	case DOS_EXEC:
		ProcessDOSInt_SetPlayStep(_MK_FP(globDOSIntregs.w.ds, globDOSIntregs.w.dx));
		break;
	case DOS_FOPEN:
		if (TRUE == logicStatus.isBufferChangeStyleExist)
		{
			// fopen 플래그 의미가 읽기전용(AL==0)이거나 읽고쓰기(AL==2)일 때에만 후킹한다.
			if (0 == globDOSIntregs.h.al || 2 == globDOSIntregs.h.al)
				ProcessDOSInt_SetMusicFileHandle(globDOSIntregs.h.al, _MK_FP(globDOSIntregs.w.ds, globDOSIntregs.w.dx));
		}
		break;
	case DOS_FSEEK:
		// fseek 플래그 의미가 파일 맨 처음(AL==0)일 때에만 후킹한다.
		if (TRUE == logicStatus.isBufferChangeStyleExist && 0 == globDOSIntregs.h.al)
		{
			uint32 localFMTrackOffset = (globDOSIntregs.w.cx);
			localFMTrackOffset <<= 16;
			localFMTrackOffset += globDOSIntregs.w.dx;
			ProcessDOSInt_BufChgFMTrack(globDOSIntregs.w.bx, localFMTrackOffset);
		}
		break;
	case DOS_SETINTVEC:
		if (FMDRV_INTERRUPT == globDOSIntregs.h.al || TICK_INTERRUPT == globDOSIntregs.h.al)
			ProcessDOSInt_SetIntr(globDOSIntregs.h.al, _MK_FP(globDOSIntregs.w.ds, globDOSIntregs.w.dx));
		break;
	case DOS_UNLOADTSR:
		terminateTSR = TRUE;
		break;
	default:
		break;
	}

	// 스택을 원래로 돌린다.
	_asm
	{
		cli //인터럽트를 중지하여, 다른 인터럽트가 스택 변환중 문제가 발생하지 않도록 조치한다.

		// 원래의 스택 포인터를 반환한다.
		mov SS, globDOSIntOldstackSeg
		mov SP, globDOSIntOldstackOff
		sti // 인터럽트를 재개한다.
	}

	// r의 레지스터를 모두 복원하여 이 값이 함수를 빠져나간 뒤에 정상적으로 세팅될 수 있도록 한다.
	mymemcpy(&r, &globDOSIntregs, sizeof(union INTPACK));

CHAINTOPREVHANDLER:
	if (FALSE == terminateTSR)
	{
		if (FALSE == skipDOSChain)
			_mvchain_intr(MK_FP(globData.prev_dos_handler_seg, globData.prev_dos_handler_off));
	}
	else
	{
		_asm cli;
		// 모든 인터럽트들을 제자리에 되돌려놓는다.
		InDOSInt_SetInterruptVector(DOS_INTERRUPT, _MK_FP(globData.prev_dos_handler_seg, globData.prev_dos_handler_off));
		_asm sti;

		// 현재 로딩중인 이 TSR을 언로드한다.
		//_puthex(freeseg(globData.pspseg));
		freeseg(globData.pspseg);
	}
}

/////////////////////////////////////////
// 여기부터는 상주되지 않는다.
// 따라서 이 아래에는 C 라이브러리 함수를 사용하여도 된다.
#pragma code_seg("_TEXT", CODE)

// 이 함수는 명백히 아무것도 하지 않으나, BEGTEXT 세그먼트의 끝(다음 세그먼트의 시작)을 확인하기 위해 필요한 코드이다.
void begtextend(void)
{
}

#include "MAINARGS.H"

// From EtherDFS
// 필요한 상주 메모리 양을 16바이트 패러그래프 단위로 계산한다.
// 계산 방법:
// 메모리 맵을 확인하고 RESDATA 세그먼트의 크기를 기록한다.
// (RESDATA: TSR에서 사용할 모든 전역변수 데이터)
// 그리고 BEGTEXT 세그먼트의 크기를 기록한다.
// (BEGTEXT: TSR에서 사용할 모든 코드)
// 
// 총합: (sizeof(RESDATA) + sizeof(BEGTEXT) + sizeof(PSP) + 15) / 16
// sizeof(PSP)는 MS-DOS에서 256바이트로 고정되어 있으며, 
// +15는 /16에 의해 버림 되는 수치를 보정하기 위해 더해준다.
static uint16 getResidentSize()
{
	uint16 res = 0;
	_asm
	{
		push ax                     // AX 보존
		mov ax, offset begtextend   // AX에 BEGTEXT끝의 오프셋을 기록한다. SMALL 메모리 모델에서 코드 세그먼트는 1개 뿐이므로 세그먼트 갯수는 고려하지 않는다.
		add ax, 256                 // PSP (256 bytes) 크기를 더한다.
		add ax, 15                  // 버림 보정값 15를 더한다.
		mov cl, 4                   // 16(=2^4)을 나누기 위해 cl에 4를 넣는다.
		shr ax, cl                  // 4비트 우측 쉬프트로 16(=2^4)을 나눈다. 8086/8088 어셈블리는 cl없이 직접 숫자 입력시 1-bit shr만 가능하기 때문에 reg, cl 방식을 사용한다. 

		// 코드 세그먼트에서 데이터 세그먼트를 빼서 RESDATA의 크기(패러그래프 단위)를 계산한다.
		add ax, seg begtextend      // 코드 세그먼트 위치 더하기
		sub ax, seg globData        // 데이터 세그먼트 위치 빼기
		mov res, ax                 // res 변수에 그간 계산한 값을 넘겨준다.
		pop ax                      // AX 복원
	}
	return(res);
}

// 상위 메모리(upper memory) 데이터 세그먼트(upper_ds)를 구한다.
static uint16 getUpperDS(uint16 upperseg)
{
	uint16 res = 0;
	_asm
	{
		push ax                     // AX 보존

		// 코드 세그먼트에서 데이터 세그먼트를 빼서 RESDATA의 크기(패러그래프 단위)를 계산한다.
		mov ax, seg begtextend      // 코드 세그먼트 위치 더하기
		sub ax, seg globData        // 데이터 세그먼트 위치 빼기
		add ax, 16                  // PSP (256 bytes = 16 paragraphs) 크기를 더한다.
		add ax, upperseg            // 입력받은 상위 메모리 세그먼트(upperseg)를 더한다.
		mov res, ax                 // res 변수에 그간 계산한 값을 넘겨준다.
		pop ax                      // AX 복원
	}
	return(res);
}

// UMB 영역의 여유 메모리 공간에서 세그먼트 할당 혹은 실패시 0 리턴
__declspec(naked) static uint16 allocseg(uint16 sz) {
	(void)sz;
	_asm {
		// http://www.techhelpmanual.com/826-accessing_upper_memory.html
		// http://www.techhelpmanual.com/523-dos_fn_5801h__set_memory_allocation_strategy.html
		mov ax, 5800h // DOS 2.11+ - 메모리 할당 전략 GET/SET 
		// AL = 0으로 메모리 할당 전략 GET

		int 21h       // 할당 전략이 AX에 입력됨
		push ax       // 현재 할당 전략을 스택에 보존
		mov ax, 5802h // AL = 2로 UMB Link 상태 확인 (DOS 5+)
		int 21h       // UMB Link 상태가 AX에 입력됨
		push ax       // 현재 UMB Link 상태를 스택에 보존
		mov ax, 5803h // AL = 3로 UMB Link 상태를 설정 (DOS 5+)
		mov bx, 1     // BX = 1로 상위 메모리를 포함함
		int 21h
		mov ax, 5801h // AL = 1로 메모리 할당전략을 SET
		mov bx, 0041h // BX = 41h로 상위 메모리에 BestFit하는 할당 전략 선택
		int 21h

		// 위에서 설정한 할당전략으로 메모리를 할당 
		mov ah, 48h     // AH = 48h 메모리 할당 (DOS 2+)
		mov bx, dx      // 할당할 용량(dx == 파라미터sz)을 BX에 SET
		mov dx, 0       // 기본(실패) 리턴값으로 0 설정
		int 21h         // 성공시 할당된 메모리 주소가 AX에 입력됨
		// 성공여부를 CF 플래그로 체크
		jc failed
		mov dx, ax		// 성공시 리턴값에 들어온 주소값을 SET

	failed :
		// 아까 보존해뒀던 UMB 링크 상태를 복원
		mov ax, 5803h
		pop bx		// 스택에 보존해 둔 UMB 링크 상태를 BX에 pop
		int 21h

		// 아까 보존해뒀던 메모리 할당 전략을 복원
		mov ax, 5801h
		pop bx		// 스택에 보존해 둔 메모리 할당 전략을 BX에 pop
		int 21h
		ret
	}
}
#pragma aux allocseg parm [dx] value [dx] modify exact [ax bx cl dx] nomemory;

static unsigned char umb_ident[9] = "OWNFMDRV";

int SetupInterrupt(uint16 newcs, uint16 newds)
{
	void(far interrupt * oldInt) = 0;
	uint16 oldds;

	union REGS regs;
	struct SREGS sregs;

	// 도스의 LH가 아닌, 옵션 /H를 통한 LoadHigh일 경우 DS가 꼬이기 때문에 전역변수에 대한 제대로된 세팅을 위해 ds를 교체한다.
	// 이 때, 아직 Transient쪽의 코드도 작동해야 하기 때문에 전역변수를 write하는 코드에만 ds를 교체할 수 있도록 한다.
#define BACKUP_OLDDS			_asm mov oldds, ds
#define SET_DS_TO_NEWDS			_asm mov ds, newds
#define RESTORE_DS_FROM_OLDDS	_asm mov ds, oldds
	oldds = 0;
	BACKUP_OLDDS;

	// Get Dos Active Flag
	regs.h.ah = 0x34;
	int86x(DOS_INTERRUPT, &regs, &regs, &sregs);

	SET_DS_TO_NEWDS;
	{
		dosActiveFlag = _MK_FP(sregs.es, regs.x.bx);

		memset((void*)&logicStatus, 0, sizeof(logicStatus));
		logicStatus.curPlayStep = PLAYSTEP_PREPARE;
		if (isBufferChangeStyleExist)
		{
			logicStatus.isBufferChangeStyleExist = TRUE;
		}

		memset((void*)&FMDRVIntStack, 0, sizeof(char) * NEWSTACKSZ);
		memset((void*)&DOSIntStack, 0, sizeof(char) * NEWSTACKSZ);
	}
	RESTORE_DS_FROM_OLDDS;

	_disable();

	// 프로세스 확인, FMDRV의 설치/삭제를 위해 DOS 인터럽트를 후킹한다.
	oldInt = _dos_getvect(DOS_INTERRUPT);
	SET_DS_TO_NEWDS;
	{
		globData.prev_dos_handler_seg = _FP_SEG(oldInt);
		globData.prev_dos_handler_off = _FP_OFF(oldInt);
	}
	RESTORE_DS_FROM_OLDDS;
	// 보내주는 세그먼트로 교체
	_dos_setvect(DOS_INTERRUPT, MK_FP(newcs, _FP_OFF(MyDOSInterrupt)));

	_enable();

	return INTERRUPT_SUCCESS;
}

int8 StartTSR(void)
{
	int i;
	uint16 residentcs;
	uint16 old_pspseg;  // 기본 메모리 영역에서 사용할 프로그램의 PSP
	uint16 upperseg;    // LOADHIGH를 사용하기 위해 구할 상위메모리 세그먼트
	unsigned char far* mcbfptr;
	const uint16 residentsize = getResidentSize();

	InfoLog("Expected Resident Size: %d bytes(0x%04X) + 16(MCB) bytes \n", residentsize * 16, residentsize * 16);
	// 런타임에서 인터럽트 핸들러의 코드 세그먼트 값을 얻는다
	// BEGTEXT에 있는 아무 인터럽트 핸들러면 된다.
	glob_newds = (FP_SEG((void far*) & globData));
	residentcs = (FP_SEG((void far*) & MyFMDRVInterrupt));

	// PSP 세그먼트 확보
	_asm
	{
		mov ah, 62h          // 현재 PSP 주소를 주세요
		int 21h              // DOS 인터럽트
		mov word ptr[globData + GLOB_DATOFF_PSPSEG], bx  // globData의 PSPSEG 부분에 복사
	}

	// 상주할 메모리 영역을 상위 메모리로 복사
	if ((args.flags & ARGFL_LOADHIGH) != 0)
	{
		upperseg = allocseg(residentsize);
		if (upperseg == 0)
		{
			InfoLog("Upper memory alloc error, loading TSR in conventional memory.\n");
			args.flags &= ~ARGFL_LOADHIGH;
		}
		else
		{
			InfoLog("Upper segment at %04X\n", upperseg);

			// 주어진 상위메모리에서 가장 앞에는 PSP가 기록되므로,
			// 우리의 데이터 세그먼트는 upperseg + sizeof(PSP)에 복사된다.
			glob_newds = upperseg + 16;	// PSP는 256바이트, 패러그래프(세그먼트) 단위로는 16

			InfoLog("Upper resident data segment at %04X\n", glob_newds);
			// upperseg의 코드 세그먼트 값을 얻는다.
			residentcs = getUpperDS(upperseg);
			InfoLog("Upper resident code segment at %04X\n", residentcs);

			// MCB의 블록 소유자 이름(DOS mem 명령시 표시될 이름)을 설정한다.
			// MCB(Memory Control Block)은 upperseg자리에서 1 패러그래프만큼 앞에 있다.
			// (즉, PSP맨 앞에서 1패러그래프 앞에 있다)
			// 이 MCB에서 8번째 오프셋에 블록 소유자 이름이 마킹된다.
			// http://www.techhelpmanual.com/368-mcb__memory_control_block.html
			mcbfptr = (unsigned char far*)MK_FP(upperseg - 1, 8);
			InfoLog("Upper MCB signature at %04X:%04X\n", FP_SEG(mcbfptr), FP_OFF(mcbfptr));
			for (i = 0; i < 8; i++) {
				mcbfptr[i] = umb_ident[i];
			}

			old_pspseg = globData.pspseg;	// 기본 메모리의 PSP 세그먼트를 저장해둔다.
			globData.pspseg = upperseg;		// globData에는 상위 메모리의 PSP 세그먼트를 덮어쓴다.

			InfoLog("Upper PSP segment at %04X\n", globData.pspseg);
			// 상주할 코드와 데이터를 상위 메모리로 복사
			_asm {
				// 인라인 어셈블리에서 사용할 레지스터의 기존 값을 스택에 보존
				push ds
				push es
				push ax
				push cx
				push si
				push di
				pushf

				// 메모리 블록 복사
				mov ax, residentsize	// AX에 복사할 용량(패러그래프 단위)을 입력
				mov cl, 3				// 복사할 용량을 패러그래프->워드 단위로 변환할 값 (2^3)의 제곱값을 세팅
				shl ax, cl				// 8086/8088은 shr의 operand 두번째를 상수로 입력할 때 1-bit만 세팅할 수 있기 때문에 reg, cl 방식 사용
				mov cx, ax				// CX에 복사할 용량(워드 단위)를 입력
				xor si, si				// 복사 준비: Source Iterator=0
				xor di, di				// 복사 준비: Destination Iterator = 0
				cld						// SI/DI가 0부터 증가하는 방식을 사용함을 알림
				mov ax, old_pspseg		// 기존 PSP의 뚜껑 세그먼트 위치를 AX(Source PTR)에 설정
				mov ds, ax				// 해당 세그먼트를 DS에 설정
				mov es, upperseg		// 복사할 PSP의 뚜껑 세그먼트 위치를 ES에 설정
				rep movsw				// 워드 단위로 DS:SI -> ES:DI 복사. 패러그래프 단위로 복사하므로 나머지가 남지 않음.

				// 레지스터 복원
				popf
				pop di
				pop si
				pop cx
				pop ax
				pop es
				pop ds
			}
		}
	}
	// PSP의 환경 값을 free한다. (PSP의 2C 오프셋에 env 세그먼트가 위치한다)
	InfoLog("Free the environment.\n");
	_asm
	{
		// PSP 세그먼트를 가져온다.
		// 상위메모리로 복사되었더라도 2C의 값은 그대로이므로 free에 문제는 없다.
		mov es, word ptr[globData + GLOB_DATOFF_PSPSEG]
		mov es, es: [2Ch]		// env 블록의 세그먼트를 ES에 세팅한다.
		mov ah, 49h				// 도스 인터럽트 free memory (DOS 2+)
		int 21h
	}
	// 인터럽트들을 세팅한다.
	SetupInterrupt(residentcs, glob_newds);

	InfoLog("Turn self into a TSR keeping %d paragraphs of memory\n", residentsize);

	// 로그 밑의 코드들이 실행되면 코드가 도달 불가(기본메모리일때) 혹은 할당 해제된 메모리(상위메모리일때)가 되므로,
	// 실제 설치되기 직전에 로그를 표시한다.
	ErrorLog("OwnFMDRV Installed.\n");

	// TSR이 상위메모리에 올라갔으면, PSP를 상위메모리에 세팅하고 기존에 받은 PSP를 해제한다.
	if ((args.flags & ARGFL_LOADHIGH) != 0)
	{
		uint8 far* umbmcbptr;
		uint16 mcbpspseg;

		// 새 MCB의 PSP 세그먼트 위치에 상위 메모리로 복사된 PSP 세그먼트 값을 덮어쓴다.
		// 이거 원래 INT 21, 50으로 되어야 할 것 같은데 왜 안되지?
		mcbpspseg = (*(uint16 far*)MK_FP(globData.pspseg - 1, 1));
		InfoLog("Previous MCB's PSP Seg: 0x%04X\n", mcbpspseg);

		umbmcbptr = (unsigned char far*)MK_FP(globData.pspseg - 1, 0);
		umbmcbptr[1] = upperseg & 0xFF;
		umbmcbptr[2] = (upperseg >> 8) & 0xFF;

		mcbpspseg = (*(uint16 far*)MK_FP(globData.pspseg - 1, 1));
		InfoLog("Current MCB's PSP Seg: 0x%04X\n", mcbpspseg);

		// 새 PSP를 상위 메모리에 세팅한다.
		InfoLog("Set new PSP to upper memory.\n");
		_asm
		{
			mov bx, upperseg    // BX: 새 PSP의 위치
			mov ah, 50h         // INT 21,50 - PSP 세그먼트 SET
			int 21h
		}

		// 이 코드 이후로 RESDATA, BEGTEXT에 없는 코드/데이터(전역변수)는 모두 접근 불가이다.
		// 기본 메모리에 남아있던 프로그램을 통으로 해제한다.
		freeseg(old_pspseg);
	}


	// TSR이 상위 메모리에 로딩될 때, 아래 코드는 순간적으로 할당 해제된 메모리에서 수행된다 (위 if문 안의 freeseg)
	// 하지만 "찰나"의 순간이므로 아직까지 메모리가 다른 프로그램에 의해 덮어씌여진 상태는 아니다.

	// 자기자신을 TSR로 만들고, TSR에 필요없는 메모리를 해제한다.
	// 이를 통해 libc 및 프로그램 초기화용 코드들이 모두 버려진다.
	// 점유 메모리 용량을 계산하는 방법은 getResidentSize()의 주석으로 잘 설명돼 있다.
	_asm
	{
		mov ax, 3100h			// AH = 31 로 terminate+stay resident, 즉 TSR을 요청한다. AL = 0은 다른 프로그램에서 참조할 종료 코드이다.
		mov dx, residentsize	// DX = 상주할 메모리 용량(패러그래프 단위)
		int 21h
	}

	return INTERRUPT_SUCCESS;	// 이 코드로는 도달하지 않지만, 없으면 컴파일이 안되므로 넣어둔다.
}

int8 IsTSRInstalled(void)
{
	void(far interrupt * intFunc) = 0;
	char far* intFuncMarker = 0;
	intFunc = _dos_getvect(DOS_INTERRUPT);

	intFuncMarker = (char __far*)((uint32)intFunc + INTSIG_OFF);

	if (intFuncMarker[0] == 'M' && intFuncMarker[1] == 'y' && intFuncMarker[2] == 'D' && intFuncMarker[3] == 'O')
		return TRUE;

	return FALSE;
}

int8 UnloadTSR(void)
{
	if (IsTSRInstalled())
	{
		union REGS regs;
		struct SREGS sregs;

		regs.h.ah = DOS_UNLOADTSR;
		int86x(DOS_INTERRUPT, &regs, &regs, &sregs);
	}

	return INTERRUPT_SUCCESS;
}
