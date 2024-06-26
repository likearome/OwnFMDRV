# OwnFMDRV
코에이 IBM MS-DOS게임들의 BGM을 CD-DA로 바꾸는 프로그램

# 사용 방법
- FMDRV.COM가 있는 게임과 같은 디렉토리에 OwnFMDRV.EXE와 OwnFMDRV.INI를 복사합니다.
- OwnFMDRV.INI에, FM 트랙과 일치하도록 정보를 적어넣고 Section 네임을 정합니다. (아래 INI파일 항목 추가 방법 참조)
  - 예) SAM3, SAM4, SAM4DEN(전뇌전격편) 등...
- 디렉토리 위에서 다음과 같이 실행합니다.
  - OwnFMDRV <INI파일에 기록된 섹션 네임> [옵션]
    - 옵션들 간에는 모두 띄어쓰기가 필요합니다.
  - 예) ```OwnFMDRV SAM4DEN /H /T=G: /V=100 /S```
  - 옵션: (옵션은 모두 대소문자를 구별하지 않습니다.)
    - /V=[숫자]
      - 게임 중 CD의 최대 볼륨을 설정합니다.
      - FM 효과음과의 볼륨 차이를 조절하는 데 유용합니다.
      - 볼륨 조절과 관련된 문제
        - DOSBOX와 윈도우 9x에서는 정상 동작합니다.
        - XDVD2.SYS, UDVD2.SYS등의 FreeDOS계열 드라이버들은 문제가 있습니다.
          - 이 드라이버들은 볼륨 조절 명령(INT 2F-1510 SubFunc3 / 드라이버에서 MODE SELECT로 translate됨)을 기기로 전달하지 않습니다.
          - 그래서 볼륨 조절이 정상 동작하지 않기 때문에, OwnFMDRV를 이들 드라이버와 함께 사용하는 것은 권장되지 않습니다.
        - 호환성이 높은 OAKCDROM.SYS나, 점유 용량이 적은 VIDE-CDD.SYS를 추천합니다.
    - /T=[오디오CD가 들어있는 드라이브]
      - 컴퓨터에 CD 드라이브가 하나 있을 경우에는 생략 가능합니다.
      - 컴퓨터에 CD 드라이브가 2개 이상 세팅된 경우에는 반드시 입력하여야 합니다.
        - 물리 CD 드라이브 1개, 데몬 툴 1개 등으로 2개 이상이 될 수 있습니다.
      - 대소문자를 구별하지 않습니다.
    - /S
      - 모든 로그 표시
      - 기술적인 분석이 필요하시면 켜고 보시면 좋습니다.
    - /H
      - UMB(상위 메모리)에 로드합니다.
      - /H를 사용할 경우 상위 메모리에 약 9kB 정도만 남아 있어도 상위 메모리에 올라갑니다.
      - /H없이 도스의 LH(LoadHigh)명령을 통해서도 상위메모리로 올릴 수 있습니다.
        - 이 경우, 최초 로드 시점에 exe파일 (약 35kB)만큼의 UMB가 없으면 그냥 기본 메모리에 올라갑니다.
        - LH로 올리면 일단 UMB에 35kB가 로드된 다음, 9kB만 남기고 모두 free됩니다.
      - 프로그램 특성상 초기화 로직이 상당히 크기 때문에 여러 TSR 트릭을 사용하여 구현하였습니다. (Thanks for EtherDFS)
    - /U
      - 이미 올라가 있는 OwnFMDRV를 메모리에서 해제합니다.
      - 섹션명 없이 /U를 사용하여도 정상 동작합니다.
