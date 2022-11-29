#include "bootpack.h"

#define PIT_CTRL 0x0043
#define PIT_CNT0 0x0040

TIMERCTL timerctl;

void init_pit(void)
{
    /* 中断周期变更 100hz */
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);

    timerctl.count   = 0;
    timerctl.timeout = 0;

    return;
}

void inthandler20(int *esp)
{
    io_out8(PIC0_OCW2, 0x60); /* 把 IRQ-00信号接收完了的信息通知给PIC */
    /*暂时啥也不做*/
    timerctl.count++;
    if(timerctl.timeout > 0)
    {
        timerctl.timeout--;
        if(timerctl.timeout == 0)
        {
            fifo8_put(timerctl.fifo, timerctl.data);
        }
    }
    return;
}

void settimer(unsigned int timeout, struct FIFO8 *fifo, unsigned char data)
{
    int eflags;
    eflags = io_load_eflags(); /* 禁止中断 */
    io_sti();
    timerctl.timeout = timeout;
    timerctl.fifo    = fifo;
    timerctl.data    = data;
    io_store_eflags(eflags);   /* 恢复中断 */
    return ;
}