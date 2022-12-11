/* asmhead.nas */
struct BOOTINFO { /* 0x0ff0-0x0fff */
	char cyls; /* 启动区读磁盘读到此为止 */
	char leds; /* 启动时键盘的LED的状态 */
	char vmode; /* 显卡模式为多少位彩色 */
	char reserve;
	short scrnx, scrny; /* 画面分辨率 */
	char *vram;
};
#define ADR_BOOTINFO	0x00000ff0

/* naskfunc.nas */
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
int io_in8(int port);
void io_out8(int port, int data);
int io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);
void load_tr(int tr);
void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
unsigned int memtest_sub(unsigned int start, unsigned int end);
void farjmp(int eip, int cs);

/* fifo.c */

/* fifo.c */
typedef struct _FIFO32_ {
	int           *buf;
	int            p, q, size, free, flags;
	struct _TASK_ *task;
}FIFO32;

void fifo32_init(FIFO32 *fifo, int size, int *buf, struct _TASK_ *task);
int  fifo32_put(FIFO32 *fifo, int data);
int  fifo32_get(FIFO32 *fifo);
int  fifo32_status(FIFO32 *fifo);

/* graphic.c */
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s);
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buf, int bxsize);

#define COL8_000000		0    /*  0:黑 */
#define COL8_FF0000		1    /*  1:梁红 */
#define COL8_00FF00		2    /*  2:亮绿 */
#define COL8_FFFF00		3    /*  3:亮黄 */
#define COL8_0000FF		4    /*  4:亮蓝 */
#define COL8_FF00FF		5    /*  5:亮紫 */
#define COL8_00FFFF		6    /*  6:浅亮蓝 */
#define COL8_FFFFFF		7    /*  7:白 */
#define COL8_C6C6C6		8    /*  8:亮灰 */
#define COL8_840000		9    /*  9:暗红 */
#define COL8_008400		10   /* 10:暗绿 */  
#define COL8_848400		11   /* 11:暗黄 */
#define COL8_000084		12   /* 12:暗青 */
#define COL8_840084		13   /* 13:暗紫 */
#define COL8_008484		14   /* 14:浅暗蓝 */
#define COL8_848484		15   /* 15:暗灰 */

/* dsctbl.c */
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};
struct GATE_DESCRIPTOR {
	short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
};
void init_gdtidt(void);
void set_segmdesc(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gatedesc(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);
#define ADR_IDT			0x0026f800
#define LIMIT_IDT		0x000007ff
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a
#define AR_TSS32		0x0089
#define AR_INTGATE32	0x008e

/* int.c */
void init_pic(void);
void inthandler27(int *esp);
#define PIC0_ICW1		0x0020
#define PIC0_OCW2		0x0020
#define PIC0_IMR		0x0021
#define PIC0_ICW2		0x0021
#define PIC0_ICW3		0x0021
#define PIC0_ICW4		0x0021
#define PIC1_ICW1		0x00a0
#define PIC1_OCW2		0x00a0
#define PIC1_IMR		0x00a1
#define PIC1_ICW2		0x00a1
#define PIC1_ICW3		0x00a1
#define PIC1_ICW4		0x00a1

/* keyboard.c */
void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(FIFO32 *fifo, int data0);

#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064

/* mouse.c */
struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};
void inthandler2c(int *esp);
void enable_mouse(FIFO32 *fifo, int data0, struct MOUSE_DEC *mdec);
int  mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);


/* memory.c */
#define MEMMAN_FREES 4090 /* 大约是32KB(4090 * 8个字节)*/
#define MEMMAN_ADDR	 0x003c0000

typedef struct _FREEINFO_ 
{ 	
	/* 可用信息 */
	unsigned int addr, size;
}FREEINFO;

typedef struct _MEMMAN_ 
{   
	/* 内存管理 */
	int frees, maxfrees, lostsize, losts;
	FREEINFO free[MEMMAN_FREES];
}MEMMAN;

