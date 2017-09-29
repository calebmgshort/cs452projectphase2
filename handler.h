#ifndef _HANDLER_H
#define _HANDLER_H

extern void nullsys(sysargs *);
extern void clockHandler2(int, void *);
extern void diskHandler(int, void *);
extern void termHandler(int, void *);
extern void syscallHandler(int, void *);

#endif /* _HANDLER_H */
