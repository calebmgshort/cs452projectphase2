/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */
#include <phase1.h>
#include <usloss.h>
#include <stddef.h>
#include <string.h>

#include "message.h"
#include "phase2.h"
#include "phase2utility.h"
#include "phase2queue.h"
#include "handler.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);

/* -------------------------- Globals ------------------------------------- */
int debugflag2 = 0;

// the mail boxes
mailbox MailBoxTable[MAXMBOX];

// The Mailbox slots
mailSlot MailSlotTable[MAXSLOTS];

// The message process table
mboxProc ProcTable[MAXPROC];

// also need array of mail slots, array of function ptrs to system call
// handlers, ...

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("start1(): Called.\n");
    }

    // Check the current mode
    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    // Initialize USLOSS_IntVec and system call handlers,
    // allocate mailboxes for interrupt handlers.  Etc...

    // Initialize the mailbox table
    for (int i = 0; i < MAXMBOX; i++)
    {
        MailBoxTable[i].mboxID = ID_NEVER_EXISTED;
    }

    // Initialize the slots table
    for (int i = 0; i < MAXSLOTS; i++)
    {
        MailSlotTable[i].mboxID = ID_NEVER_EXISTED;
    }

    // Initialize the process table
    for (int i = 0; i < MAXPROC; i++)
    {
        ProcTable[i].pid = ID_NEVER_EXISTED;
    }

    // Create mailboxes for devices
    for (int i = 0; i < NUM_DEVICES; i++)
    {
        MboxCreate(0, MAX_MESSAGE);
        disableInterrupts();
    }

    // Initialize the interrupt handlers
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = termHandler;

    // Enable interrupts
    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("start1(): fork'ing start2 process.\n");
    }
    int kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    int status;
    if (join(&status) != kid_pid)
    {
        USLOSS_Console("start2(): join returned something other than start2's pid.\n");
    }

    return 0;
} /* start1 */

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    // Check kernel mode
    check_kernel_mode("MboxCreate");

    // Disable interrupts
    disableInterrupts();

    // Check args
    if (slots < 0 || slots > MAXSLOTS)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid number of slots (%d).\n");
        }
        enableInterrupts();
        return -1;
    }
    if (slot_size < 0 || slot_size > MAX_MESSAGE)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): Tried to create a mailbox with an invalid slot size (%d).\n");
        }
        enableInterrupts();
        return -1;
    }

    // Get the id and index
    int id = getNextMboxID();
    if (id == -1)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCreate(): No mailbox slots remaining.\n");
        }
        enableInterrupts();
        return -1;
    }
    mailboxPtr box = &MailBoxTable[id];
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxCreate(): Creating mailbox with id %d.\n", id);
    }

    // Set fields
    box->mboxID = id;
    box->size = slots;
    box->numSlotsOccupied = 0;
    box->slotSize = slot_size;
    box->slotsHead = NULL;
    box->slotsTail = NULL;
    box->blockedProcsHead = NULL;
    box->blockedProcsTail = NULL;

    enableInterrupts();

    // return the id of the new box
    return id;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Return values:
      -3: process zap’d or the mailbox released while blocked on the mailbox.
      -1: illegal values given as arguments.
       0: message sent successfully.
