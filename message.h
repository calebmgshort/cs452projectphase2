#ifndef _MESSAGE_H
#define _MESSAGE_H

#define DEBUG2 1

#include "phase2.h"

struct mailSlot
{
    int       mboxID;             // The ID of the mailbox this slot is stored in.
    int       status;
    char      data[MAX_MESSAGE];  // The message stored in this slot

    // other items as needed...
};

typedef struct mailSlot * slotPtr;
typedef struct mailbox    mailbox;
typedef struct mailbox  * mailboxPtr;
typedef struct mboxProc * mboxProcPtr;

struct mailbox
{
    int       mboxID;             // The ID of this mailbox
    struct mailSlot  slots[MAXSLOTS];    // The slots for this mailbox
    int       size;               // The size of this mailbox

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
#define SLOT_STATUS_EMPTY 0

#endif /* _MESSAGE_H */