# 시연 영상
- 삼국지 영걸전의 DOSBOX-X 실행 영상
  - [![Video Label](http://img.youtube.com/vi/hhRVDfjSdKA/0.jpg)](https://youtu.be/hhRVDfjSdKA?t=0s)

- 삼국지 3의 DOSBOX-X 실행 영상
  - [![Video Label](http://img.youtube.com/vi/9q0iQJApq7M/0.jpg)](https://youtu.be/9q0iQJApq7M?t=0s)

- 삼국지 4의 실제기기 윈도우 98에서 실제 CD를 가지고 실행한 영상
  - [![Video Label](http://img.youtube.com/vi/J4qxaqdfRV8/0.jpg)](https://youtu.be/J4qxaqdfRV8?t=0s)

- 삼국지 4의 실제기기 윈도우 98에서 데몬 툴을 사용하여 실행한 영상
  - [![Video Label](http://img.youtube.com/vi/9yDCxxPBXms/0.jpg)](https://youtu.be/9yDCxxPBXms?t=0s)

# INI 파일 항목 추가 방법
- 예시
<img width="468" alt="image" src="https://github.com/likearome/OwnFMDRV/assets/39750066/04f9703a-eff6-4a90-9259-fa8c3ff8fb59">

- 섹션을 대괄호 안에 영어로 입력합니다.
  - 섹션은 게임 제목과 연관이 있는 것으로 지으면 관리가 편합니다.
  - 이 섹션명이 차후에 OwnFMDRV를 실행할 때 입력할 파라미터가 됩니다.
- [고급] PLAYMARGIN
  - 대부분의 경우 필요하지 않습니다.
  - 오디오 CD의 구성상 앞부분에 쓸데없는 무음 영역이 많을 경우에 사용합니다.
  - PLAYMARGIN에 무음 영역만큼의 값을 주면 전체 트랙의 노래 시작지점을 그만큼 스킵 후 재생하게 됩니다.
    - 단위는 CD의 기본 단위인 "프레임" 입니다. 1초는 75프레임입니다.
  - 일부 오디오 CD외에는 거의 필요하지 않으므로 일반적인 경우에는 사용하실 필요가 없습니다.
  - [참고] 현재 OwnFMDRV는 실제 트랙 시작부분의 앞 2초부터 연주를 시작하고 있습니다.
    - DOS의 지연시간 등으로 인한 정확하지 않은 노래 시작을 막기 위해 앞부분에 일정량의 무음을 추가하기 위함입니다.
    - 대부분의 시스템에서는 이렇게 앞 2초로 실행해도 실제 정확한 위치에서 노래가 시작될 걸로 생각됩니다.
    - 만약 시스템에 따라 앞 트랙 노래가 조금 연주된다던가 하는 느낌이 드시면 PLAYMARGIN에 적절한 값을 설정해 보는 것도 방법입니다.
- 이제 버퍼 변경형인지 체크하지 않아도 됩니다.
  - 음악파일이 별도로 존재하는 게임의 경우 아래 항목들을 있는만큼만 채우면 됩니다.
    - OPEN_MUSIC_FILENAME
    - MAIN_MUSIC_FILENAME
    - END_MUSIC_FILENAME
  - 신들의 대지:고사기외전, 제독의 결단 2, 항유기와 같은 게임들은 오프닝/엔딩은 EXE파일에, 인게임 배경음악은 별도의 음악파일에 있는 이상한 구조인데 거기에 노래 중복도 심하고 노래 갯수도 노래파일안에 없는 형태입니다.
  - 따라서 위와 같은 게임들은 노래 갯수를 알아낼 필요가 있습니다.
    - 일단 신들의 대지는 32개, 제독의 결단 2는 23개, 항유기는 15개로 알려져 있습니다.
- 각 실행파일들을 입력합니다.
  - 코에이 게임은 대체로 OPEN.EXE , MAIN.EXE , END.EXE 로 구성됩니다.
  - 아래 항목에 오프닝, 게임메인, 엔딩 실행파일 이름을 입력합니다.
    - OPEN_EXE_FILENAME
    - MAIN_EXE_FILENAME
    - END_EXE_FILENAME
  - 해당 게임의 오프닝, 게임메인, 엔딩 실행파일이름이 위와 같으면 예시와 같이 비워놓으셔도 됩니다.
- 오프닝 곡에 매칭되는 CD 트랙 번호를 입력합니다.
  - 오프닝 곡은 아래와 같은 항목에 입력합니다.
    - OPENn (OPEN1, OPEN2, ...)
    - OPEN 뒤에 붙는 숫자가 FM Track의 번호입니다.
  - Audio CD에서의 위치를 입력하는 방법은 2가지가 있습니다.
    - 숫자
      - 심플하게 트랙번호를 의미합니다.
      - 해당 트랙의 시작부터 끝까지 모두 연주하고, 노래가 끝나면 루프를 돌도록 합니다.
    - TMSF (Track:Minute:Second:Frame)
      - 실제로 CD에서 특정 위치를 지목할 때 이런 타입으로 표현합니다.
      - 몇몇 For Windows 95 CD들이 위와 같이 배경음악 몇 개를 한 트랙에 믹싱해놓고, 이를 쪼개서 배경음악으로 사용합니다.
        - 대표적으로 삼국지 4 PK For Windows95, 삼국지 영걸전 For Windows95가 그렇습니다.
      - TMSF로 입력할 때의 포맷은 다음과 같습니다.
        - [트랙번호][시작분][시작초][시작프레임]-[끝분][끝초][끝프레임]
        - 위 예시를 참고하세요.
    - 입력하지 않을 경우 그 노래는 FM 사운드 그대로 재생됩니다.
- 메인 곡에 매칭되는 CD 트랙 번호를 입력합니다.
  - 메인 곡은 아래와 같은 항목에 입력합니다.
    - n (1, 2, 3, ...)
    - 숫자가 FM Track의 번호입니다.
  - Audio CD에서의 위치를 입력하는 방법은 오프닝 곡과 같습니다.
- 엔딩 곡에 매칭되는 CD 트랙번호를 입력합니다.
  - 엔딩 곡은 아래와 같은 항목에 입력합니다.
    - ENDn (END1, END2, END3, ...)
    - 숫자가 FM Track의 번호입니다.
  - Audio CD에서의 위치를 입력하는 방법은 오프닝 곡과 같습니다.
- 가독성을 위해 다음 섹션과는 1칸 띄웁니다.
# 적용 범위
FMDRV.COM을 이용하는 코에이의 모든 IBM 도스게임이 이론적으로 가능합니다.
OwnFMDRV.INI에 추가하셔서 사용하실 수 있습니다.
현재는 본 개발자가 플레이한 몇 가지 게임만 등록돼 있습니다.
아래 게임들을 대응시킬 수 있을 것으로 예상됩니다.
### 사운드웨어(CD OST)가 있는 게임 (FMDRV.COM/SBOPL2.COM 등 존재)
- 삼국지2/3/4
  - 4는 OST 부제가 전뇌전격편
- 대항해시대 1/2
- 노부나가의 야망 4(무장풍운록)/5(패왕전)
  - 4 의 경우 DOS/V 일본어 버전은 배경음악이 나오지 않으며 중문판은 배경음악이 있습니다.
- 유럽전선
- 제독의 결단 1/2
- 로얄블러드 1
- 랑펠로
- 태합입지전 1/2
- 항유기
- 징기스칸(푸른 늑대와 흰 사슴) 2, 3:원조비사 (일본기준3/한국기준2가 원조비사)
- 이인도: 타도 노부나가
### IBM DOS/V로 출시된 이후 CD-DA 대응 윈도우 리메이크 버전이 출시된 게임
- 삼국지 2/3/4
  - 4는 대응 가능한 CD-DA가 윈도우판/전뇌전격편으로 2개입니다.
- 삼국지 영걸전
- 제독의 결단 3
- 켈트의 전설
  - 윈도우판은 한국어화 정식발매. DOS판은 영문버전
- 대항해시대 2
### 사운드웨어가 없는 IBM DOS/V 게임 (다른 플랫폼의 사운드를 녹음해서 가져올 수 있음)
- 겐페이 전쟁
- 에어 매니지먼트 1/2
  - 에어 매니지먼트 1은 FM-TOWNS판에서 오프닝/엔딩 곡을 CD-DA 오디오트랙으로 제공했습니다.
- 독립전쟁: 자유 아니면 죽음
### 대응되지 않는 게임
#### 사운드웨어가 있지만 IBM DOS/V로 발매되지 않은(PC98전용) 게임
- 톱 매니지먼트
- 안젤리크
#### FMDRV.COM 혹은 SBOPL2.COM 등이 없는 게임(자체적으로 배경음악이 나오지 않는 게임)
- 수호전:천명의 맹세
  - 중문판, 영문판, 한국어판을 모두 찾아봤으나 FMDRV.COM이 없었습니다.
  - CD-DA급 음질의 배경음악이 나오는 한국어 번역 완료된 윈도우 리메이크 버전이 존재합니다.
    - https://www.youtube.com/watch?v=XFHjkhCF2f4
- 노부나가의 야망 2:전국판, 3: 전국군웅전
  - 두 게임 모두 FMDRV.COM이 없습니다.
  - 노부나가의 야망 35주년 기념판/스팀판으로
    - 2편은 PC98 에뮬레이터급 배경음악이 나오는 윈도우 버전이 존재합니다. (일어판)
      - https://www.youtube.com/watch?v=AC07lb6yy1k
    - 3편은 CD-DA급 음질의 배경음악이 나오는 윈도우 버전이 존재합니다. (일어판)
      - https://www.youtube.com/watch?v=qMp46PoEYCQ
- 노부나가의 야망 4: 무장풍운록
  - 4의 경우 일본어 DOS/V판에는 배경음악이 나오지 않고, 대만 수출판(중문판)에서는 배경음악이 나옵니다.
  - 노부나가의 야망 35주년 기념판/스팀판으로 CD-DA급 음질의 배경음악이 나오는 윈도우버전이 존재합니다. (일어판)
    - https://www.youtube.com/watch?v=1sojWzdanxI
- 삼국지 1
- 징기스칸 2 (일본기준 2, 한국 기준 1)
  
### 그외
- 코에이는 FM-TOWNS 발매 게임의 CD에 기본적으로 게임 + 사운드웨어형태로 제공하였습니다.
  - 이들 게임은 1번 트랙에 게임, 그 외 트랙에 사운드가 있는 형태로 제공되었기 때문에, 별도의 오디오CD로 제공되었던 사운드웨어와 트랙번호가 다릅니다.
  - 만약 이러한 CD를 구하셨을 경우 원본 오디오 CD와의 트랙과 어떤 부분의 차이가 있는지 체크하여 수정하실 필요가 있습니다.
  - 별도의 사운드웨어가 발매되지 않았으나 FM-TOWNS로 오디오 트랙이 제공된 게임은 현재까지 단 하나 발견되었습니다.
    - 에어 매니지먼트 1
- PC98의 OPN이나 Playstation, 슈퍼패미컴의 사운드 등을 CD이미징하여 들으실 수도 있습니다.
  - 대표적으로 대항해시대1/2, 삼국지 등
- 실제로 슈퍼패미컴(SNES)의 사운드를 녹음하여 발매된 사운드웨어들도 있습니다. (KOEI Original BGM Collection 시리즈)
  - 일반 사운드웨어 대비 크게 기대하시면 안됩니다.
# 참조한 것들
## 책
- https://www.yes24.com/Product/Goods/133864
  - C로 하드웨어 주무르기 (김석주) - 절판됨
  - IBM DOS의 하드웨어(CD 등) 인터럽트 사용법 및 설명, 예제 등이 충실한 책
- https://product.kyobobook.co.kr/detail/S000001093768
  - IBM PC로 창조하는 음악의 세계 (김종수) - 지금도 판매중
  - OPL3, 인텔리전트 MIDI등에 대한 자세한 설명을 볼 수 있는 책
## 문서
- http://www.techhelpmanual.com/95-interrupts_and_bios_services.html
  - IBM DOS Interrupt 매뉴얼
- https://bochs.sourceforge.io/techspec/PORTS.LST
  - IBM DOS "알려진(well-known)" 장치들의 사용 포트 번호
- https://www.vogons.org/viewtopic.php?p=1231161#p1231161
  - 윈도우95/98에서 이미지 탑재하고 도스 게임에서 CD-DA 듣는 법
- https://www.compuphase.com/int70.txt
  - 문서화가 잘 되어 있지 않은 인터럽트 70번 (Real Time Clock)에 대한 자세한 사용법
## 라이브러리
- https://github.com/benhoyt/inih
  - 심플 INI 파서
  - 소스코드 인용
- https://github.com/davidebreso/etherdfs-client
  - 도스 기반 리눅스 네트워크 드라이브 마운트 프로그램
  - TSR 아이디어 인용
# 빌드 방법
  - [Open Watcom v2.0](https://github.com/open-watcom/open-watcom-v2) 사용
  - 윈도우/리눅스 모두에서 빌드 가능
    - MakeFile 상단의 WATCOM 디렉토리 수정 등을 통해 대응
    - 윈도우 Open Watcom 사용 추천
    - 에디터로 Visual Studio 2022 사용. Visual Studio 2022에서 Makefile프로젝트로 생성
   
# 제한 사항
<s>- 메모리 사용량을 줄이기 위해 CD 오디오트랙 최대 갯수를 42개로 설정했습니다.
  - 현재까지 발매된 사운드웨어들 중 가장 많은 트랙을 기준으로 빌드
  - 더 많은 오디오트랙이 필요하시면 Makefile을 수정하여 최대 98개까지 올릴 수 있습니다.</s>
- 이제 메모리 사용량이 많이 조정되었으므로 기본 오디오트랙 갯수를 98개로 조정하였습니다.
