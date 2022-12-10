/* bootpack*/

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

void task_b_main()
{
	FIFO32 fifo;
	TIMER *timer;
	int    i, fifobuf[128];
	fifo32_init(&fifo, 128, fifobuf);
	timer = timer_alloc();
	timer_init(timer, &fifo, 1);
	timer_settime(timer, 500);


	for(;;)
	{
		io_cli(); /* 先禁止中断发生，获取信息 */
		if(fifo32_status(&fifo) == 0)
		{
			io_stihlt(); /* 刚发现，如果只用io_hlt，会让CPU直接挂起，不工作了，操作系统也不运行了，哈哈*/
		}
		else
		{
			i = fifo32_get(&fifo);
			io_sti(); /* 如果前面的if不成立，就开放中断，允许中断发生 */
			if(i == 1)
			{
				taskswitch3();
			}
		}

	}
}

#define MEMMAN_ADDR 0x003c0000  /* 栈及其他的空间里面*/

void HariMain(void)
{
	struct BOOTINFO           *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	FIFO32                     fifo;
	char                       s[40];
	int                        fifobuf[128];
   
	TIMER                     *timer, *timer2, *timer3;
	int                        mx, my, i, cursor_x, cursor_c;
	struct MOUSE_DEC           mdec;
   
	unsigned int               memtotal, count = 0;
	MEMMAN                    *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL                    *shtctl;
	SHEET                     *sht_back, *sht_mouse, *sht_win;
	unsigned char             *buf_back, buf_mouse[256], *buf_win;
	TSS32                      tss_a, tss_b;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
	int                        task_b_esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024; /* 指向栈尾，塞入之后地址逐渐减小 */

	static char      keytable[0x54] = 
	{
		0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0, 0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0, 0, 'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0, 0, ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.'
	};

	tss_a.ldtr   = 0;
	tss_a.iomap  = 0x40000000;
	tss_b.ldtr   = 0;
	tss_b.iomap  = 0x40000000;

	tss_b.eip    = (int)&task_b_main;
	tss_b.eflags = 0x00000202;
	tss_b.eax    = 0;
	tss_b.ecx    = 0;
	tss_b.edx    = 0;
	tss_b.ebx    = 0;
	tss_b.esp    = task_b_esp;
	tss_b.ebp    = 0;
	tss_b.esi    = 0;
	tss_b.edi    = 0;
	tss_b.es     = 1 * 8;
	tss_b.cs     = 2 * 8;
	tss_b.ss     = 1 * 8;
	tss_b.ds     = 1 * 8;
	tss_b.fs     = 1 * 8;
	tss_b.gs     = 1 * 8;

	init_gdtidt();
	init_pic();
	io_sti(); /* IDT/PIC的初始化已经完成，于是开放CPU的中断 */

	set_segmdesc(gdt + 3, 103, (int)&tss_a, AR_TSS32); /* 任务 a */
	set_segmdesc(gdt + 4, 103, (int)&tss_b, AR_TSS32); /* 任务 b */
	load_tr(3 * 8); /* 先让其当前运行GDTw为3的任务 */

	fifo32_init(&fifo, 128, fifobuf);
	init_pit();
	init_keyboard(&fifo, 256);
	enable_mouse(&fifo, 512, &mdec);
	io_out8(PIC0_IMR, 0xf8); /* 开放PIC1和键盘中断(11111000) */
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断(11101111) */

	timer = timer_alloc();
	timer_init(timer, &fifo,  10);
	timer_settime(timer, 1000);

	timer2 = timer_alloc();
	timer_init(timer2, &fifo, 3);
	timer_settime(timer2, 300);

	timer3 = timer_alloc();
	timer_init(timer3, &fifo, 1);
	timer_settime(timer3, 50);

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
	make_window8(buf_win, 160, 52, "Window");

	make_textbox8(sht_win, 8, 28, 144, 16, COL8_FFFFFF);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;

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

	for (;;) 
	{
		io_cli();
		if (fifo32_status(&fifo) == 0) 
		{
			io_stihlt();
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
			else if(i == 10)
			{
				putfonts8_asc_sht(sht_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[Sec]", 7);
				taskswitch4(); /* 做任务切换，而非函数调用 */
				//sprintf(s, "%010d", count);
				//putfonts8_asc_sht(sht_win, 40, 28, COL8_000000, COL8_C6C6C6, s, 10);
			}
			else if (i == 3)
			{
				putfonts8_asc_sht(sht_back, 0, 80, COL8_FFFFFF, COL8_008484, "3[Sec]",  6);
				count = 0;
			}
			else if(i <= 1)
			{
				if(i != 0)
				{
					timer_init(timer3, &fifo, 0); /* 设置为0 */
					cursor_c = COL8_000000;
				}
				else
				{
					timer_init(timer3, &fifo, 1); /* 设置为0 */
					cursor_c = COL8_FFFFFF;
				}

				timer_settime(timer3, 50); /* 继续设定0.5s的定时，让其一闪一闪亮晶晶 */
				boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43); /* 显示光标 */
				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			}

 
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

