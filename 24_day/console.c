#include "bootpack.h"
#include <stdio.h>
#include <string.h>

#define CURSOR_INIT_POS  64


void cons_putchar(CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;

	if(s[0] == 0x09) /* 制表符 */
	{
		for(;;)
		{
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, ' ', 1);
			cons->cur_x += 8;
			if(cons->cur_x == 8 + 240)
			{
				cons->cur_x = 8;
				cons_newline(cons);
			}
			if(((cons->cur_x - 8) & 0x1f) == 0)
			{
				break;
			}
		}
	}
	else if(s[0] == 0x0a) /* 换行 */
	{
		cons_newline(cons);
	}
	else if(s[0] == 0x0d)
	{
		/* 暂时不做操作 */
	}
	else
	{
		putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		if(move != 0)
		{
			/* move为0时光标不后移 */
			cons->cur_x += 8;
			if(cons->cur_x == 8 + 240)
			{
				cons_newline(cons);
			}
		}
	}
	return;
}

void display_prompt(CONSOLE *cons, char *str, char move)
{
	int len = strlen(str);
	int i;
	for(i = 0; i < len; i++)
	{
		cons_putchar(cons, str[i], move);
	}
	return;
}

void console_task(SHEET *sheet, unsigned int memtotal)
{
	TIMER                     *timer;
	TASK                      *task            = task_now();
	MEMMAN                    *memman          = (MEMMAN *)MEMMAN_ADDR;
	int                        cursor_init_pos = CURSOR_INIT_POS;
	int                        i, fifobuf[128];
	char                       s[30];
	int                        x,y;
	char                       cmdline[30], *p;
	CONSOLE                    cons;
	FILEINFO                  *finfo  = (FILEINFO *)(ADR_DISKIMG + 0x002600);
	int                       *fat    = (int *)memman_alloc_4k(memman, 4 * 2880);
	char                      *prompt = "YaoOS> ";
    //struct SEGMENT_DESCRIPTOR *gdt    = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;

	cons.sht                          = sheet;
	cons.cur_x                        = 8;
	cons.cur_y                        = 28;
	cons.cur_c                        = -1;
	*((int *)0x0fec)                  = (int)&cons;

	fifo32_init(&task->fifo, 128, fifobuf, task);
	timer = timer_alloc();
	timer_init(timer, &task->fifo, 1);
	timer_settime(timer, 50);
	file_readfat(fat, (unsigned char*)(ADR_DISKIMG + 0x000200));

	/* 显示提示符 */
	//putfonts8_asc_sht(sheet, 8, 29, COL8_FFFFFF, COL8_000000, "YaoOs>", 6);
    display_prompt(&cons, prompt, 1);

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
					if(cons.cur_c >= 0) /* 光标其实还是在闪，只是没有颜色 */
					{
						cons.cur_c = COL8_FFFFFF;
					}

				}
				else
				{
					timer_init(timer, &task->fifo, 1);
					if(cons.cur_c >= 0) /* 光标其实还是在闪，只是没有颜色 */
					{
						cons.cur_c = COL8_000000;
					}

				}
				timer_settime(timer, 50);
			}
			if(i == 2)
			{
				cons.cur_c = COL8_FFFFFF; 
			}
			if(i == 3) /* 光标off */
			{
				/* 因为背景是黑色，所以用黑色擦除 */
				boxfill8(sheet->buf, sheet->bxsize, COL8_000000, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15); /* 擦除 */
				cons.cur_c = -1;
			}
			if(i >= 256 && i <= 511)
		    {
				if(i == 8 + 256)
				{
					/* 控制退格键移动*/
					if(cons.cur_x > cursor_init_pos)
					{
						/* 用空白擦除光标后将光标前移一位 */
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				}
				else if(i == 10 + 256)
				{
					/* 回车键 */
					/* 用空格将光标擦除 */
					cons_putchar(&cons, ' ', 0);
					cmdline[cons.cur_x / 8 - cursor_init_pos / 8] = 0;
					cons_newline(&cons);
					/* 执行命令 */
					cons_runcmd(cmdline, &cons, fat, memtotal);
					/* 执行完之后显示提示符 */
					display_prompt(&cons, prompt, 1);
				}
				else
				{
					if(cons.cur_x < 240)
					{
						/* 显示一个字符之后将光标后移一位 */
						cmdline[cons.cur_x / 8 - cursor_init_pos / 8] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}
			/* 重新显示光标 */	
			if(cons.cur_x >= 0)
			{
				boxfill8(sheet->buf, sheet->bxsize, cons.cur_c, cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
			}			
			sheet_refresh(sheet, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
		}
	}
}


void  cons_newline(CONSOLE *cons)
{
	int    x, y;
	SHEET *sheet = cons->sht;
	if(cons->cur_y < 28 + 112)
	{
		cons->cur_y += 16;
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
	cons->cur_x = 8;
	return;
}

void cons_runcmd(char *cmdline, CONSOLE *cons, int *fat, unsigned int memtotal)
{
	if(strcmp(cmdline, "mem") == 0)
	{
		cmd_mem(cons, memtotal);
	}
	else if(strcmp(cmdline, "clear") == 0)
	{
		cmd_clear(cons);
	}
	else if(strcmp(cmdline, "luoyao") == 0)
	{
		cmd_ly(cons);
	}
	else if(strcmp(cmdline, "ls") == 0)
	{	
		cmd_ls(cons);
	}
	else if(strncmp(cmdline, "type ", 5) == 0)
	{
		cmd_type(cons, fat, cmdline);
	}
	else if(cmdline[0] != 0)
	{
		if(cmd_app(cons, fat, cmdline) == 0)
		{
			/* 不是命令，不是应用程序，也不是空行 */
			cons_putstr0(cons, "Bad command.\n\n");
			//putfonts8_asc_sht(cons->sht, CURSOR_INIT_POS, cons->cur_y, COL8_FFFFFF, COL8_000000, "Bad command", 12);
			cons_newline(cons);
			cons_newline(cons);
		}

	}
	return ;
}

void cmd_mem(CONSOLE *cons, unsigned int memtotal)
{
	MEMMAN                    *memman          = (MEMMAN *)MEMMAN_ADDR;
	char                       s[30];
	/* */
	sprintf(s, "total memory %dMB\n", memtotal / (1024 * 1024));
	/* 在背景图层上显示信息 */
	cons_putstr0(cons, s);
	sprintf(s, "free : %dKB\n", memman_total(memman) / 1024);
	/* 在背景图层上显示信息 */
	cons_putstr0(cons, s);
	return;
}

void cmd_clear(CONSOLE *cons)
{
	int x, y;
	SHEET *sheet = cons->sht;
	/* 清除界面的命令 */
	for(y = 28; y < 28 + 128; y++)
	{
		for(x = 8; x < 8 + 240; x++)
		{
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(cons->sht, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return ;
}

void cmd_ly(CONSOLE *cons)
{
	char s[30];
	sprintf(s, "%s", "LuoY is a handsome man.");
	/* 在背景图层上显示信息 */
	putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 24);
	cons_newline(cons);
	return;
}

void cmd_ls(CONSOLE *cons)
{
	FILEINFO    *finfo  = (FILEINFO *)(ADR_DISKIMG + 0x002600);
	int          i, j;
	char         s[30];

	/* ls命令 */
	for(i = 0; i < 224; i++)
	{
		if(finfo[i].name[0] == 0x00)
		{
			break; /* 不包含任何文件信息 */
		}
		if(finfo[i].name[0] != 0xe5)
		{
			if((finfo[i].type & 0x18) == 0)
			{
				sprintf(s, "filename.ext %7d\n", finfo[i].size);
				for(j = 0; j < 8; j++)
				{
					s[j] = finfo[i].name[j];
				}
				s[9]  = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];
				/* 在背景图层上显示信息 */
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}

void cmd_type(CONSOLE *cons, int *fat, char *cmdline)
{
	MEMMAN   *memman          = (MEMMAN *)MEMMAN_ADDR;
	FILEINFO *finfo           = file_search(cmdline + 5, (FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	char     *p;
	int       i;
	if(finfo != 0)
	{
		p = (char *)memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
		cons_putstr1(cons, p, finfo->size);
		memman_free_4k(memman, (int)p, finfo->size);
	}
	else
	{
		cons_putstr0(cons, "File not found.\n");
	}
	cons_newline(cons);
	return;
}

int cmd_app(CONSOLE *cons, int *fat, char *cmdline)
{
	MEMMAN                    *memman = (MEMMAN *)MEMMAN_ADDR;
	struct SEGMENT_DESCRIPTOR *gdt    = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
	FILEINFO                  *finfo;
	char                       name[18], *p, *q;
	TASK                      *task = task_now();
	int                        i;
	int                        segsiz, datsiz, esp, dathrb;
	SHTCTL                    *shtctl;
	SHEET					  *sht;

	/* 根据命令行生成文件名 */
	for(i = 0; i < 13; i++)
	{
		if(cmdline[i] <= ' ') /* 他想表达的是遇到空格就结束，而小于空格的ascii码就是些换行,tab,回车等 */
		{
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0;

	/* 寻找文件 */
	finfo  = file_search(name, (FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	if(finfo == 0 && name[i - 1] != '.')
	{
		/* 当找不到文件的时候，在文件后面加上".hrb"继续寻找, 这里的hrb类似于windows的.exe*/
		name[i]     = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo       = file_search(name, (FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	}

	if(finfo != 0)
	{
		/* 分配读入的存储空间 */
		p = (char *)memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));

		if(finfo->size >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00)
		{
			segsiz = *((int *)(p + 0x0000));
			esp    = *((int *)(p + 0x000c));
			datsiz = *((int *)(p + 0x0010));
			dathrb = *((int *)(p + 0x0014));
			q 	   = (char *)memman_alloc_4k(memman, segsiz);
			*((int *) 0xfe8) = (int)q;
			set_segmdesc(gdt + 1003, finfo->size - 1, (int)p, AR_CODE32_ER + 0x60); /* 只是注册了分段的内存 */
			set_segmdesc(gdt + 1004, segsiz      - 1, (int)q, AR_DATA32_RW + 0x60); /* 只是注册了分段的内存 */
			for(i = 0; i < datsiz; i++)
			{
				q[esp + i] = p[dathrb + i];
			}
			start_app(0x1b, 1003 * 8, esp, 1004 * 8, &(task->tss.esp0));
			
			shtctl = (SHTCTL *)*((int *)0xfe4);
			for(i = 0; i < MAX_SHEETS; i++)
			{
				sht = &(shtctl->sheets0[i]);
				if(sht->flags != 0 && sht->task == task)
				{
					/* 找到被应用程序遗留的窗口 */
					sheet_free(sht);
				}
			}

			memman_free_4k(memman, (int)q, segsiz);
		}
		else
		{
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (int)p, finfo->size);
		cons_newline(cons);
		return 1; /* 是应用程序 */
	}

	return 0;

}

void cons_putstr0(CONSOLE *cons, char *s)
{
	for(; *s != 0; s++)
	{
		cons_putchar(cons, *s, 1);
	}
	return;
}

void hrb_api_linewin(SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;
	dx = x1 - x0;
	dy = y1 - y0;
	x  = x0 << 10;
	y  = y0 << 10;
	if(dx < 0)
	{
		dx = -dx; /* 取绝对值 */
	}
	if(dx >= dy)
	{
		len = dx + 1;
		if(x0 > x1)
		{
			dx = -1024; /* 一个1024代表1*/
		}
		else
		{
			dx = 1024;
		}

		if(y0 <= y1)
		{
			dy = ((y1 - y0 + 1) << 10) / len;
		}
		else
		{
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	}
	else
	{
		len = dy + 1;
		if(y0 > y1)
		{
			dy = -1024;
		}
		else
		{
			dy = 1024;
		}
		if(x0 <= x1)
		{
			dx = ((x1 - x0 + 1) << 10) / len;
		}
		else
		{
			dy = ((x1 - x0 - 1) << 10) / len;
		}
	}
	for(i = 0; i < len; i++)
	{
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}
	return;
}

void cons_putstr1(CONSOLE *cons, char *s, int l)
{
	int i;
	for(i = 0; i < l; i++)
	{
		cons_putchar(cons, s[i], 1);
	}
	return;
}

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	int      ds_base = *((int *) 0xfe8); 
	TASK    *task    = task_now();
	CONSOLE *cons    = (CONSOLE *)*((int *)0x0fec);
	SHTCTL  *shtctl  = (SHTCTL *)*((int *)0x0fe4);
	SHEET   *sht;
	int     *reg     = &eax + 1;
	int      i;

	/* 强行改写通过PUSHAD保存的值 */
	/* reg[0]:edi, reg[1]:esi, reg[2]:ebp, reg[3]:esp */
	/* reg[4]:ebx, reg[5]:edx, reg[6]:ecx, reg[7]:eax */

	if(edx == 1)
	{
		cons_putchar(cons, eax & 0xff, 1);
	}
	else if(edx == 2)
	{
		cons_putstr0(cons, (char *)ebx + ds_base);
	}
	else if(edx == 3)
	{
		cons_putstr1(cons, (char *)ebx + ds_base, ecx);
	}
	else if(edx == 4)
	{
		return &(task->tss.esp0);
	}
	else if(edx == 5)
	{
		sht = sheet_alloc(shtctl);
		sht->task = task;
		sheet_setbuf(sht, (char *)ebx + ds_base, esi, edi, eax);
		make_window8((char *)ebx + ds_base, esi, edi, (char *)ecx + ds_base, 0);
		sheet_slide(sht, 100, 50);
		sheet_updown(sht, 3);
		reg[7] = (int)sht;
	}
	else if(edx == 6)
	{
		sht = (SHEET *)(ebx & 0xfffffffe); /* 获取真实地址 */
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *)ebp + ds_base);
		if((ebx & 1) == 0)
		{
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	}
	else if(edx == 7)
	{
		sht = (SHEET *)(ebx & 0xfffffffe); /* 获取真实地址 */
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if((ebx & 1) == 0)
		{
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if(edx == 8)
	{
		memman_init((MEMMAN *)(ebx + ds_base));
		ecx &= 0xfffffff0; /* 以16字节为单位 */
		memman_free((MEMMAN *)(ebx + ds_base), eax, ecx);
	}
	else if(edx == 9)
	{
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 以16字节为单位进行取整,内存分配大小取整 */  
		reg[7] = memman_alloc((MEMMAN *) (ebx + ds_base), ecx);
	}
	else if(edx == 10)
	{
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 以16字节为单位进行取整,内存释放大小取整 */
		memman_free((MEMMAN *)(ebx + ds_base), eax, ecx);
	}
	else if(edx == 11)
	{
		sht = (SHEET *)(ebx & 0xfffffffe); /* 获取真实地址 */
		sht->buf[sht->bxsize * edi + esi] = eax; /* 给地址写值，eax在a_nask中代表颜色 */
		if((ebx & 1) == 0) /* 地址为偶数才更新*/
		{
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	}
	else if(edx == 12)
	{
		sht = (SHEET *)ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	}
	else if(edx == 13)
	{
		sht = (SHEET *)(ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if((ebx & 1) == 0)
		{
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	}
	else if(edx == 14)
	{
		sheet_free((SHEET *)ebx);
	}
	else if(edx == 15)
	{
		for(;;)
		{
			io_cli();
			if(fifo32_status(&task->fifo) == 0)
			{
				if(eax != 0)
				{
					task_sleep(task);
				}
				else
				{
					io_sti();
					reg[7] = -1;
					return 0;
				}
			}
			i = fifo32_get(&task->fifo);
			io_sti();
			if(i <= 1)
			{
				/* 光标用定时器 */
				/* 应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1 */
				timer_init(cons->timer, &task->fifo, 1);
				timer_settime(cons->timer, 50);
			}
			if(i == 2)
			{
				cons->cur_c = COL8_FFFFFF;
			}
			if(i == 3)
			{
				cons->cur_c = -1;
			}
			if(256 <= i && i <= 511)
			{
				reg[7] = i - 256;
				return 0;
			}
			
		}
	}
	return 0;
}

int *inthandler0d(int *esp)
{
	CONSOLE *cons = (CONSOLE *)*((int *)0x0fec);
	TASK    *task = task_now();
	char     s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);
}

int *inthandler0c(int *esp)
{
	CONSOLE *cons = (CONSOLE *)*((int *)0x0fec);
	TASK    *task = task_now();
	char     s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0); // 强制结束应用程序
}