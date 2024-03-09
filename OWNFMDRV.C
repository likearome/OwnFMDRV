#include <dos.h>
#include <string.h>
#include <stdlib.h>

#include "chint.h"
#include "FMDRVDEF.H"
#include "CDROMDRV.H"
#include "INTERUP.H"
#include "INIPARSE.H"
#include "MAINARGS.H"

#define OWNFMDRV_SUCCESS		(int)(0)
#define OWNFMDRV_FAIL			(int)(-1)

#define DEFAULT_INI				("OWNFMDRV.INI")
#define INI_OPEN				("OPEN")
#define INI_END					("END")
#define INI_TMSF				("TMSF")
#define INI_TMSF_SEP			('-')
#define INI_TMSF_TIMESEP		(":")
#define INI_PLAYMARGIN			("PLAYMARGIN")

#define INI_OPEN_MUSIC_FILENAME	("OPEN_MUSIC_FILENAME")
#define INI_MAIN_MUSIC_FILENAME	("MAIN_MUSIC_FILENAME")
#define INI_END_MUSIC_FILENAME	("END_MUSIC_FILENAME")
#define INI_OPEN_MUSIC_NUM		("OPEN_MUSIC_NUM")
#define INI_MAIN_MUSIC_NUM		("MAIN_MUSIC_NUM")
#define INI_END_MUSIC_NUM		("END_MUSIC_NUM")
#define INI_OPEN_EXE_FILENAME	("OPEN_EXE_FILENAME")
#define INI_MAIN_EXE_FILENAME	("MAIN_EXE_FILENAME")
#define INI_END_EXE_FILENAME	("END_EXE_FILENAME")

#define DEFAULT_OPEN_EXE_FILENAME	("OPEN.EXE")
#define DEFAULT_MAIN_EXE_FILENAME	("MAIN.EXE")
#define DEFAULT_END_EXE_FILENAME	("END.EXE")

#define DEFAULT_VOLUME				(0x64)  // == 100
#define DEFAULT_FRAME_TO_SEC		(100)

#define MUSIC_FILE_ALIGNSIZE		2
#define MUSIC_FILE_MUSICNUM_SIZE	2
#define MUSIC_FILE_METAINFO_SIZE	6

// 전역변수들의 실제 위치는 INTERUP.C 에 있다.
// RESDATA segment에 모여 있다.

struct argstruct args;
// 커맨드라인 파라미터 파싱
static int parseargv(struct argstruct* args) {
	int i;

	// 파라미터 첫번째는 게임 타입이다.
	if (args->argc < 2)
		return OWNFMDRV_FAIL;
	args->gametype = args->argv[1];
	for (i = 2; i < args->argc; i++) {
		char opt;
		char* arg;
		char drv;
		// 슬래쉬(/)로 시작하는 파라미터
		if (args->argv[i][0] == '/') {
			// 바로 뒤에 문자가 없으면 사용할 수 없다.
			if (args->argv[i][1] == 0) return OWNFMDRV_FAIL;

			// 이번 옵션을 opt에 입력
			opt = args->argv[i][1];
			// 뒤따라오는 옵션이 있는지 확인
			if (args->argv[i][2] == 0) { // 뒤따라오는 옵션 없음
				arg = NULL;
			}
			else if (args->argv[i][2] == '=') { //뒤따라오는 옵션 있음
				arg = args->argv[i] + 3;
			}
			else {
				return OWNFMDRV_FAIL;
			}
			// 이번 옵션을 소문자로 변환
			if ((opt >= 'A') && (opt <= 'Z')) opt += ('a' - 'A');

			switch (opt) {
			case 't':
				if (arg == NULL) return OWNFMDRV_FAIL;
				// G드라이브가 타겟일 때 /t=G: 또는 t=G 모두 가능함
				if ((arg[0] == 0) || (arg[2] != 0)) return OWNFMDRV_FAIL;
				// 드라이브를 소문자로 변환
				drv = arg[0];
				if ((drv >= 'A') && (drv <= 'Z')) drv += ('a' - 'A');
				args->targetcddrive = arg[0] - 'a';
				if (args->targetcddrive >= MAX_CDDRIVE) return OWNFMDRV_FAIL;
				break;
			case 'v':
				if (arg == NULL) return OWNFMDRV_FAIL;
				// /v=0 부터 v=255까지 가능함
				if ((arg[0] == 0) || (arg[3] != 0)) return OWNFMDRV_FAIL;
				args->volume = (char)atoi(&arg[0]);
				if (args->volume > 255) return OWNFMDRV_FAIL;
				break;
			case 'u':  // 언로드
				if (arg != NULL) return OWNFMDRV_FAIL;
				args->flags |= ARGFL_UNLOAD;
				break;
			case 'h':  // 상위 메모리 사용
				if (arg != NULL) return OWNFMDRV_FAIL;
				args->flags |= ARGFL_LOADHIGH;
				break;
			case 's':   // Verbose 모드
				if (arg != NULL) return OWNFMDRV_FAIL;
				args->flags |= ARGFL_VERBOSE;
				break;
			case 'd':
				if (arg != NULL) return OWNFMDRV_FAIL;
				args->flags |= ARGFL_DIGITALVOLUME; // Future Work
				break;
			default: // 받을 수 없는 파라미터
				return OWNFMDRV_FAIL;
			}
			continue;
		}
	}

	return OWNFMDRV_SUCCESS;
}