unsigned int memtest(unsigned int start, unsigned int end);
void         memman_init(MEMMAN *man);
unsigned int memman_total(MEMMAN *man);
unsigned int memman_alloc(MEMMAN *man, unsigned int size);
int          memman_free(MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memman_alloc_4k(MEMMAN *man, unsigned int size);
int          memman_free_4k(MEMMAN *man, unsigned int addr, unsigned int size);


/* 图层信息 */
#define MAX_SHEETS         256
#define SHEET_USE          1

typedef struct _SHEET_
{
    unsigned char *buf;
    int            bxsize,   // x方向大小
                   bysize,   // y方向大小
                   vx0,      // 左上x
                   vy0,      // 左上y
                   col_inv,  // 透明色色号
                   height,   // 图层高度(代表三维上的高度，比如有三个图层，如果是最上面的图层，那高度就是3（2？）)
                   flags;    // 图层的设定信息
	void          *ctl;
}SHEET;

typedef struct _SHTCTL_
{
    unsigned char *vram, *map;  // 整个屏幕的显示首地址,map用于区分不同图层信息
    int            xsize, // 屏幕分辨率x
                   ysize, // 屏幕分辨率y
                   top;   // 最上面图层的高度（代表最上面图层的高度，也表达了图层的个数）
    SHEET         *sheets[MAX_SHEETS];
    SHEET          sheets0[MAX_SHEETS];
}SHTCTL;

SHTCTL* shtctl_init(MEMMAN *memman, unsigned char *vram, int xsize, int ysize);
SHEET*  sheet_alloc(SHTCTL *ctl);
void    sheet_setbuf(SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv);
void    sheet_refresh(SHEET *sht, int bx0, int by0, int bx1, int by1);
void    sheet_slide(SHEET *sht, int vx0, int vy0);
void    sheet_free(SHEET *sht);
void    sheet_updown(SHEET *sht, int height);
void    sheet_refreshsub(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1);
void    sheet_refreshmap(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0);


/* timer.c */
#define MAX_TIMER 500
typedef struct _TIMER_
{
	/* data */
	struct _TIMER_ *next;
	unsigned int    timeout, flags; /* flag 用于记录各个定时器的状态 */
	FIFO32         *fifo;
	unsigned char  *data;
}TIMER;

typedef struct _TIMERCTL_{
	unsigned int  count, next; /* next: 下一个超时的时间；*/
	TIMER         timers0[MAX_TIMER];
	TIMER        *t0;
}TIMERCTL;

void   init_pit(void);
void   settimer(unsigned int timeout, FIFO32 *fifo, unsigned char data);
TIMER *timer_alloc(void);
void   timer_free(TIMER *timer);
void   timer_init(TIMER *timer, FIFO32 *fifo, int data);
void   timer_settime(TIMER *timer, unsigned int timeout);
void   inthandler20(int *esp);

/* multitask */
#define MAX_TASKS     1000
#define TASK_GDT0     3

typedef struct _TSS32_
{
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3; /* 任务切换状态 */
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi; /* 32位寄存器 */
	int es, cs, ss, ds, fs, gs; /* 16位寄存器 */
	int ldtr, iomap; /* 任务状态记录 */
}TSS32; /* 104字节 */

extern TIMER *task_timer;
extern int    mt_tr;

typedef struct _TASK_
{
	int     sel, flags;   /* sel用来存放GDT的编号 */
	TSS32   tss;
}TASK;

typedef struct _TASKCTL_
{
	int   running; /* 正在运行的任务数量 */
	int   now;     /* 当前正在运行的是哪个任务 */
	TASK *tasks[MAX_TASKS];   /* 存放正在运行的任务 */
	TASK  tasks0[MAX_TASKS];
}TASKCTL;



TASK *task_init(MEMMAN *memman);
TASK *task_alloc();
void  task_run(TASK *task);
void  task_switch();
void  task_sleep(TASK *task);