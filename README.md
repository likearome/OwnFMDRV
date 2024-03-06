# OwnFMDRV
코에이 IBM MS-DOS게임들의 BGM을 CD-DA로 바꾸는 프로그램
# 사용 방법
- FMDRV.COM가 있는 게임과 같은 디렉토리에 OwnFMDRV.EXE와 OwnFMDRV.INI를 복사합니다.
- OwnFMDRV.INI에, FM 트랙과 일치하도록 정보를 적어넣고 Section 네임을 정합니다. (아래 INI파일 항목 추가 방법 참조)
  - 예) SAM3, SAM4, SAM4DEN(전뇌전격편) 등...
- 디렉토리 위에서 다음과 같이 실행합니다.
  - OwnFMDRV <INI파일에 기록된 섹션 네임> [옵션]
  - 예) ```OwnFMDRV SAM4DEN /H /T=G: /V=100 /S```
  - 옵션:
    - /V=[숫자]
      - CD 볼륨 조절
      - CD의 자체 DAC 볼륨을 조절하는 기능입니다.
      - CD드라이브가 사운드카드와 디지털 S/PDIF로 연결돼 있으면 볼륨조절이 되지 않습니다.
      - 윈도우에서 데몬툴 등으로 이미지를 삽입하고 실행시에도 볼륨 조절이 되지 않습니다.
    - /T=[오디오CD가 들어있는 드라이브]
      - 컴퓨터에 CD 드라이브가 하나 있을 경우에는 생략 가능합니다.
      - 컴퓨터에 CD 드라이브가 2개 이상 세팅된 경우에는 반드시 입력하여야 합니다.
        - 물리 CD 드라이브 1개, 데몬 툴 1개 등으로 2개 이상이 될 수 있습니다.
    - /S
      - 모든 로그 표시
      - 기술적인 분석이 필요하시면 켜고 보시면 좋습니다.
    - /H
      - UMB(상위 메모리)에 로드합니다.
      - /H없이 도스의 LH(LoadHigh)명령을 통해서도 상위메모리로 올릴 수 있습니다.
        - 이 경우, 최초 로드 시점에 exe파일 (약 32kB)만큼의 UMB가 없으면 로드되지 않습니다.
        - 일단 32kB가 로드된 다음, 8kB만 남기고 모두 free됩니다.
      - /H를 사용할 경우 상위 메모리에 약 8kB 정도만 남아 있어도 상위 메모리에 올라갑니다.
      - 프로그램 특성상 초기화 로직이 상당히 크기 때문에 여러 TSR 트릭을 사용하여 구현하였습니다. (Thanks for EtherDFS)
# INI 파일 항목 추가 방법
# 적용 범위
FMDRV.COM을 이용하는 코에이의 모든 IBM 도스게임이 이론적으로 가능합니다.
OwnFMDRV.INI에 추가하셔서 사용하실 수 있습니다.
현재는 본 개발자가 플레이한 몇 가지 게임만 등록돼 있습니다.
아래 게임들을 대응시킬 수 있을 것으로 예상됩니다.
### 사운드웨어(CD OST)가 있는 게임 (FMDRV.COM/SBOPL2.COM 등 존재)
- 삼국지1/2/3/4
  - 4는 OST 부제가 전뇌전격편
- 대항해시대 1/2
- 노부나가의 야망 2(전국판)/3(전국군웅전)4(무장풍운록)/5(패왕전)
  - 4의 IBM DOS/V 영문판은 FM배경음악이 나오지 않음. 중문판은 나옴.
- 겐페이 합전
- 유신의 폭풍
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
  - 4는 대응 가능한 CD-DA가 윈도우판/전뇌전격편으로 2개이다.
- 삼국지 영걸전
- 제독의 결단 3
- 켈트의 전설
  - 윈도우판은 한국어화 정식발매. DOS판은 영문버전
- 대항해시대 2
### 사운드웨어가 있지만 IBM DOS/V로 발매되지 않은(PC98전용) 게임
- 톱 매니지먼트
- 안젤리크
### 그외
- 수호전:천명의 맹세는 사운드웨어가 있지만 FMDRV.COM 이 있는 DOS/V 버전을 찾을 수 없었습니다.
  - 중문판도 영문판도 한국어판도 모두 FMDRV.COM이 없네요.
- PC98의 OPN이나 Playstation, 슈퍼패미컴의 사운드 등을 CD이미징하여 들으실 수도 있습니다.
- 실제로 슈퍼패미컴(SNES)의 사운드를 녹음하여 발매된 사운드웨어들도 있습니다. (KOEI Original BGM Collection 시리즈)
  - 일반 사운드웨어 대비 크게 기대하시면 안됩니다.
- 대표적으로 대항해시대1/2, 삼국지 등
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
  - 가장 정확한 IBM DOS Interrupt 매뉴얼
- https://bochs.sourceforge.io/techspec/PORTS.LST
  - 가장 정확한 IBM DOS "알려진(well-known)" 장치들의 사용 포트 번호
- https://www.vogons.org/viewtopic.php?p=577721#p577721
  - 윈도우95/98에서 이미지 탑재하고 도스 게임에서 CD-DA 듣는 법
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
- 메모리 사용량을 줄이기 위해 CD 오디오트랙 최대 갯수를 42개로 설정했습니다.
  - 현재까지 발매된 사운드웨어들 중 가장 많은 트랙을 기준으로 빌드
  - 더 많은 오디오트랙이 필요하시면 Makefile을 수정하여 최대 98개까지 올릴 수 있습니다.
  - 올리면 올릴수록 메모리 사용량이 증가합니다.
