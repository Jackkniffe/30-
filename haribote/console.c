/* 命令行窗口相关 */

#include "bootpack.h"
#include <stdio.h>
#include <string.h>

//命令行任务处理函数
void console_task(struct SHEET *sheet, int memtotal)
{
	struct TASK *task = task_now();//获得当前任务
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;//获得内存管理
	int i, *fat = (int *) memman_alloc_4k(memman, 4 * 2880);//为读取文件分配内存
	struct CONSOLE cons;
	struct FILEHANDLE fhandle[8];//文件句柄
	char cmdline[30];//用于存储输入的命令
	unsigned char *nihongo = (char *) *((int *) 0x0fe8);

	cons.sht = sheet;
	cons.cur_x =  8;
	cons.cur_y = 28;
	cons.cur_c = -1;
	task->cons = &cons;
	task->cmdline = cmdline;
	//为命令行任务设置定时器
	if (cons.sht != 0) {
		cons.timer = timer_alloc();
		timer_init(cons.timer, &task->fifo, 1);
		timer_settime(cons.timer, 50);
	}
	//从磁盘中读取文件读取文件
	file_readfat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));
	//实例换文件句柄
	for (i = 0; i < 8; i++) {
		fhandle[i].buf = 0;	/* 表明未使用*/
	}
	//设置任务中文件句柄和文件在内存中的地址
	task->fhandle = fhandle;
	task->fat = fat;
	//语言相关判断
	if (nihongo[4096] != 0xff) {	/* 是否载入了日文字库？ */
		task->langmode = 1;
	} else {
		task->langmode = 0;
	}
	task->langbyte1 = 0;

	/* 显示提示符 */
	cons_putchar(&cons, '>', 1);

	for (;;) {
		io_cli();//在读取缓冲区数据时禁止中断
		if (fifo32_status(&task->fifo) == 0) {
			//没有数据就让人物休眠
			task_sleep(task);
			io_sti();//开启可中断
		} else {
			i = fifo32_get(&task->fifo);
			io_sti();//开启可中断
			if (i <= 1 && cons.sht != 0) { /* 光标用的定时器 */
				if (i != 0) {
					timer_init(cons.timer, &task->fifo, 0); /* 下次定时器返回时，向FIFO写入0 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_FFFFFF;//改变光标显示的颜色
					}
				} else {
					timer_init(cons.timer, &task->fifo, 1); /* 下次定时器返回时，向FIFO写入01 */
					if (cons.cur_c >= 0) {
						cons.cur_c = COL8_000000;//改变光标显示的颜色
					}
				}
				timer_settime(cons.timer, 50);//给定时器设置超值时间
			}
			if (i == 2) {	/* 光标ON */
				cons.cur_c = COL8_FFFFFF;
			}
			if (i == 3) {	/* 光标OFF */
				if (cons.sht != 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, COL8_000000,
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				cons.cur_c = -1;
			}
			if (i == 4) {	/*点击命令行窗口的"x"按钮*/
				cmd_exit(&cons, fat);
			}
			if (256 <= i && i <= 511) { /* 键盘数据（通过任务A） */
				if (i == 8 + 256) {
					/* 退格键 */
					if (cons.cur_x > 16) {
						/* 将光标用空格擦除后将光标前移一位 */
						cons_putchar(&cons, ' ', 0);
						cons.cur_x -= 8;
					}
				} else if (i == 10 + 256) {
					/* Enter 回车键*/
					/* 将光标用空格擦除后换行 */
					cons_putchar(&cons, ' ', 0);
					//保存命令行输入的命令
					cmdline[cons.cur_x / 8 - 2] = 0;
					cons_newline(&cons);
					cons_runcmd(cmdline, &cons, fat, memtotal);	/* 运行命令 */
					if (cons.sht == 0) {
						cmd_exit(&cons, fat);//
					}
					/* 显示提示符 */
					cons_putchar(&cons, '>', 1);
				} else {
					/* 一般文字 */
					if (cons.cur_x < 240) {
						/* 显示一个字符后键贯标后移一位 */
						cmdline[cons.cur_x / 8 - 2] = i - 256;
						cons_putchar(&cons, i - 256, 1);
					}
				}
			}
			/* 重新显示光标 */
			if (cons.sht != 0) {
				if (cons.cur_c >= 0) {
					boxfill8(cons.sht->buf, cons.sht->bxsize, cons.cur_c, 
						cons.cur_x, cons.cur_y, cons.cur_x + 7, cons.cur_y + 15);
				}
				sheet_refresh(cons.sht, cons.cur_x, cons.cur_y, cons.cur_x + 8, cons.cur_y + 16);
			}
		}
	}
}

