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
	else if(strcmp(cmdline, "hlt") == 0)
	{
		cmd_hlt(cons, fat);
	}
	else if(cmdline[0] != 0)
	{
		putfonts8_asc_sht(cons->sht, CURSOR_INIT_POS, cons->cur_y, COL8_FFFFFF, COL8_000000, "Bad command", 12);
		cons_newline(cons);
		cons_newline(cons);
	}
	return ;
}

void cmd_mem(CONSOLE *cons, unsigned int memtotal)
{
	MEMMAN                    *memman          = (MEMMAN *)MEMMAN_ADDR;
	char                       s[30];
	/* */
	sprintf(s, "total memory %dMB", memtotal / (1024 * 1024));
	/* 在背景图层上显示信息 */
	putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
	cons_newline(cons);
	sprintf(s, "free : %dKB", memman_total(memman) / 1024);
	/* 在背景图层上显示信息 */
	putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
	cons_newline(cons);
	cons_newline(cons);
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
				sprintf(s, "filename.ext %7d", finfo[i].size);
				for(j = 0; j < 8; j++)
				{
					s[j] = finfo[i].name[j];
				}
				s[9]  = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];
				/* 在背景图层上显示信息 */
				putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 30);
				cons_newline(cons);
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
		for(i = 0; i < finfo->size; i++)
		{
			cons_putchar(cons, p[i], 1);
		}
		memman_free_4k(memman, (int)p, finfo->size);
	}
	else
	{
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "file not found.", 15);
		cons_newline(cons);
	}
	return;
}

void cmd_hlt(CONSOLE *cons, int *fat)
{
	MEMMAN                    *memman = (MEMMAN *)MEMMAN_ADDR;
	FILEINFO                  *finfo  = file_search("HLT.HRB", (FILEINFO *)(ADR_DISKIMG + 0x002600), 224);
	struct SEGMENT_DESCRIPTOR *gdt    = (struct SEGMENT_DESCRIPTOR *)ADR_GDT;
	char                      *p;

	if(finfo != 0)
	{
		/* 分配读入的存储空间 */
		p = (char *)memman_alloc_4k(memman, finfo->size);
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *)(ADR_DISKIMG + 0x003e00));
		set_segmdesc(gdt + 1003, finfo->size - 1, (int)p, AR_CODE32_ER);
		//farjmp(0, 1003 * 8);
		farcall(0, 1003 * 8);
		memman_free_4k(memman, (int)p, finfo->size);
	}
	else
	{
		putfonts8_asc_sht(cons->sht, 8, cons->cur_y, COL8_FFFFFF, COL8_000000, "file not found.", 15);
		cons_newline(cons);
	}
	cons_newline(cons);
	return;
}