/* 内存相关 */

#include "bootpack.h"

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000
/*
内存检测函数：
原理：在CPU禁止缓存的情况下，向没有个内存地址写入一个数据，然后再读出改数据，比较读出和写入的数据是否一样
 */
unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 确认CPU是386爱是486以上的 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) { /* 如果是386，即使设定AC=1，AC的值还会自动回到0  */
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
 /*内粗管理初始化*/
void memman_init(struct MEMMAN *man)
{
	man->frees = 0;			/* frees:可用内存块的数量 */
	man->maxfrees = 0;		/* maxfrees:最大数量的可用内存块 */
	man->lostsize = 0;		/* lostsize:释放失败的内存的大小的总和 */
	man->losts = 0;			/* losts:释放失败的次数 */
	return;
}

unsigned int memman_total(struct MEMMAN *man)
/*计算空余内存的大小*/
{
	unsigned int i, t = 0;
	for (i = 0; i < man->frees; i++) {
		t += man->free[i].size;
	}
	return t;
}
/*内存分配函数，此种分配方式易造成内部碎片*/
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size)
/* 分配*/
{
	unsigned int i, a;
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].size >= size) {
			/*找到足够大的内存*/
			a = man->free[i].addr;
			man->free[i].addr += size;
			man->free[i].size -= size;
			if (man->free[i].size == 0) {
				/* 如果free[i]==0，就要减掉一条可用内存块的信息 */
				man->frees--;
				for (; i < man->frees; i++) {
					man->free[i] = man->free[i + 1]; /*数组操作，将i后面的数据向前移一位*/
				}
			}
			return a;
		}
	}
	return 0; /* 没有可用的内存空间了 */
}

int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size)
/* 释放 */
{
	int i, j;
	/* 为便于归纳内存，将free[]按照addr的顺序（从小到大）排列 */
	/*所以，先决定放在哪里*/
	for (i = 0; i < man->frees; i++) {
		if (man->free[i].addr > addr) {
			break;
		}
	}
	/* free[i - 1].addr < addr < free[i].addr */
	if (i > 0) {
		/*1. 前面有可用内存*/
		if (man->free[i - 1].addr + man->free[i - 1].size == addr) {
			/* 可与前面的可用内存块归纳到一起 */
			man->free[i - 1].size += size;
			if (i < man->frees) {
				/*后面也有*/
				if (addr + size == man->free[i].addr) {
					/* 也可以和后面的可用内存块归纳到一起*/
					man->free[i - 1].size += man->free[i].size;
					/* man->free[i]删除*/
					/* free[i]变成0后归纳到前面去 */
					man->frees--;
					for (; i < man->frees; i++) {
						man->free[i] = man->free[i + 1]; /*数组移位操作，向前移一位，删除free[i]*/
					}
				}
			}
			return 0; /*完成此种情况下的内存回收和合并*/
		}
	}
	/* 2.不能与前面的可用内存块合并到一起*/
	if (i < man->frees) {
		/*可以和后面的可用内存块合并到一起*/
		if (addr + size == man->free[i].addr) { 
			man->free[i].addr = addr;
			man->free[i].size += size;
			return 0; /* 合并成功 */
		}
	}
	/* 3.不能与前面和后面的可用内存块合并到一起 */
	if (man->frees < MEMMAN_FREES) {
		/* free[i]之后的，向后移动，腾出i位置，数据从i位置向后移动一位 */
		for (j = man->frees; j > i; j--) {
			man->free[j] = man->free[j - 1];
		}
		man->frees++;
		if (man->maxfrees < man->frees) {
			man->maxfrees = man->frees; /*更新最大值*/
		}
		man->free[i].addr = addr;
		man->free[i].size = size;
		return 0; /*释放内存块成功*/
	}
	/*以上三种情况都不能回收内存块，就表示本次内存回收失败，并标注*/
	man->losts++;
	man->lostsize += size;
	return -1;  
}
//每次分配4k内存
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size)
{
	unsigned int a;
	size = (size + 0xfff) & 0xfffff000;//向上舍入
	a = memman_alloc(man, size);
	return a;
}
//每次释放4k内存
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size)
{
	int i;
	size = (size + 0xfff) & 0xfffff000;//向上舍入
	i = memman_free(man, addr, size);
	return i;
}