void cons_putchar(struct CONSOLE *cons, int chr, char move)
{
	char s[2];
	s[0] = chr;
	s[1] = 0;
	if (s[0] == 0x09) {	/*制表符*/
		for (;;) {
			if (cons->sht != 0) {
				putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, " ", 1);
			}
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {//到达一行的末尾，就要换到下一行
				cons_newline(cons);
			}
			if (((cons->cur_x - 8) & 0x1f) == 0) {//命令行窗口有8个像素的边框，所以要减8
				break;	/* 被32整除则break */
			}
		}
	} else if (s[0] == 0x0a) {	/* 换行 */
		cons_newline(cons);
	} else if (s[0] == 0x0d) {	/* 回车 */
		/* 暂时不做任何操作 */
	} else {	/* 一般的字符 */
		if (cons->sht != 0) {
			putfonts8_asc_sht(cons->sht, cons->cur_x, cons->cur_y, COL8_FFFFFF, COL8_000000, s, 1);
		}
		if (move != 0) {
			/* move为0时光标不后移 */
			cons->cur_x += 8;
			if (cons->cur_x == 8 + 240) {//到达一行的末尾，就要换到下一行
				cons_newline(cons);
			}
		}
	}
	return;
}
//换行操作
void cons_newline(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	struct TASK *task = task_now();
	if (cons->cur_y < 28 + 112) {
		cons->cur_y += 16; /* 到下一行 */
	} else {
		/* 滚动 */
		if (sheet != 0) {
			/*将显示的内容都向上一行*/
			for (y = 28; y < 28 + 112; y++) {
				for (x = 8; x < 8 + 240; x++) {
					
					sheet->buf[x + y * sheet->bxsize] = sheet->buf[x + (y + 16) * sheet->bxsize];
				}
			}
			/*将最后一行清空*/
			for (y = 28 + 112; y < 28 + 128; y++) {
				for (x = 8; x < 8 + 240; x++) {
					sheet->buf[x + y * sheet->bxsize] = COL8_000000;
				}
			}
			sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
		}
	}
	cons->cur_x = 8;
	if (task->langmode == 1 && task->langbyte1 != 0) {
		cons->cur_x = 16;
	}
	return;
}
//显示字符串方法1
void cons_putstr0(struct CONSOLE *cons, char *s)
{
	for (; *s != 0; s++) {
		cons_putchar(cons, *s, 1);
	}
	return;
}
//显示字符串方法2
void cons_putstr1(struct CONSOLE *cons, char *s, int l)
{
	int i;
	for (i = 0; i < l; i++) {
		cons_putchar(cons, s[i], 1);
	}
	return;
}
//运行命令
void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, int memtotal)
{
	if (strcmp(cmdline, "mem") == 0 && cons->sht != 0) {
		cmd_mem(cons, memtotal);
	} else if (strcmp(cmdline, "cls") == 0 && cons->sht != 0) {
		cmd_cls(cons);
	} else if (strcmp(cmdline, "dir") == 0 && cons->sht != 0) {
		cmd_dir(cons);
	} else if (strcmp(cmdline, "exit") == 0) {
		cmd_exit(cons, fat);
	} else if (strncmp(cmdline, "start ", 6) == 0) {
		cmd_start(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "ncst ", 5) == 0) {
		cmd_ncst(cons, cmdline, memtotal);
	} else if (strncmp(cmdline, "langmode ", 9) == 0) {
		cmd_langmode(cons, cmdline);
	} else if (cmdline[0] != 0) {
		if (cmd_app(cons, fat, cmdline) == 0) {
			/* 不是命令，不是应用程序，也不是空行 */
			cons_putstr0(cons, "Bad command.\n\n");
		}
	}
	return;
}

