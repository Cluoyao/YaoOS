/* bootpack*/

#include "bootpack.h"
#include <stdio.h>


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
	CONSOLE                   *cons;
	

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
	task_run(task_a, 1, 2); /* task_a设置为level1 */
	*((int *) 0x0fe4) = (int)shtctl;

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
				if(i == 256 + 0x3b && key_shift != 0 && task_cons->tss.ss0 != 0)
				{
					/* shift + f1 */
					cons = (CONSOLE *)*((int *) 0x0fec);
					cons_putstr0(cons, "\nBreak(key):\n");
					io_cli(); /* 不能在改变寄存器值时切换到其他任务 */
					task_cons->tss.eax = (int) &(task_cons->tss.esp0);
					task_cons->tss.eip = (int) asm_end_app;
					io_sti();
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



