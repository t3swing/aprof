#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>   /*引用/usr/include/i386-linux-gnu/sys/user.h*/

#include "deps.h"
#include "readSym.h"

#define SHELL_FILE      "/bin/sh"
#define MAX_FUNC        1024

typedef struct
{
    int count;
    ADDR_T addr;
} SAMPLE_S;

typedef struct
{
    int running;
    int childPid;
    int clone;
    int funcCnt;
    int sampleTotal;
    int profTotal;
    int timeCnt;
    SAMPLE_S sample[MAX_FUNC];
} SPROF_S;

SPROF_S gSprof = {0};

#ifdef PLATFORM_X86
ADDR_T getPcAddrX86(int pid)
{
    int ret = 0;
    struct user_regs_struct regs;

    ret = ptrace(PTRACE_GETREGS, pid , NULL, &regs);
    if (ret < 0)
    {
        return ret;
    }
    //printf("regs ebx:0x%lx ecx:0x%lx edx:0x%lx eip:0x%lx\n",regs.ebx, regs.ecx,regs.edx,regs.eip);
    //printf("signal prof cnt:%2d eip:0x%lx %s\n",++cnt,regs.eip,addrToFunc((void *)regs.eip,NULL));
    return (ADDR_T)regs.eip;
}
#endif

#ifdef PLATFORM_ARM
ADDR_T getPcAddrArm(int pid)
{
    int ret = 0;
    struct user_regs regs;

    ret = ptrace(PTRACE_GETREGS, pid , NULL, &regs);
    if (ret < 0)
    {
        return ret;
    }

    return (ADDR_T)regs.uregs[15]; /* */
}
#endif

int signalProfProcess()
{
    int i = 0;
    ADDR_T addrIn, addrOut = 0;

    gSprof.profTotal++;
#ifdef PLATFORM_X86
    addrIn = getPcAddrX86(gSprof.childPid);
#endif
#ifdef PLATFORM_ARM
    addrIn = getPcAddrArm(gSprof.childPid);
#endif
    //printf("addrIn:%#x\n",(int)addrIn);
    if (addrIn)
    {
        addrToFunc(addrIn, &addrOut);
        CHECK_RTN(addrOut == 0, 0);
    }

    SAMPLE_S * sample = NULL;
    for (i = 0; i < gSprof.funcCnt; i++)
    {
        sample = &gSprof.sample[i];
        if (sample->addr == addrOut)
        {
            sample->count++;
            gSprof.sampleTotal++;
            return 0;
        }
    }

    if (gSprof.funcCnt < MAX_FUNC)
    {
        sample = &gSprof.sample[gSprof.funcCnt];
        sample->addr = addrOut;
        sample->count++;
        gSprof.sampleTotal++;
        gSprof.funcCnt++;
    }

    return 0;
}

int signalStopProcess(int stopSig)
{
    int ret = 0;

    /* 信号默认continue处理，prof信号做统计处理 */
    switch (stopSig)
    {
        case SIGINT:
        case SIGSEGV:
        {
            printf("Pause signal:%d!\n", stopSig);
            return -1;
        }
        case SIGPROF:
        {
            signalProfProcess();
            break;
        }
        default:
        {
            printf("signalStopProcess stopSig:%d \n", stopSig);
            break;
        }
    }

    ret = ptrace(PTRACE_CONT, gSprof.childPid, NULL, NULL);
    CHECK_MSG(ret < 0, "ptrace PTRACE_CONT error %s\n", strerror(errno));
    //printf("signalStopProcess stopSig:%d \n", stopSig);

    return 0;
}

void signalRecv()
{
    int ret = 0, waitStat = 0, stopSig = 0;

    printf("childPid:%d \n", gSprof.childPid);
    while (gSprof.running)
    {
        if (gSprof.clone)
        {
            ret = waitpid(gSprof.childPid, &waitStat, __WCLONE);
        }
        else
        {
            ret = wait(&waitStat);
        }
        CHECK_RTNVM(ret < 0, "wait error clone:%d %s \n", gSprof.clone, strerror(errno));

        /* 子进程正常终止 */
        if (WIFEXITED(waitStat))
        {
            printf("signal WIFEXITED \n");
            break;
        }

        /* 子进程暂停 */
        if (WIFSTOPPED(waitStat))
        {
            //printf("signal WIFSTOPPED \n");
            stopSig = WSTOPSIG(waitStat);
            ret = signalStopProcess(stopSig);
            if (ret < 0)
            {
                break;
            }
        }
        if (WIFSIGNALED(waitStat))
        {
            printf("WIFSIGNALED\n");
        }
        if (WIFCONTINUED(waitStat))
        {
            printf("WIFCONTINUED\n");
        }
    }
}

void signalSend()
{
    usleep(1000 * 1000);
    while (gSprof.running)
    {
        kill(gSprof.childPid, SIGPROF);
        usleep(10 * 1000);
    }
}

