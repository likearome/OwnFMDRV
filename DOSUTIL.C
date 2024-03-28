#include"DOSUTIL.H"
#include"FMDRVDEF.H"
#include <i86.h>

/////////////////////////////////////////
// 이 코드는 메모리에 남는다.
#pragma code_seg(BEGTEXT, CODE)

// E9 포트는 Bochs와 DosBox에서 디버그 아웃풋 용으로 사용되는 포트이며,
// 실 기기에서는 아무런 동작도 하지 않는 포트이다.

void _puts(const char* str)
{
	union REGPACK r;
	for (; *str; str++)
	{
		r.h.al = *str;
		_asm
		{
			out 0xE9, AL;
		}

		if (r.h.al == '\n')
		{
			_asm
			{
				mov AL, 0dh;
				out 0xE9, AL;
			}
		}
	}
	return;
}

void _putchar(char c)
{
	_asm
	{
		mov AL, c;
		out 0xE9, AL;
	}

	if (c == '\n')
	{
		_asm
		{
			mov AL, 0dh;
			out 0xE9, AL;
		}
	}

	return;
}

void _putonehex(unsigned hex)
{
	char hexval;

	hexval = (hex & 0xF0) >> 4;
	if (hexval < 10)
		_putchar('0' + hexval);
	else
		_putchar('A' + hexval - 10);

	hexval = (hex & 0x0F);
	if (hexval < 10)
		_putchar('0' + hexval);
	else
		_putchar('A' + hexval - 10);
}

void _put8hex(unsigned hex)
{
	_putchar('0'); _putchar('x');

	_putonehex(hex);
	return;
}

void _put32hex(uint32 hex)
{
	_putchar('0'); _putchar('x');

	_putonehex(hex >> 24 & 0xFF);
	_putonehex(hex >> 16 & 0xFF);
	_putonehex(hex >> 8 & 0xFF);
	_putonehex(hex & 0xFF);
	return;
}

void _reset_cursor(void)
{
	_asm
	{
		mov ah, 2h
		mov bh, 0
		mov dx, 0
		int 10h
	}
}