// CD 연주를 위한 정보
extern int8 cddrive;
//uint32 trackStartPos[MAX_CDAUDIOTRACK + 1];
//uint8 trackStatus[MAX_CDAUDIOTRACK];
extern int8 playMargin;
extern uint8 orginalCDVolume;
extern int8 isBufferChangeStyleExist;

// CD 트랙 정보.
extern CD_TMSF trackMap[MAX_CDAUDIOTRACK + 1];
extern CD_TMSF trackMapOpening[MAX_CDAUDIOTRACK + 1];
extern CD_TMSF trackMapEnding[MAX_CDAUDIOTRACK + 1];

extern int8 fmTrackSongNum;
extern int8 fmTrackSongNumOpening;
extern int8 fmTrackSongNumEnding;
extern uint32 fmTrackOffset[MAX_CDAUDIOTRACK + 1];
extern uint32 fmTrackOffsetOpening[MAX_CDAUDIOTRACK + 1];
extern uint32 fmTrackOffsetEnding[MAX_CDAUDIOTRACK + 1];

// OPEN MAIN END 각각의 MUSIC 파일 이름
extern int8 isBufferChangeStyle[PLAYSTEP_MAX];
extern char openMusicFileName[20];
extern char mainMusicFileName[20];
extern char endMusicFileName[20];

// OPEN MAIN END 각각의 EXE 파일 이름
extern char openExeFileName[20];
extern char mainExeFileName[20];
extern char endExeFileName[20];

// 트랙 정보 구축을 위한 로컬 정보
typedef struct
{
	const char* gameType;

	uint32 trackStartPos[MAX_CDAUDIOTRACK + 1];
	uint8 trackStatus[MAX_CDAUDIOTRACK + 1];
	uint8 firstTrack;
	uint8 lastTrack;
	uint32 leadoutTrack;
	uint8 trackNum;
} GameTrackInfo;

GameTrackInfo gameTrackInfo;
static bool isSectionFound = FALSE;

