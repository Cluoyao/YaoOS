/* bootpack*/

#include "bootpack.h"
#include <stdio.h>

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);

void putfonts8_asc_sht(SHEET *sht, int x, int y, int c, int b, char *s, int l);

void make_textbox8(SHEET *sht, int x0, int y0, int sx, int sy, int c);

void task_b_main(SHEET *sht_back);

#define MEMMAN_ADDR 0x003c0000  /* 栈及其他的空间里面*/

void HariMain(void)
{
	struct BOOTINFO           *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	FIFO32                     fifo;
	char                       s[40];
	int                        fifobuf[128];
   
	TIMER                     *timer;
	int                        mx, my, i, cursor_x, cursor_c;
	struct MOUSE_DEC           mdec; 
   
	unsigned int               memtotal, count = 0;
	MEMMAN                    *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL                    *shtctl;
	SHEET                     *sht_back, *sht_mouse, *sht_win, *sht_win_b[3];
	unsigned char             *buf_back, buf_mouse[256], *buf_win, *buf_win_b;
	TASK                      *task_a, *task_b[3];

	static char      keytable[0x54] = 
	{
		0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.'
	};

	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */

	fifo32_init(&fifo, 128, fifobuf, 0);
	init_pit();
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /* 开放PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

	memtotal = memtest(0x00400000, 0xbfffffff);
	memman_init(memman);
	memman_free(memman, 0x00001000, 0x0009e000); /* 0x00001000 - 0x0009efff */
	memman_free(memman, 0x00400000, memtotal - 0x00400000); /* 用于管理的内存区域 */

	init_palette();
	shtctl    = shtctl_init(memman, binfo->vram, binfo->scrnx, binfo->scrny);
	task_a    = task_init(memman);
	fifo.task = task_a;

	sht_back  = sheet_alloc(shtctl); /* 从管理单元中拿一个图层出来用，作为背景图层 */
	buf_back  = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); /* 分配图层缓存，存放背景信息 */
	sheet_setbuf(sht_back,  buf_back,  binfo->scrnx, binfo->scrny, -1); /* 没有透明色 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

	/* sht_win_b */
	for(i = 0; i < 3; i++)
	{
		sht_win_b[i]  = sheet_alloc(shtctl); /* 从管理单元中拿一个图层出来用，作为背景图层 */
		buf_win_b     = (unsigned char *)memman_alloc_4k(memman, 144 * 52); /* 分配图层缓存，存放背景信息 */
		sheet_setbuf(sht_win_b[i],  buf_win_b,  144, 52, -1); /* 没有透明色 */
		sprintf(s, "task_b%d", i);
		make_window8(buf_win_b, 144, 52, s, 0);

		task_b[i]          = task_alloc();
		task_b[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
		task_b[i]->tss.eip = (int)&task_b_main;
		task_b[i]->tss.es  = 1 * 8;
		task_b[i]->tss.cs  = 2 * 8;
		task_b[i]->tss.ss  = 1 * 8;
		task_b[i]->tss.ds  = 1 * 8;
		task_b[i]->tss.fs  = 1 * 8;
		task_b[i]->tss.gs  = 1 * 8;
		*((int *)(task_b[i]->tss.esp + 4)) = (int)sht_win_b[i]; /* 现在显示在窗口中,task_b的入参数sheet */
		task_run(task_b[i]);
	}

	/* sht_win */
	sht_win   = sheet_alloc(shtctl); 
	buf_win   = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);
	make_window8(buf_win, 144, 52, "Task_A", 1);
	make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;
	timer = timer_alloc();
	timer_init(timer, &fifo,  1);
	timer_settime(timer, 50);

	/* sht_mouse */
	sht_mouse = sheet_alloc(shtctl); /* 从管理单元拿出一个图层来用，作为鼠标图层 */
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); /* 透明色号99 */
	init_mouse_cursor8(buf_mouse, 99); /* 背景色号99 */
	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;



	/* 从左上角(0,0)点开始绘制显示界面 */
	sheet_slide(sht_back, 0, 0);
	sheet_slide(sht_win_b[0], 168, 56);
	sheet_slide(sht_win_b[1], 8,   116);
	sheet_slide(sht_win_b[2], 168, 116);

	/* 移动鼠标图层到指定位置 */
	sheet_slide(sht_win,   8, 56);
	sheet_slide(sht_mouse, mx, my);

	/* 设置显示背景（图层）高度为0 */
	sheet_updown(sht_back,      0);
	sheet_updown(sht_win_b[0],  1);
	sheet_updown(sht_win_b[1],  2);
	sheet_updown(sht_win_b[2],  3);
	sheet_updown(sht_win,       4);
	sheet_updown(sht_mouse,     5);

	sprintf(s, "(%d, %d)", mx, my);
	/* 在背景图层上显示信息 */
	putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);

	sprintf(s, "memory %dMB free : %dKB", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	/* 在背景图层上显示信息 */
	putfonts8_asc_sht(sht_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

	for (;;) 
	{
		io_cli(); /* 关闭中断 */
		if (fifo32_status(&fifo) == 0) 
		{
			task_sleep(task_a); /* 注意调用顺序 */
			io_sti();
		} 
		else 
		{
			i = fifo32_get(&fifo);
			io_sti();

			if (256 <= i && i <= 511) /* 键盘数据 */
			{
				sprintf(s, "%02X", i - 256);
				putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				if (i < 256 + 0x54) 
				{
					if(keytable[i - 256] != 0 && cursor_x < 144) /* 一般字符 */
					{
						s[0] = keytable[i - 256];
						s[1] = 0;
						putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
						cursor_x += 8;
					}
				}
				if(i == 256 + 0x0e && cursor_x > 8)
				{
					putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
					cursor_x -= 8;
				}
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43); /* 擦除 */
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			} 
			else if (512 <= i && i <= 767) /* 鼠标数据 */
			{
				if (mouse_decode(&mdec, i - 512) != 0) 
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
					putfonts8_asc_sht(sht_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
					sheet_slide(sht_mouse, mx, my);
					if((mdec.btn & 0x01) != 0)
					{
						/* 按下左键，移动sht_win,因为原点在左上角，-80是将窗口起点相对鼠标往左偏，-8是往上偏 */
						sheet_slide(sht_win, mx - 80, my - 8);
					}
				}
			}
			else if(i <= 1)
			{
				if(i != 0)
				{
					timer_init(timer, &fifo, 0); /* 设置为0 */
					cursor_c = COL8_000000;
				}
				else
				{
					timer_init(timer, &fifo, 1); /* 设置为0 */
					cursor_c = COL8_FFFFFF;
				}

				timer_settime(timer, 50); /* 继续设定0.5s的定时，让其一闪一闪亮晶晶 */
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43); /* 显示光标 */
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}

 
		}
	}
}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act)
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
	char c, tc, tbc; /* tc:标题颜色，tbc：标题背景颜色 */

	if(act != 0)
	{
		/* 颜色不变 */
		tc  = COL8_FFFFFF;
		tbc = COL8_000084;
	}
	else
	{
		tc  = COL8_C6C6C6;
		tbc = COL8_848484;
	}

	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
	boxfill8(buf, xsize, tbc,         3, 3, xsize - 4, 20 );
	boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);
	putfonts8_asc(buf, xsize, 24, 4, tc, title);

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

