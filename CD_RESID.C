// CDROMDRV.C
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FMDRVDEF.H"
#include "CDROMDRV.H"

// struct는 int등에 대해 바이트 얼라인을 맞추기 때문에,
// 인터럽트 콜을 위해서는 반드시 바이트 얼라인을 1로 맞춰주어야 한다.
#pragma pack(push, 1)
typedef struct
{
	uint8   headerLength;
	uint8   subunitCode;
	uint8   command;
	uint16  status;
	uint8   reserved1[8];
	uint8   addressingMode;
	void far* ptr;
	uint32  length;
	uint8   reserved2[4];
} RequestHeader;

typedef struct
{
	uint8   subfunc;
	uint32  driveState;
} DriveStatus;

typedef struct
{
	uint8   subfunc;
	uint8   firstTrack;
	uint8   lastTrack;
	uint32  leadoutTrack;
} DiskInfo;

typedef struct
{
	uint8   subfunc;
	uint32  volumeSize;
} VolumeSize;

typedef struct
{
	uint8   subfunc;
	uint8   trackNo;
	uint32  startPos;
	uint8   status;
} TrackInfo;

typedef struct
{
	uint8   subfunc;
	uint16  audioStatus;
	uint32  resumeStart;
	uint32  resumeEnd;
} AudioInfo;
#pragma pack(pop)

// 이 코드는 메모리에 남는다.
#pragma code_seg(BEGTEXT, CODE)

/* zero out an object of l bytes */
static void zerobytes(void* obj, unsigned short l) {
	unsigned char* o = obj;
	while (l-- != 0) {
		*o = 0;
		o++;
	}
}

// ======== Utility Functions ========
static int8 CallCDROMDriver(int16 drive, uint8 command, void far* ptr, uint32 size, uint16* status)
{
	RequestHeader reqHeader;
	void far* reqHdrPtr = &reqHeader;
	unsigned short reqHdrPtrSeg = _FP_SEG(reqHdrPtr);
	unsigned short reqHdrPtrOff = _FP_OFF(reqHdrPtr);
	char isFail = 0;

	//union REGS cdrom;
	//struct SREGS scdrom;

	reqHeader.headerLength = 13;
	reqHeader.subunitCode = 0;
	reqHeader.command = command;
	reqHeader.status = 0;
	reqHeader.addressingMode = 0;
	reqHeader.ptr = ptr;
	reqHeader.length = size;

	_asm
	{
		push ax
		push bx
		push cx
		push es

		mov cx, drive
		mov es, reqHdrPtrSeg
		mov bx, reqHdrPtrOff

		mov ax, 1510h
		int 2Fh

		jnc CFLAG_SUCCESS
		mov isFail, 1

	CFLAG_SUCCESS:
		pop es
		pop cx
		pop bx
		pop ax
	}

	if (isFail)
	{
		return CDAUDIO_FAIL;
	}
	if (0 == (reqHeader.status & CD_OK))
	{
		return CDAUDIO_FAIL;
	}
	if (0 != status)
	{
		*status = reqHeader.status;
	}

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_SetVolume(int8 INPARAM drive, uint8 INPARAM volume)
{
	AudioChannelSet channelInfo;
	zerobytes(&channelInfo, sizeof(channelInfo));
	channelInfo.notUsed = 3;        //subfunc
	channelInfo.outChannel0 = 0;
	channelInfo.outChannel1 = 1;
	channelInfo.outChannel2 = 2;
	channelInfo.outChannel3 = 3;
	channelInfo.outVolume0 = volume;
	channelInfo.outVolume1 = volume;
	channelInfo.outVolume2 = volume;
	channelInfo.outVolume3 = volume;

	CallCDROMDriver(drive, CMD_IOCTL_OUT, &channelInfo, sizeof(channelInfo), 0);
	return CDAUDIO_SUCCESS;
}

int8 CDAudio_PlaySector(int8 INPARAM drive, uint32 INPARAM startSector, uint32 INPARAM endSector)
{
	return CallCDROMDriver(drive, CMD_PLAY, (void far*)(startSector - TRACK_MARGIN), endSector - startSector, 0);
}

int8 CDAudio_Stop(int8 INPARAM drive)
{
	int8 ret = CallCDROMDriver(drive, CMD_STOP, 0, 0, 0);
	ret |= CallCDROMDriver(drive, CMD_STOP, 0, 0, 0);
	return ret;
}

int8 CDAudio_CheckCDBusy(int8 INPARAM drive)
{
	AudioInfo audioInfo;
	uint16 status;

	zerobytes(&audioInfo, sizeof(audioInfo));
	audioInfo.subfunc = 15;
	if (CDAUDIO_FAIL == CallCDROMDriver(drive, CMD_IOCTL_IN, &audioInfo, sizeof(audioInfo), &status))
	{
		return CDAUDIO_FAIL;
	}

	if (0 != (status & CD_BUSYBIT))
	{
		return CDAUDIO_BUSY;
	}
	else
	{
		return CDAUDIO_NOTBUSY;
	}
}
