// _chain_intr 함수의 복사본. OpenWatcom 1.9의 소스코드로부터 얻어온 것이다.
// 이렇게 복사해 오는 이유는 상주시에 libc를 모두 버릴건데 _chain_intr함수는
// libc를 버린 이후에도 사용하여야 하기 때문이다.

/* 원본 선언
 _WCRTLINK extern void     _chain_intr( void
                                       (_WCINTERRUPT _DOSFAR *__handler)() );
 */

#ifndef chint_h_sentinel
#define chint_h_sentinel

_WCRTLINK extern void _mvchain_intr(void far* __handler);

extern unsigned short far glob_newds;

#endif
