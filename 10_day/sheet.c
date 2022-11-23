#include "bootpack.h"

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
}SHEET;

typedef struct _SHTCTL_
{
    unsigned char *vram;  // 整个屏幕的显示首地址
    int            xsize, // 屏幕分辨率x
                   ysize, // 屏幕分辨率y
                   top;   // 最上面图层的高度（代表最上面图层的高度，也表达了图层的个数）
    SHEET         *sheets[MAX_SHEETS];
    SHEET          sheets0[MAX_SHEETS];
}SHTCTL;

/* 给多图层管理结构体分配内存信息 */
SHTCTL *shtctl_init(MEMMAN *memman, unsigned char *vram, int xsize, int ysize)
{
    SHTCTL *ctl;
    int     i;
    ctl     = (SHTCTL *)memman_alloc_4k(memman, sizeof(SHTCTL));
    if(ctl == 0)
    {
        goto err;
    }

    ctl->vram  = vram;
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->top   = -1;                /* 一个SHEET都没有 */
    for(i = 0; i<MAX_SHEETS;i++)
    {
        ctl->sheets0[i].flags == 0; /* 标记为未使用 */
    }

err:
    return ctl;
}

/* 获取新生成的未使用的图层 */
SHEET* sheet_alloc(SHTCTL *ctl)
{
    SHEET *sht;
    int    i;
    for(i=0; i<MAX_SHEETS; i++)
    {
        if(ctl->sheets0[i].flags == 0) /* 找到未使用的图层 */
        {
            sht         = &ctl->sheets0[i];
            sht->flags  = SHEET_USE;            /* 标记为正在使用 */
            sht->height = -1;                   /* 高度设置为-1，表示图层的高度还没有设置，因而不是显示对象，隐藏 */
            return sht;
        }
    }

    return 0;
}

void sheet_setbuf(SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv)
{
    sht->buf     = buf;
    sht->bxsize  = xsize;
    sht->bysize  = ysize;
    sht->col_inv = col_inv;

    return ;
}


/* 设定地板高度 */
void sheet_updown(SHTCTL *ctl, SHEET *sht, int height)
{
    int h, old = sht->height;

    /* 如果高度过高或者过低，则进行修正 */
    if(height > ctl->top + 1) 
    {
        /* 如果底板高度大于最上面的图层高度，将底板高度设置为跟最上面图层一样高 */
        height = ctl->top + 1;
    }
    if(height < -1)
    {
        height = -1;
    }

    sht->height = height; /* 设定底板图层的高度 */
    /* 下面主要进行sheets[]的重新排列 */
    if(old > height)
    {
        /* 如果比以前低*/
        if(height >= 0)
        {
            /* 把小于old大于height的那些图层往上提 */
            for(h = old; h > height; h--)
            {
                ctl->sheets[h]         = ctl->sheets[h-1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
        }
        else
        {
            
        }
    }
}