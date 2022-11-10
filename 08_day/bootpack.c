#include <stdio.h>
#include "bootpack.h"

extern keyfifo;
extern mousefifo;

typedef struct _MOUSE_DEC_
{
	unsigned char buf[3], phase;
	int           x, y, btn;
}MOUSE_DEC;



void enable_mouse(MOUSE_DEC *mdec);
int  mouse_decode(MOUSE_DEC *mdec, unsigned char dat); 
void init_keyboard();


void HariMain(void)
{
	//char *vram;/* 声明变量vram、用于BYTE [...]地址 */
	//int xsize, ysize;
	struct BOOTINFO   *binfo = (struct BOOTINFO *)ADR_BOOTINFO;
	unsigned char      var[64];
	char               mcursor[16][16];
	unsigned char      keybuf[32], mousebuf[128];
	unsigned char      mouse_dbuf[3], mouse_phase;
	int                mx;
	int                my;
	int                i;
	MOUSE_DEC          mdec;

	init_gdtidt();
	init_pic();
	io_sti();

	fifo8_init(&keyfifo,   32,  keybuf);
	fifo8_init(&mousefifo, 128, mousebuf);
	io_out8(PIC0_IMR, 0xf9); /* 开放PIC1和键盘中断 11111001*/
	io_out8(PIC1_IMR, 0xef); /* 开放鼠标中断 */

	init_keyboard();

	init_palette();/* 设定调色板 */
    init_screen(binfo->vram, binfo->scrnx, binfo->scrny);
	//putfont8_ascii(binfo->vram, binfo->scrnx, 8,  8,  COL8_FFFFFF, "Hello, world!");
	//putfont8_ascii(binfo->vram, binfo->scrnx, 31, 31, COL8_FFFFFF, "YaoOS.");

	mx = (binfo->scrnx - 16) / 2;
	my = (binfo->scrny - 28 - 16) / 2;
	
	// 绘制鼠标
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16);

	// 写入信息
	sprintf(var, "(%d, %d)", mx, my);
	// 显示要打印的信息
	putfont8_ascii(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, var);

	enable_mouse(&mdec);
	mouse_phase = 0;

	for (;;) 
	{
		io_cli(); // 屏蔽中断
		if(fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) // 没有要处理的中断数据
		{
			io_stihlt();
		}
		else
		{
			if(fifo8_status(&keyfifo) != 0)
			{
				i = fifo8_get(&keyfifo); // 此处需要取地址，不然一直打印ffffff
				io_sti();   // 允许中断发生
				sprintf(var, "%02x", i);
				boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 16, 15, 31);
				putfont8_ascii(binfo->vram, binfo->scrnx, 0, 16, COL8_FFFFFF, var);
			}
			else if(fifo8_status(&mousefifo) != 0)
			{
				i = fifo8_get(&mousefifo); // 此处需要取地址，不然一直打印ffffff
				io_sti();   // 允许中断发生
				/* 如果不等于0，说明三个字节的数据已经解码完成了 */
				if(mouse_decode(&mdec, i) != 0)
				{
					sprintf(var, "[lcr %4d %4d]", mdec.x, mdec.y);
					if((mdec.btn & 0x01) != 0)
					{
						var[1] = 'L';
					}
					if((mdec.btn & 0x02) != 0)
					{
						var[3] = 'R';
					}
					if((mdec.btn & 0x04) != 0)
					{
						var[2] = 'C';
					}

					/* 擦除之前的打印信息 [lcr %4d %4d]有15个字节*/
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
					/* 写上新的打印信息 */
					putfont8_ascii(binfo->vram, binfo->scrnx, 32, 16, COL8_FFFFFF, var);

					/* 鼠标指针的移动 */
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, mx, my, mx + 15, my + 15);
					mx += mdec.x;
					my += mdec.y;
					if(mx < 0)
					{
						mx = 0;
					}
					if(my < 0)
					{
						my = 0;
					}
					if(mx > binfo->scrnx - 16)
					{
						mx = binfo->scrnx - 16;
					}
					if(my > binfo->scrny - 16)
					{
						my =binfo->scrny - 16;
					}
					sprintf(var, "(%3d, %3d)", mx, my);
					boxfill8(binfo->vram, binfo->scrnx, COL8_008484, 0, 0, 79, 15); /* 隐藏坐标（编程背景色） */
					putfont8_ascii(binfo->vram, binfo->scrnx, 0, 0, COL8_FFFFFF, var); /* 显示坐标 */
					putblock8_8(binfo->vram, binfo->scrnx, 16, 16, mx, my, mcursor, 16); /* 显示鼠标 */
				}
			}
		}
	}
}

