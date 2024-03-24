// CDROMDRV.C
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FMDRVDEF.H"
#include "CDROMDRV.H"

// struct는 int등에 대해 바이트 얼라인을 맞추기 때문에,
// 인터럽트 콜을 위해서는 반드시 바이트 얼라인을 1로 맞춰주어야 한다.
#pragma pack(1)

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
#pragma pack(pop)  // 바이트 얼라인 복원

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

// ======== External Functions ========
int8 CDAudio_GetCDROMNum(int8 OUTPARAM* drive)
{
	union REGS cdrom;

	if (!drive) return CDAUDIO_FAIL;

	cdrom.h.ah = 0x15;
	cdrom.h.al = 0x00;
	int86(0x2F, &cdrom, &cdrom);

	if (NULL != drive)
	{
		(*drive) = cdrom.h.cl;
	}
	return (cdrom.h.bl);
}

int8 CDAudio_GetCDROMDriverVersion(int8 OUTPARAM* majorver, int8 OUTPARAM* minorver)
{
	union REGS cdrom;

	if (!majorver || !minorver) return CDAUDIO_FAIL;

	cdrom.h.ah = 0x15;
	cdrom.h.al = 0x0C;

	int86(0x02F, &cdrom, &cdrom);

	(*majorver) = cdrom.h.bh;
	(*minorver) = cdrom.h.bl;

	return CDAUDIO_SUCCESS;
}


