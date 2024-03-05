/////////////////////////////////////////
// 이 코드는 메모리에 남는다.
#pragma code_seg(BEGTEXT, CODE)

// 유틸리티 함수
// memcpy
void __declspec(naked) mymemcpy(void far* dst, void far* src, unsigned int len) {
	(void)dst;
	(void)src;
	(void)len;
	_asm {
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

// strlen
unsigned short __declspec(naked) mystrlen(void far* s) {
	(void)s;	// es:di

	_asm {
		//스택에 레지스터와 플래그 값을 보존한다.
		pushf
		cld                // si/di 증가쪽 플래그로 세팅
		mov al, 0          // Zero 터미네이터 세팅
		mov cx, 0xFFFF     // CX에 uint16 최대 길이값을 세팅
		repne scasb        // 문자열에서 Zero 터미네이터(NULL)를 탐색
		neg cx             // 위 명령에 의해 얻은 CX값을 (-CX - 2)로 계산하면 문자열 길이를 얻는다.
		dec cx
		dec cx

		// 보존했던 레지스터/플래그를 복원한다.
		popf
		ret
	}
}
#pragma aux mystrlen parm [es di] value [cx] modify exact [ax cx di] nomemory;

int mystrstr(char far* srcFilePtr, char far* dstFilePtr)
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

int mystrcmp(const char far* s, const char far* t)
{
	for (; *s == *t; s++, t++)
		if (*s == '\0')
			return(0);
	return (*s - *t);
}

void __declspec(naked) (__interrupt __far* mygetvect(unsigned interruptnum))()
{
	(void)interruptnum;

	_asm
	{
		mov ah, 35h
		int 21h
	}
}
#pragma aux mygetvect parm [ax] value [es bx] modify exact [ax es bx] nomemory;

void __declspec(naked) mysetvect(unsigned interruptnum, void (__interrupt __far* vect)())
{
	(void)interruptnum;
	(void)vect;

	_asm
	{
		push ds
		mov ds, cx
		mov ah, 25h
		int 21h
		pop ds
	}
}
#pragma aux mysetvect parm caller [ax] [cx dx] modify exact [ax] nomemory;

// allocseg로 확보한 세그먼트를 free한다.
void freeseg(unsigned short segm)
{
	_asm {
		mov ah, 49h   // 49h 세그먼트 free MS-DOS 2+
		mov es, segm  // 주어진 세그먼트 값을 es에 입력
		int 21h
	}
}

//uint8 IsCtrlPressed(void)
//{
//	uint16 keyStat = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
//	return (0 != (keyStat & 0x04)) ? 1 : 0;
//}
//
//uint8 IsLeftShiftPressed(void)
//{
//	uint16 keyStat = _bios_keybrd(_KEYBRD_SHIFTSTATUS);
//	return (0 != (keyStat & 0x02)) ? 1 : 0;
//}