// INTHELP.H
#ifndef _INTHELP_H_
#define _INTHELP_H_
#include "FMDRVDEF.H"

extern void __declspec(naked) mymemcpy(void far* dst, void far* src, unsigned int len);
extern unsigned short __declspec(naked) mystrlen(void far* s);
extern int mystrstr(char far* srcFilePtr, char far* dstFilePtr);
extern int mystrcmp(const char far* s, const char far* t);
extern void __declspec(naked) (__interrupt __far* mygetvect(unsigned interruptnum))();
extern void __declspec(naked) mysetvect(unsigned interruptnum, void (__interrupt __far* vect)());
extern void freeseg(unsigned short segm);

//extern uint8 IsCtrlPressed(void);
//extern uint8 IsLeftShiftPressed(void);
#endif