static int iniHandler(void* user, const char* section, const char* name, const char* value)
{
	GameTrackInfo* localGameTrackInfo = (GameTrackInfo*)user;

	int16 FMTrack = 0;
	int16 CDTrack = 0;
	int16 playMarginToSec = 0;

	int8 fromMinute = 0;
	int8 fromSecond = 0;
	int8 fromFrame = 0;

	int8 toMinute = 0;
	int8 toSecond = 0;
	int8 toFrame = 0;

	uint32 trackSector = 0;
	uint32 nextTrackSector = 0;
	uint32 fromSector = 0;
	uint32 toSector = 0;

	char* trackStr;
	char* TrackFromSepPos;
	char* fromStr;
	char* toStr;
	char* FromToSepPos;
	char* token;

	bool isTMSF = 0;

	if (!stricmp(section, localGameTrackInfo->gameType) && 0 != strlen(name) && 0 != strlen(value))
	{
		isSectionFound = TRUE;
		if (!stricmp(name, INI_PLAYMARGIN))
		{
			playMargin = atoi(value);
			playMarginToSec = (playMargin * DEFAULT_FRAME_TO_SEC / 75);
			InfoLog("Play Margin: %d frames ( %d.%d secs )\n", playMargin, (playMarginToSec / DEFAULT_FRAME_TO_SEC), (playMarginToSec % DEFAULT_FRAME_TO_SEC));
		}
		else if (!stricmp(name, INI_OPEN_MUSIC_FILENAME) && strlen(value) < (sizeof(openMusicFileName) / sizeof(char)))
		{
			strcpy(openMusicFileName, value);
			isBufferChangeStyleExist = TRUE;
			isBufferChangeStyle[PLAYSTEP_OPEN] = TRUE;
			InfoLog("Open MUSIC Filename: '%s'\n", openMusicFileName);
		}
		else if (!stricmp(name, INI_OPEN_MUSIC_NUM))
		{
			fmTrackSongNumOpening = atoi(value);
			InfoLog("Open MUSIC Num: %d songs\n", fmTrackSongNumOpening);
		}
		else if (!stricmp(name, INI_MAIN_MUSIC_FILENAME) && strlen(value) < (sizeof(mainMusicFileName) / sizeof(char)))
		{
			strcpy(mainMusicFileName, value);
			isBufferChangeStyleExist = TRUE;
			isBufferChangeStyle[PLAYSTEP_MAIN] = TRUE;
			InfoLog("Main MUSIC Filename: '%s'\n", mainMusicFileName);
		}
		else if (!stricmp(name, INI_MAIN_MUSIC_NUM))
		{
			fmTrackSongNum = atoi(value);
			InfoLog("Main MUSIC Num: %d songs\n", fmTrackSongNum);
		}
		else if (!stricmp(name, INI_END_MUSIC_FILENAME) && strlen(value) < (sizeof(endMusicFileName) / sizeof(char)))
		{
			strcpy(endMusicFileName, value);
			isBufferChangeStyleExist = TRUE;
			isBufferChangeStyle[PLAYSTEP_END] = TRUE;
			InfoLog("End MUSIC Filename: '%s'\n", endMusicFileName);
		}
		else if (!stricmp(name, INI_MAIN_MUSIC_NUM))
		{
			fmTrackSongNumEnding = atoi(value);
			InfoLog("End MUSIC Num: %d songs\n", fmTrackSongNumEnding);
		}
		else if (!stricmp(name, INI_OPEN_EXE_FILENAME) && strlen(value) < (sizeof(openExeFileName) / sizeof(char)))
		{
			strcpy(openExeFileName, value);
			InfoLog("Open EXE Filename: '%s'\n", openExeFileName);
		}
		else if (!stricmp(name, INI_MAIN_EXE_FILENAME) && strlen(value) < (sizeof(mainExeFileName) / sizeof(char)))
		{
			strcpy(mainExeFileName, value);
			InfoLog("Main EXE Filename: '%s'\n", mainExeFileName);
		}
		else if (!stricmp(name, INI_END_EXE_FILENAME) && strlen(value) < (sizeof(endExeFileName) / sizeof(char)))
		{
			strcpy(endExeFileName, value);
			InfoLog("End EXE Filename: '%s'\n", endExeFileName);
		}
		else
		{
			// CD Track No.를 추출
			if (!strnicmp(value, INI_TMSF, strlen(INI_TMSF)))
			{
				isTMSF = 1;

				trackStr = (char*)value + strlen(INI_TMSF);
				TrackFromSepPos = strchr(trackStr, INI_TMSF_SEP);
				fromStr = TrackFromSepPos + 1;
				FromToSepPos = strchr(fromStr, INI_TMSF_SEP);
				toStr = FromToSepPos + 1;

				if (!TrackFromSepPos || !FromToSepPos || !trackStr || !fromStr || !toStr)
				{
					ErrorLog("illegal TMSF Format! %s = '%s' \n", name, value);
					exit(OWNFMDRV_FAIL);
				}

				// 우선 사이사이에 마킹한다.
				*TrackFromSepPos = '\0';
				*FromToSepPos = '\0';

				// 1) 트랙 값을 얻는다.
				CDTrack = atoi(trackStr);

				// 2) From 값을 얻는다.
				token = strtok(fromStr, INI_TMSF_TIMESEP);
				fromMinute = atoi(token);
				token = strtok(NULL, INI_TMSF_TIMESEP);
				fromSecond = atoi(token);
				token = strtok(NULL, INI_TMSF_TIMESEP);
				fromFrame = atoi(token);

				// 3) To 값을 얻는다.
				token = strtok(toStr, INI_TMSF_TIMESEP);
				toMinute = atoi(token);
				token = strtok(NULL, INI_TMSF_TIMESEP);
				toSecond = atoi(token);
				token = strtok(NULL, INI_TMSF_TIMESEP);
				toFrame = atoi(token);
			}
			else
			{
				CDTrack = atoi(value);
			}

			if (CDTrack < localGameTrackInfo->firstTrack || CDTrack > localGameTrackInfo->lastTrack)
			{
				ErrorLog("CD Track Number Limit (%d <= Track <= %d) exceeded! (CDTrack:%d)\n", localGameTrackInfo->firstTrack, localGameTrackInfo->lastTrack, CDTrack);
				exit(OWNFMDRV_FAIL);
			}

			if (0 != (localGameTrackInfo->trackStatus[CDTrack - localGameTrackInfo->firstTrack] & DATATRACK_BIT))
			{
				ErrorLog("CDTrack %d is DATA TRACK!\n", CDTrack);
				exit(OWNFMDRV_FAIL);
			}

			// "OPEN", "END", 숫자 등에 따라 다른 동작을 취함
			if (!strnicmp(name, INI_OPEN, strlen(INI_OPEN)))
			{
				FMTrack = atoi(name + strlen(INI_OPEN));
				InfoLog("Map OPEN %d Track -> CDTrack %d", FMTrack, CDTrack);
				if (FMTrack < 0 || FMTrack >= MAX_CDAUDIOTRACK)
				{
					ErrorLog("FM Track Number Limit (0 <= Track < %d) exceeded! (FMTrack:%d)\n", MAX_CDAUDIOTRACK, FMTrack);
					exit(OWNFMDRV_FAIL);
				}
				trackMapOpening[FMTrack].track = CDTrack - localGameTrackInfo->firstTrack;
				trackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[(int)trackMapOpening[FMTrack].track]);

				if (isTMSF)
				{
					InfoLog("(%02d:%02d:%02d - %02d:%02d:%02d)\n", fromMinute, fromSecond, fromFrame, toMinute, toSecond, toFrame);
					fromSector = CDAudio_Pos2SectorMSF(fromMinute, fromSecond, fromFrame);
					toSector = CDAudio_Pos2SectorMSF(toMinute, toSecond, toFrame);

					trackMapOpening[FMTrack].fromSector = trackSector + fromSector;
					trackMapOpening[FMTrack].toSector = trackSector + toSector;
				}
				else
				{
					InfoLog("\n");
					nextTrackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[trackMapOpening[FMTrack].track + 1]);

					trackMapOpening[FMTrack].fromSector = trackSector;
					trackMapOpening[FMTrack].toSector = nextTrackSector;
				}
				//InfoLog("fromSector: 0x%08lX, toSector: 0x%08lX\n", trackMapOpening[FMTrack].fromSector, trackMapOpening[FMTrack].toSector);
			}
			else if (!strnicmp(name, INI_END, strlen(INI_END)))
			{
				FMTrack = atoi(name + strlen(INI_END));
				InfoLog("Map END %d Track -> CDTrack %d", FMTrack, CDTrack);
				if (FMTrack < 0 || FMTrack >= MAX_CDAUDIOTRACK)
				{
					ErrorLog("FM Track Number Limit (0 <= Track < %d) exceeded! (FMTrack:%d)\n", MAX_CDAUDIOTRACK, FMTrack);
					exit(OWNFMDRV_FAIL);
				}
				trackMapEnding[FMTrack].track = CDTrack - localGameTrackInfo->firstTrack;
				trackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[(int)trackMapEnding[FMTrack].track]);
				if (isTMSF)
				{
					InfoLog("(%02d:%02d:%02d - %02d:%02d:%02d)\n", fromMinute, fromSecond, fromFrame, toMinute, toSecond, toFrame);
					fromSector = CDAudio_Pos2SectorMSF(fromMinute, fromSecond, fromFrame);
					toSector = CDAudio_Pos2SectorMSF(toMinute, toSecond, toFrame);

					trackMapEnding[FMTrack].fromSector = trackSector + fromSector;
					trackMapEnding[FMTrack].toSector = trackSector + toSector;
				}
				else
				{
					InfoLog("\n");
					nextTrackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[trackMapEnding[FMTrack].track + 1]);

					trackMapEnding[FMTrack].fromSector = trackSector;
					trackMapEnding[FMTrack].toSector = nextTrackSector;
				}
				//InfoLog("fromSector: 0x%08lX, toSector: 0x%08lX\n", trackMapEnding[FMTrack].fromSector, trackMapEnding[FMTrack].toSector);
			}
			else
			{
				FMTrack = atoi(name);
				InfoLog("Map FMTrack %d -> CDTrack %d", FMTrack, CDTrack);
				if (FMTrack < 0 || FMTrack >= MAX_CDAUDIOTRACK)
				{
					ErrorLog("FM Track Number Limit (0 <= Track < %d) exceeded! (FMTrack:%d)\n", MAX_CDAUDIOTRACK, FMTrack);
					exit(OWNFMDRV_FAIL);
				}
				trackMap[FMTrack].track = CDTrack - localGameTrackInfo->firstTrack;
				trackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[(int)trackMap[FMTrack].track]);
				if (isTMSF)
				{
					InfoLog("(%02d:%02d:%02d - %02d:%02d:%02d)\n", fromMinute, fromSecond, fromFrame, toMinute, toSecond, toFrame);
					fromSector = CDAudio_Pos2SectorMSF(fromMinute, fromSecond, fromFrame);
					toSector = CDAudio_Pos2SectorMSF(toMinute, toSecond, toFrame);

					trackMap[FMTrack].fromSector = trackSector + fromSector;
					trackMap[FMTrack].toSector = trackSector + toSector;
				}
				else
				{
					InfoLog("\n");
					nextTrackSector = CDAudio_Pos2Sector(localGameTrackInfo->trackStartPos[trackMap[FMTrack].track + 1]);

					trackMap[FMTrack].fromSector = trackSector;
					trackMap[FMTrack].toSector = nextTrackSector;
				}
				//(stderr, "trackSector: 0x%08lX, fromSector: 0x%08lX, toSector: 0x%08lX\n", trackSector, trackMap[FMTrack].fromSector, trackMap[FMTrack].toSector);
			}
		}
	}
	return 1;
}

