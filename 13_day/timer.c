#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

#define TIMER_FLAGS_ALLOC 1 /* 已配置状态 */
#define TIMER_FLAGS_USING 2 /* 定时器运行中 */

TIMERCTL timerctl;

void init_pit(void)
{
    int i;
    /* 中断周期变更 100hz */
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    timerctl.count   = 0;
    timerctl.next    = 0xffffffff; /*  */
    for(i=0; i<MAX_TIMER; i++)
    {
        timerctl.timers0[i].flags = 0; /* 未使用状态 */
    }

    return;
}

TIMER *timer_alloc(void)
{
    int i;
    for(i=0; i<MAX_TIMER; i++)
    {
        if(timerctl.timers0[i].flags == 0)
        {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timers0[i];
        }
    }
    return 0;
}

void timer_free(TIMER *timer)
{
    timer->flags = 0; /* 置为未使用状态 */
    return;
}

void timer_init(TIMER *timer, FIFO32 *fifo, int data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime(TIMER *timer, unsigned int timeout)
{
    int    e;
    TIMER *t, *s;
    timer->timeout = timeout + timerctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    e = io_load_eflags(); /* 记录中断状态 */
    io_cli(); /* 禁止中断 */
    timerctl.using++;
    if(timerctl.using == 1)
    {
        /* 处于运行状态的定时器只有一个时 */
        timerctl.t0 = timer;
        timer->next        = 0; /* 没有下一个 */
        io_store_eflags(e);
    }

    /* 先看链表头 */
    t = timerctl.t0;
    if(timer->timeout <= t->timeout)
    {
        timerctl.t0 = timer;
        timer->next        = t; /* 链表插入 */
        timerctl.next      = timer->timeout; /* 下一个超时时间 */
        io_store_eflags(e);
        return;
    }

    for(;;)
    {
        s = t;
        t = t->next;  /* 链表顺序遍历 */
        if(t == 0)
        {
            break; /* 遍历链表到末尾了 */
        }
        if(timer->timeout <= t->timeout)
        {
            /* 插入到s,和t之间 */
            s->next = timer;
            timer->next = t;
            io_store_eflags(e);
            return;
        }
    }

    /* 插入到最后面的情况 */
    s->next     = timer;
    timer->next = 0;
    io_store_eflags(e);

    return;
}

void inthandler20(int *esp)
{
    int    i, j;
    TIMER *timer;
    io_out8(PIC0_OCW2, 0x60); /* 把 IRQ-00信号接收完了的信息通知给PIC */
    /*暂时啥也不做*/
    timerctl.count++;
    if(timerctl.next > timerctl.count)
    {
        return; /* 如果还不到下一个时刻，就不执行后面的 */
    }
    timer = timerctl.t0; /* 首先把最前面的地址付给timer */
    for(i = 0; i < timerctl.using; i++)
    {
        /* timers的定时器都处于动作中，所以不确认flags */
        if(timerctl.t0->timeout > timerctl.count)
        {
            /* 如果进来，说明有没有到的定时器*/
            break;
        }
        /* 这都是超时的 */
        timer->flags = TIMER_FLAGS_ALLOC;
        fifo32_put(timer->fifo, timer->data);
        timer = timer->next; /* 类似于链表的逻辑 */
    }

    /* 前面的执行结果表明，前i个都已经被赋值为TIMER_FLAGS_ALLOC了，所以，处于动作中的定时器就应该减少i*/
    /* 可以理解为0->i-1的timers都报废了 */
    timerctl.using -= i;
    /* 新移位 */
    timerctl.t0 = timer;

    if(timerctl.using > 0)
    {
        /* 不做骚操作，直接把第一个timeout拿给next */
        timerctl.next = timerctl.t0->timeout; 
    }
    else
    {
        timerctl.next = 0xffffffff;
    }

    return;
}
