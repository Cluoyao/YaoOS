#include "bootpack.h"

#define PORT_KEYDAT  0x0060

Fifo8 keyfifo;
Fifo8 mousefifo;

void init_pic(void)
{
    io_out8(PIC0_IMR, 0xff);   // 禁止所有中断
    io_out8(PIC1_IMR, 0xff);   // 禁止所有中断

    io_out8(PIC0_ICW1, 0x11);   // 边沿触发模式
    io_out8(PIC0_ICW2, 0x20);   // IRQ 0-7由INT 20-27接收
    io_out8(PIC0_ICW3, 1 << 2); // pic1由irq2连接
    io_out8(PIC0_ICW4, 0x01);   // 无缓冲区模式

    io_out8(PIC1_ICW1, 0x11);
    io_out8(PIC1_ICW2, 0x28);
    io_out8(PIC1_ICW3, 2);
    io_out8(PIC1_ICW4, 0x01);

    io_out8(PIC0_IMR, 0xfb); // 11111011 PIC1以外全部禁止
    io_out8(PIC1_IMR, 0xff); // 11111111 禁止所有中断

    return;
}

void inthandler21(int *esp)
{
    unsigned char data;
    io_out8(PIC0_OCW2, 0x61); /* 通知PIC IRQ-01已经受理完毕 */
    data = io_in8(PORT_KEYDAT);  /* 从端口（设备）取得数据*/
    fifo8_put(&keyfifo, data);
    return;    
}

void inthandler2c(int *esp)
{
    unsigned char data;
    io_out8(PIC1_OCW2, 0x64);    /* 通知PIC1 IRQ-12的受理已经完成（这是从PIC）*/
    io_out8(PIC0_OCW2, 0x62);    /* 通知PIC0 IRQ-02的受理已经完成，这是主PIC跟从PIC通信的端口*/
    data = io_in8(PORT_KEYDAT);
    fifo8_put(&mousefifo, data);
    return ;
}

void inthandler27(int *esp)
{
    io_out8(PIC0_OCW2, 0x67);
    return;
}