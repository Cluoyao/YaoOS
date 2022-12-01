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
    for(i=0; i<MAX_TIMER; i++)
    {
        timerctl.timer[i].flags = 0; /* 未使用状态 */
    }

    return;
}

TIMER *timer_alloc(void)
{
    int i;
    for(i=0; i<MAX_TIMER; i++)
    {
        if(timerctl.timer[i].flags == 0)
        {
            timerctl.timer[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timer[i];
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
    timer->timeout = timeout + timerctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    return;
}

void inthandler20(int *esp)
{
    int i;
    io_out8(PIC0_OCW2, 0x60); /* 把 IRQ-00信号接收完了的信息通知给PIC */
    /*暂时啥也不做*/
    timerctl.count++;
    for(i=0; i<MAX_TIMER; i++)
    {
        if(timerctl.timer[i].flags == TIMER_FLAGS_USING)
        {
            if(timerctl.timer[i].timeout <= timerctl.count)
            {
                timerctl.timer[i].flags = TIMER_FLAGS_ALLOC; /* 表示这个timer又是有效的 */
                fifo8_put(timerctl.timer[i].fifo, timerctl.timer[i].data);
            }
        }
    }
    return;
}