void make_textbox8(SHEET *sht, int x0, int y0, int sx, int sy, int c)
{
	int x1 = x0 + sx, y1 = y0 + sy;
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
	boxfill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
	boxfill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
	boxfill8(sht->buf, sht->bxsize, c, x0 - 1, y0 - 1, x1 + 0, y1 + 0);
	return;
}

void task_b_main(SHEET *sht_back_b)
{
	FIFO32 fifo;
	TIMER *timer_1s;
	int    i, fifobuf[128], count = 0, count0 = 0;
	char   s[12];

	fifo32_init(&fifo, 128, fifobuf, 0);
	timer_1s = timer_alloc();
	timer_init(timer_1s, &fifo, 100);
	timer_settime(timer_1s, 100);

	for(;;)
	{
		count++;
		
		io_cli(); /* 先禁止中断发生，获取信息 */
		if(fifo32_status(&fifo) == 0)
		{
			io_sti(); /* 刚发现，如果只用io_hlt，会让CPU直接挂起，不工作了，操作系统也不运行了，哈哈*/
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti(); /* 如果前面的if不成立，就开放中断，允许中断发生 */

			if(i == 100)
			{
				sprintf(s, "%11d", count - count0);
				putfonts8_asc_sht(sht_back_b, 24, 28, COL8_000000, COL8_C6C6C6, s, 11);
				count0 = count;
				timer_settime(timer_1s, 100);
			}
		}

	}
}

