/*
 * This file is done for you.
 * Probably you will not need to change anything.
 * This file represents an emulated clock for simulation purpose only.
 * It is not a real part of operating system!
 */
#include <stdlib.h>
#include <sys/shm.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "clk.h"
#include "colors.h"

#define SHKEY 300
///==============================
// don't mess with this variable//
int* shmaddr = NULL; //
//===============================

int shmid;

/* Clear the resources before exit */
void _cleanup(__attribute__((unused)) int signum)
{
    shmctl(shmid, IPC_RMID, NULL);
    printf(ANSI_COLOR_CYAN"[CLOCK] Clock terminating!\n"ANSI_COLOR_RESET);
    exit(0);
}

void init_clk()
{
    printf(ANSI_COLOR_CYAN"[CLOCK] Clock starting\n"ANSI_COLOR_RESET);
    signal(SIGINT, _cleanup);
    int clk = 0;
    // Create shared memory for one integer variable 4 bytes
    shmid = shmget(SHKEY, 4, IPC_CREAT | 0644);
    if ((long)shmid == -1)
    {
        perror("Error in creating shm!");
        exit(-1);
    }
    int* shmaddr = (int*)shmat(shmid, (void*)0, 0);
    if ((long)shmaddr == -1)
    {
        perror("Error in attaching the shm in clock!");
        exit(-1);
    }
    *shmaddr = clk; /* initialize shared memory */
}

void run_clk()
{
    while (1)
    {
        printf(ANSI_COLOR_CYAN"[CLOCK] current time is %d\n"ANSI_COLOR_RESET, (*shmaddr));
        sleep(1);
        (*shmaddr)++;
    }
}

int get_clk()
{
    if (shmaddr == NULL)
    {
        perror("[CLOCK] SHMADDR IS BEING ACCESSED ALTHOUGH NULL");
        return -1;
    }
    else
        return *shmaddr;
}

void sync_clk()
{
    int shmidLocal = shmget(SHKEY, 4, 0444);
    while ((int)shmidLocal == -1)
    {
        // Make sure that the clock exists
        if(DEBUG)
        printf(ANSI_COLOR_CYAN"[CLOCK] Wait! The clock not initialized yet!\n"ANSI_COLOR_RESET);
        sleep(1);
        shmidLocal = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int*)shmat(shmidLocal, (void*)0, 0);
}

void destroy_clk(short terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