static uint16 makeFMTrackOffset(char filename[20], uint32 trackOffsetInfo[MAX_CDAUDIOTRACK], uint8 inputSongNum)
{
	//#define MUSIC_FILE_MUSICNUM_SIZE      2
	//#define MUSIC_FILE_METAINFO_SIZE      6

	uint16 alignBuf;
	uint16 hdrLength;
	uint16 songNum;
	uint16 beforeSongLength;

	uint8 i;

	FILE* fp = fopen(filename, "rb");
	if (NULL == fp)
	{
		ErrorLog("cannot open '%s'\n", filename);
		return OWNFMDRV_FAIL;
	}

	// 게임 음악이 미리 몇 개인지 알고 있어야 하는 타입
	// 신들의 대지 고사기외전, 제독의 결단 2, 항유기
	if (0 != inputSongNum)
	{
		// 이 타입은 
		// 2바이트 * 곡 수만큼 시작 오프셋,
		// 2바이트 * 곡 수만큼 데이터 길이
		// 가 저장되는 방식이다.
		songNum = inputSongNum;

		// 음악 시작 오프셋 2바이트씩 크기가 들어있다.
		for (i = 0; i < songNum; ++i)
		{
			// 2byte of offset of track
			if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
			{
				ErrorLog("cannot read '%s'\n", filename);
				return OWNFMDRV_FAIL;
			}
			trackOffsetInfo[i + 1] = alignBuf;
		}
	}
	else
	{
		if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
		{
			ErrorLog("cannot read '%s'\n", filename);
			return OWNFMDRV_FAIL;
		}

		// 노래 갯수가 없는 방식
		if (0 == alignBuf)
		{
			fseek(fp, 0, SEEK_SET);

			songNum = 0;

			trackOffsetInfo[0] = 0;
			beforeSongLength = 0;

			for (i = 0; ; i++)
			{
				// lower byte of offset of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
				trackOffsetInfo[i + 1] = alignBuf;

				// higher byte of offset of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
				trackOffsetInfo[i + 1] += (uint32)(alignBuf) << 16;

				if (trackOffsetInfo[i] + beforeSongLength != trackOffsetInfo[i + 1])
				{
					trackOffsetInfo[i + 1] = 0;
					break;
				}

				songNum++;
				if (songNum >= MAX_CDAUDIOTRACK)
				{
					ErrorLog("'%s' has more than %d songs that is not allowed. (had %d)\n", filename, MAX_CDAUDIOTRACK, songNum);
					return OWNFMDRV_FAIL;
				}
				// length of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
				beforeSongLength = alignBuf;
			}
			//songNum = 1;

			//// 6바이트의 메타 정보를 지나면 노래 시작 버퍼가 된다.
			//hdrLength = MUSIC_FILE_METAINFO_SIZE;
			//trackOffsetInfo[1] = hdrLength;
		}
		// 노래가 여러 개
		else
		{
			songNum = alignBuf;
			if (songNum >= MAX_CDAUDIOTRACK)
			{
				ErrorLog("'%s' has more than %d songs that is not allowed. (had %d)\n", filename, MAX_CDAUDIOTRACK, songNum);
				return OWNFMDRV_FAIL;
			}

			hdrLength = MUSIC_FILE_MUSICNUM_SIZE + songNum * MUSIC_FILE_METAINFO_SIZE;

			for (i = 0; i < songNum; ++i)
			{
				// lower byte of offset of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
				trackOffsetInfo[i + 1] = alignBuf + hdrLength;

				// higher byte of offset of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
				trackOffsetInfo[i + 1] += (uint32)(alignBuf) << 16;

				// length of track
				if (1 != fread((void*)(&alignBuf), sizeof(alignBuf), 1, fp))
				{
					ErrorLog("cannot read '%s'\n", filename);
					return OWNFMDRV_FAIL;
				}
			}
		}
	}

	fclose(fp);
	InfoLog("File '%s' had %d songs.\n", filename, songNum);

	return songNum;
}

