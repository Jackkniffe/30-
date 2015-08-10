/* 鼠标相关 */

#include "bootpack.h"

struct FIFO32 *mousefifo;
int mousedata0;
//鼠标中断处理函数
void inthandler2c(int *esp)
/* 基于PS/2鼠标的中断 */
{
	int data;
	io_out8(PIC1_OCW2, 0x64);	/*  把IRQ-12接收到信号结束的信息通知给PIC1*/
	io_out8(PIC0_OCW2, 0x62);	/*  把IRQ-02接收到信号结束的信息通知给PIC0*/
	data = io_in8(PORT_KEYDAT);
	fifo32_put(mousefifo, data + mousedata0);
	return;
}

#define KEYCMD_SENDTO_MOUSE		0xd4
#define MOUSECMD_ENABLE			0xf4
//使能鼠标
void enable_mouse(struct FIFO32 *fifo, int data0, struct MOUSE_DEC *mdec)
{
	/* 将FIFO缓冲区的信息保存到全局变量里*/
	mousefifo = fifo;
	mousedata0 = data0;
	/* 鼠标有效 */
	wait_KBC_sendready();
	io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
	wait_KBC_sendready();
	io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);
	/* 顺利的话ACK(0xfa)会被发送 */
	mdec->phase = 0; /* 等待鼠标的0xfa阶段 */
	return;
}
//收集鼠标数据
int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat)
{
	/*首先把最初读到的0xfa舍弃掉(0xfa是激活鼠标后，鼠标控制器返回给cpu的应答信息，不是数据所以舍弃)*/
	if (mdec->phase == 0) {
		/* 等待鼠标的0xfa阶段 */
		if (dat == 0xfa) {
			mdec->phase = 1;
		}
		return 0;
	}
	if (mdec->phase == 1) {
		/* 等待鼠标第一个字节的阶段 */
		if ((dat & 0xc8) == 0x08) {
			/* 如果第一个字节正确 ：mdec->buf[0]获得的是鼠标的按键状态 */
			mdec->buf[0] = dat;
			mdec->phase = 2;
		}
		return 0;
	}
	if (mdec->phase == 2) {
		/* 等待鼠标第二个字节的阶段 ：mdec->buf[1]获得的是鼠标的x*/
		mdec->buf[1] = dat;
		mdec->phase = 3;
		return 0;
	}
	if (mdec->phase == 3) {
		/* 等待鼠标第三个字节的阶段 ：mdec->buf[2]获得的是鼠标的y*/
		mdec->buf[2] = dat;
		mdec->phase = 1;
		//解析鼠标数据
		mdec->btn = mdec->buf[0] & 0x07;
		mdec->x = mdec->buf[1];
		mdec->y = mdec->buf[2];
		if ((mdec->buf[0] & 0x10) != 0) {
			mdec->x |= 0xffffff00;
		}
		if ((mdec->buf[0] & 0x20) != 0) {
			mdec->y |= 0xffffff00;
		}
		mdec->y = - mdec->y; /* 鼠标的y方向与画面符号相反*/
		return 1;
	}
	return -1; /* 应该不会到这儿 */
}
