#include "bootpack.h"


TASKCTL *taskctl;
TIMER   *task_timer;

TASK *task_init(MEMMAN *memman)
{
    int   i;
    TASK *task;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
    taskctl = (TASKCTL *)memman_alloc_4k(memman, sizeof(TASKCTL));
    for(i = 0; i < MAX_TASKS; i++)
    {
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel   = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss, AR_TSS32);
    }
    task              = task_alloc();
    task->flags       = 2; /* 活动中的标志 */
    task->priority    = 2; /* 初始值都设置为0.02s */
    taskctl->running  = 1;
    taskctl->now      = 0; 
    taskctl->tasks[0] = task; /* 管理任务的任务 */
    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_settime(task_timer, task->priority);
    return task;
}

TASK *task_alloc()
{
    int   i;
    TASK *task;
    for(i = 0; i < MAX_TASKS; i++)
    {
        if(taskctl->tasks0[i].flags == 0)
        {
            task             = &taskctl->tasks0[i];
            task->flags      = 1; /* 不工作的标志 */
            task->tss.eflags = 0x00000202; /* 中断打开 IF = 1*/
            task->tss.eax    = 0;
            task->tss.ecx    = 0;
            task->tss.edx    = 0;
            task->tss.ebx    = 0;
            task->tss.ebp    = 0;
            task->tss.esi    = 0;
            task->tss.edi    = 0;
            task->tss.es     = 0;
            task->tss.ds     = 0;
            task->tss.fs     = 0;
            task->tss.gs     = 0;
            task->tss.ldtr   = 0;
            task->tss.iomap  = 0x40000000;
            return task;
        }
    }
    return 0; /* 没找到，返回空 */
}

void task_run(TASK *task, int priority)
{
    /* 当为0时不改变任务优先级 */
    if(priority > 0)
    {
        task->priority = priority;
    }
    if(task->flags != 2)
    {
        task->flags = 2; /* 活动中的标志 */
        taskctl->tasks[taskctl->running] = task; /* 这在任务调度里面是不是算准备就绪了？ */
        taskctl->running++;
    }

    return;
}

void task_switch()
{
    TASK *task;
    taskctl->now++; /* 直接切当前正在运行的任务的下一个任务 */
    if(taskctl->now == taskctl->running) /* 如果下一个任务达到活动任务的边界，起始是0，如果现在有两个活动任务，now会是0,1,自增后到2，此刻就必须重置 */
    {
        taskctl->now = 0; /* 切换到任务调度本身（相当于重置） */
    }
    task = taskctl->tasks[taskctl->now];
    timer_settime(task_timer, task->priority);
    if(taskctl->running >= 2)
    {
        farjmp(0, task->sel);
    }
    return;
}
/* 当前正在运行任务A让任务A休眠，需要在处理结束之后，马上切换到下一个任务 */
/* 当前正在运行任务A让任务B休眠，主要还是移动任务 */
void task_sleep(TASK *task)
{
    int  i;
    char ts = 0;
    if(task->flags == 2) /* 如果指定任务处于活动状态 */
    {
        if(task == taskctl->tasks[taskctl->now]) /* 当前正在运行任务a让任务a休眠 */
        {
            ts = 1; /* 让自己休眠的话，稍后需要进行任务切换 */
        }

        /* 寻找task所在的位置 */
        for(i = 0; i < taskctl->running; i++)
        {
            if(taskctl->tasks[i] == task)
            {
                break;
            }
        }
        taskctl->running--;
        if(i < taskctl->now) /* i在当前执行的任务前面 ，因为待会要移动，此刻现将now提前；如果在后面就不用执行函数体 */
        {
            taskctl->now--; /* 需要移动成员，要相应的进行处理 */
        }
        /* 移动成员 */
        for(; i < taskctl->running; i++)
        {
            taskctl->tasks[i] = taskctl->tasks[i + 1]; /* 后面的向前移动 */
        }
        task->flags = 1; /* 不工作的状态 */
        if(ts != 0)
        {
            /* 任务切换 */
            if(taskctl->now >= taskctl->running)
            {
                /* 如果now的值出现异常，则进行修正 */
                taskctl->now = 0;
            }
            /* 现在的now可能没变，但是tasks中的排列变化了，因为输入的task被移除了，可以看做是task的下一个任务顶上来了 */
            farjmp(0, taskctl->tasks[taskctl->now]->sel);
        }
    }
}