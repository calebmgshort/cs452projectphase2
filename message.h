#ifndef _MESSAGE_H
#define _MESSAGE_H

#define DEBUG2 1

#include "phase2.h"

typedef struct mailSlot   mailSlot;
typedef struct mailSlot * slotPtr;

typedef struct mailbox    mailbox;
typedef struct mailbox  * mailboxPtr;

typedef struct mboxProc * mboxProcPtr;

struct mailbox
{
    int       mboxID;             // The ID of this mailbox
    int       size;               // The size of this mailbox
    int       slotSize;           // The size for each slot of this mailbox
    slotPtr   slotsHead;          // The list of slots for this mailbox
    slotPtr   slotsTail;

    // other items as needed...
};

struct mailSlot
{
    int       mboxID;             // The ID of the mailbox this slot is stored in.
    int       status;             // The status of this mailSlot
    char      data[MAX_MESSAGE];  // The message stored in this slot
    int       size;               // The max size of a message that can be stored in this slot (in bytes)
    slotPtr   next;               // Linked list next pointer

    // other items as needed...
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

#endif /* _MESSAGE_H */
