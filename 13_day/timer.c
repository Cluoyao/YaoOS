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

void timer_init(TIMER *timer, struct FIFO8 *fifo, unsigned char data)
{
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime(TIMER *timer, unsigned int timeout)
{
    int e, i, j;
    timer->timeout = timeout + timerctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    e = io_load_eflags(); /* 记录中断状态 */
    io_cli(); /* 禁止中断 */

    /* 搜索注册位置 */
    for(i = 0; i < timerctl.using; i++)
    {
        /* 找到一个比timers里面的timeout大的位置 */
        if(timerctl.timers[i]->timeout >= timer->timeout)
        {
            break;
        }
    }

    /* timer此时应该放在i的位置，i之后的往后挪 */
    for(j = timerctl.using; j > i; j--)
    {
        timerctl.timers[j] = timerctl.timers[j - 1];
    }
    timerctl.using++;
    timerctl.timers[i] = timer;
    timerctl.next      = timerctl.timers[0]->timeout;
    io_store_eflags(e);

    return;
}

void inthandler20(int *esp)
{
    int i, j;
    io_out8(PIC0_OCW2, 0x60); /* 把 IRQ-00信号接收完了的信息通知给PIC */
    /*暂时啥也不做*/
    timerctl.count++;
    if(timerctl.next > timerctl.count)
    {
        return; /* 如果还不到下一个时刻，就不执行后面的 */
    }

    for(i = 0; i < timerctl.using; i++)
    {
        /* timers的定时器都处于动作中，所以不确认flags */
        if(timerctl.timers[i]->timeout > timerctl.count)
        {
            /* 如果进来，说明又没有到的定时器*/
            break;
        }
        /* 这都是超时的 */
        timerctl.timers[i]->flags = TIMER_FLAGS_ALLOC;
        fifo8_put(timerctl.timers[i]->fifo, timerctl.timers[i]->data);
    }

    /* 前面的执行结果表明，前i个都已经被赋值为TIMER_FLAGS_ALLOC了，所以，处于动作中的定时器就应该减少i*/
    /* 可以理解为0->i-1的timers都报废了 */
    timerctl.using -= i;
    for(j=0; j<timerctl.using; j++)
    {
        /* 进入这里，说明定时器都是处于动作中的 */
        /* 把剩下的都往前面挪 */
        timerctl.timers[j] = timerctl.timers[i + j]; 
    }
    if(timerctl.using > 0)
    {
        /* 不做骚操作，直接把第一个timeout拿给next */
        timerctl.next = timerctl.timers[0]->timeout; 
    }
    else
    {
        timerctl.next = 0xffffffff;
    }

    return;
}
