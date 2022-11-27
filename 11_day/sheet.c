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

    ctl->vram  = vram;              /* 用于显示的显存地址 */
    ctl->xsize = xsize;
    ctl->ysize = ysize;
    ctl->top   = -1;                /* 一个SHEET都没有 */
    for(i = 0; i<MAX_SHEETS;i++)
    {
        ctl->sheets0[i].flags = 0;   /* 标记为未使用 */
        ctl->sheets0[i].ctl   = ctl; /* 记录所属的ctl */
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

void sheet_refresh(SHEET *sht, int bx0, int by0, int bx1, int by1)
{
    SHTCTL *ctl = (SHTCTL *)sht->ctl;
    if(sht->height >= 0)
    {
        sheet_refreshsub(ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1, sht->vy0 + by1, sht->height);
    }
    return ;
}

void sheet_slide(SHEET *sht, int vx0, int vy0)
{
    SHTCTL *ctl     = (SHTCTL *)sht->ctl;
    int     old_vx0 = sht->vx0;
    int     old_vy0 = sht->vy0;

    sht->vx0 = vx0;
    sht->vy0 = vy0;

    if(sht->height >= 0)
    {
        /* 如果正在显示 */
        /* 按新图层的信息刷新画面 */
        sheet_refreshsub(ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0); /* 两对x,y描述矩形图层 */
        sheet_refreshsub(ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);
        //sheet_refresh(ctl);
    }
    return;
}

void sheet_free(SHEET *sht)
{
    SHTCTL *ctl     = (SHTCTL *)sht->ctl;
    if(sht->height >= 0)
    {
        /* 如果处于显示状态，则先设定为隐藏 */
        sheet_updown(sht, -1);
    }
    /* 标记 未使用 标志*/
    sht->flags = 0;
    return;
}

/* 设定底板高度 */
void sheet_updown(SHEET *sht, int height)
{
    int     h, old = sht->height;
    SHTCTL *ctl    = (SHTCTL *)sht->ctl;

    /* 如果高度过高或者过低，则进行修正 */
    if(height > ctl->top + 1) 
    {
        /* 如果添加的图层高度大于最上面的图层高度，将ctl高度设置为跟最上面图层一样高（+1） */
        height = ctl->top + 1;
    }
    if(height < -1)
    {
        height = -1;
    }
    
    /* 经过上面的clip操作，现在将合理的图层的高度赋值给sht的结构体信息 */
    sht->height = height; /* 设定底板图层的高度 */
    /* 下面主要进行sheets[]的重新排列，其中old代表sht目前的高度，而height代表将要在ctl中设置的高度 */
    if(old > height)
    {
        /* 如果比以前低，说明想把sht图层降下来（那大于其设定高度height图层就需要提到其上面） */
        if(height >= 0)
        {
            /* 把小于old大于height的那些图层往上提 */
            for(h = old; h > height; h--)
            {
                ctl->sheets[h]         = ctl->sheets[h-1];
                ctl->sheets[h]->height = h;
            }
            // 塞到想要设置的height位置
            ctl->sheets[height] = sht;
            /* 刷新其上面的图层 */
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height + 1);
        }
        else
        {
            /* 首先height < 0,这是进入这里的条件，其次，old > height,而height最小为-1，说明old要么为0，要么 > 0 */
            /* 这里不光sht之前的高度为多少，此时说明其想隐藏 */
            if(ctl->top > old)
            {
                /* 因此sht将要隐藏，因此之前在sht之上的那些图层就可以降下来 */
                for(h = old; h < ctl->top; h++)
                {
                    ctl->sheets[h]         = ctl->sheets[h + 1];
                    ctl->sheets[h]->height = h;
                }
            }
            /* 由于图层减少了一个，所以最上面的图层高度下降，这里说明一个问题，图层高度仅代表所有显示的图层，如果图层为-1，不会影响
               ctl的高度的！ */
            ctl->top--;
            /* 刷新背景 */
            sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, 0);
        }

    }
    else if(old < height)
    {
        /* 如果比之前要高 */
        if(old >= 0)
        {   
            /* old >= 0 说明之前是处于显示状态的，说明该图层想往上提，那留下的空位置就将其上面的图层依次往下挪 */
            /* 把中间的拉下去 */
            for(h = old; h < height; h++)
            {
                ctl->sheets[h]         = ctl->sheets[h + 1];
                ctl->sheets[h]->height = h; /* 更新每个图层的高度，h+1位置的图层放到h上，所以图层高度也需要更新为h */
            }
            /* 安放想到height高度的图层 */
            ctl->sheets[height] = sht;
        }
        else
        {
            /* 进入这里，说明之前old < 0, 处于隐藏状态，现在想转成显示状态了 */
            /* 将上面的提上来 */
            for(h = ctl->top; h >= height; h--)
            {
                ctl->sheets[h + 1] = ctl->sheets[h]; /* 你会发现，最上面已经到ctl->top + 1了，上界需要小心！ */
                ctl->sheets[h + 1]->height = h + 1;  /* h位置的图层放到h+1上，所以高度也更新为h+1*/
            }
            /* 安放想到height高度的图层 */
            ctl->sheets[height] = sht;
            /* 由于已显示图层增加了1个，所以最上面的图层高度增加 */
            ctl->top++;
        }
        /* 刷新该图层 */
        sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize, sht->vy0 + sht->bysize, height);
    }
    return ;
}

/* vx0,vy0:移动前的位置；vx1,vy1:移动后的位置 */
void sheet_refreshsub(SHTCTL *ctl, int vx0, int vy0, int vx1, int vy1, int h0)
{
    int            h, bx, by, vx, vy, bx0, by0, bx1, by1;
    unsigned char *buf, c, *vram = ctl->vram;
    SHEET         *sht;

    if(vx0 < 0){vx0 = 0;}
    if(vy0 < 0){vy0 = 0;}
    if(vx1 > ctl->xsize){vx1 = ctl->xsize;}
    if(vy1 > ctl->ysize){vy1 = ctl->ysize;}

    for(h=h0; h<=ctl->top; h++)
    {
        sht = ctl->sheets[h];
        buf = sht->buf;

        /* 使用vx0~vy1,对bx0~by1进行倒推 */
        bx0 = vx0 - sht->vx0;
        by0 = vy0 - sht->vy0;
        bx1 = vx1 - sht->vx0;
        by1 = vy1 - sht->vy0;
        if(bx0 < 0){bx0 = 0;}
        if(by0 < 0){by0 = 0;}
        if(bx1 > sht->bxsize){bx1 = sht->bxsize;}
        if(by1 > sht->bysize){by1 = sht->bysize;}

        for(by = by0; by < by1; by++)
        {
            vy = sht->vy0 + by;
            for(bx = bx0; bx < bx1; bx++)
            {
                vx = sht->vx0 + bx;

                c  = buf[by * sht->bxsize + bx]; /* 这是访问图层信息的方式 */
                if(c != sht->col_inv)
                {
                    /* 这是访问显存信息的方式 */
                    vram[vy * ctl->xsize + vx] = c;
                }

            }
        }
    }

    return;
}