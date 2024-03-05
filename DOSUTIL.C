#include"DOSUTIL.H"
#include <i86.h>

/////////////////////////////////////////
// 이 코드는 메모리에 남는다.
#pragma code_seg(BEGTEXT, CODE)

void _puts(const char* str)
{
	union REGPACK r;
	for (; *str; str++)
	{
		r.h.al = *str;
		_asm
		{
			int 29h;
		}

		if (r.h.al == '\n')
		{
			_asm
			{
				mov AL, 0dh;
				int 29h;
			}
		}
	}
	return;
}

void _putchar(char c)
{
	union REGPACK r;
	_asm
	{
		mov AL, c;
		int 29h;
	}

	if (r.h.al == '\n')
	{
		_asm
		{
			mov AL, 0dh;
			int 29h;
		}
	}

	return;
}

void _puthex(unsigned hex)
{
	char hexval;
	_putchar('0'); _putchar('x');

	hexval = (hex & 0xF0) >> 8;
	if (hexval < 10)
		_putchar('0' + hexval);
	else
		_putchar('A' + hexval - 10);

	hexval = (hex & 0x0F);
	if (hexval < 10)
		_putchar('0' + hexval);
	else
		_putchar('A' + hexval - 10);
	return;
}
