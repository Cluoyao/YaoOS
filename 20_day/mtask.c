#include "bootpack.h"


TASKCTL *taskctl;
TIMER   *task_timer;

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
        if(taskctl->level[i].running > 0)
        {
            break; /* 找到了 */
        }
    }
    taskctl->now_lv    = i;
    taskctl->lv_change = 0; /* 在下次任务切换时不改变level */
    return;
}
TASK *task_init(MEMMAN *memman)
{
    int                        i;
    TASK                      *task, *idle;
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
        taskctl->level[i].running = 0; /* 当前活动任务数量 */
        taskctl->level[i].now     = 0; /* 当前运行哪个任务 */
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

    idle              = task_alloc();
    idle->tss.esp     = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
    idle->tss.eip     = (int)&task_idle;
    idle->tss.es      = 1 * 8;
    idle->tss.cs      = 2 * 8;
    idle->tss.ss      = 1 * 8;
    idle->tss.ds      = 1 * 8;
    idle->tss.fs      = 1 * 8;
    idle->tss.gs      = 1 * 8;
    task_run(idle, MAX_TASKLEVELS - 1, 1);    

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
    TASKLEVEL *tl       = &taskctl->level[taskctl->now_lv]; /* 先找level */
    TASK      *now_task = tl->tasks[tl->now]; /* 再找level对应下的活动task */
    TASK      *new_task;
    tl->now++;/* 顺位执行下一个task */
    if(tl->now == tl->running)
    {
        tl->now = 0; /* 如果已经切换到末尾了，则回到起点 */
    }
    if(taskctl->lv_change != 0)
    {
        /* 如果要进行level切换 */
        task_switchsub();
        tl = &taskctl->level[taskctl->now_lv];
    }
    new_task = tl->tasks[tl->now];
    timer_settime(task_timer, new_task->priority);
    if(new_task != now_task)
    {
        farjmp(0, new_task->sel);
    }

    return;
}
/* 当前正在运行任务A让任务A休眠，需要在处理结束之后，马上切换到下一个任务 */
/* 当前正在运行任务A让任务B休眠，主要还是移动任务 */
void task_sleep(TASK *task)
{
   TASK *now_task;
   if(task->flags == 2)
   {
        /* 如果处于活动状态 */
        now_task = task_now();
        task_remove(task); /* 执行此语句的话flags将变为1 */
        if(task == now_task)
        {
            /* 如果是让自己休眠，则需要进行任务切换,以level为最高优先级 */
            task_switchsub();  /* 他主要修改当前的level */
            now_task = task_now(); /* 在设定后获取当前任务的值(根据当前的level,找到该level下活动的task) */
            farjmp(0, now_task->sel);
        }
   }
   return;
}

void task_idle()
{
    for(;;)
    {
        io_hlt();
    }
}

