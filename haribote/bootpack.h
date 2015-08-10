/* asmhead.nas */
//显卡相关信息，从BIOS中获得的
struct BOOTINFO { /* 0x0ff0-0x0fff */
	char cyls; /* 引导扇区读取到磁盘的哪个位置 */
	char leds; /* 引导是键盘的LED状态 */
	char vmode; /* 显卡的颜色位数 */
	char reserve;
	short scrnx, scrny; /* 画面的分辨率 */
	char *vram;
};
#define ADR_BOOTINFO	0x00000ff0 //存储显卡相关信息的内存地址
#define ADR_DISKIMG		0x00100000 //磁盘镜像地址

/* naskfunc.nas */
void io_hlt(void);//让cpu停止，等着指令
void io_cli(void);//禁止中断，将中断许可标志位置0
void io_sti(void);//恢复中断
void io_stihlt(void);//恢复中断，并让cpu空转
int io_in8(int port);//从一个port中读取一字节
void io_out8(int port, int data);//向port中写入data
int io_load_eflags(void);//读取标志寄存器
void io_store_eflags(int eflags);//写入标志寄存器
void load_gdtr(int limit, int addr);//设置全局描述表寄存器
void load_idtr(int limit, int addr);//设置中断描述符表寄存器
int load_cr0(void);//读取cr0控制寄存器（cr0控制器用来开启保护模式）
void store_cr0(int cr0);//写入cr0控制寄存器
void load_tr(int tr);//读取任务寄存器s
void asm_inthandler0c(void);//栈异常中断处理函数
void asm_inthandler0d(void);//通用异常中断处理函数
void asm_inthandler20(void);//
void asm_inthandler21(void);//键盘中断处理函数
void asm_inthandler2c(void);//鼠标中断处理函数
unsigned int memtest_sub(unsigned int start, unsigned int end);//内存校验函数
void farjmp(int eip, int cs);//far模式的jmp
void farcall(int eip, int cs);//far模式的call
void asm_hrb_api(void);//系统调用中断处理函数
void start_app(int eip, int cs, int esp, int ds, int *tss_esp0);//启动一个新的app的处理函数
void asm_end_app(void);//关闭一个应用程序的处理函数

/* fifo.c 缓冲区相关处理*/
/*
数据缓存区，用于存储外部中断返回的数据
FIFO32：
buf:缓冲区的起始地址
p:下一个数据写入的位置
q:下一个数据读出的位置
size:缓冲区的大小
free:剩余缓冲区的大小
flags:标志缓冲区状态
task:次此缓冲区所属的task

 */
struct FIFO32 {
	int *buf;
	int p, q, size, free, flags;
	struct TASK *task;
};
void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK *task);
int fifo32_put(struct FIFO32 *fifo, int data);
int fifo32_get(struct FIFO32 *fifo);
int fifo32_status(struct FIFO32 *fifo);

/* graphic.c  图形相关处理
   init_palette：初始化调色板函数
   set_palette：调色板设置的硬件操作相关函数
   boxfill8：在界面上画矩形
   init_screen8：初始化界面
   putfont8：输出显示单个字符
   putfonts8_asc：输出显示字符串
   init_mouse_cursor8:初始化鼠标
   putblock8_8：
*/

void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_asc(char *vram, int xsize, int x, int y, char c, unsigned char *s);
void init_mouse_cursor8(char *mouse, char bc);
void putblock8_8(char *vram, int vxsize, int pxsize,
	int pysize, int px0, int py0, char *buf, int bxsize);
//16中颜色的数组下标
#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15
//描述符表相关定义
/* dsctbl.c 设置全局描述符表和中断描述符表
   操作系统代码段：2*8
   操作系统数据段：1*8
   应用程序代码段：1003*8
   应用程序数据段：1004*8
   .
   .
   .
   TSS所使用的段：3*8——>1002*8
*/
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
/*
ADR_IDT,LIMIT_IDT:用于设置中断描述符表表寄存器(GDTR 48位 ，LIMIT_IDT占用低16位，ADR_IDT占用高32位)
ADR_GDT,LIMIT_GDT:用于设置全局描述符表寄存器(GDTR 48位 ，LIMIT_GDT占用低16位，ADR_GDT占用高32位)
 */
#define ADR_IDT			0x0026f800//中断描述符表的基址
#define LIMIT_IDT		0x000007ff//中断描述符表的大小
#define ADR_GDT			0x00270000//全局描述符表的基址
#define LIMIT_GDT		0x0000ffff//全局描述符表的大小
//一个用于系统段的全局描述符(长度为8个字节)：
#define ADR_BOTPAK		0x00280000//全局描述符中描述此段的基址是多少
#define LIMIT_BOTPAK	0x0007ffff//全局描述符中描述此段的长度是多少
//全局描述符中段的访问权限：
#define AR_DATA32_RW	0x4092//系统段专用，可读写的段
#define AR_CODE32_ER	0x409a//系统段专用,可执行的段
//局部描述符中段的访问权限
#define AR_LDT			0x0082
//任务描述符的访问权限
#define AR_TSS32		0x0089
 
#define AR_INTGATE32	0x008e

