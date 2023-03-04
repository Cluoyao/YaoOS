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
	int                        fifobuf[128], keycmd_buf[32], *cons_fifo[2];
   
	TIMER                     *timer;
	int                        mx, my, i, new_mx = -1, new_my = 0, new_wx = 0x7fffffff, new_wy = 0;
	struct MOUSE_DEC           mdec; 
   
	unsigned int               memtotal, count = 0;
	MEMMAN                    *memman = (MEMMAN *) MEMMAN_ADDR;
	SHTCTL                    *shtctl;
	SHEET                     *sht_back, *sht_mouse;
	unsigned char             *buf_back, buf_mouse[256], *buf_cons[2];
	TASK                      *task_a, *task_cons[2], *task;
	int                        key_to = 0, key_shift = 0, key_leds = (binfo->leds >> 4) & 7, keycmd_wait = -1;
	int                        key_capslk = 0;
	CONSOLE                   *cons;
	int                        j, x, y, mmx = -1, mmy = -1, mmx2 = 0;
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
	fifo32_init(&keycmd, 32, keycmd_buf, 0);

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

	key_win = open_console(shtctl, memtotal);

	/* sht_mouse */
	sht_mouse = sheet_alloc(shtctl); /* 从管理单元拿出一个图层来用，作为鼠标图层 */
	sheet_setbuf(sht_mouse, buf_mouse, 16, 16, 99); /* 透明色号99 */
	init_mouse_cursor8(buf_mouse, 99); /* 背景色号99 */
	mx = (binfo->scrnx - 16) / 2; /* 计算画面中心坐标 */
	my = (binfo->scrny - 28 - 16) / 2;

	/* 从左上角(0,0)点开始绘制显示界面 */
	sheet_slide(sht_back, 0,  0);
	sheet_slide(key_win, 32, 4);
	/* 移动鼠标图层到指定位置 */
	sheet_slide(sht_mouse, mx, my);

	/* 设置显示背景（图层）高度为0 */
	sheet_updown(sht_back,      0);
	sheet_updown(key_win,       1);
	sheet_updown(sht_mouse,     2);
	keywin_on(key_win);

	*((int *)0x0fec) = (int)&fifo;

	for (;;) 
	{
		if (fifo32_status(&keycmd) > 0 && keycmd_wait < 0) 
		{
			/* 如果存在向键盘控制器发送的数据，则发送它 */
			keycmd_wait = fifo32_get(&keycmd);
			wait_KBC_sendready();
			io_out8(PORT_KEYDAT, keycmd_wait);
		}
		io_cli(); /* 关闭中断 */
		if (fifo32_status(&fifo) == 0) 
		{
			/*fifo为空，当存在搁置的绘图操作时立即执行*/
			if(new_mx >= 0)
			{
				io_sti();
				sheet_slide(sht_mouse, new_mx, new_my);
				new_mx = -1;
			}
			else if(new_wx != 0x7fffffff)
			{
				io_sti();
				sheet_slide(sht, new_wx, new_wy);
				new_wx = 0x7fffffff;
			}
			else
			{
				task_sleep(task_a); /* 注意调用顺序 */
				io_sti();
			}
		} 
		else 
		{
			i = fifo32_get(&fifo);
			io_sti();

			if(key_win != 0 && key_win->flags == 0)
			{
				if(shtctl->top == 1)
				{
					key_win = 0;
				}
				else
				{
					key_win  = shtctl->sheets[shtctl->top - 1];
					keywin_on(key_win);
				}

			}

			if (256 <= i && i <= 511) /* 键盘数据 */
			{
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

				if(s[0] != 0 && key_win != 0) /*一般字符，退格键，回车键*/
				{
					fifo32_put(&key_win->task->fifo, s[0] + 256);
				}

				if(i == 256 + 0x3c && key_shift != 0)
				{
					/*自动将输入焦点切换到新打开的命令行窗口*/
					if(key_win != 0)
					{
						keywin_off(key_win);
					}
					
					key_win = open_console(shtctl, memtotal);
					sheet_slide(key_win, 32, 4);
					sheet_updown(key_win, shtctl->top);
					keywin_on(key_win);
				}

				if(i == 256 + 0x0f && key_win != 0) /* Tab键 */
				{
					keywin_off(key_win);
					j = key_win->height - 1;
					if(j == 0)
					{
						j = shtctl->top - 1;
					}
					key_win = shtctl->sheets[j];
					keywin_on(key_win);
				}
				if(i == 256 + 0x0e) /*退格键*/
				{
					/*发送至命令行窗口*/
					fifo32_put(&key_win->task->fifo, 8 + 256);
				}
				if(i == 256 + 0x1c) /*回车键*/
				{
					fifo32_put(&key_win->task->fifo, 10 + 256);
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
				if(i == 256 + 0x3a)
				{
					key_capslk = !key_capslk;
				}

				if(i == 256 + 0x3b && key_shift != 0 && key_win != 0)
				{
					/* shift + f1 */
					task = key_win->task;
					if(task != 0 && task->tss.ss0 != 0)
					{
						cons_putstr0(task->cons, "\nBreak(key):\n");
						io_cli(); /* 不能在改变寄存器值时切换到其他任务 */
						task->tss.eax = (int) &(task->tss.esp0);
						task->tss.eip = (int) asm_end_app;
						io_sti();
					}
				}
				if(i == 256 + 0x57)
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
					new_mx = mx;
					new_my = my;
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
											keywin_off(key_win);
											key_win  = sht;
											keywin_on(key_win);
										}
										if(3 <= x && x < sht->bxsize - 3 && 3 <= y && y < 21)
										{
											/* 鼠标按着标题栏区域 */
											mmx    = mx;
											mmy    = my;
											mmx2   = sht->vx0;
											new_wy = sht->vy0;
										}
										if(sht->bxsize - 21 <= x && x < sht->bxsize - 5 && 5 <= y && y < 19)
										{
											/* 点击X按钮 */
											if((sht->flags & 0x10) != 0) 
											{
												/* 该窗口若为应用程序窗口 */
												task = sht->task;
												cons_putstr0(task->cons, "\nBreak(mouse): \n");
												io_cli();
												task->tss.eax = (int)&(task->tss.esp0);
												task->tss.eip = (int)asm_end_app;
												io_sti();
											}
											else
											{
												task = sht->task;
												io_cli();
												fifo32_put(&task->fifo, 4);
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
							new_wx = (mmx2 + x + 2) & ~3;
							new_wy = new_wy + y;
							mmy    = my;
						}
					}
					else
					{
						mmx = -1;
						if(new_wx != 0x7fffffff)
						{
							sheet_slide(sht, new_wx, new_wy);
							new_wx = 0x7fffffff;
						}
					}
				}
			}
			else if(768 <= i && i <= 1023)
			{
				close_console(shtctl->sheets0 + (i - 768));
			}
		}
	}
}

void keywin_off(SHEET *key_win)
{
	change_wtitle8(key_win, 0);

	if((key_win->flags & 0x20) != 0)
	{
		fifo32_put(&key_win->task->fifo, 3);
	}

	return ;
}
void keywin_on(SHEET *key_win)
{
	change_wtitle8(key_win, 1);

	if((key_win->flags & 0x20) != 0)
	{
		fifo32_put(&key_win->task->fifo, 2);
	}

	return ;
}