#define PORT_KEYDAT          0x0060
#define PORT_KEYSTA          0x0064
#define PORT_KEYCMD          0x0064
#define KEYSTA_SEND_NOTREADY 0x02
#define KEYCMD_WRITE_MODE    0x60
#define KBC_MODE             0x47
void wait_KBC_sendready()
{
	for(;;)
	{
		/* 让键盘控制电路（keyboard controller, KBC）做好准备动作，等待控制指令的到来 */
		/* 如果此处不加括号的话，会导致黑屏，无法进入主界面，其中==的优先级高于&*/
		if((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0)
		{
			/* 如果键盘控制电路可以接受CPU指令了，则CPU从设备PORT_KEYSTA处所读取数据的倒数第二位应该是0！*/
			break;
		}
	}

	return ;
}

void init_keyboard()
{
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE); //确认可否向键盘控制电路传送信息
	wait_KBC_sendready(); // 如果该函数执行完成，则表明键盘控制电路可以接受CPU指令了
	io_out8(PORT_KEYDAT, KBC_MODE); // 设定为鼠标工作模式

	return ;
}

#define KEYCMD_SENDTO_MOUSE  0xd4
#define MOUSECMD_ENABLE      0xf4

void enable_mouse(MOUSE_DEC *mdec)
{
	/* 激活鼠标 */
	wait_KBC_sendready(); // 执行完之后，键盘控制电路已做好准备
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE); // 写入KEYCMD_SENDTO_MOUSE这个指令之后，下一次再发送数据，就是发送给鼠标了
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	/* 顺利的话，ACK(0xfa)会被送过来 */
	mdec->phase = 0; /* 等待解析0xfa的阶段*/
	return;
}

int mouse_decode(MOUSE_DEC *mdec, unsigned char dat)
{
	if(mdec->phase == 0)
	{
		/*接受到0xfa的背景是之前我们调用了enable_mouse函数，调用完之后，鼠标会首先发送过来0xfa确认信号，因此才会有这样的确认方式 */
		/* 等待鼠标的0xfa的状态 */
		if(dat == 0xfa)
		{
			mdec->phase = 1;
		}
		return 0;
	}
	/* 解析鼠标的移动 */
	if(mdec->phase == 1)
	{
		/* c控制0~3，8如果只是移动鼠标，不会变化*/
		if((dat & 0xc8) == 0x08)
		{
			mdec->buf[0] = dat;
			mdec->phase  = 2;
		}

		return 0;
	}
	/* 鼠标的第二个字节 */
	if(mdec->phase == 2)
	{
		mdec->buf[1] = dat;
		mdec->phase  = 3;
		return 0;
	}
	if(mdec->phase == 3)
	{
		mdec->buf[2] = dat;
		mdec->phase  = 1;
		/* 取buf[0]的低3位*/
		mdec->btn    = mdec->buf[0] & 0x07; 
		mdec->x      = mdec->buf[1];
		mdec->y      = mdec->buf[2];
		if((mdec->buf[0] & 0x10) != 0)
		{
			/* 0xffffff00 : 111111111111111111111111 0000 0000*/
			mdec->x |= 0xffffff00;
		}
		if((mdec->buf[0] & 0x20) != 0)
		{
			mdec->y |= 0xffffff00;
		}
		mdec->y = -mdec->y;
		
		return 1;
	}
	return -1;
}
