/* bootpackのメイン */

#include "bootpack.h"
#include <stdio.h>

/*
 * x,y:显示位置的坐标
 * c :字符颜色
 * b :背景颜色
 * s:字符串
 * l:字符串长度
 */
void putfonts8_asc_sht(SHEET *sht, int x, int y, int c, int b, char *s, int l)
{
	/* 一个字符占8个bit，所以有l * 8 - 1*/
	boxfill8(sht->buf, sht->bxsize, b, x, y, x + l * 8 - 1, y + 15); /* 用背景颜色擦除掉之前的内容 */
	putfonts8_asc(sht->buf, sht->bxsize, x, y, c, s); /* 写上要写的内容 */
	sheet_refresh(sht, x, y, x + l * 8, y + 16);
	return;
}

#define MEMMAN_ADDR 0x003c0000  /* 栈及其他的空间里面*/

void HariMain(void)
{
	struct BOOTINFO *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	struct FIFO8     timerfifo, timerfifo2, timerfifo3;
	char             s[40], mcursor[256], keybuf[32], mousebuf[128], timerbuf[8], timerbuf2[8], timerbuf3[8];
	TIMER           *timer, *timer2, *timer3;
	int              mx, my, i;
	struct MOUSE_DEC mdec;

	unsigned int     memtotal, count = 0;
	MEMMAN          *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL          *shtctl;
	SHEET           *sht_back, *sht_mouse, *sht_win;
	unsigned char   *buf_back, buf_mouse[256], *buf_win;


	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */

	fifo8_init(&keyfifo, 32, keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);

	init_pit();

	fifo8_init(&timerfifo,  8, timerbuf);
	timer = timer_alloc();
	timer_init(timer, &timerfifo,  10);
	timer_settime(timer, 1000);

	fifo8_init(&timerfifo, 8, timerbuf2);
	timer2 = timer_alloc();
	timer_init(timer2, &timerfifo, 3);
	timer_settime(timer2, 300);

	fifo8_init(&timerfifo, 8, timerbuf3);
	timer3 = timer_alloc();
	timer_init(timer3, &timerfifo, 1);
	timer_settime(timer3, 50);

	io_out8(PIC0_IMR, 0xf8); /* 开放PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

	init_keyboard();
	enable_mouse(&mdec);
	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000); /* 用于管理的内存区域 */

	init_palette();
	shtctl    = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	sht_back  = sheet_alloc(shtctl); /* 从管理单元中拿一个图层出来用，作为背景图层 */
	sht_mouse = sheet_alloc(shtctl); /* 从管理单元拿出一个图层来用，作为鼠标图层 */
	sht_win   = sheet_alloc(shtctl); 
	buf_back  = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); /* 分配图层缓存，存放背景信息 */
	buf_win   = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_back,  buf_back,  binfo->scrnx, binfo->scrny, -1); /* 没有透明色 */
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); /* 透明色号99 */
	sheet_setbuf(sht_win, buf_win, 160, 52, -1);

	/* 填充图层中每个像素的点上的颜色信息 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);
	init_mouse_cursor8(buf_mouse, 99); /* 背景色号99 */
	make_window8(buf_win, 160, 52, "Counter");

	/* 从左上角(0,0)点开始绘制显示界面 */
	sheet_slide(sht_back, 0, 0);

	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;

	/* 移动鼠标图层到指定位置 */
	sheet_slide(sht_mouse, mx, my);
	sheet_slide(sht_win,   80, 72);
	/* 设置显示背景（图层）高度为0 */
	sheet_updown(sht_back,  0);
	sheet_updown(sht_win,   1);
	/* 设置鼠标（图层）高度为1 */
	sheet_updown(sht_mouse, 2);

	sprintf(s, "(%d, %d)", mx, my);
	/* 在背景图层上显示信息 */
	putfonts8_asc(buf_back, binfo->scrnx, 0, 0, COL8_FFFFFF, s);

	sprintf(s, "memory %dMB free : %dKB", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	/* 在背景图层上显示信息 */
	putfonts8_asc(buf_back, binfo->scrnx, 0, 32, COL8_FFFFFF, s);
	/* 刷新两个图层 */
	sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48); /* 刷新打印信息 */

	for (;;) {

		count++;
		sprintf(s, "%010d", count);
		putfonts8_asc_sht(sht_win, 40, 28, COL8_000000, COL8_C6C6C6, s, 10);

		io_cli();
		if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) + fifo8_status(&timerfifo) == 0) 
		{
			io_stihlt();
		} 
		else 
		{
			if (fifo8_status(&keyfifo) != 0) 
			{
				i = fifo8_get(&keyfifo);
				io_sti();
				sprintf(s, "%02X", i);
				putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 15);
			} 
			else if (fifo8_status(&mousefifo) != 0) 
			{
				i = fifo8_get(&mousefifo);
				io_sti();
				if (mouse_decode(&mdec, i) != 0) 
				{
					/* 3字节都凑齐了，所以把它们显示出来*/
					sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
					if ((mdec.btn & 0x01) != 0) 
					{
						s[1] = 'L';
					}
					if ((mdec.btn & 0x02) != 0) 
					{
						s[3] = 'R';
					}
					if ((mdec.btn & 0x04) != 0) 
					{
						s[2] = 'C';
					}
					putfonts8_asc_sht(sht_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);

					mx += mdec.x;
					my += mdec.y;
					if (mx < 0)
					{
						mx = 0;
					}
					if (my < 0) 
					{
						my = 0;
					}
					if (mx > binfo->scrnx - 1) 
					{
						mx = binfo->scrnx - 1;
					}
					if (my > binfo->scrny - 1) 
					{
						my = binfo->scrny - 1;
					}
					sprintf(s, "(%3d, %3d)", mx, my);
					putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 80 - 1);
					sheet_slide(sht_mouse, mx, my);
				}
			}
			else if(fifo8_status(&timerfifo) != 0)
			{
				i = fifo8_get(&timerfifo);
				io_sti(); // 允许中断发生

				if(i == 10)
				{
					putfonts8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[Sec]", 7);
				}
				else if (i == 3)
				{
					putfonts8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[Sec]",  6);
				}
				else 
				{
					if(i != 0)
					{
						timer_init(timer3, &timerfifo, 0); /* 设置为0 */
						boxfill8(buf_back, binfo->scrnx, COL8_FF00FF, 8, 96, 15, 111); /* 显示光标 */
					}
					else
					{
						timer_init(timer3, &timerfifo, 1); /* 设置为1 */
						boxfill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111); /* 擦除 */
					}
					timer_settime(timer3, 50); /* 继续设定0.5s的定时，让其一闪一闪亮晶晶 */
					sheet_refresh(sht_back, 8, 96, 16, 112); 
				}
			}
			// else if(fifo8_status(&timerfifo2) != 0)
			// {
			// 	i = fifo8_get(&timerfifo2);
			// 	io_sti(); // 允许中断发生
			// 	putfonts8_asc(buf_back, binfo->scrnx, 0, 80, COL8_FFFFFF, "3[Sec]");
			// 	sheet_refresh(sht_back, 0, 80, 48, 96); /* 一个字符之前设计的是16(h)*4(w) */
			// }
			// else if(fifo8_status(&timerfifo3) != 0)
			// {
			// 	i = fifo8_get(&timerfifo3);
			// 	io_sti(); // 允许中断发生
			// 	if(i != 0)
			// 	{
			// 		timer_init(timer3, &timerfifo3, 0); /* 设置为0 */
			// 		boxfill8(buf_back, binfo->scrnx, COL8_FFFFFF, 8, 96, 15, 111);
			// 	}
			// 	else
			// 	{
			// 		timer_init(timer3, &timerfifo3, 1); /* 设置为1 */
			// 		boxfill8(buf_back, binfo->scrnx, COL8_008484, 8, 96, 15, 111);
			// 	}
			// 	timer_settime(timer3, 50);
			// 	sheet_refresh(sht_back, 8, 96, 16, 112); 
			// }
		}
	}
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title)
{
	static char closebtn[14][16] = {
		"OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@"
	};

	int x, y;
	char c;
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_000084, 3, 3, xsize - 4, 20 );
	boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);
	putfonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);

	for (y = 0; y < 14; y++) {
		for (x = 0; x < 16; x++) {
			c = closebtn[y][x];
			if (c == '@') {
				c = COL8_000000;
			} else if (c == '$') {
				c = COL8_848484;
			} else if (c == 'Q') {
				c = COL8_C6C6C6;
			} else {
				c = COL8_FFFFFF;
			}
			buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
		}
	}
	return;
}

