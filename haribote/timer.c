/* 定时器相关 */

#include "bootpack.h"

#define PIT_CTRL	0x0043
#define PIT_CNT0	0x0040

struct TIMERCTL timerctl;

#define TIMER_FLAGS_ALLOC		1	/* 已分配 */
#define TIMER_FLAGS_USING		2	/* 正在使用 */
//初始化定时器
void init_pit(void)
{
	int i;
	struct TIMER *t;
	//实例化定时器硬件相关操作，如设置时钟频率
	io_out8(PIT_CTRL, 0x34);
	io_out8(PIT_CNT0, 0x9c);
	io_out8(PIT_CNT0, 0x2e);
	timerctl.count = 0;
	for (i = 0; i < MAX_TIMER; i++) {
		timerctl.timers0[i].flags = 0; /* 没有使用 */
	}
	t = timer_alloc(); /*取得一个定时器，充当哨兵，始终在链表的最后一个*/
	t->timeout = 0xffffffff;
	t->flags = TIMER_FLAGS_USING;
	t->next = 0; /* 末尾*/
	timerctl.t0 = t; /*因为现在只有哨兵，所以在定时器链表的第一个*/
	timerctl.next = 0xffffffff; /*因为现在只有哨兵，所以下一个超时的时刻就是哨兵的时刻*/
	return;
}

//获得一个定时器
struct TIMER *timer_alloc(void)
{
	int i;
	for (i = 0; i < MAX_TIMER; i++) {
		if (timerctl.timers0[i].flags == 0) {
			timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;//标记此定时器已被分配
			timerctl.timers0[i].flags2 = 0;
			return &timerctl.timers0[i];
		}
	}
	return 0; /*没有可用的定时器了*/
}
//回收一个定时器
void timer_free(struct TIMER *timer)
{
	timer->flags = 0; /* 未使用 */
	return;
}
//实例化定时器的缓冲区和数据
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data)
{
	timer->fifo = fifo;
	timer->data = data;
	return;
}
//设置定时器的超时时间
void timer_settime(struct TIMER *timer, unsigned int timeout)
{
	int e;
	struct TIMER *t, *s;
	timer->timeout = timeout + timerctl.count;//多少时间后超时+当前的时间计数=定时器超时的时间
	timer->flags = TIMER_FLAGS_USING;//标记当前定时器被使用
	e = io_load_eflags();//保存标志寄存器
	io_cli();//禁止中断
	t = timerctl.t0;//获得定时器链表的头
	//按照timeout的从小到大的顺序将timer插入链表
	if (timer->timeout <= t->timeout) {
		/* 插入最前面的情况，就是插入到链表的第一的位置 */
		timerctl.t0 = timer;
		timer->next = t; /* 下一个是t */
		timerctl.next = timer->timeout;
		//恢复标志寄存器的值
		io_store_eflags(e);
		return;
	}
	/*寻找插入的位置*/
	for (;;) {
		s = t;
		t = t->next;
		if (timer->timeout <= t->timeout) {
			/* 插入到s和t之间 */
			s->next = timer; /* s的先一个指向timer */
			timer->next = t; /* timer的下一个指向t */
			//恢复标志寄存器的值
			io_store_eflags(e);
			return;
		}
	}
}

//定时器的中断函数
void inthandler20(int *esp)
{
	struct TIMER *timer;
	char ts = 0;
	io_out8(PIC0_OCW2, 0x60);	/* 把IRQ-00接收到的消息通知给PIC */
	timerctl.count++;//时间计数
	if (timerctl.next > timerctl.count) {//比较时间计数是否到达定时时间
		return;
	}
	timer = timerctl.t0; /* 到达定时时间，冲链表中取得到达定时时间的timer */
	for (;;) {
		/* 因为timers的定时器都处于运行状态，所以不确认flags,判断当前的定时器是否已超时 */
		if (timer->timeout > timerctl.count) {
			break;
		}
		/*超时*/
		timer->flags = TIMER_FLAGS_ALLOC;
		//判断是否是切换任务的定时器
		if (timer != task_timer) {
			fifo32_put(timer->fifo, timer->data);
		} else {
			ts = 1; /* task_timer切换任务用的时钟 */
		}
		timer = timer->next; /* 获得定时器链表的下一个timer */
	}
	timerctl.t0 = timer;//标记下一个超时的定时器
	timerctl.next = timer->timeout;
	if (ts != 0) {
		task_switch();//切换任务
	}
	return;
}
//取消一个定时器
int timer_cancel(struct TIMER *timer)
{
	int e;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 在设置过程中禁止改变定时器状态 */
	if (timer->flags == TIMER_FLAGS_USING) {	/* 如果当前的定时器在使用则才取消 */
		if (timer == timerctl.t0) {
			/*如果当前的定时器是即将超时的定时器，即定时器链表的第一个*/
			t = timer->next;
			timerctl.t0 = t;
			timerctl.next = t->timeout;
		} else {
			/* 如果要取消的定时器不在链表的第一个 */
			/* 在链表中搜索timer */
			t = timerctl.t0;
			for (;;) {
				if (t->next == timer) {
					break;
				}
				t = t->next;
			}
			t->next = timer->next; /*  */
		}
		timer->flags = TIMER_FLAGS_ALLOC;
		io_store_eflags(e);
		return 1;	/* 取消定时器成功 */
	}
	io_store_eflags(e);
	return 0; /*定时器没有使用不需求取消*/
}

//取消并回收定时器
void timer_cancelall(struct FIFO32 *fifo)
{
	int e, i;
	struct TIMER *t;
	e = io_load_eflags();
	io_cli();	/* 在设置过程中禁止改变定时器状态 */
	for (i = 0; i < MAX_TIMER; i++) {
		t = &timerctl.timers0[i];
		if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo) {
			timer_cancel(t);
			timer_free(t);
		}
	}
	io_store_eflags(e);
	return;
}