0: message sent successfully.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mboxID, void *msgPtr, int msgSize)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxSend(): Called.\n");
    }

    // Defer to conditional send.
    int result = MboxCondSend(mboxID, msgPtr, msgSize);

    // Disable interrupts
    disableInterrupts();

    // Return MboxCondSend's result if nothing more needs to be done
    if (result == 0 || result == -1 || result == -3)
    {
        enableInterrupts();
        return result;
    }

    // Get the mailbox that mboxID corresponds to
    mailboxPtr box = getMailbox(mboxID);

    // Block on a send to the mailbox
    int mboxReleased = blockCurrent(box, STATUS_BLOCK_SEND, msgPtr, msgSize);

    // Redisable interrupts
    disableInterrupts();

    // Mark the current proc as unblocked
    clearProc(getpid());

    // Check if we were zapped or if the box was released while we were blocked
    if (mboxReleased || isZapped())
    {
        enableInterrupts();
        return -3;
    }

    // Handle 0 slot boxes
    if (box->size == 0)
    {
        // Receive will handle the memory copying
        enableInterrupts();
        return 0;
    }

    // Drop the message into a slot in box
    result = MboxCondSend(mboxID, msgPtr, msgSize);
    if (result == -2)
    {
        // No free slots were avaialable
        USLOSS_Halt(1);
    }
    return 0;
}

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Conditionally send a message to a mailbox. Do not block the invoking process.
             Rather, if there is no empty slot in the mailbox in which to place the message,
             the value is returned. Also return -2 in the case that all the mailbox slots in the
             system are used and none are available to allocate for this message.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.

   Return values:
      -3: process was zap’d.
      -2: mailbox full, message not sent; or no slots available in the system.
      -1: illegal values given as arguments.
       0: message sent successfully.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mboxID, void *msgPtr, int msgSize)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxCondSend(): Called.\n");
    }

    // Check kernel mode
    check_kernel_mode("MboxCondSend");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mboxID corresponds to
    mailboxPtr box = getMailbox(mboxID);
    if (box == NULL)
    {
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (msgSize < 0 || msgSize > box->slotSize)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondSend(): msgSize out of range for box %d.\n", mboxID);
        }
        enableInterrupts();
        return -1;
    }

    // Check if we were zapped beforehand
    if (isZapped())
    {
        // TODO do any of the side effects occur when zapped beforehand?
        enableInterrupts();
        return -3;
    }

    // Is anything blocked on a receive?
    mboxProcPtr proc = box->blockedProcsHead;
    if (proc != NULL && proc->status == STATUS_BLOCK_RECEIVE)
    {
        // Put the message directly into the proc buffer

        // Take the blocked proc off of the blocklist
        removeBlockedProcsHead(box);

        // Check if the message is small enough to be received
        if (msgSize > proc->bufSize)
        {
            if (DEBUG2 && debugflag2)
            {
                USLOSS_Console("MboxCondSend(): Message sent directly to process %d is too large (%d) for buffer (%d).\n", proc->pid, msgSize, proc->bufSize);
            }
            proc->msgSize = -1;
            unblockProc(proc->pid);
            return -1;
        }

        // Copy the message into the buffer
        memcpy(proc->msgBuf, msgPtr, msgSize);
        proc->msgSize = msgSize;
        unblockProc(proc->pid);
        return 0;
    }

    // Check if the box has space for the message
    if (box->numSlotsOccupied == box->size)
    {
        // The box has no space, so the message cannot be delivered without blocking
        enableInterrupts();
        return -2;
    }

    // The message has space, so put the message into a slot

    // Get a slot for the new message
    slotPtr slot = findEmptyMailSlot();
    if (slot == NULL)
    {
        // No space is available.
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondSend(): No more space in the slots table.\n");
        }
        enableInterrupts();
        return -2;
    }

    // Init slot's fields
    slot->mboxID = mboxID;
    memcpy(slot->data, msgPtr, msgSize);
    slot->size = msgSize;

    // Add slot to box
    addMailSlot(box, slot);

    enableInterrupts();
    return 0;
} /* MboxCondSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int max_msg_size)
{
    if(DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxReceive(): called. Checking mode and disabling interrupts.\n");
    }

    // Check kernel mode
    check_kernel_mode("MboxReceive");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mbox_id corresponds to
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxReceive(): getting mailbox corresponding to id %d.\n", mbox_id);
    }
    mailboxPtr box = getMailbox(mbox_id);
    if (box == NULL)
    {
        enableInterrupts();
        return -1;
    }

    // Check the message size
    if (max_msg_size < 0)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): max_msg_size out of range for box %d.\n", mbox_id);
        }
        enableInterrupts();
        return -1;
    }

    // Handle 0 slot boxes
    if (box->size == 0 && box->blockedProcsHead != NULL)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): box %d has 0 slots and a blocked process.\n", mbox_id);
        }

        // Check if proc is blocked on a send
        mboxProcPtr proc = box->blockedProcsHead;
        if (proc->status == STATUS_BLOCK_SEND)
        {
            if (DEBUG2 && debugflag2)
            {
                USLOSS_Console("MboxReceive(): process was blocked on a send. Receiving message directly from proc buffer.\n");
            }

            // Check that the message will fit in the buffer
            if (proc->bufSize > max_msg_size)
            {
                return -1;
            }

            if (DEBUG2 && debugflag2)
            { // TODO
                USLOSS_Console("MboxReceive(): proc buffer %s\n", proc->msgBuf);
                USLOSS_Console("MboxReceive(): proc msg size %d\n", proc->bufSize);
            }

            memcpy(msg_ptr, proc->msgBuf, proc->bufSize);

            // Unblock the process
            unblockProc(proc->pid);

            enableInterrupts();
            return proc->bufSize;
        }
        // If we're blocked on a receive, procede like normal
    }

    // Get the first message
    slotPtr slot = box->slotsHead;

    // No message is in the box, so we block
    if (slot == NULL)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): No message contained in box %d.\n", mbox_id);
        }
        int mboxReleased = blockCurrent(box, STATUS_BLOCK_RECEIVE, msg_ptr, max_msg_size);

        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): Unblocked.\n");
        }

        // redisable interrupts
        disableInterrupts();

        // Clear the unblocked proc from the table
        clearProc(getpid());

        // Check if zapped or if the mailbox was released
        if (mboxReleased || isZapped())
        {
            enableInterrupts();
            return -3;
        }

        enableInterrupts();
        return ProcTable[getpid() % MAXPROC].msgSize;
    }

    // Check that the message will fit in the buffer
    if (slot->size > max_msg_size)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): message received from box %d is too large (%d) for buffer (%d).\n", box->mboxID, slot->size, max_msg_size);
        }
        enableInterrupts();
        return -1;
    }

    // The slot is valid so we can remove it from the box
    removeSlotsHead(box);

    // Check to see if anything is blocked on a send to box
    if (box->blockedProcsHead != NULL)
    {
        // Check the amount of space left in box
        mboxProcPtr proc = box->blockedProcsHead;
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxReceive(): unblocking process %d.\n", proc->pid);
        }
        removeBlockedProcsHead(box);
        unblockProc(proc->pid); // enables Interrupts
    }

    // Copy the message
    memcpy(msg_ptr, slot->data, slot->size);
    enableInterrupts();
    return slot->size;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Conditionally receive a message from a mailbox. Do not block the invoking
             process. Rather, if there is no message in the mailbox, the value -2 is returned.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns -
      -3: process was zap’d while blocked on the mailbox.
      -2: no message available to receive.
      -1: illegal values given as arguments; or, message sent is too large for receiver’s
          buffer (no data copied in this case).
     >=0: the size of the message received.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mboxID, void *msgPtr, int maxMsgSize)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("MboxCondReceive(): Called.\n");
    }

    // Check kernel mode
    check_kernel_mode("MboxCondReceive");

    // Disable interrupts
    disableInterrupts();

    // Get the mailbox that mboxID corresponds to
    mailboxPtr box = getMailbox(mboxID);
    if (box == NULL)
    {
        enableInterrupts();
        return -1;
    }

    // Check the max message size
    if (maxMsgSize < 0)
    {
        if (DEBUG2 && debugflag2)
        {
            USLOSS_Console("MboxCondReceive(): maxMsgSize must be non-negative.\n");
        }
        enableInterrupts();
        return -1;
    }

    // Check if zapped beforehand
    if (isZapped())
    {
        // TODO do any of the side effects occur when zapped beforehand?
        enableInterrupts();
        return -3;
    }

    // Does a 0 slot mailbox have a blocked sender?
    if (box->size == 0)
    {
        mboxProcPtr proc = box->blockedProcsHead;
        if (proc != NULL && proc->status == STATUS_BLOCK_SEND)
        {
            // Receive the message directly from proc

            // Check if the message will fit in the buffer
            if (proc->bufSize > maxMsgSize)
            {
                unblockProc(proc->pid);
                return -1;
            }

            // Copy the message
            memcpy(msgPtr, proc->msgBuf, proc->bufSize);
            unblockProc(proc->pid);
            return proc->bufSize;
        }
    }

    // Check for messages in box's slots
    slotPtr slot = box->slotsHead;
    if (slot == NULL)
    {
        // Box doesn't have a message, so the receive fails
        enableInterrupts();
        return -2;
    }
    removeSlotsHead(box);

    // Receive the message from the slot

    // Check if the message will fit in the buffer and copy if possible
    int copiedMsg = -1;
    if (slot->size <= maxMsgSize)
    {
        memcpy(msgPtr, slot->data, slot->size);
        copiedMsg = slot->size;
    }

    // Unblock one process blocked on a send to box
    if (box->blockedProcsHead != NULL)
    {
        mboxProcPtr proc = box->blockedProcsHead;
        // proc must be blocked on a send
        removeBlockedProcsHead(box);
        unblockProc(proc->pid);
    }

    enableInterrupts();
    return copiedMsg;
} /* MboxCondReceive */