/* int.c 
可编程中断控制器操作定义
主要用于初始化中断
*/
void init_pic(void);
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

/* keyboard.c 
键盘事件相关处理
*/
void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(struct FIFO32 *fifo, int data0);//初始化键盘的控制电路
#define PORT_KEYDAT		0x0060
#define PORT_KEYCMD		0x0064

/* mouse.c 
鼠标事件相关处理*/
struct MOUSE_DEC {
	unsigned char buf[3], phase;
	int x, y, btn;
};
void inthandler2c(int *esp);
void enable_mouse(struct FIFO32 *fifo, int data0, struct MOUSE_DEC *mdec);//激活鼠标
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat);

/* memory.c 
内存管理，管理用的数据结构大约32KB
*/
#define MEMMAN_FREES		4090	/*将内存分成4090块进行管理*/
#define MEMMAN_ADDR			0x003c0000/*MEMMAN数据结构的首地址*/
/*单个可用内存块的信息
  addr:可用内存块的起始地址
  size:可用内存块的大小
*/
struct FREEINFO {	
	unsigned int addr, size;
};
/*内存管理数据结构
  frees:可用内存块的数量
  maxfrees:最大数量的可用内存块
  lostsize:释放失败的内存的大小的总和
  losts:释放失败的次数
  free:可用内存块的数量
*/
struct MEMMAN {		 
	int frees, maxfrees, lostsize, losts;
	struct FREEINFO free[MEMMAN_FREES];
};
unsigned int memtest(unsigned int start, unsigned int end);
void memman_init(struct MEMMAN *man);
unsigned int memman_total(struct MEMMAN *man);
unsigned int memman_alloc(struct MEMMAN *man, unsigned int size);
int memman_free(struct MEMMAN *man, unsigned int addr, unsigned int size);
unsigned int memman_alloc_4k(struct MEMMAN *man, unsigned int size);
int memman_free_4k(struct MEMMAN *man, unsigned int addr, unsigned int size);

/* sheet.c 
多图层管理
*/
#define MAX_SHEETS		256//最多的层数

/*图层定义
SHEET:
     buf:图层内容数据缓冲区
     bxsize:图层的宽(单位是像素)
     bysize:图层的长
     vx0,vy0:图层在界面上的位置
     col_inv:图层的颜色
     height：图层的高度
     flags:图层的各种设定信息

*/
struct SHEET {
	unsigned char *buf;
	int bxsize, bysize, vx0, vy0, col_inv, height, flags;
	struct SHTCTL *ctl;
	struct TASK *task;
};
/*图层管理定义
	vram:显卡缓冲区地址
	map:图层混合后的显示缓冲区
	xsize,ysize:界面的大小(单位是像素)
	top:最上面的图层高度
	sheets0:存储所有的图层信息
	sheets：按顺序描述图层
*/
struct SHTCTL {
	unsigned char *vram, *map;
	int xsize, ysize, top;
	struct SHEET *sheets[MAX_SHEETS];
	struct SHEET sheets0[MAX_SHEETS];
};
struct SHTCTL *shtctl_init(struct MEMMAN *memman, unsigned char *vram, int xsize, int ysize);
struct SHEET *sheet_alloc(struct SHTCTL *ctl);
void sheet_setbuf(struct SHEET *sht, unsigned char *buf, int xsize, int ysize, int col_inv);
void sheet_updown(struct SHEET *sht, int height);
void sheet_refresh(struct SHEET *sht, int bx0, int by0, int bx1, int by1);
void sheet_slide(struct SHEET *sht, int vx0, int vy0);
void sheet_free(struct SHEET *sht);

/* timer.c 
多定时器管理*/
#define MAX_TIMER		500//可用定时器的最大数量

/*
  定时器定义
  next:指向下一个定时器
  timeout：定时器的超时时间
  flags：标记定时器是否被使用
  flags2：用来区分改定时器是否需要在应用程序结束时自动取消
  fifo:定时器中断返回数据时要用的缓冲区
  data:初始数据，用于区分缓冲区中的其他的数据
 */
struct TIMER {
	struct TIMER *next;
	unsigned int timeout;
	char flags, flags2;
	struct FIFO32 *fifo;
	int data;
};
/*定时管理
   count:当前的时间计数
   next:下一个超时的时间
   t0：指向当前的定时器
   timers0:存储定时器数组
  */
struct TIMERCTL {
	unsigned int count, next;
	struct TIMER *t0;
	struct TIMER timers0[MAX_TIMER];
};
extern struct TIMERCTL timerctl;
void init_pit(void);
struct TIMER *timer_alloc(void);
void timer_free(struct TIMER *timer);
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data);
void timer_settime(struct TIMER *timer, unsigned int timeout);
void inthandler20(int *esp);
int timer_cancel(struct TIMER *timer);
void timer_cancelall(struct FIFO32 *fifo);

