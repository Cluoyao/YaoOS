/* bootpack*/

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);

void putfonts8_asc_sht(SHEET *sht, int x, int y, int c, int b, char *s, int l);

void make_textbox8(SHEET *sht, int x0, int y0, int sx, int sy, int c);

void task_b_main(SHEET *sht_back);

void console_task(SHEET *sheet, unsigned int memtotal);

int  cons_newline(int cursor_y, SHEET *sheet);

#define MEMMAN_ADDR 0x003c0000  /* 栈及其他的空间里面*/
#define KEYCMD_LED  0xed

void HariMain(void)
{
	struct BOOTINFO           *binfo = (struct BOOTINFO *) ADR_BOOTINFO;
	FIFO32                     fifo, keycmd;
	char                       s[40];
	int                        fifobuf[128];
   
	TIMER                     *timer;
	int                        mx, my, i, cursor_x, cursor_c;
	struct MOUSE_DEC           mdec; 
   
	unsigned int               memtotal, count = 0;
	MEMMAN                    *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL                    *shtctl;
	SHEET                     *sht_back, *sht_mouse, *sht_win,  *sht_cons;
	unsigned char             *buf_back, buf_mouse[256], *buf_win, *buf_cons;
	TASK                      *task_a, *task_cons;
	int                        key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	int                        key_capslk = 0;

	static char keytable0[0x80] = {
		0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`',   0,   '\\', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
	static char keytable1[0x80] = {
		0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0,   '|', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
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
	task_run(task_a, 1, 0); /* task_a设置为level1 */

	sht_back  = sheet_alloc(shtctl); /* 从管理单元中拿一个图层出来用，作为背景图层 */
	buf_back  = (unsigned char *)memman_alloc_4k(memman, binfo->scrnx * binfo->scrny); /* 分配图层缓存，存放背景信息 */
	sheet_setbuf(sht_back,  buf_back,  binfo->scrnx, binfo->scrny, -1); /* 没有透明色 */
	init_screen8(buf_back, binfo->scrnx, binfo->scrny);

	sht_cons  = sheet_alloc(shtctl);
	buf_cons  = (unsigned char *)memman_alloc_4k(memman, 256 * 165);
	sheet_setbuf(sht_cons, buf_cons, 256, 165, -1); /* 无透明颜色 */
	make_window8(buf_cons, 256, 165, "Console", 0);
	make_textbox8(sht_cons, 8, 28, 240, 128, COL8_000000);
	task_cons = task_alloc();
	task_cons->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
	task_cons->tss.eip = (int)&console_task;
	task_cons->tss.es  = 1 * 8;
	task_cons->tss.cs  = 2 * 8;
	task_cons->tss.ss  = 1 * 8;
	task_cons->tss.ds  = 1 * 8;
	task_cons->tss.fs  = 1 * 8;
	task_cons->tss.gs  = 1 * 8;
	*((int *) (task_cons->tss.esp + 4)) = (int)sht_cons;
	*((int *) (task_cons->tss.esp + 8)) = (int)memtotal;
	task_run(task_cons, 2, 2); /* level=2, priority=2 */ 

	/* sht_win */
	sht_win   = sheet_alloc(shtctl); 
	buf_win   = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);
	make_window8(buf_win, 144, 52, "Task_A", 1); /* 默认使用原来是色系 */
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
	sheet_slide(sht_back, 0,  0);
	sheet_slide(sht_cons, 32, 4);

	/* 移动鼠标图层到指定位置 */
	sheet_slide(sht_win,   64, 56);
	sheet_slide(sht_mouse, mx, my);

	/* 设置显示背景（图层）高度为0 */
	sheet_updown(sht_back,      0);
	sheet_updown(sht_cons,      1);
	sheet_updown(sht_win,       2);
	sheet_updown(sht_mouse,     3);

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
				// sprintf(s, "%02X", i - 256);
				// putfonts8_asc_sht(sht_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
				if (i < 256 + 0x80) 
				{
					if(key_shift == 0)
					{
						s[0] = keytable0[i - 256];
					}
					else
					{
						s[0] = keytable1[i - 256];
					}
				}
				else
				{
					s[0] = 0;
				}
				if('A' <= s[0] && s[0] <= 'Z')
				{
					/* 当输入字符为英文字母时 */
					if((key_capslk == 0 && key_shift == 0) || (key_capslk != 0 && key_shift != 0))
					{
						s[0] += 0x20; /* 将大写字母转换成小写字母 */
					}
				}

				if(s[0] != 0)
				{
					if(key_to == 0)
					{
						if(cursor_x < 128)
						{
							/* 显示一个字符之后，将光标后移一位 */
							s[1] = 0;
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
							cursor_x += 8;
						}
					}
					else
					{
						fifo32_put(&task_cons->fifo, s[0] + 256);
					}
				}
				if(i == 256 + 0x0e)
				{
					/* 如果发送给任务A */
					if(key_to == 0)
					{
						if(cursor_x > 8)
						{
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
							cursor_x -= 8;
						}
					}
					else
					{
						fifo32_put(&task_cons->fifo, 8 + 256);
					}

				}
				if(i == 256 + 0x0f)
				{
					/* 如果按下tab键，第一次的话，key_to == 0, 此时task_a颜色变化， */
					if(key_to == 0)
					{
						key_to = 1;
						make_wtitle8(buf_win,  sht_win->bxsize,  "Task_A",  0); /* 变灰 */
						make_wtitle8(buf_cons, sht_cons->bxsize, "Console", 1); /* 切到暗青 */
						cursor_c = -1; /* 不显示光标 */
						boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cursor_x, 28, cursor_x + 7, 43); /* 擦除 */
						fifo32_put(&task_cons->fifo, 2); /* 命令行光标打开 */
					}
					else
					{
						key_to   = 0;
						make_wtitle8(buf_win,  sht_win->bxsize,  "Task_A",  1);
						make_wtitle8(buf_cons, sht_cons->bxsize, "Console", 0);
						cursor_c = COL8_000000; /* 显示光标 */
						fifo32_put(&task_cons->fifo, 3); /* 命令行光标关闭 */
					}
					sheet_refresh(sht_win,  0, 0, sht_win->bxsize,  21);
					sheet_refresh(sht_cons, 0, 0, sht_cons->bxsize, 21);
				}
				if(i == 256 + 0x3a)
				{
					key_capslk = !key_capslk;
				}
				if(i == 256 + 0x2a) /* 左键shift on*/
				{
					key_shift |= 1;
				}
				if(i == 256 + 0x36) /* 右键shift on*/
				{
					key_shift |= 2;
				}
				if(i == 256 + 0xaa) /* 左键shift off*/
				{
					key_shift &= ~1;
				}
				if(i == 256 + 0xb6) /* 右键shift off*/
				{
					key_shift &= ~2;
				}

				if(i == 256 + 0x1c) /* 回车键 */
				{
					if(key_to != 0) /* 发送给命令行 */
					{
						fifo32_put(&task_cons->fifo, 10 + 256);
					}
				}

				/* 重新显示光标 */
				if(cursor_c >= 0)
				{
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43); /* 擦除 */
				}

				sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
			} 
			else if (512 <= i && i <= 767) /* 鼠标数据 */
			{
				if (mouse_decode(&mdec, i - 512) != 0) 
				{

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
					if(cursor_c >= 0)
					{
						cursor_c = COL8_000000;
					}
				}
				else
				{
					timer_init(timer, &fifo, 1); /* 设置为0 */
					if(cursor_c >= 0)
					{
						cursor_c = COL8_FFFFFF;
					}

				}

				timer_settime(timer, 50); /* 继续设定0.5s的定时，让其一闪一闪亮晶晶 */
				if(cursor_c >= 0)
				{
					boxfill8(sht_win->buf, sht_win->bxsize, cursor_c, cursor_x, 28, cursor_x + 7, 43); /* 显示光标 */
					sheet_refresh(sht_win, cursor_x, 28, cursor_x + 8, 44);
				}

			}
		}
	}
}

void make_wtitle8(unsigned char *buf, int xsize, char *title, char act)
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

	int  x, y;
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

	boxfill8(buf, xsize, tbc,         3, 3, xsize - 4, 20 );
	putfonts8_asc(buf, xsize, 24, 4, tc, title);

	for (y = 0; y < 14; y++)
	{
		for (x = 0; x < 16; x++) 
		{
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

}

void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act)
{
	
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0 );
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1 );
	boxfill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
	boxfill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
	boxfill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
	boxfill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
	boxfill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
	boxfill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);

	make_wtitle8(buf, xsize, title, act);

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

void console_task(SHEET *sheet, unsigned int memtotal)
{
	FIFO32     fifo;
	TIMER     *timer;
	TASK      *task            = task_now();
	int        cursor_init_pos = 64;
	int        i, fifobuf[128], cursor_x = cursor_init_pos, cursor_y = 28, cursor_c = -1;
	char       s[2];
	int        x,y;
	char       cmdline[30], *p;
	MEMMAN    *memman = (MEMMAN *)MEMMAN_ADDR;
	FILEINFO  *finfo  = (FILEINFO *)(ADR_DISKIMG + 0x002600);

	fifo32_init(&task->fifo, 128, fifobuf, task);

	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);

	/* 显示提示符 */
	putfonts8_asc_sht(sheet, 8, 29, COL8_FFFFFF, COL8_000000, "YaoOs>", 6);

	for(;;)
	{
		io_cli();
		if(fifo32_status(&task->fifo) == 0)
		{
			task_sleep(task);
			io_sti();
		}
		else
		{
			i = fifo32_get(&task->fifo);
			io_sti();
			if(i <= 1)  /* 光标用定时器 */
			{
				if(i != 0)
				{
					timer_init(timer, &task->fifo, 0); /* 下次置0 */
					if(cursor_c >= 0) /* 光标其实还是在闪，只是没有颜色 */
					{
						cursor_c = COL8_FFFFFF;
					}

				}
				else
				{
					timer_init(timer, &task->fifo, 1);
					if(cursor_c >= 0) /* 光标其实还是在闪，只是没有颜色 */
					{
						cursor_c = COL8_000000;
					}

				}
				timer_settime(timer, 50);
			}
			if(i == 2)
			{
				cursor_c = COL8_FFFFFF; 
			}
			if(i == 3) /* 光标off */
			{
				/* 因为背景是黑色，所以用黑色擦除 */
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cursor_x, 28, cursor_x + 7, 43); /* 擦除 */
				cursor_c = -1;
			}
			if(i >= 256 && i <= 511)
		    {
				if(i == 8 + 256)
				{
					/* 控制退格键移动*/
					if(cursor_x > cursor_init_pos)
					{
						/* 用空白擦除光标后将光标前移一位 */
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
						cursor_x -= 8;
					}
				}
				else if(i == 10 + 256)
				{
					/* 回车键 */
					/* 用空格将光标擦除 */
					putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
					cmdline[cursor_x / 8 - cursor_init_pos / 8] = 0;
					cursor_y                  = cons_newline(cursor_y, sheet);
					/* 执行命令 */
					if(strcmp(cmdline, "mem") == 0)
					{
						/* */
						sprintf(s, "total memory %dMB", memtotal / (1024 * 1024));
						/* 在背景图层上显示信息 */
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						sprintf(s, "free : %dKB", memman_total(memman) / 1024);
						/* 在背景图层上显示信息 */
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);

					}
					else if(strcmp(cmdline, "clear") == 0)
					{
						/* 清除界面的命令 */
						for(y = 28; y < 28 + 128; y++)
						{
							for(x = 8; x < 8 + 240; x++)
							{
								sheet->buf[x + y * sheet->bxsize] = COL8_000000;
							}
						}
						sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
						cursor_y = 28;
					}
					else if(strcmp(cmdline, "luoyao") == 0)
					{
						sprintf(s, "%s", "LuoY is a handsome man.");
						/* 在背景图层上显示信息 */
						putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 24);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if(strcmp(cmdline, "ls") == 0)
					{
						/* ls命令 */
						for(x = 0; x < 224; x++)
						{
							if(finfo[x].name[0] == 0x00)
							{
								break; /* 不包含任何文件信息 */
							}
							if(finfo[x].name[0] != 0xe5)
							{
								if((finfo[x].type & 0x18) == 0)
								{
									sprintf(s, "filename.ext %7d", finfo[x].size);
									for(y = 0; y < 8; y++)
									{
										s[y] = finfo[x].name[y];
									}
									s[9]  = finfo[x].ext[0];
									s[10] = finfo[x].ext[1];
									s[11] = finfo[x].ext[2];
									/* 在背景图层上显示信息 */
									putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, 30);
									cursor_y = cons_newline(cursor_y, sheet);
								}
							}
						}
						cursor_y = cons_newline(cursor_y, sheet);
					}
					else if(strncmp(cmdline, "type ", 5))
					{
						for(y = 0; y < 11; y++)
						{
							s[y] = ' ';
						}
						y = 0; 
						for(x = 5; y < 11 && cmdline[x] != 0; x++)
						{
							if(cmdline[x] == '.' && y <= 8)
							{
								y = 8;
							}
							else
							{
								s[y] = cmdline[x];
								if('a' <= s[y] && s[y] <= 'z')
								{
									/* 将小写字母转换成大写字母 */
									s[y] -= 0x20;
								}
								y++;
							}
							/* 寻找文件 */
							for(x = 0; x < 224; )
							{
								if(finfo[x].name[0] == 0x00)
								{
									break;
								}
								if((finfo[x].type & 0x18) == 0)
								{
									for(y = 0; y < 11; y++)
									{
										if(finfo[x].name[y] != s[y])
										{
											goto type_next_file;
										}
									}
									break;
								}
type_next_file:
								x++;

							}

							if(x < 224 && finfo[x].name[0] != 0x00)
							{
								/* 找到文件的情况 */
								y = finfo[x].size;
								
							}
						}
					}
					else if(cmdline[0] != 0)
					{
						putfonts8_asc_sht(sheet, cursor_init_pos, cursor_y, COL8_FFFFFF, COL8_000000, "Bad command", 12);
						cursor_y = cons_newline(cursor_y, sheet);
						cursor_y = cons_newline(cursor_y, sheet);
					}
					
					putfonts8_asc_sht(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "YaoOs>", 6);
					cursor_x  = cursor_init_pos;
				}
				else
				{
					if(cursor_x < 240)
					{
						/* 显示一个字符之后将光标后移一位 */
						s[0] = i - 256;
						s[1] = 0;
						cmdline[cursor_x / 8 - cursor_init_pos / 8] = i - 256;
						putfonts8_asc_sht(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
						cursor_x += 8;
					}
				}
			}
			/* 重新显示光标 */	
			if(cursor_c >= 0)
			{
				boxfill8(sheet->buf, sheet->bxsize, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
			}			
			sheet_refresh(sheet, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);
		}
	}
}

int  cons_newline(int cursor_y, SHEET *sheet)
{
	int x, y;
	if(cursor_y < 28 + 112)
	{
		cursor_y += 16;
	}
	else
	{
		/* 滚动 */
		for(y = 28; y < 28 + 112; y++)
		{
			for(x = 8; x < 8 + 240; x++)
			{
				sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
			}
		}
		for(y = 28 + 112; y < 28 + 128; y++)
		{
			for(x = 8; x < 8 + 240; x++)
			{
				sheet->buf[x + y * sheet->bxsize] = COL8_000000;
			}
		}
		sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	}

	return cursor_y;
}