void sprofResultShow()
{
    int i = 0;
    SAMPLE_S * sample = NULL;

    usleep(100 * 1000);

    while (gSprof.running)
    {
        /* Clear the screen */
        printf("\033[H\033[J");
        printf("Aprof v0.1 Total: %d samples Pid: %d profTotal:%d timeCnt:%d\n\n", gSprof.sampleTotal, gSprof.childPid, gSprof.profTotal, gSprof.timeCnt);
        printf("Id   Address     Count   Percent   FuctionName\n");
        for (i = 0; i < gSprof.funcCnt; i++)
        {
            sample = &gSprof.sample[i];
            if (sample->count)
            {
                printf("%2d %#10x %8d %8.1f%%    %s\n", i, (int)sample->addr, sample->count, sample->count * 100.0 / gSprof.sampleTotal, addrToFunc(sample->addr, NULL));
            }
        }

        usleep(2 * 1000 * 1000);
    }
}

int sprofRun(const char * name)
{
    int pid = 0, ptStat = 0, waitStat = 0;

    pid = vfork();
    if (pid < 0)
    {
        printf("vfork\n");
        exit(-1);
    }
    if (pid == 0)
    {
        ptStat = ptrace(PTRACE_TRACEME, 0, 0, 0);
        if (ptStat < 0)
        {
            printf("PTRACE_TRACEME error!\n");
        }
        char * shellFile = getenv("SHELL");
        if (shellFile == NULL)
        {
            shellFile = SHELL_FILE;
        }

        execlp(shellFile, shellFile, "-c", name, (char *) 0);
        printf("Cannot exec %s:\n", name);
        _exit(0177);
    }

    pid = wait(&waitStat);
    CHECK_MSG(pid < -1, "wait error %s\n", strerror(errno));

    if (WIFSTOPPED(waitStat))
    {
        CHECK_RTNM(WSTOPSIG(waitStat) != SIGSTOP, -1, "waitpid error! stat:%d\n", WSTOPSIG(waitStat));

        ptStat = ptrace(PTRACE_CONT, pid, 0, 0);
        CHECK_RTNM(ptStat < 0, -1, "sprofAttach PTRACE_CONT error pid:%d %s\n", gSprof.childPid, strerror(errno));
    }

    gSprof.childPid = pid;

    return 0;
}

int sprofAttach(int pid)
{
    int ptStat = 0, waitStat = 0, childPid = 0;

    ptStat = ptrace(PTRACE_ATTACH, pid, 0, 0);
    CHECK_RTNM(ptStat < 0, -1, "sprofAttach error:%s\n", strerror(errno));

    childPid = waitpid(pid, &waitStat, 0);
    if (childPid == -1 && errno == ECHILD)
    {
        DBG_INFO("%d is a cloned process\n", pid);
        childPid = waitpid(pid, &waitStat, __WCLONE);
        CHECK_RTNM(childPid < 0, -1, "sprofAttach waitpid error %s\n", strerror(errno));
        gSprof.clone = 1;
    }

    if (WIFSTOPPED(waitStat))
    {
        CHECK_RTNM(WSTOPSIG(waitStat) != SIGSTOP, -1, "waitpid error! stat:%d\n", WSTOPSIG(waitStat));

        ptStat = ptrace(PTRACE_CONT, childPid, 0, 0);
        CHECK_RTNM(ptStat < 0, -1, "sprofAttach PTRACE_CONT error pid:%d %s\n", gSprof.childPid, strerror(errno));
    }

    gSprof.childPid = childPid;

    return 0;
}

void hideCursor(BOOL_E hide)
{
    if (hide)
    {
        printf("\033[?25l");
    }
    else
    {
        printf("\033[?25h");
    }
}

void signalCatcher(int signalNum)
{
    hideCursor(FALSE_E);
    ptrace(PTRACE_DETACH, gSprof.childPid, 0, 0);

    exit(-1);
}

int main(int argc, char * argv[])
{
    int ret = 0, pid = 0;
    const char * prog;
    char realPath[256] = {0};
    pthread_t tidSigSend, tidResultShow;

    signal(SIGINT, signalCatcher);
    signal(SIGPIPE, signalCatcher);
    signal(SIGSEGV, signalCatcher);
    signal(SIGBUS, signalCatcher);

    if (argc < 2)
    {
        printf("Usage: aprof program [pid]\n");
        return 0;
    }

    if (argc >= 2)
    {
        prog = argv[1];
    }
    if (argc == 3)
    {
        pid = atoi(argv[2]);
    }

    DBG_INFO("realPath:%s\n", realpath(prog, realPath));
    if (pid == 0)
    {
        ret = sprofRun(prog);
    }
    else
    {
        ret = sprofAttach(pid);
    }
    CHECK_RTN(ret < 0, ret);

    ret = loadElfSym(prog, pid);
    CHECK_GOTOM(ret < 0, RELEASE, "loadElfSym error!\n");

    gSprof.running = 1;
    pthread_create(&tidSigSend, NULL, (void *)signalSend, NULL);
    pthread_create(&tidResultShow, NULL, (void *)sprofResultShow, NULL);

    hideCursor(TRUE_E);
    signalRecv();

RELEASE:
    gSprof.running = 0;
    pthread_join(tidSigSend, NULL);
    pthread_join(tidResultShow, NULL);
    unloadElfSym();
    hideCursor(FALSE_E);

    return 0;
}