//mem 命令：显示内存使用情况
void cmd_mem(struct CONSOLE *cons, int memtotal)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	char s[60];
	sprintf(s, "total   %dMB\nfree %dKB\n\n", memtotal / (1024 * 1024), memman_total(memman) / 1024);
	cons_putstr0(cons, s);
	return;
}
//cls命令：将命令行窗口清空
void cmd_cls(struct CONSOLE *cons)
{
	int x, y;
	struct SHEET *sheet = cons->sht;
	for (y = 28; y < 28 + 128; y++) {
		for (x = 8; x < 8 + 240; x++) {
			sheet->buf[x + y * sheet->bxsize] = COL8_000000;
		}
	}
	sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
	cons->cur_y = 28;
	return;
}
//dir命令:显示文件
void cmd_dir(struct CONSOLE *cons)
{
	struct FILEINFO *finfo = (struct FILEINFO *) (ADR_DISKIMG + 0x002600);//获得文件信息
	int i, j;
	char s[30];
	for (i = 0; i < 224; i++) {
		if (finfo[i].name[0] == 0x00) {
			break;
		}
		if (finfo[i].name[0] != 0xe5) {
			if ((finfo[i].type & 0x18) == 0) {
				//获得文件大小，名称，扩展名
				sprintf(s, "filename.ext   %7d\n", finfo[i].size);
				for (j = 0; j < 8; j++) {
					s[j] = finfo[i].name[j];
				}
				s[ 9] = finfo[i].ext[0];
				s[10] = finfo[i].ext[1];
				s[11] = finfo[i].ext[2];
				cons_putstr0(cons, s);
			}
		}
	}
	cons_newline(cons);
	return;
}
//退出命名行窗口
void cmd_exit(struct CONSOLE *cons, int *fat)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct TASK *task = task_now();
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct FIFO32 *fifo = (struct FIFO32 *) *((int *) 0x0fec);
	if (cons->sht != 0) {
		timer_cancel(cons->timer);
	}
	memman_free_4k(memman, (int) fat, 4 * 2880);
	io_cli();
	if (cons->sht != 0) {
		fifo32_put(fifo, cons->sht - shtctl->sheets0 + 768);	/* 768〜1023 */
	} else {
		fifo32_put(fifo, task - taskctl->tasks0 + 1024);	/* 1024〜2023 */
	}
	io_sti();
	for (;;) {
		task_sleep(task);
	}
}
//启动一个命令行窗口
void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht = open_console(shtctl, memtotal);
	struct FIFO32 *fifo = &sht->task->fifo;
	int i;
	sheet_slide(sht, 32, 4);
	sheet_updown(sht, shtctl->top);
	/* 将命令行输入的字符串逐字复制到新的命令行窗口中 */
	for (i = 6; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	/* Enter 回车键*/
	cons_newline(cons);
	return;
}
//启动一个应用程序是时不打开新的命令行窗口
void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal)
{
	struct TASK *task = open_constask(0, memtotal);
	struct FIFO32 *fifo = &task->fifo;
	int i;
	/* 将命令行输入的字符串逐字复制到新的命令行窗口中 */
	for (i = 5; cmdline[i] != 0; i++) {
		fifo32_put(fifo, cmdline[i] + 256);
	}
	fifo32_put(fifo, 10 + 256);	/* Enter 回车键*/
	cons_newline(cons);
	return;
}
//语言设置
void cmd_langmode(struct CONSOLE *cons, char *cmdline)
{
	struct TASK *task = task_now();
	unsigned char mode = cmdline[9] - '0';
	if (mode <= 2) {
		task->langmode = mode;
	} else {
		cons_putstr0(cons, "mode number error.\n");
	}
	cons_newline(cons);
	return;
}
//启动一个应用程序
int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline)
{
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	struct FILEINFO *finfo;
	char name[18], *p, *q;
	struct TASK *task = task_now();
	int i, segsiz, datsiz, esp, dathrb;
	struct SHTCTL *shtctl;
	struct SHEET *sht;

	/* 根据命令生成文件名 */
	for (i = 0; i < 13; i++) {
		if (cmdline[i] <= ' ') {
			break;
		}
		name[i] = cmdline[i];
	}
	name[i] = 0; /*暂时将文件名的后面置为0 */

	/* 搜索文件信息 */
	finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	if (finfo == 0 && name[i - 1] != '.') {
		/* 由于找不到文件所以在文件名的后面加上".HRB"，再重新寻找*/
		name[i    ] = '.';
		name[i + 1] = 'H';
		name[i + 2] = 'R';
		name[i + 3] = 'B';
		name[i + 4] = 0;
		finfo = file_search(name, (struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
	}

	if (finfo != 0) {
		/* 找到文件的情况 */
		p = (char *) memman_alloc_4k(memman, finfo->size);
		//将文件内容加载到内存p处
		file_loadfile(finfo->clustno, finfo->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
		if (finfo->size >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
			segsiz = *((int *) (p + 0x0000));//请求操作系统为应用程序准备的数据段的大小
			esp    = *((int *) (p + 0x000c));//ESP初始值&数据部分传送的目的地址
			datsiz = *((int *) (p + 0x0010));//hrb文件内数据部分的大小
			dathrb = *((int *) (p + 0x0014));//hrb文件内数据部分从哪里开始
			q = (char *) memman_alloc_4k(memman, segsiz);
			task->ds_base = (int) q;
			//设置局部描述符
			set_segmdesc(task->ldt + 0, finfo->size - 1, (int) p, AR_CODE32_ER + 0x60);
			set_segmdesc(task->ldt + 1, segsiz - 1,      (int) q, AR_DATA32_RW + 0x60);
			for (i = 0; i < datsiz; i++) {
				q[esp + i] = p[dathrb + i];
			}
			//启动应用程序
			start_app(0x1b, 0 * 8 + 4, esp, 1 * 8 + 4, &(task->tss.esp0));
			shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
			for (i = 0; i < MAX_SHEETS; i++) {
				sht = &(shtctl->sheets0[i]);
				if ((sht->flags & 0x11) == 0x11 && sht->task == task) {
					/* 找到被应用程序遗留的窗口 */
					sheet_free(sht);	/* 关闭 */
				}
			}
			for (i = 0; i < 8; i++) {	/* 释放任务所持有为文件 */
				if (task->fhandle[i].buf != 0) {
					memman_free_4k(memman, (int) task->fhandle[i].buf, task->fhandle[i].size);
					task->fhandle[i].buf = 0;
				}
			}
			timer_cancelall(&task->fifo);
			memman_free_4k(memman, (int) q, segsiz);
			task->langbyte1 = 0;
		} else {
			cons_putstr0(cons, ".hrb file format error.\n");
		}
		memman_free_4k(memman, (int) p, finfo->size);
		cons_newline(cons);
		return 1;
	}
	/* 没有找到文件的情况 */
	return 0;
}
//系统调用处理
int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax)
{
	struct TASK *task = task_now();
	int ds_base = task->ds_base;
	struct CONSOLE *cons = task->cons;
	struct SHTCTL *shtctl = (struct SHTCTL *) *((int *) 0x0fe4);
	struct SHEET *sht;
	struct FIFO32 *sys_fifo = (struct FIFO32 *) *((int *) 0x0fec);
	int *reg = &eax + 1;	/* eax后面的地址 */
		/* 强行改写通过PUSHAD保存的值 */
		/* reg[0] : EDI,   reg[1] : ESI,   reg[2] : EBP,   reg[3] : ESP */
		/* reg[4] : EBX,   reg[5] : EDX,   reg[6] : ECX,   reg[7] : EAX */
	int i;
	struct FILEINFO *finfo;
	struct FILEHANDLE *fh;
	struct MEMMAN *memman = (struct MEMMAN *) MEMMAN_ADDR;
	//系统调用处理
	/*显示字符
	  EDX=1----显示单个字符(AL=字符编码)
	      2----显示字符串0(EBX=字符串地址)
	      3----显示字符串1(EBX=字符串地址，ECX=字符串长度)*/
	if (edx == 1) {//显示单个字符
		cons_putchar(cons, eax & 0xff, 1);
	} else if (edx == 2) {//显示字符串0
		cons_putstr0(cons, (char *) ebx + ds_base);
	} else if (edx == 3) {//显示字符串1
		cons_putstr1(cons, (char *) ebx + ds_base, ecx);
	} else if (edx == 4) {
		/*结束应用程序*/
		return &(task->tss.esp0);
	} else if (edx == 5) {
		/*显示窗口
		EDX=5
		EBX=窗口缓冲区
		ESI=窗口在x轴方向上的大小(即窗口宽度)
		EDI=窗口在y轴方向上的大小(即窗口高度)
		EAX=透明色
		ECX=窗口名称
		EAX=用于操作窗口的句柄(用于刷新窗口等操作)*/
		sht = sheet_alloc(shtctl);
		sht->task = task;
		sht->flags |= 0x10;
		sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
		make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
		sheet_slide(sht, ((shtctl->xsize - esi) / 2) & ~3, (shtctl->ysize - edi) / 2);
		sheet_updown(sht, shtctl->top); /* 将窗口图层高度指定为当前鼠标所在图层的高度，鼠标移动到上层 */
		reg[7] = (int) sht;
	} else if (edx == 6) {
		/*在窗口上显示字符
		EDX=6
		EBX=窗口句柄
		ESI=显示位置的x坐标
		EDI=显示位置的y坐标
		EAX=色号
		ECX=字符串长度
		EBP=字符串*/
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		putfonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
		}
	} else if (edx == 7) {
		/*在画面上绘制方块
		EDX=7
		EBX=窗口句柄
		EAX=x0
		ECX=y0
		ESI=x1
		EDI=y1
		EBP=色号*/
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		boxfill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 8) {
		/*内存memman初始化
		EDX=8
		EBX=memman的地址
		EAX=memman所管理的内存空间的起始地址
		ECX=memman所管理的内存空间的字节数*/
		memman_init((struct MEMMAN *) (ebx + ds_base));
		ecx &= 0xfffffff0;	/* 以16字节为单位进位取整 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 9) {
		/*内存分配：
		EDX=9
		EBX=memman的地址
		ECX=需要请求的字节数
		EAX=分配到的内存空间地址
		*/
		ecx = (ecx + 0x0f) & 0xfffffff0; /*  以16字节为单位进位取整 */
		reg[7] = memman_alloc((struct MEMMAN *) (ebx + ds_base), ecx);
	} else if (edx == 10) {
		/*内存释放
		EDX=10
		EBX=memman的地址
		EAX=需要释放的内存空间地址
		ECX-需要释放的字节数*/
		ecx = (ecx + 0x0f) & 0xfffffff0; /* 以16字节为单位进位取整 */
		memman_free((struct MEMMAN *) (ebx + ds_base), eax, ecx);
	} else if (edx == 11) {
		/*在窗口中画点
		EDX=11
		EBX=窗口句柄
		ESI=显示位置的x坐标
		EDI=显示位置的y坐标
		EAX=色号*/
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		sht->buf[sht->bxsize * edi + esi] = eax;
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, esi, edi, esi + 1, edi + 1);
		}
	} else if (edx == 12) {
		/*刷新窗口
		EDX=12
		EBX=窗口句柄
		EAX=x0
		ECX=y0
		ESI=x1
		EDI=y1*/
		sht = (struct SHEET *) ebx;
		sheet_refresh(sht, eax, ecx, esi, edi);
	} else if (edx == 13) {
		/*在窗口上画直线
		EDX=13
		EBX=窗口句柄
		EAX=x0
		ECX=y0
		ESI=x1
		EDI=y1
		EBP=色号*/
		sht = (struct SHEET *) (ebx & 0xfffffffe);
		hrb_api_linewin(sht, eax, ecx, esi, edi, ebp);
		if ((ebx & 1) == 0) {
			sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
		}
	} else if (edx == 14) {
		/*关闭窗口
		EDX=14
		EBX=窗口句柄*/
		sheet_free((struct SHEET *) ebx);
	} else if (edx == 15) {
		/*键盘输入：
			EDX=15
			EAX=0----没有键盘输入时返回-1，不休眠
			   =1----休眠直到发生键盘输入
			EAX=输入的字符*/
		for (;;) {
			io_cli();
			if (fifo32_status(&task->fifo) == 0) {
				if (eax != 0) {
					task_sleep(task);	/* FIFO为空，休眠并等待 */
				} else {
					io_sti();
					reg[7] = -1;
					return 0;
				}
			}
			i = fifo32_get(&task->fifo);
			io_sti();
			if (i <= 1 && cons->sht != 0) { /* 光标用的定时器 */
				/* 应用程序运行时不需要显示光标，因此总是将下次显示用的值置为1 */
				timer_init(cons->timer, &task->fifo, 1); /* 下次置为1 */
				timer_settime(cons->timer, 50);
			}
			if (i == 2) {	/* 光标ON */
				cons->cur_c = COL8_FFFFFF;
			}
			if (i == 3) {	/* 光标OFF */
				cons->cur_c = -1;
			}
			if (i == 4) {	/* コンソールだけを閉じる */
				timer_cancel(cons->timer);
				io_cli();
				fifo32_put(sys_fifo, cons->sht - shtctl->sheets0 + 2024);	/* 2024〜2279 */
				cons->sht = 0;
				io_sti();
			}
			if (i >= 256) { /* 键盘数据（通过任务A） */
				reg[7] = i - 256;
				return 0;
			}
		}
	} else if (edx == 16) {
		/*获取定时器
			EDX=16
			EAX=定时器句柄(由操作系统返回)*/
		reg[7] = (int) timer_alloc();
		((struct TIMER *) reg[7])->flags2 = 1;	/* 允许自动取消効 */
	} else if (edx == 17) {
		/*设置定时器发送的数据
			EDX=17
			EBX=定时器句柄
			EAX=数据*/
		timer_init((struct TIMER *) ebx, &task->fifo, eax + 256);
	} else if (edx == 18) {
		/*设置定时器超时的时间
			EDX=18
			EBX=定时器句柄
			EAX=超时时间*/
		timer_settime((struct TIMER *) ebx, eax);
	} else if (edx == 19) {
		/*释放定时器 
			EDX=19
			EBX=定时器句柄
			*/
		timer_free((struct TIMER *) ebx);
	} else if (edx == 20) {
	/*使蜂鸣器发声
		EDX=20
		DAX-声音频率(单位HZ)
		*/
	//蜂鸣器相关硬件操作
		if (eax == 0) {
			i = io_in8(0x61);
			io_out8(0x61, i & 0x0d);
		} else {
			i = 1193180000 / eax;
			io_out8(0x43, 0xb6);
			io_out8(0x42, i & 0xff);
			io_out8(0x42, i >> 8);
			i = io_in8(0x61);
			io_out8(0x61, (i | 0x03) & 0x0f);
		}
	} else if (edx == 21) {
	/*打开文件
		EDX=21
		EBX=文件名
		EAX=文件句柄*/
		for (i = 0; i < 8; i++) {
			if (task->fhandle[i].buf == 0) {
				break;
			}
		}
		fh = &task->fhandle[i];
		reg[7] = 0;
		if (i < 8) {
			finfo = file_search((char *) ebx + ds_base,
					(struct FILEINFO *) (ADR_DISKIMG + 0x002600), 224);
			if (finfo != 0) {
				reg[7] = (int) fh;
				fh->buf = (char *) memman_alloc_4k(memman, finfo->size);
				fh->size = finfo->size;
				fh->pos = 0;
				file_loadfile(finfo->clustno, finfo->size, fh->buf, task->fat, (char *) (ADR_DISKIMG + 0x003e00));
			}
		}
	} else if (edx == 22) {
	/*关闭文件：
		EDX=22
		EAX=文件句柄*/
		fh = (struct FILEHANDLE *) eax;
		memman_free_4k(memman, (int) fh->buf, fh->size);
		fh->buf = 0;
	} else if (edx == 23) {
	/*文件定位
		EDX=23
		EAX=文件句柄
		ECX=定位模式
		   =0：定位的起点为文件的开头
		   =1：定位的起点为当前的访问位置
		   =2：定位的起点为文件的末尾
		EBX=定位的偏移量*/
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			fh->pos = ebx;
		} else if (ecx == 1) {
			fh->pos += ebx;
		} else if (ecx == 2) {
			fh->pos = fh->size + ebx;
		}
		if (fh->pos < 0) {
			fh->pos = 0;
		}
		if (fh->pos > fh->size) {
			fh->pos = fh->size;
		}
	} else if (edx == 24) {
	/*获取文件大小
		EDX=24
		EAX=文件句柄
		ECX=文件大小获取模式
		   =0：普通文件大小
		   =1：当前读取位置从文件开头起算的偏移量
		   =2：当前读取位置从文件末尾起算的偏移量
		EAX=文件大小（由操作系统返回）*/
		fh = (struct FILEHANDLE *) eax;
		if (ecx == 0) {
			reg[7] = fh->size;
		} else if (ecx == 1) {
			reg[7] = fh->pos;
		} else if (ecx == 2) {
			reg[7] = fh->pos - fh->size;
		}
	} else if (edx == 25) {
	/*文件读取
		EDX=25
		EAX=文件句柄
		EBX=缓冲区地址
		ECX=最大读取字节数
		EAX=本次读取到的字节数（由操作系统返回）*/
		fh = (struct FILEHANDLE *) eax;
		for (i = 0; i < ecx; i++) {
			if (fh->pos == fh->size) {
				break;
			}
			*((char *) ebx + ds_base + i) = fh->buf[fh->pos];
			fh->pos++;
		}
		reg[7] = i;
	} else if (edx == 26) {
		/*获取命令行
		EDX=26
		EBX=存放命令行内容的地址
		ECX=最多可存放多少字节
		EAX=实际存放了多少字节（由操作系统返回）*/
		i = 0;
		for (;;) {
			*((char *) ebx + ds_base + i) =  task->cmdline[i];
			if (task->cmdline[i] == 0) {
				break;
			}
			if (i >= ecx) {
				break;
			}
			i++;
		}
		reg[7] = i;
	} else if (edx == 27) {
		/*获得langmode:
			获得语言模式：
			EDX=27
			EAX=langmode(由操作系统返回)*/
		reg[7] = task->langmode;
	}
	return 0;
}
//栈异常中断处理函数
int *inthandler0c(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0C :\n Stack Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);	/*强制结束程序*/
}
//通用中断异常处理函数
int *inthandler0d(int *esp)
{
	struct TASK *task = task_now();
	struct CONSOLE *cons = task->cons;
	char s[30];
	cons_putstr0(cons, "\nINT 0D :\n General Protected Exception.\n");
	sprintf(s, "EIP = %08X\n", esp[11]);
	cons_putstr0(cons, s);
	return &(task->tss.esp0);	/* 强制结束程序 */
}
//在窗口上画线
void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col)
{
	int i, x, y, len, dx, dy;

	dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	} else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sht->buf[(y >> 10) * sht->bxsize + (x >> 10)] = col;
		x += dx;
		y += dy;
	}

	return;
}
