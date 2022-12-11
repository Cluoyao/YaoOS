/* FIFO */

#include "bootpack.h"

#define FLAGS_OVERRUN		0x0001

void fifo32_init(FIFO32 *fifo, int size, int *buf, TASK *task)
/* 初始化FIFO缓冲区 */
{
	fifo->size  = size;
	fifo->buf   = buf;
	fifo->free  = size; /* 缓冲区大小 */
	fifo->flags = 0;
	fifo->p     = 0; /* 下一个数据写入位置 */
	fifo->q     = 0; /* 下一个数据读出位置 */
	fifo->task  = task; /* 有数据写入时需要唤醒的任务 */

	return;
}

int fifo32_put(FIFO32 *fifo, int data)
/* 向FIFO传送数据并保存 */
{
	if (fifo->free == 0) {
		/* 没有空间了，溢出 */
		fifo->flags |= FLAGS_OVERRUN;
		return -1;
	}
	fifo->buf[fifo->p] = data;
	fifo->p++;
	if (fifo->p == fifo->size) {
		fifo->p = 0;
	}
	fifo->free--;

	if(fifo->task != 0) /* 如果设置了自动唤醒任务 */
	{
		if(fifo->task->flags != 2) /* 2是正在使用，1是不使用（休眠状态），*/
		{
			task_run(fifo->task); /* 让任务工作起来 */
		}
	}

	return 0;
}

int fifo32_get(FIFO32 *fifo)
/* 从FIFO取得一个数据 */
{
	int data;
	if (fifo->free == fifo->size) {
		/* 如果缓冲区为空则返回-1 */
		return -1;
	}
	data = fifo->buf[fifo->q];
	fifo->q++;
	if (fifo->q == fifo->size) {
		fifo->q = 0;
	}
	fifo->free++; /* 拿一个少一个，free因此增加一个 */
	return data;
}

int fifo32_status(FIFO32 *fifo)
/* 报告一下积攒是数据量 */
{
	return fifo->size - fifo->free;
}
