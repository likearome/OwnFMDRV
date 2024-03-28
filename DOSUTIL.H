#ifndef __DOSUTIL_H__
#define __DOSUTIL_H__

extern void _puts(const char *str);
extern void _putchar(char c);
extern void _put8hex(unsigned hex);
extern void _put32hex(unsigned long hex);
extern void _reset_cursor(void);

#define PUTCHAR(ARG)	_putchar(ARG)
#define PUTHEX(ARG)		_put8hex(ARG)
#define PUT32HEX(ARG)	_put32hex(ARG)

//#define PUTCHAR(ARG)
//#define PUTHEX(ARG)

#endif