/* mtask.c 
多任务管理*/
#define MAX_TASKS		1000	/*操作系统可支持的最大任务数*/
#define TASK_GDT0		3		/*定义从GDT的从第几号开始分配给TSS */
#define MAX_TASKS_LV	100     /*每一个任务级别下可有100*/
#define MAX_TASKLEVELS	10      /*将任务可分为10个级别*/
//任务描述段
struct TSS32 {
	int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
	int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
	int es, cs, ss, ds, fs, gs;
	int ldtr, iomap;
};
/*
任务：
sel:用来存放GDT的编号
flags:记录任务状态
level：任务处于哪一个优先级
priority：任务切换的时间间隔

 */
struct TASK {
	int sel, flags;  
	int level, priority;
	struct FIFO32 fifo;
	struct TSS32 tss;
	struct SEGMENT_DESCRIPTOR ldt[2];
	struct CONSOLE *cons;
	int ds_base, cons_stack;
	struct FILEHANDLE *fhandle;
	int *fat;
	char *cmdline;
	unsigned char langmode, langbyte1;
};
/*任务级别描述：
running：正在运行的任务数量
now：记录当前运行的是哪一个任务
tasks：记录当前任务级别下的任务数量
 */
struct TASKLEVEL {
	int running;  
	int now;  
	struct TASK *tasks[MAX_TASKS_LV];
};
/*任务管理描述：
now_lv：现在活动中的级别LEVEL
lv_change:在下次任务切换时是否需要改变LEVEL
*/
struct TASKCTL {
	int now_lv;  
	char lv_change;  
	struct TASKLEVEL level[MAX_TASKLEVELS];
	struct TASK tasks0[MAX_TASKS];
};
extern struct TASKCTL *taskctl;
extern struct TIMER *task_timer;
struct TASK *task_now(void);
struct TASK *task_init(struct MEMMAN *memman);
struct TASK *task_alloc(void);
void task_run(struct TASK *task, int level, int priority);
void task_switch(void);
void task_sleep(struct TASK *task);

/* window.c 
多窗口管理操作
*/
void make_window8(unsigned char *buf, int xsize, int ysize, char *title, char act);
void putfonts8_asc_sht(struct SHEET *sht, int x, int y, int c, int b, char *s, int l);
void make_textbox8(struct SHEET *sht, int x0, int y0, int sx, int sy, int c);
void make_wtitle8(unsigned char *buf, int xsize, char *title, char act);
void change_wtitle8(struct SHEET *sht, char act);

/* console.c 
命令行窗口管理操作
sht:命令行窗口所在的图层
cur_x，cur_y:光标所在图层中的坐标
cur_c:光标显示的颜色
*/
struct CONSOLE {
	struct SHEET *sht;
	int cur_x, cur_y, cur_c;
	struct TIMER *timer;
};
/*文件句柄*/
struct FILEHANDLE {
	char *buf;
	int size;
	int pos;
};
//命令行任务函数
void console_task(struct SHEET *sheet, int memtotal);
//在命令行中输入字符
void cons_putchar(struct CONSOLE *cons, int chr, char move);
//换行处理
void cons_newline(struct CONSOLE *cons);
////在命令行中输入字符串
void cons_putstr0(struct CONSOLE *cons, char *s);
void cons_putstr1(struct CONSOLE *cons, char *s, int l);
//运行输入的命令
void cons_runcmd(char *cmdline, struct CONSOLE *cons, int *fat, int memtotal);
//mem命令处理函数
void cmd_mem(struct CONSOLE *cons, int memtotal);
//cls命令处理函数
void cmd_cls(struct CONSOLE *cons);
//dir命令处理函数
void cmd_dir(struct CONSOLE *cons);
//exit命令处理函数
void cmd_exit(struct CONSOLE *cons, int *fat);
//start命令处理函数：启动一个应用程序时，同时打开一个命令行窗口
void cmd_start(struct CONSOLE *cons, char *cmdline, int memtotal);
//ncst命令处理函数：启动一个应用程序时，不打开一个命令行窗口
void cmd_ncst(struct CONSOLE *cons, char *cmdline, int memtotal);
//langmode命令处理函数
void cmd_langmode(struct CONSOLE *cons, char *cmdline);
//启动应用程序处理函数
int cmd_app(struct CONSOLE *cons, int *fat, char *cmdline);
//系统调用处理函数
int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax);
//
int *inthandler0d(int *esp);
int *inthandler0c(int *esp);
void hrb_api_linewin(struct SHEET *sht, int x0, int y0, int x1, int y1, int col);

/* file.c 
文件管理操作
name:文件名
ext:扩展名
type:文件的属性信息
reserve：保留
time：文件存放的时间
date:文件存放的日期
clustno:簇号
size：文件的大小
*/
struct FILEINFO {
	unsigned char name[8], ext[3], type;
	char reserve[10];
	unsigned short time, date, clustno;
	unsigned int size;
};
void file_readfat(int *fat, unsigned char *img);
void file_loadfile(int clustno, int size, char *buf, int *fat, char *img);
struct FILEINFO *file_search(char *name, struct FILEINFO *finfo, int max);

/* bootpack.c 
界面命令行窗体管理*/
struct TASK *open_constask(struct SHEET *sht, unsigned int memtotal);
struct SHEET *open_console(struct SHTCTL *shtctl, unsigned int memtotal);
