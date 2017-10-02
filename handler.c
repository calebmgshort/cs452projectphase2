#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"
#include "phase2utility.h"

extern int debugflag2;

/* an error method to handle invalid syscalls */
void nullsys(sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall. Halting...\n");
    USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int type, void* unitPointer)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("clockHandler2(): called\n");
    }
    long unit = (long) unitPointer;

    if(type != USLOSS_CLOCK_DEV)
    {
        USLOSS_Console("clockHandler2(): called with wrong type %d\n", type);
        USLOSS_Halt(1);
    }
    if(unit != 0)
    {
        USLOSS_Console("clockHandler2(): called with invalid unit number %d\n", unit);
        USLOSS_Halt(1);
    }

    timeSlice();
    static int clockIteration = 0;
    clockIteration++;
    if(clockIteration == 5)
    {
        int mboxID = getDeviceMboxID(type, unit);
        MboxCondSend(mboxID, NULL, 0);
        clockIteration = 0;
    }


} /* clockHandler */


void diskHandler(int type, void* unitPointer)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("diskHandler(): called\n");
    }
    long unit = (long) unitPointer;
    if(type != USLOSS_DISK_DEV)
    {
        USLOSS_Console("diskHandler(): called with wrong type %d\n", type);
        USLOSS_Halt(1);
    }
    if(unit != 0 && unit != 1)
    {
        USLOSS_Console("diskHandler(): called with invalid unit number %d\n", unit);
        USLOSS_Halt(1);
    }
    int mboxID = getDeviceMboxID(type, unit);
    int status = -1;
    int result = USLOSS_DeviceInput(type, unit, &status);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("diskHandler(): Could not get input from device.\n");
        USLOSS_Halt(1);
    }
    MboxCondSend(mboxID, (void*) &status, sizeof(int));

} /* diskHandler */


void termHandler(int type, void* unitPointer)
{
    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("termHandler(): called\n");
    }
    long unit = (long) unitPointer;
    if (type != USLOSS_TERM_DEV)
    {
        USLOSS_Console("termHandler(): called with wrong type %d.\n", type);
        USLOSS_Halt(1);
    }
    if (unit < 0 || unit >= USLOSS_TERM_UNITS)
    {
        USLOSS_Console("termHandler(): called with invalid unit number %d.\n", unit);
        USLOSS_Halt(1);
    }
    int mboxID = getDeviceMboxID(type, unit);
    int status = -1;
    int result = USLOSS_DeviceInput(type, unit, &status);
    if (result != USLOSS_DEV_OK)
    {
        USLOSS_Console("termHandler(): Could not get input from device.\n");
        USLOSS_Halt(1);
    }
    MboxCondSend(mboxID, (void*) &status, sizeof(int));

} /* termHandler */


void syscallHandler(int type, void* unitPointer)
{

    if (DEBUG2 && debugflag2)
    {
        USLOSS_Console("syscallHandler(): called\n");
    }


} /* syscallHandler */
