#include "bootpack.h"

#define FLAGS_OVERFLOW   0x0001

/*
    fifo: 输入/输出结构体
    size: 这个缓冲区的大小
    buf：缓冲区的地址
*/

void fifo8_init(Fifo8 *fifo, int size, unsigned char *buf)
{
    fifo->size  = size; /* 缓冲区大小*/
    fifo->buf   = buf;
    fifo->free  = size; /* 缓冲区大小*/
    fifo->flags = 0;    /* 是否溢出*/
    fifo->p     = 0;    /* 下一个数据写入位置 */
    fifo->q     = 0;    /* 下一个数据读出位置 */

    return;
}

/*
    向fifo传送数据并保存
*/
int fifo8_put(Fifo8 *fifo, unsigned char data)
{
    /* 没有剩余空间了 */
    if(fifo->free == 0)
    {
        /* 写入溢出标记 */
        fifo->flags |= FLAGS_OVERFLOW;
        return -1;
    }

    fifo->buf[fifo->p] = data;
    fifo->p++;
    if(fifo->p == fifo->size)
    {
        fifo->p = 0;
    }
    /* 写入一个少一个 */
    fifo->free--;

    return 0;
}

/*
    从fifo取得一个数据
*/
int fifo8_get(Fifo8 *fifo)
{
    int data;
    /* 如果缓冲区为空，返回错误值(没有数据)*/
    if(fifo->free == fifo->size)
    {
        return -1;
    }

    data = fifo->buf[fifo->q];
    fifo->q++;
    if(fifo->q == fifo->size)
    {
        fifo->q = 0;
    }
    fifo->free++;

    return data;
}

/*
    报告FIFO8到底积攒了多少数据
*/
int fifo8_status(Fifo8 *fifo)
{
    return fifo->size - fifo->free;
}