int main(int argc, char** argv)
{
	int8 drivenum = 0;
	int i;

	uint8 cddriveLetter[MAX_CDDRIVE];

	ErrorLog("FMDRV Wrapper Version " OWNFMDRV_VER "\n");
	ErrorLog("Copyright (C) 2024 Jeong Jinwook\n\n");

	if (_osmajor < 5)
	{
		ErrorLog("Unsupported DOS version(=%d)! OwnFMDRV requires MS-DOS 5+\n", _osmajor);
		return OWNFMDRV_FAIL;
	}

	memset(&args, 0, sizeof(args));
	args.argc = argc;
	args.argv = argv;
	args.targetcddrive = MAX_CDDRIVE;

	if (OWNFMDRV_FAIL == parseargv(&args))
	{
		ErrorLog("\n");
		ErrorLog("Usage:\n");
		ErrorLog("\tOWNFMDRV.EXE <GameType in OWNFMDRV.INI> [options]\n");
		ErrorLog("\n");
		ErrorLog("Options:\n");
		ErrorLog("\t/v=XXX\tSet CD-DA Volume\n");
		ErrorLog("\t\tCaution: 0 <= CD Volume <= 255. Default=100\n");
		ErrorLog("\t/t=X[:]\tSet Target CD-DA Drive (skip if there is just one cd drive.)\n");
		ErrorLog("\t/s\tLog Verbose mode(print-at-all)\n");
		ErrorLog("\t/h\tInstall OwnFMDRV in upper memory\n");
		ErrorLog("\t/u\tUnload OwnFMDRV from memory\n");
		ErrorLog("\tex)\n");
		ErrorLog("\t\tOWNFMDRV.EXE LEMPEREUR /v=100 /t=G: /h");
		ErrorLog("\n");
		return OWNFMDRV_FAIL;
	}

	if (IsTSRInstalled())
	{
		if ((args.flags & ARGFL_UNLOAD) == 0)
		{
			ErrorLog("OwnFMDRV already installed!\n");
			return OWNFMDRV_FAIL;
		}
		// TODO: Unload 구현
		return OWNFMDRV_SUCCESS;
	}

	// INI 정보 읽어들이기
	orginalCDVolume = args.volume == 0 ? (uint8)DEFAULT_VOLUME : (uint8)args.volume;
	InfoLog("Set CD Volume to %d\n", orginalCDVolume);

	isBufferChangeStyleExist = FALSE;
	memset((void*)isBufferChangeStyle, FALSE, sizeof(int8) * PLAYSTEP_MAX);
	fmTrackSongNumOpening = 0;
	fmTrackSongNum = 0;
	fmTrackSongNumEnding = 0;
	memset((void*)openMusicFileName, 0, sizeof(char) * sizeof(openMusicFileName));
	memset((void*)mainMusicFileName, 0, sizeof(char) * sizeof(mainMusicFileName));
	memset((void*)endMusicFileName, 0, sizeof(char) * sizeof(endMusicFileName));

	strcpy(openExeFileName, DEFAULT_OPEN_EXE_FILENAME);
	strcpy(mainExeFileName, DEFAULT_MAIN_EXE_FILENAME);
	strcpy(endExeFileName, DEFAULT_END_EXE_FILENAME);

	memset((void*)trackMap, -1, sizeof(CD_TMSF) * MAX_CDAUDIOTRACK);
	memset((void*)trackMapOpening, -1, sizeof(CD_TMSF) * MAX_CDAUDIOTRACK);
	memset((void*)trackMapEnding, -1, sizeof(CD_TMSF) * MAX_CDAUDIOTRACK);
	memset((void*)&gameTrackInfo, 0, sizeof(GameTrackInfo));
	gameTrackInfo.gameType = argv[1];

	drivenum = CDAudio_GetCDROMNum(&cddrive);
	if (!drivenum || drivenum >= MAX_CDDRIVE || cddrive >= MAX_CDDRIVE)
	{
		ErrorLog("CD-ROM drive not exist.\n");
		return OWNFMDRV_FAIL;
	}

	// CDROM이 있을 경우 MSCDEX 버전 체크
	{
		int8 cd_majorver = 0;
		int8 cd_minorver = 0;

		if (CDAUDIO_FAIL == CDAudio_GetCDROMDriverVersion(&cd_majorver, &cd_minorver))
		{
			ErrorLog("Cannot get CDROM Driver Version! OwnFMDRV requires CDROM Driver(MSCDEX) version 2.1+\n");
			return OWNFMDRV_FAIL;
		}
		InfoLog("CD Driver Version :%d.%d\n", cd_majorver, cd_minorver);

		if (cd_majorver < 2 || cd_minorver < 1)
		{
			ErrorLog("Unsupported CD(%d.%d) version! OwnFMDRV requires CDROM Driver(MSCDEX) version 2.1+\n", cd_majorver, cd_minorver);
			return OWNFMDRV_FAIL;
		}
	}

	// Get CD Information
	if (drivenum > 1)
	{
		CDAudio_GetCDROMDriveList(cddriveLetter);

		InfoLog("Founded CD-ROM Drive: ");
		for (i = 0; i < drivenum; ++i)
		{
			InfoLog("%c:\\ ", 'A' + cddriveLetter[i]);
		}
		InfoLog("\n");

		for (i = 0; i < drivenum; ++i)
		{
			if (cddriveLetter[i] == args.targetcddrive)
			{
				cddrive = cddriveLetter[i];
				break;
			}
		}

		if (MAX_CDDRIVE == args.targetcddrive)
		{
			ErrorLog("There are many CD-ROM drives!\n");
			ErrorLog("You must designate target drive letter using /t option!\n");
			ErrorLog("# of drives: %d ( ", drivenum);
			for (i = 0; i < drivenum; ++i)
			{
				ErrorLog("%c:\\ ", 'A' + cddriveLetter[i]);
			}
			ErrorLog(")\n");
			return OWNFMDRV_FAIL;
		}
		else if (i == drivenum)
		{
			ErrorLog("Your /t=%c designated target drive not found! drive num:%d\n", args.targetcddrive, drivenum);
			return OWNFMDRV_FAIL;
		}
		else
		{
			InfoLog("Select %c:\\\n", 'A' + cddrive);
		}
	}

	if (CDAUDIO_FAIL == CDAudio_GetDiskInfo(cddrive, &gameTrackInfo.firstTrack, &gameTrackInfo.lastTrack, &gameTrackInfo.leadoutTrack))
	{
		printf("Get DiskInfo from '%c:\\' Failed!\n", 'A' + cddrive);
		return OWNFMDRV_FAIL;
	}
	gameTrackInfo.trackNum = (gameTrackInfo.lastTrack - gameTrackInfo.firstTrack + 1);

	InfoLog("CD-ROM (%c:\\) Total Track: %d \n", 'A' + cddrive, gameTrackInfo.trackNum);
	InfoLog("FirstTrack: %d\n", gameTrackInfo.firstTrack);
	InfoLog("LastTrack: %d\n", gameTrackInfo.lastTrack);

	memset((void*)&gameTrackInfo.trackStartPos, 0, sizeof(uint32) * MAX_CDAUDIOTRACK);
	memset((void*)&gameTrackInfo.trackStatus, 0, sizeof(uint8) * MAX_CDAUDIOTRACK);

	if (CDAUDIO_FAIL == CDAudio_GetTrackInfo(cddrive, gameTrackInfo.firstTrack, gameTrackInfo.lastTrack, gameTrackInfo.trackStartPos, gameTrackInfo.trackStatus))
	{
		ErrorLog("Get TrackInfo from '%c:\\' Failed!\n", 'A' + cddrive);
		return OWNFMDRV_FAIL;
	}

	// CD의 끝을 의미하는 leadoutTrack을 trackStartPos의 맨 마지막에 넣어주자.
	gameTrackInfo.trackStartPos[gameTrackInfo.trackNum] = gameTrackInfo.leadoutTrack;

	if (ini_parse(DEFAULT_INI, iniHandler, (void*)&gameTrackInfo) < 0)
	{
		ErrorLog("cannot load %s!\n", DEFAULT_INI);
		return -1;
	}
	if (FALSE == isSectionFound)
	{
		ErrorLog("cannot found [%s] section in %s!\n", gameTrackInfo.gameType, DEFAULT_INI);
		return -1;
	}

	InfoLog("Open EXE Filename: '%s'\n", openExeFileName);
	InfoLog("Main EXE Filename: '%s'\n", mainExeFileName);
	InfoLog("End EXE Filename: '%s'\n", endExeFileName);

	if (isBufferChangeStyleExist)
	{
		// 1. Open 작업
		if (isBufferChangeStyle[PLAYSTEP_OPEN])
		{
			if (0 == strlen(openMusicFileName))
			{
				ErrorLog("Open MUSIC Filename not valid!\n");
				return OWNFMDRV_FAIL;
			}

			InfoLog("Open MUSIC Filename: '%s'\n", openMusicFileName);

			fmTrackSongNumOpening = makeFMTrackOffset(openMusicFileName, fmTrackOffsetOpening, fmTrackSongNumOpening);
			if (OWNFMDRV_FAIL == fmTrackSongNumOpening)
			{
				ErrorLog("cannot make track offset from '%s' file. Maybe not exist?\n", openMusicFileName);
				return OWNFMDRV_FAIL;
			}
		}

		// 2. Main 작업
		if (isBufferChangeStyle[PLAYSTEP_MAIN])
		{
			if (0 == strlen(mainMusicFileName))
			{
				ErrorLog("Main MUSIC Filename not valid!\n");
				return OWNFMDRV_FAIL;
			}

			InfoLog("Main MUSIC Filename: '%s'\n", mainMusicFileName);

			fmTrackSongNum = makeFMTrackOffset(mainMusicFileName, fmTrackOffset, fmTrackSongNum);
			if (OWNFMDRV_FAIL == fmTrackSongNum)
			{
				ErrorLog("cannot make track offset from '%s' file. Maybe not exist?\n", mainMusicFileName);
				return OWNFMDRV_FAIL;
			}
		}

		// 2. End 작업
		if (isBufferChangeStyle[PLAYSTEP_END])
		{
			if (0 == strlen(endMusicFileName))
			{
				ErrorLog("End MUSIC Filename not valid!\n");
				return OWNFMDRV_FAIL;
			}

			InfoLog("End MUSIC Filename: '%s'\n", endMusicFileName);

			fmTrackSongNumEnding = makeFMTrackOffset(endMusicFileName, fmTrackOffsetEnding, fmTrackSongNumEnding);
			if (OWNFMDRV_FAIL == fmTrackSongNumEnding)
			{
				ErrorLog("cannot make track offset from '%s' file. Maybe not exist?\n", endMusicFileName);
				return OWNFMDRV_FAIL;
			}
		}
	}

	CDAudio_SetVolume(cddrive, orginalCDVolume);

	StartTSR();
	// Unreachable code
	return OWNFMDRV_SUCCESS;
}