int MboxRelease(int mbox_id)
{
    // Check kernel mode
    check_kernel_mode("MboxRelease");

    // Disable interrupts
    disableInterrupts();

    mailboxPtr box = getMailbox(mbox_id);
    if (box == NULL)
    {
        enableInterrupts();
        return -1;
    }

    mboxProcPtr proc = box->blockedProcsHead;
    while (proc != NULL)
    {
        // Flag proc as having a released mailbox and unblock
        proc->mboxReleased = 1;
        unblockProc(proc->pid); // enables interrupts
        disableInterrupts();
        proc = proc->nextBlockedProc;
    }

    box->mboxID = ID_NEVER_EXISTED;

    enableInterrupts();

    // Check if we were zapped while releasing the mailbox
    if (isZapped())
    {
        return -3;
    }
    return 0;
} /* MboxRelease */

int waitDevice(int type, int unit, int *status)
{
    // Check kernel mode
    check_kernel_mode("waitDevice");

    // Disable interrupts
    disableInterrupts();

    int mboxID = getDeviceMboxID(type, unit);

    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("waitDevice(): mailbox corresponding to device type %d, unit %d is %d.\n", type, unit, mboxID);
    }

    // Do the receive on the mailbox
    MboxReceive(mboxID, (void *) status, sizeof(int)); // enables interrupts

    // Check if zapped
    if (isZapped())
    {
        return -1;
    }
    return 0;
}