int8 CDAudio_GetCDROMDriveList(uint8 OUTPARAM driveLetter[])
{
	union REGS cdrom;
	struct SREGS scdrom;
	void far* driveLetterPtr = driveLetter;

	cdrom.h.ah = 0x15;
	cdrom.h.al = 0x0D;

	scdrom.es = _FP_SEG(driveLetterPtr);
	cdrom.x.bx = _FP_OFF(driveLetterPtr);

	int86x(0x02F, &cdrom, &cdrom, &scdrom);

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_GetDiskInfo(int8 INPARAM drive, uint8 OUTPARAM* firstTrack, uint8 OUTPARAM* lastTrack, uint32 OUTPARAM* leadoutTrack)
{
	DiskInfo diskInfo;

	if (!firstTrack || !lastTrack || !leadoutTrack) return CDAUDIO_FAIL;

	memset(&diskInfo, 0, sizeof(diskInfo));
	diskInfo.subfunc = 10;
	if (CDAUDIO_FAIL == CallCDROMDriver(drive, CMD_IOCTL_IN, &diskInfo, sizeof(diskInfo), 0))
	{
		return CDAUDIO_FAIL;
	}
	*firstTrack = diskInfo.firstTrack;
	*lastTrack = diskInfo.lastTrack;
	*leadoutTrack = diskInfo.leadoutTrack;

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_GetTrackInfo(int8 INPARAM drive, uint8 INPARAM firstTrack, uint8 INPARAM lastTrack, uint32 OUTPARAM startPos[], uint8 OUTPARAM status[])
{
	TrackInfo trackInfo;
	int i = 0;

	for (i = 0; i < (lastTrack - firstTrack + 1); i++)
	{
		memset(&trackInfo, 0, sizeof(trackInfo));
		trackInfo.subfunc = 11;
		trackInfo.trackNo = i + firstTrack;
		CallCDROMDriver(drive, CMD_IOCTL_IN, &trackInfo, sizeof(trackInfo), 0);

		startPos[i] = trackInfo.startPos;
		status[i] = trackInfo.status;
	}

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_IsAudioTrack(uint8 status)
{
	if (0 != (status & 0x40))
	{
		// Data Track;
		return CDAUDIO_FAIL;
	}

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_GetPlayInfo(int8 INPARAM drive, QChannelInfo OUTPARAM* qInfo)
{
	if (!qInfo)
	{
		return CDAUDIO_FAIL;
	}
	memset(qInfo, 0, sizeof(qInfo));
	qInfo->notUsed = 12;      //subfunc
	CallCDROMDriver(drive, CMD_IOCTL_IN, qInfo, sizeof(*qInfo), 0);

	return CDAUDIO_SUCCESS;
}

uint32 CDAudio_Pos2SectorMSF(const uint8 min, const uint8 sec, const uint8 frame)
{
	const uint32 sector = (uint32)min * 60L * 75L +
		(uint32)sec * 75L +
		(uint32)frame;

	return sector;
}

uint32 CDAudio_Pos2Sector(uint32 INPARAM pos)
{
	/*
	return  (uint32)
					((pos & 0x00FF0000) >> 16) * 60L * 75L +        // 분
					((pos & 0x0000FF00) >> 8) * 75L +               // 초
					((pos & 0x000000FF));                           // 프레임
	*/

	const uint8 min = (uint8)(pos >> 16) & 0xFF;
	const uint8 sec = (uint8)(pos >> 8) & 0xFF;
	const uint8 frame = (uint8)(pos >> 0) & 0xFF;

	return CDAudio_Pos2SectorMSF(min, sec, frame);
}

int8 CDAudio_Play(int8 INPARAM drive, uint32 INPARAM startPos, uint32 INPARAM endPos)
{
	// Pos를 Sector로 변환한다.
	uint32 startSector = CDAudio_Pos2Sector(startPos);
	uint32 endSector = CDAudio_Pos2Sector(endPos);

	return CDAudio_PlaySector(drive, startSector, endSector);
}

int8 CDAudio_PlayTrack(int8 INPARAM drive, int8 INPARAM trackNo, uint8 INPARAM firstTrack, uint8 INPARAM lastTrack, uint32 INPARAM startPos[], uint32 INPARAM leadoutPos)
{
	uint32 trackStartPos = startPos[trackNo - firstTrack];
	uint32 trackEndPos = 0;

	if (trackNo == lastTrack)
	{
		trackEndPos = leadoutPos;
	}
	else
	{
		trackEndPos = startPos[trackNo + 1 - firstTrack];
	}

	return CDAudio_Play(drive, trackStartPos, trackEndPos);
}

int8 CDAudio_Pause(int8 INPARAM drive)
{
	return CallCDROMDriver(drive, CMD_STOP, 0, 0, 0);
}

int8 CDAudio_Resume(int8 INPARAM drive)
{
	return CallCDROMDriver(drive, CMD_RESUME, 0, 0, 0);
}

int8 CDAudio_GetVolume(int8 INPARAM drive, AudioChannelSet OUTPARAM* channelInfo)
{
	if (!channelInfo)
	{
		return CDAUDIO_FAIL;
	}
	memset(channelInfo, 0, sizeof(channelInfo));
	channelInfo->subfunc = 4;       //subfunc
	CallCDROMDriver(drive, CMD_IOCTL_IN, channelInfo, sizeof(*channelInfo), 0);

	return CDAUDIO_SUCCESS;
}

int8 CDAudio_TestCDROM(void)
{
	int i, j;
	int8 num, drive;

	DriveStatus drvStatus;
	VolumeSize volumeSize;
	int trackNum;
	uint32* trackStartPos;
	uint8* trackStatus;

	uint8* deviceLetter;

	uint8 firstTrack;
	uint8 lastTrack;
	uint32 leadoutTrack;

	// CDROM 갯수 확인
	num = CDAudio_GetCDROMNum(&drive);

	if (!num)
	{
		printf("CD-ROM not exist.\n");
		return CDAUDIO_FAIL;
	}

	deviceLetter = (uint8*)malloc(num * sizeof(uint8));
	CDAudio_GetCDROMDriveList(deviceLetter);
	for (j = 0; j < num; j++)
	{
		printf("No. %d - CD-ROM (%c:\\)\n", j, 'A' + deviceLetter[j]);

		drive = deviceLetter[j];

		memset(&volumeSize, 0, sizeof(volumeSize));
		volumeSize.subfunc = 8;
		CallCDROMDriver(drive, CMD_IOCTL_IN, &volumeSize, sizeof(volumeSize), 0);
		printf("CDROM Volume Size:%ld\n", volumeSize.volumeSize);

		memset(&drvStatus, 0, sizeof(drvStatus));
		drvStatus.subfunc = 6;
		CallCDROMDriver(drive, CMD_IOCTL_IN, &drvStatus, sizeof(drvStatus), 0);

		printf("CDROM status : 0x%04X\n", drvStatus.driveState);

		CDAudio_GetDiskInfo(drive, &firstTrack, &lastTrack, &leadoutTrack);

		printf("FirstTrack: %d\n", firstTrack);
		printf("LastTrack: %d\n", lastTrack);
		printf("LeadOutTrack: %ld (0x%08lX)\n", leadoutTrack, leadoutTrack);

		trackNum = (lastTrack - firstTrack + 1);
		trackStartPos = (uint32*)malloc(trackNum * sizeof(uint32));
		trackStatus = (uint8*)malloc(trackNum * sizeof(uint8));

		CDAudio_GetTrackInfo(drive, firstTrack, lastTrack, trackStartPos, trackStatus);
		for (i = 0; i < trackNum; i++)
		{
			printf("\nTrack No. %d Info:\n", i + firstTrack);
			printf("StartPos: %ld(0x%08lX)\n", trackStartPos[i], trackStartPos[i]);
			printf("Status: %d(0x%02X)\n", trackStatus[i], trackStatus[i]);
		}


		//Stop(drive);
		//Play(drive, trackStartPos[3], trackStartPos[4]);

		free(trackStartPos);
		free(trackStatus);
	}
	free(deviceLetter);


	return CDAUDIO_SUCCESS;
}
