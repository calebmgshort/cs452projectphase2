#include <phase1.h>
#include <phase2.h>
#include <usloss.h>

#include "message.h"
#include "phase2utility.h"

extern mailbox MailBoxTable[];

void check_kernel_mode(char* functionName){

}

void disableInterrupts(){

}

void enableInterrupts(){

}

int mboxIDToSlot(int id)
{
    return id % MAXMBOX;
}

int getNextMboxID()
{
    // Check for empty slots
    for (int i = 0; i < MAXMBOX; i++)
    {
        if (MailBoxTable[i].mboxID == ID_NEVER_EXISTED)
        {
            return i;
        }
    }
    // TODO Check for slots to repurpose

    // No slots available
    return -1;
}
