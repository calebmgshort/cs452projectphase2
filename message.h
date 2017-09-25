#ifndef _MESSAGE_H
#define _MESSAGE_H

#define DEBUG2 1

#include "phase2.h"

typedef struct mailSlot   mailSlot;
typedef struct mailSlot * slotPtr;

typedef struct mailbox    mailbox;
typedef struct mailbox  * mailboxPtr;

typedef struct mboxProc   mboxProc;
typedef struct mboxProc * mboxProcPtr;

struct mailbox
{
    int          mboxID;           // The ID of this mailbox
    int          size;             // The size of this mailbox
    int          numSlotsOccupied; // The current number of slots occupied by this mailbox
    int          slotSize;         // The size for each slot of this mailbox
    slotPtr      slotsHead;        // The list of slots for this mailbox
    slotPtr      slotsTail;
    mboxProcPtr  blockedProcsHead; // The list of procs blocked on this mailbox
    mboxProcPtr  blockedProcsTail;

    // other items as needed...
};

struct mailSlot
{
    int      mboxID;            // The ID of the mailbox this slot is stored in.
    int      status;            // The status of this mailSlot
    char     data[MAX_MESSAGE]; // The message stored in this slot
    int      size;              // The size of the message currently stored in data.
    slotPtr  next;              // LL next pointer (for each mbox's list of slots)

    // other items as needed...
};

struct mboxProc
{
    int          pid;             // The pid of this proc
    mboxProcPtr  nextBlockedProc; // LL next pointer (for each mbox's list of blocked procs)
    void        *msgBuf;          // Pointer to the buffer that send should write its message to.
    int          bufSize;         // The size of the buffer
    int          msgSize;         // The size of the copied message. -1 if no message was copied
};

struct psrBits
{
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues
{
    struct psrBits bits;
    unsigned int integerPart;
};

// Some useful constants
#define ID_NEVER_EXISTED -1

#define STATUS_BLOCK_RECEIVE 11
#define STATUS_BLOCK_SEND 12

#endif /* _MESSAGE_H */
