#include "bootpack.h"


TASKCTL *taskctl;
TIMER   *task_timer;

TASK *task_init(MEMMAN *memman)
{
    int                        i;
    TASK                      *task;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
    taskctl                        = (TASKCTL *)memman_alloc_4k(memman, sizeof(TASKCTL));
    for(i = 0; i < MAX_TASKS; i++)
    {
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel   = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss, AR_TSS32);
    }

    for(i = 0; i < MAX_TASKLEVELS; i++)
    {
        taskctl->level[i]->running = 0; /* 当前活动任务数量 */
        taskctl->level[i]->now     = 0; /* 当前运行哪个任务 */
    }

    task              = task_alloc();
    task->flags       = 2; /* 活动中的标志 */
    task->priority    = 2; /* 初始值都设置为0.02s */
    task->level       = 0; /* 初始都设置成最高级 */
    task_add(task);
    task_switchsub();  /* 刚开始只有level0运行 */
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

void task_run(TASK *task, int level, int priority)
{
    if(level < 0)
    {
        level = task->level; /* 用task默认的level */
    }

    /* 当为0时不改变任务优先级 */
    if(priority > 0)
    {
        task->priority = priority;
    }

    /* 改变活动中的level */
    if(task->flags == 2 && task->level != level)
    {
        task_remove(task); /* 此处执行之后，flag = 1，会进入下面的if（其实就是在所任务移动）*/
    }

    if(task->flags != 2)
    {
        task->level = level;
        task_add(task);  /* 加入任务，其实就是将flags = 2，使其为活动态 */
    }   
    taskctl->lv_change = 1; /* 下次任务切换的时候检查level(为了把你顶上去！！！) */

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

/* 返回现在活动中的TASK的内存地址 */
TASK *task_now()
{   
    TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
    return tl->tasks[tl->now];
}

void  task_add(TASK *task)
{
    TASKLEVEL *tl          = &taskctl->level[task->level]; /* 先找到这个任务对应的level级 */
    tl->tasks[tl->running] = task; /* 追加到tasks列表的末尾 */
    tl->running++; /* 数量自增 */
    task->flags = 2; /* 改为活动中,为何进来就要编程任务中呢？因为主要是task_run在调用，别被名字误解了 */
    return;
}

void task_remove(TASK *task)
{
    int i;
    TASKLEVEL *tl = &taskctl->level[task->level]; /* 先看这个任务属于哪个level */

    /* 在该level下寻找task所在的位置 */
    for(i = 0; i < tl->running; i++)
    {
        if(tl->tasks[i] == task)
        {
            break;
        }
    }

    tl->running--; /* 首先数量上减一 */
    if(i < tl->now)
    {
        tl->now--; /* 需要移动成员，此时的now应该往前移一个 */
    }
    if(tl->now >= tl->running)
    {
        /* 如果now的值出现异常，则进行修正 */
        tl->now = 0;
    }
    task->flags = 1; /* 休眠中 */
    /* 移动中 */
    for(; i < tl->running; i++)
    {
        tl->tasks[i] = tl->tasks[i + 1];
    }

    return;
}

void  task_switchsub()
{
    int i;
    /* 寻找最上层的LEVEL */
    for(i = 0; i < MAX_TASKLEVELS; i++)
    {
        /* 只要上层有任务存在 */
        if(taskctl->level[i]->running > 0)
        {
            break; /* 找到了 */
        }
    }
    taskctl->now_lv    = i;
    taskctl->lv_change = 0; /* 在下次任务切换时不改变level */
    return;
}
