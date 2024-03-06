// CDROMDRV.H
#ifndef _CDROMDRV_H_
#define _CDROMDRV_H_

#include "FMDRVDEF.H"

#define INPARAM
#define OUTPARAM

#define CDAUDIO_SUCCESS (int8)(0)
#define CDAUDIO_FAIL            (int8)(-1)
#define CDAUDIO_BUSY            (int8)(-2)
#define CDAUDIO_NOTBUSY (int8)(-3)

#define MAX_CDDRIVE             26

// CD-DA 정의에 따른 설정
#define DATATRACK_BIT           0x40

// CMD 정의값
#define CMD_IOCTL_IN    3
#define CMD_IOCTL_OUT   12

#define CMD_PLAY                132
#define CMD_STOP                133
#define CMD_RESUME              136

#define TRACK_MARGIN    150L    // 트랙간 2초 마진
#define CD_OK                   (0x0100)
#define CD_BUSYBIT              (0x0200)

// struct는 int등에 대해 바이트 얼라인을 맞추기 때문에,
// 인터럽트 콜을 위해서는 반드시 바이트 얼라인을 1로 맞춰주어야 한다.
#pragma pack(push, 1)
typedef struct
{
        uint8   notUsed;

        uint8   status;
        uint8   trackNo;
        uint8   index;
        uint8   trackMin;
        uint8   trackSec;
        uint8   trackFrame;
        uint8   zero;
        uint8   diskMin;
        uint8   diskSec;
        uint8   diskFrame;
} QChannelInfo;

typedef struct
{
        uint8   notUsed;

        uint8   outChannel0;
        uint8   outVolume0;
        uint8   outChannel1;
        uint8   outVolume1;
        uint8   outChannel2;
        uint8   outVolume2;
        uint8   outChannel3;
        uint8   outVolume3;
} AudioChannelSet;
#pragma pack(pop)

typedef struct
{
        int track;

        int32 fromSector;
        int32 toSector;
} CD_TMSF;

// 램상주하게 될 함수들. CD_RESID.C 구현
extern int8 CDAudio_SetVolume(int8 INPARAM drive, uint8 INPARAM volume);
extern int8 CDAudio_PlaySector(int8 INPARAM drive, uint32 INPARAM startSector, uint32 INPARAM endSector);
extern int8 CDAudio_Stop(int8 INPARAM drive);
extern int8 CDAudio_CheckCDBusy(int8 INPARAM drive);
// 램상주하게 될 함수들 끝

extern int8 TestCDROM(void);

extern int8 CDAudio_GetCDROMNum(int8 OUTPARAM* drive);
extern int8 CDAudio_GetCDROMDriverVersion(int8 OUTPARAM* majorver, int8 OUTPARAM* minorver);
extern int8 CDAudio_GetCDROMDriveList(int8 OUTPARAM driveLetter[]);    // 반드시 GetCDROMNum에서 얻은 드라이브 수 만큼의 버퍼 크기를 주어야 한다.
extern int8 CDAudio_GetDiskInfo(int8 INPARAM drive, uint8 OUTPARAM *firstTrack, uint8 OUTPARAM *lastTrack, uint32 OUTPARAM *leadoutTrack);
extern int8 CDAudio_GetTrackInfo(int8 INPARAM drive, uint8 INPARAM firstTrack, uint8 INPARAM lastTrack, uint32 OUTPARAM startPos[], uint8 OUTPARAM status[]);  // 반드시 Track 수 만큼의 버퍼 크기를 주어야 한다.
extern int8 CDAudio_IsAudioTrack(uint8 status);
extern int8 CDAudio_GetPlayInfo(int8 INPARAM drive, QChannelInfo OUTPARAM *qInfo);
extern uint32 CDAudio_Pos2SectorMSF(const uint8 min, const uint8 sec, const uint8 frame);
extern uint32 CDAudio_Pos2Sector(uint32 INPARAM pos);
extern int8 CDAudio_Play(int8 INPARAM drive, uint32 INPARAM startPos, uint32 INPARAM endPos);
extern int8 CDAudio_PlayTrack(int8 INPARAM drive, int8 INPARAM trackNo, uint8 INPARAM firstTrack, uint8 INPARAM lastTrack, uint32 INPARAM startPos[], uint32 INPARAM leadoutPos);
extern int8 CDAudio_Pause(int8 INPARAM drive);
extern int8 CDAudio_Resume(int8 INPARAM drive);
extern int8 CDAudio_GetVolume(int8 INPARAM drive, AudioChannelSet OUTPARAM *channelInfo);
#endif // _CDROMDRV_H_
