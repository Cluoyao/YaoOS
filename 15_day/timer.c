#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

#define TIMER_FLAGS_ALLOC 1 /* 已配置状态 */
#define TIMER_FLAGS_USING 2 /* 定时器运行中 */

TIMERCTL timerctl;

void init_pit(void)
{
    int    i;
    TIMER *t;
    /* 中断周期变更 100hz */
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    timerctl.count   = 0;
    for(i=0; i<MAX_TIMER; i++)
    {
        timerctl.timers0[i].flags = 0; /* 未使用状态 */
    }

    t          = timer_alloc(); /* 取得一个定时器,这是哨兵！！！ */
    t->timeout = 0xffffffff;
    t->flags   = TIMER_FLAGS_USING;
    t->next    = 0; /* 因为现在只有他一个，所以他的后面应该是0 */

    timerctl.t0    = t; /* 因为现在只有哨兵，所以他就在最前面 */
    timerctl.next  = 0xffffffff; /* 因为只有哨兵，所以下一个超时的时刻就是哨兵的时刻 */

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
/*
    *建立链表
    *timer:插入的定时器timer,
    *timeout:设定该定时器的超时时间
*/
void timer_settime(TIMER *timer, unsigned int timeout)
{
    int    e;
    TIMER *t, *s;
    timer->timeout = timeout + timerctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    e = io_load_eflags(); /* 记录中断状态 */
    io_cli(); /* 禁止中断 */

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

    /* 再看链表中 */
    for(;;)
    {
        s = t;
        t = t->next;  /* 链表顺序遍历 */
        if(timer->timeout <= t->timeout)
        {
            /* 插入到s,和t之间 */
            s->next = timer;
            timer->next = t;
            io_store_eflags(e);
            return;
        }
    }
}

/* 接受定时器中断的函数 */
void inthandler20(int *esp)
{
    char   ts = 0; /* 仅作为一个标志位来使用，没啥大道理 */
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
    /* 链表遍历 */
    for(;;)
    {
        /* timers的定时器都处于动作中，所以不确认flags */
        if(timer->timeout > timerctl.count)
        {
            /* 如果进来，说明有没有到的定时器*/
            break;
        }
        /* 这都是超时的 */
        timer->flags = TIMER_FLAGS_ALLOC;
        if(timer != mt_timer)
        {
            fifo32_put(timer->fifo, timer->data);
        }
        else
        {
            ts = 1; /* mt_timer 超时时间*/
        }
        
        timer = timer->next; /* 类似于链表的逻辑 */
    }

    /* 新移位 */
    timerctl.t0 = timer;

    /* 不做骚操作，直接把第一个timeout拿给next */
    timerctl.next = timer->timeout; 

    if(ts != 0)
    {
        mt_taskswitch();
    }

    return;
}
