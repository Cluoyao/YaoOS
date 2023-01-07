#include "bootpack.h"

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int memtest(unsigned int start, unsigned int end) 
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 确认CPU是386还是486以上的 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) {
		/* 如果是386，即使设定AC=1，AC的值还会自动回到0 */
		flg486 = 1;
	}

	eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 0 */
	io_store_eflags(eflg);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* 禁止缓存 */ 
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* 允许缓存 */
		store_cr0(cr0);
	}

	return i;
}

void memman_init(MEMMAN *man){
	man->frees = 0;    /* 可用信息数目 */
	man->maxfrees = 0; /* 用于观察可用状况：frees的最大值 */
	man->lostsize = 0; /* 释放失败的内存的大小总和 */
	man->losts = 0;    /* 释放失败次数 */
	return;
}

unsigned int memman_total(MEMMAN *man)
/* 报告空余内存大小的合计 */
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;   /* 每个size存放的是该空闲内存块的大小 */
	}
	return t;
}

unsigned int memman_alloc(MEMMAN *man, unsigned int size)
/* 分配 */
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) 
	{
		if (man->free[i].size >= size)
		{
			/* 找到了足够大的内存 */
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0)  /* 此处不用考虑<=0，因为man->free[i].size >= size*/
			{
				/* 如果free[i]变成了0，就减掉一条可用信息 */
				man->frees--;
				for (; i < man->frees; i++) 
				{	
					/* 依次向前覆盖 */
					man->free[i] = man->free[i + 1]; /* 代入结构体 */
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用空间 */
}

int memman_free(MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
	int i, j;
	/* 为便于归纳内存，将free[]按照addr的顺序排列 */
	/* 所以，先决定应该放在哪里 */
	for (i = 0; i < man->frees; i++) 
	{
		if (man->free[i].addr > addr) 
		{
			/* 找到第一个大于空闲列表内存地址的位置，将释放地址的信息写入进去 */
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) 
	{
		/* 前面有可用内存（如果要释放的内存跟之前的空闲内存连续，则进行合并） */
		if (man->free[i - 1].addr + man->free[i - 1].size == addr)
		{
			/* 可以与前面的可用内存归纳到一起 */
			man->free[i - 1].size += size;
			if (i < man->frees) 
			{
				/* 后面也有（如果要释放的内存跟之后的空闲内存连续，则也进行合并） */
				if (addr + size == man->free[i].addr)
				{
					/* 也可以与后面的可用内存归纳到一起 */
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]删除 */
					/* free[i]变成0后归纳到前面去 */
					man->frees--;
					for (; i < man->frees; i++) 
					{
						/* 后面的内存列表向前整理 */
						man->free[i] = man->free[i + 1]; /* 结构体赋值 */
					}
				}
			}
			return 0; /* 成功完成 */
		}
		/* 如果上面的这个if语句未执行，则函数会继续执行下去，不会提前结束函数执行*/
	}
	/* 不能与前面的可用空间归纳到一起 */
	if (i < man->frees)
	{
		/* 后面还有 */
		if (addr + size == man->free[i].addr) 
		{
			/* 可以与后面的内容归纳到一起 */
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 成功完成，此时内存链表的数量不用变化，因为这是在释放，已经将释放的内存大小写到i的size里面去了 */
		}
		/* 如果不能与后面的空间合并，则继续执行后面的语句，不会进行函数返回*/
	}
	/* 既不能与前面归纳到一起，也不能与后面归纳到一起 */
	if (man->frees < MEMMAN_FREES) 
	{
		/* 插入一个新的空闲内存区域 */
		/* free[i]之后的，向后移动，腾出一点可用空间 */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /* 更新最大值 */
		}
		/* i作为新的内存块信息 */
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /* 成功完成 */
	}
	// 如果上面的三个if语句都没有命中，说明已经空闲列表已经满了，装不下释放的内存信息了，下面只进行了记录，并没有其他操作~
	/* 不能往后移动 */
	man->losts++;
	man->lostsize += size;
	return -1; /* 失败 */
}

unsigned int memman_alloc_4k(MEMMAN *man, unsigned int size)
{
    unsigned int a;
    size = (size + 0xfff) & 0xfffff000; /* 向上取整 */
    a    = memman_alloc(man, size);     /* 返回分配的地址 */
    return a;
}
int memman_free_4k(MEMMAN *man, unsigned int addr, unsigned int size)
{
    int i;
    size = (size + 0xfff) & 0xfffff000; /* 向上取整 */
    i    = memman_free(man, addr, size); /* 返回0则成功 */
    return i; 
}