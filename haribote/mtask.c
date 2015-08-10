/* 任务相关 */

#include "bootpack.h"

struct TASKCTL *taskctl;
struct TIMER *task_timer;

//获得当前正在运行的任务
struct TASK *task_now(void)
{
	struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];
	return tl->tasks[tl->now];
}
//添加一个任务
void task_add(struct TASK *task)
{
	struct TASKLEVEL *tl = &taskctl->level[task->level];
	tl->tasks[tl->running] = task;
	tl->running++;
	task->flags = 2; /* 标志为活动中 */
	return;
}
//删除一个任务
void task_remove(struct TASK *task)
{
	int i;
	struct TASKLEVEL *tl = &taskctl->level[task->level];

	/* 寻找task所在的位置 */
	for (i = 0; i < tl->running; i++) {
		if (tl->tasks[i] == task) {
			/*找到task跳出循环 */
			break;
		}
	}
	//当前级别任务数减一
	tl->running--;
	if (i < tl->now) {
		tl->now--; /* 需要移动成员，要相应的处理 */
	}
	if (tl->now >= tl->running) {
		/* now的值出现异常，则修正 */
		tl->now = 0;
	}
	task->flags = 1; /* 修改任务标志为休眠状态 */

	/* 移动 */
	for (; i < tl->running; i++) {
		tl->tasks[i] = tl->tasks[i + 1];
	}

	return;
}
//重新排列任务的优先级
void task_switchsub(void)
{
	int i;
	/* 寻找最上层的LEVEL */
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		if (taskctl->level[i].running > 0) {
			break; /* 找到优先级较高的任务 */
		}
	}
	taskctl->now_lv = i;
	taskctl->lv_change = 0;
	return;
}
//空闲任务
void task_idle(void)
{
	for (;;) {
		io_hlt();
	}
}
//初始化任务数据结构
struct TASK *task_init(struct MEMMAN *memman)
{
	int i;
	struct TASK *task, *idle;
	struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;//获得全局描述符表地址

	taskctl = (struct TASKCTL *) memman_alloc_4k(memman, sizeof (struct TASKCTL));
	//将任务描述符存入全局描述符表中
	for (i = 0; i < MAX_TASKS; i++) {
		taskctl->tasks0[i].flags = 0;
		taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;//段选择符,用于查找TSS
		taskctl->tasks0[i].tss.ldtr = (TASK_GDT0 + MAX_TASKS + i) * 8;//段选择符，用于查找LDT
		//在全局描述符表中初始化每个任务的TSS描述符
		set_segmdesc(gdt + TASK_GDT0 + i, 103, (int) &taskctl->tasks0[i].tss, AR_TSS32);
		//在全局描述符表中初始化每个任务的LDT描述符
		set_segmdesc(gdt + TASK_GDT0 + MAX_TASKS + i, 15, (int) taskctl->tasks0[i].ldt, AR_LDT);
	}
	for (i = 0; i < MAX_TASKLEVELS; i++) {
		taskctl->level[i].running = 0;
		taskctl->level[i].now = 0;
	}

	task = task_alloc();
	task->flags = 2;	/* 活动中标志 */
	task->priority = 2; /* 0.02秒 */
	task->level = 0;	/* 设置此任务为最高级别 */
	task_add(task);
	task_switchsub();	/* 运行LEVEL设置 */
	load_tr(task->sel);
	task_timer = timer_alloc();
	//设置任务定时器，任务切换的超时时间
	timer_settime(task_timer, task->priority);
	//分配一个空闲任务
	idle = task_alloc();
	idle->tss.esp = memman_alloc_4k(memman, 64 * 1024) + 64 * 1024;
	idle->tss.eip = (int) &task_idle;
	idle->tss.es = 1 * 8;
	idle->tss.cs = 2 * 8;
	idle->tss.ss = 1 * 8;
	idle->tss.ds = 1 * 8;
	idle->tss.fs = 1 * 8;
	//以最低优先级别运行空闲任务
	task_run(idle, MAX_TASKLEVELS - 1, 1);

	return task;
}
//分配一个task
struct TASK *task_alloc(void)
{
	int i;
	struct TASK *task;
	for (i = 0; i < MAX_TASKS; i++) {
		if (taskctl->tasks0[i].flags == 0) {
			task = &taskctl->tasks0[i];
			task->flags = 1; /* 标志正在使用 */
			task->tss.eflags = 0x00000202; /* IF = 1; */
			task->tss.eax = 0; /* 其他寄存器初始化位0 */
			task->tss.ecx = 0;
			task->tss.edx = 0;
			task->tss.ebx = 0;
			task->tss.ebp = 0;
			task->tss.esi = 0;
			task->tss.edi = 0;
			task->tss.es = 0;
			task->tss.ds = 0;
			task->tss.fs = 0;
			task->tss.gs = 0;
			task->tss.iomap = 0x40000000;
			task->tss.ss0 = 0;
			return task;
		}
	}
	return 0; /* 全部task正在使用 */
}

void task_run(struct TASK *task, int level, int priority)
{
	if (level < 0) {
		level = task->level; /* 不改变LEVEL */
	}
	if (priority > 0) {
		task->priority = priority;
	}

	if (task->flags == 2 && task->level != level) { /* 改变活动中的LEVEL */
		task_remove(task); /* 从原来的LEVEL中退出，这里执行之后flag的值会变为1，于是下面的if语句会执行 */
	}
	if (task->flags != 2) {
		/* 冲休眠状态唤醒，并改变LEVEL */
		task->level = level;
		task_add(task);
	}

	taskctl->lv_change = 1; /* 下次任务切换时需要检查LEVEL */
	return;
}

void task_sleep(struct TASK *task)
{
	struct TASK *now_task;
	if (task->flags == 2) {
		/* 如果指定task处于活动状态 */
		now_task = task_now();
		task_remove(task); /*  执行此语句后flag会为1 */
		if (task == now_task) {
			/* 如果让自己休眠，则需要进行任务切换 */
			task_switchsub();//重新排列任务的优先级
			now_task = task_now(); /* 设定后，获得当前的任务*/
			farjmp(0, now_task->sel);//切换到新的任务
		}
	}
	return;
}

//切换任务
void task_switch(void)
{
	struct TASKLEVEL *tl = &taskctl->level[taskctl->now_lv];//获得当前任务同级别的任务数组
	struct TASK *new_task, *now_task = tl->tasks[tl->now];//获得当前的任务
	tl->now++;//指向要切换到的下一个任务
	if (tl->now == tl->running) {
		tl->now = 0;
	}
	//判断是否需要重新排列任务的优先级
	if (taskctl->lv_change != 0) {
		task_switchsub();
		tl = &taskctl->level[taskctl->now_lv];
	}
	//获得新的任务
	new_task = tl->tasks[tl->now];
	//给新的任务设置定时器
	timer_settime(task_timer, new_task->priority);
	if (new_task != now_task) {
		//让cpu切换到新的任务中执行
		farjmp(0, new_task->sel);
	}
	return;
}
