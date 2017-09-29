
#include <stddef.h>
#include "usloss.h"
#include "message.h"
#define DEBUG 0
extern int debugflag;
extern mailbox MailBoxTable[];

void
p1_fork(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

/*
 *  Returns 1 if any process is blocked on an i/o Mailbox
 *  Returns 0 otherwise
 */
int check_io()
{
    int i;
    for(i = 0; i < 7; i++)  // For each of the 7 io mailboxes
    {
        mailboxPtr ioMailboxPointer = &MailBoxTable[i];
        if(ioMailboxPointer->blockedProcsHead != NULL)   // If the mailbox is blocked on something
        {
            return 1;
        }
    }
    return 0;
}
