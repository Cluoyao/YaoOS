#include "bootpack.h"


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

void sheet_refresh(SHTCTL *ctl)
{
    int            h, bx, by, vx, vy;
    unsigned char *buf, c, *vram = ctl->vram;
    SHEET         *sht;

    /* 从下往上绘制 */
    for(h = 0; h <= ctl->top; h++)
    {
        sht = ctl->sheets[h]; /* 图层结构体指针*/
        buf = sht->buf;       /* 图层存放首地址*/

        for(by = 0; by < sht->bysize; by++)
        {
            vy = sht->vy0 + by; /* 左上角加偏移量 */
            for(bx = 0; bx < sht->bxsize; bx++)
            {
                vx = sht->vx0 + bx;
                c  = buf[by * sht->bxsize + bx]; /* 说明该图层仅是用来记录图层信息，并不负责在显存显示 */
                if(c != sht->col_inv)
                {
                    /* 在显存上进行绘制 */
                    vram[vy * ctl->xsize + vx] = c;
                }
            }
        }
    }

    return ;
}

void sheet_slide(SHTCTL *ctl, SHEET *sht, int vx0, int vy0)
{
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if(sht->height >= 0)
    {
        /* 如果正在显示 */
        /* 按新图层的信息刷新画面 */
        sheet_refresh(ctl);
    }
    return;
}

void sheet_free(SHTCTL *ctl, SHEET *sht)
{
    if(sht->height >= 0)
    {
        /* 如果处于显示状态，则先设定为隐藏 */
        sheet_updown(ctl, sht, -1);
    }
    /* 标记 未使用 标志*/
    sht->flags = 0;
    return;
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
            // 塞到当前位置
            ctl->sheets[height] = sht;
        }
        else
        {
            /* 隐藏 */
            if(ctl->top > old)
            {
                /* 把上面的降下来 */
                for(h = old; h < ctl->top; h++)
                {
                    ctl->sheets[h] = ctl->sheets[h + 1];
                    ctl->sheets[h]->height = h;
                }
            }
            /* 由于图层减少了一个，所以最上面的图层高度下降 */
            ctl->top--;
        }
        /* 按新图层的信息重新绘制画面 */
        sheet_refresh(ctl);
    }
    else if(old < height)
    {
        if(old >= 0)
        {
            /* 把中间的拉下去 */
            for(h = old; h < height; h++)
            {
                ctl->sheets[h]         = ctl->sheets[h + 1];
                ctl->sheets[h]->height = h;
            }
            ctl->sheets[height] = sht;
        }
        else
        {
            /* 由隐藏状态转为显示状态 */
            /* 将上面的提上来 */
            for(h = ctl->top; h >= height; h--)
            {
                ctl->sheets[h + 1] = ctl->sheets[h];
                ctl->sheets[h + 1]->height = h + 1;
            }
            ctl->sheets[height] = sht;
            /* 由于已显示图层增加了1个，所以最上面的图层高度增加 */
            ctl->top++;
        }
        sheet_refresh(ctl);
    }
    return ;
}