// _chain_intr �Լ��� ���纻. OpenWatcom 1.9�� �ҽ��ڵ�κ��� ���� ���̴�.
// �̷��� ������ ���� ������ ���ֽÿ� libc�� ��� �����ǵ� _chain_intr�Լ���
// libc�� ���� ���Ŀ��� ����Ͽ��� �ϱ� �����̴�.

/* ���� ����
 _WCRTLINK extern void     _chain_intr( void
                                       (_WCINTERRUPT _DOSFAR *__handler)() );
 */

#ifndef chint_h_sentinel
#define chint_h_sentinel

_WCRTLINK extern void _mvchain_intr(void far* __handler);

extern unsigned short far glob_newds;

#endif
