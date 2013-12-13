#include <dos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* --------------
 *线程状态
 * READY 就绪
 * RUNNING 运行中
 * BLOCKED 阻塞
 * FINISHED 结束或者空白
 ---------------*/
#define READY 1
#define RUNNING 2
#define BLOCKED 3
#define FINISHED 4

/*--------------------------
 * TCB_MAX_NUM 线程的最大数量
 ---------------------------*/
#define TCB_MAX_NUM 10

/*--------------------------
 * NAME_LENGTH 线程名字的最大长度
 ---------------------------*/
#define NAME_LENGTH 10

#define GET_INDOS 0x34
#define GET_CRIT_ERR 0x5d06

/*---------------
 * TCB 结构体 表示一个线程
 * stack 堆栈起始地址
 * unsigned ss;  堆栈段址
 * unsigned sp; 堆栈指针
 * char state; 线程状态
 * char name[NAME_LENGTH] 线程的名字
 * struct TCB *next 为了方便形成链表，所设的指向下一个tcb的指针
-----------------*/
struct TCB{
	unsigned char *stack; 
	unsigned ss;
	unsigned sp; 
	char state; 
	char name[NAME_LENGTH];
	struct TCB *next;
};

/*--------------
 *线程数组
 -----------------*/
struct TCB tcb[TCB_MAX_NUM];

/*--------------
 *INDOS 标志的地址
 -----------------*/
char far *indos_ptr = 0;

/*--------------
 *严重错误标志的地址
 -----------------*/
char far *crit_err_ptr = 0;

/*
 * 模拟堆栈
 */
struct int_regs{
	unsigned bp, di, si, ds, es, dx, cs,bx, ax, ip, cs, flags, off, seg;
};

/*
 * 定义函数指针类型
 * 关于函数类型 见http://learn.akae.cn/media/ch23s08.html
 */
typedef int (far *codeptr)(void);

/*--------------
 *初始化tcb数组
 -----------------*/
void initTCB();

/*--------------
 *初始化DOS环境
 *获得INDOS标志的地址和严重错误标志的地址
 -----------------*/
void initDOS();

/*
 * DOS是否处于系统调用中
 */
int DosBusy();

/*
 * 线程创建函数
 * name: 线程的名字
 * code 要执行的代码入口
 * stck_length 新创建的线程的堆栈长度
 * 返回值： 新创建线程的内部标示符，即tcb数组的数组下标
 */
int create(char *name, codeptr code, int stck_length);

/*--------------
 *打印所有tcb数组
 -----------------*/
void printTCB();

int main()
{
	initTCB();
	initDOS();


	return 0;
}

void initTCB()
{
	int i;
	for(i = 0; i < TCB_MAX_NUM; i++)
	{
		tcb[i].stack = NULL;
		tcb[i].ss = NULL;
		tcb[i].sp = NULL;
		tcb[i].state = FINISHED;
		memset(name, '\0', NAME_LENGTH);
		tcb[i].next = NULL;
	}
}

void initDOS()
{
	union REGS regs;
	struct SREGS segregs;

	regs.h.ah = GET_INDOS;
	/*
	 *Turbo C 的库函数， 调用DOS的INT21H中断
	 */
	intdosx(&regs, &regs, &segregs);
	/*
	 *MK_FP 一个宏，获取实际地址
	 */
	indos_ptr = MK_FP(segregs.es, regs.x.bx);
	/*
	 * 获得严重错误标志的地址
	 * _osmajor Turbo C 的全程变量，为DOS版本号的主要部分
	 * _osminor Turbo C 的全程变量，为DOS版本号的次要部分
	 */
	if (_osmajor < 3)
	{
		crit_err_ptr = indos_ptr + 1;
	}
	else if (_osmajor == 3 && _osminor == 0)
	{
		crit_err_ptr = indos_ptr - 1;
	}
	else
	{
		regs.x.ax = GET_CRIT_ERR;
		intdosx(&regs, &regs, &segregs);
		crit_err_ptr = MK_FP(segregs.ds, regs.x.si);
	}

}

int DosBusy()
{
	if (indos_ptr && crit_err_ptr)
	{
		return (*indos_ptr || *crit_err_ptr);
	}
	else
	{
		return -1;
	}
}

int create(char *name, codeptr code, int stck_length)
{
	/*
	 * 0号线程即为主线程，已经被占用,所以从1开始寻找
	 */
	int i = 1;
	/*
	 * id 寻找到的空闲线程的内部标识符
	 */
	int id = -1;
	/*
	 *保存新线程的初始现场信息
	 */
	struct int_regs far *regs;

	disalbe();
		for (i; i < TCB_MAX_NUM; i++)
		{
			if (tcb[i].state == FINISHED)
			{
				id = i;
				break;
			}
		}
		if (id == -1)
		{
			printf("Error Code 1: there is no finished pthread!\n");
			return -1;
		}

		if (strlen(name) > NAME_LENGTH)
		{
			printf("the name of pthread \"%s\" is too long!\n", name);
			return -1;
		}
		
		memcpy(tcb[id].name, name, strlen(name));
		tcb[id].stack = (unsigned char *) malloc(sizeof(unsigned char) * stck_length);
		regs = (struct int_regs *) (tcb[id].stack + stck_length - 1);
		regs --;

		tcb[id].ss = FP_SEG(regs);
		tcb[id].sp = FP_OFF(regs);
		tcb[id].state = READY;

		regs->DS = _DS;
		regs->ES = _ES;
		regs->IP = FP_OFF(code);
		regs->CS = FP_SEG(code);
		regs->Flags = 0x200;
		regs->off = FP_OFF(over);
		regs->seg = FP_SEG(over);
	enable();
	return id;
}

void printTCB()
{
	int i;
	for(i = 0; i < TCB_MAX_NUM; i++)
	{
		printf("pthread %d[%s]: state-----%d\n", i, tcb[i].name, tcb[i].state);
	}
}