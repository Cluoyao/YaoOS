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
	SHEET                     *sht_back, *sht_mouse, *sht_win,  *sht_cons[2];
	unsigned char             *buf_back, buf_mouse[256], *buf_win, *buf_cons[2];
	TASK                      *task_a, *task_cons[2];
	int                        key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	int                        key_capslk = 0;
	CONSOLE                   *cons;
	int                        j, x, y, mmx = -1, mmy = -1;
	SHEET					  *sht = 0, *key_win;
	

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

	for(i = 0; i < 2; i++)
	{
		sht_cons[i]  = sheet_alloc(shtctl);
		buf_cons[i]  = (unsigned char *)memman_alloc_4k(memman, 256 * 165);
		sheet_setbuf(sht_cons[i], buf_cons[i], 256, 165, -1); /* 无透明颜色 */
		make_window8(buf_cons[i], 256, 165, "Console", 0);
		make_textbox8(sht_cons[i], 8, 28, 240, 128, COL8_000000);
		task_cons[i]          = task_alloc();
		task_cons[i]->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024 - 8;
		task_cons[i]->tss.eip = (int)&console_task;
		task_cons[i]->tss.es  = 1 * 8;
		task_cons[i]->tss.cs  = 2 * 8;
		task_cons[i]->tss.ss  = 1 * 8;
		task_cons[i]->tss.ds  = 1 * 8;
		task_cons[i]->tss.fs  = 1 * 8;
		task_cons[i]->tss.gs  = 1 * 8;
		*((int *) (task_cons[i]->tss.esp + 4)) = (int)sht_cons[i];
		*((int *) (task_cons[i]->tss.esp + 8)) = (int)memtotal;
		task_run(task_cons[i], 2, 2); /* level=2, priority=2 */ 
		sht_cons[i]->task   = task_cons[i];
	    sht_cons[i]->flags |= 0x20;        /* 有光标 */
	}

	

	/* sht_win */
	sht_win   = sheet_alloc(shtctl); 
	buf_win   = (unsigned char *)memman_alloc_4k(memman, 160 * 52);
	sheet_setbuf(sht_win, buf_win, 144, 52, -1);
	make_window8(buf_win, 144, 52, "Task_A", 1); /* 默认使用原来是色系 */
	make_textbox8(sht_win, 8, 28, 128, 16, COL8_FFFFFF);
	cursor_x = 8;
	cursor_c = COL8_FFFFFF;
	timer    = timer_alloc();
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
	sheet_slide(sht_cons[1], 56, 6);
	sheet_slide(sht_cons[0], 8, 2);

	/* 移动鼠标图层到指定位置 */
	sheet_slide(sht_win,   64, 56);
	sheet_slide(sht_mouse, mx, my);

	/* 设置显示背景（图层）高度为0 */
	sheet_updown(sht_back,      0);
	sheet_updown(sht_cons[1],   1);
	sheet_updown(sht_cons[0],   2);
	sheet_updown(sht_win,       3);
	sheet_updown(sht_mouse,     4);
	key_win          = sht_win;


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

			if(key_win->flags == 0)
			{
				key_win  = shtctl->sheets[shtctl->top - 1];
				cursor_c = keywin_on(key_win, sht_win, cursor_c);
			}

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
					if(key_win == sht_win)
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
						fifo32_put(&key_win->task->fifo, s[0] + 256);
					}
				}
				if(i == 256 + 0x0e) /*退格键*/
				{
					/* 如果发送给任务A */
					if(key_win == sht_win)
					{
						if(cursor_x > 8)
						{
							putfonts8_asc_sht(sht_win, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
							cursor_x -= 8;
						}
					}
					else
					{
						/*发送至命令行窗口*/
						fifo32_put(&key_win->task->fifo, 8 + 256);
					}

				}
				if(i == 256 + 0x1c) /*回车键*/
				{
					if(key_win != sht_win)
					{
						fifo32_put(&key_win->task->fifo, 10 + 256);
					}
				}
				if(i == 256 + 0x0f) /*tab键*/
				{
					/* 如果按下tab键，第一次的话，key_to == 0, 此时task_a颜色变化， */

					cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
					j        = key_win->height - 1;
					if(j == 0)
					{
						j = shtctl->top - 1;
					}
					key_win  = shtctl->sheets[j];
					cursor_c = keywin_on(key_win, sht_win, cursor_c);
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

				if(i == 256 + 0x3b && key_shift != 0 && task_cons[0]->tss.ss0 != 0)
				{
					/* shift + f1 */
					cons = (CONSOLE *)*((int *) 0x0fec);
					cons_putstr0(cons, "\nBreak(key):\n");
					io_cli(); /* 不能在改变寄存器值时切换到其他任务 */
					task_cons[0]->tss.eax = (int) &(task_cons[0]->tss.esp0);
					task_cons[0]->tss.eip = (int) asm_end_app;
					io_sti();
				}
				if(i == 256 + 0x57 && shtctl->top > 2)
				{
					/* F11 */
					sheet_updown(shtctl->sheets[1], shtctl->top - 1); /* top 存放的是最上面的高度，给绘制鼠标用的 */
				}
				if (i == 256 + 0xfa) { /*键盘成功接收到数据*/
					keycmd_wait = -1;
				}
				if (i == 256 + 0xfe) { /*键盘没有成功接收到数据*/
					wait_KBC_sendready();
					io_out8(PORT_KEYDAT, keycmd_wait);
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

					sheet_slide(sht_mouse, mx, my);/* 包含sheet_refresh含sheet_refresh */

					if((mdec.btn & 0x01) != 0)
					{
						if(mmx < 0)
						{
							/* 如果处于通常的模式 */
							/* 按照从上到下的顺序寻找鼠标所指向的图层 */
							for(j = shtctl->top - 1; j > 0; j--)
							{
								sht = shtctl->sheets[j];
								x   = mx - sht->vx0;
								y   = my - sht->vy0;
								if(0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize)
								{
									/* 说明在该图层中 */
									if(sht->buf[y * sht->bxsize + x] != sht->col_inv) /* 如果不是背景颜色 */
									{
										sheet_updown(sht, shtctl->top - 1); /* 把该图层提升 */
										if(sht != key_win)
										{
											cursor_c = keywin_off(key_win, sht_win, cursor_c, cursor_x);
											key_win  = sht;
											cursor_c = keywin_on(key_win, sht_win, cursor_c);
										}
										if(3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21)
										{
											/* 鼠标按着标题栏区域 */
											mmx = mx;
											mmy = my;
										}
										if(sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19)
										{
											/* 点击X按钮 */
											if((sht->flags & 0x10) != 0) 
											{
												/* 该窗口若为应用程序窗口 */
												cons = (CONSOLE *)*((int *) 0xfec);
												cons_putstr0(cons, "\nBreak(mouse): \n");
												io_cli();
												task_cons[0]->tss.eax = (int)&(task_cons[0]->tss.esp0);
												task_cons[0]->tss.eip = (int)asm_end_app;
												io_sti();
											}
										}
										break;
									}
								}
							}
						}
						else
						{
							/* 如果处于窗口移动模式 */
							x   = mx - mmx; /* 计算鼠标的移动距离 */
							y   = my - mmy;
							sheet_slide(sht, sht->vx0 + x, sht->vy0 + y);
							mmx = mx; /* 更新为移动后的坐标 */
							mmy = my;
						}
					}
					else
					{
						mmx = -1;
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

int keywin_off(SHEET *key_win, SHEET *sht_win, int cur_c, int cur_x)
{
	change_wtitle8(key_win, 0);
	if(key_win == sht_win)
	{
		cur_c = -1;/*删除光标*/
		boxfill8(sht_win->buf, sht_win->bxsize, COL8_FFFFFF, cur_x, 28, cur_x + 7, 43);
	}
	else
	{
		if((key_win->flags & 0x20) != 0)
		{
			fifo32_put(&key_win->task->fifo, 3);
		}
	}

	return cur_c;
}
int keywin_on(SHEET *key_win, SHEET *sht_win, int cur_c)
{
	change_wtitle8(key_win, 1);
	if(key_win == sht_win)
	{
		cur_c = COL8_000000;
	}
	else
	{
		if((key_win->flags & 0x20) != 0)
		{
			fifo32_put(&key_win->task->fifo, 2);
		}
	}

	return cur_c;
}

