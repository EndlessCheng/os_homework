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

/*
 * GET_INDOS 是否调用系统功能的标志
 * GET_CRIT_ERR
 */
#define GET_INDOS 0x34
#define GET_CRIT_ERR 0x5d06

/*
 *每个线程能执行的时间片
 */
#define TL 8

#define MENU_ITEM 4

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
	unsigned BP, DI, SI, DS, ES, DX, CX, BX, AX, IP, CS, Flags, off, seg;
};

/*
 * 定义函数指针类型
 * 关于函数类型 见http://learn.akae.cn/media/ch23s08.html
 */
typedef int (far *codeptr)(void);

/*
 * 定义函数指针类型，用来保存旧的时钟中断处理函数首地址
 */
void interrupt (*old_int8)();

/*
 * 目前正在运行的线程的内部标识符
 */
int current;

/*
 *线程切换的时候，用来保存线程堆栈地址
 */
unsigned char ss1, sp1, ss2, sp2;

/*
 * 线程已经执行的时间
 */
int timecount = 0;

/*
 *菜单信息
 */
char menu[MENU_ITEM][80] = {
	"A-nter key 1 to run the f1 and f2",
	"B-Enter key 2 to run the producer and customer",
	"C-nter key 3 to passing message bettwen the phread",
	"D-Enter key 0 to exit"
};

/*
 * 记录型信号量
 */
struct semaphore {
	int value;
	struct TCB *wq;
};

/*
 * 商品的信号量
 */
struct semaphore goods;

/*--------------
 *初始化tcb数组
 -----------------*/
void initTCB();

/* func initDOS
 * 初始化DOS环境
 * 获得INDOS标志的地址和严重错误标志的地址
 * 获取系统时钟中断处理函数，并保存在old_int8中
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

/*
 * 线程销毁函数
 * id 线程内部标识符
 */
void destroy(int id);

/*
 * 线程自动撤销函数
 */
void over();

/*
 * 实现在两个线程间的切换
 * 暂时先注释掉void interrupt swtch();
*/

/*
 * 处理因时间片到了的调度
 */
void interrupt new_int8();

/*
 *线程调度
 */
void interrupt my_swtch();

/*
 * 找到一个新的就绪线程
 */
int find();

/*
 * 其他线程是否已经结束
 */
int finished();

/*
 * 对记录型信号量的P操作
 */
void p(struct semaphore *sem);

/*
 * 对记录型信号量的V操作
 */
void v(struct semaphore *sem);

/*
 * 阻塞线程
 */
void block(struct TCB **qp);

/*
 * 唤醒线程
 */
void wakeup_first(struct TCB **qp);

/*--------------
 *打印所有tcb数组
 -----------------*/
void printTCB();

/*
 *两个打印字符的线程
 */
void f1();
void f2();

/*
 * 生产者 函数
 */
void producer();

/*
 * 消费者
 */
void customer();

/*
 * main 函数
 */
int main()
{
	int i, chosen_item;
	initTCB();
	initDOS();

	strcpy(tcb[0].name, "main");
	tcb[0].state = RUNNING;
	current = 0;

	printf("/*-------------*/\n");
	for(i = 0; i < 3; i++)
	{
		printf("%s\n", menu[i]);
	}
	printf("please select a item over: ");
	scanf("%d", &chosen_item);

	switch(chosen_item)
	{
		case 1:
			create("f1", (codeptr)f1, 1024);
			create("f2", (codeptr)f2, 1024);
			break;
		case 2:
			/*
			 *对商品进行初始化
			 */
			goods.value = 0;
			goods.wq = NULL;
			create("producer", (codeptr)producer, 1024);
			create("customer", (codeptr)customer, 1024);
			break;
		case 3:
			break;
		default:
			printf("Please enter the correct choice\n");
			break;
	}

	setvect(8, new_int8);
	my_swtch();
	while(finished() != 1)
	{
		continue;
	}

	memset(tcb[0].name, '\0', NAME_LENGTH);
	tcb[0].state = FINISHED;
	setvect(8, old_int8);
	
	system("pause");
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
		memset(tcb[i].name, '\0', NAME_LENGTH);
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
	/*
	 * 获取系统时钟中断处理函数，并保存在old_int8中
	 */
	old_int8 = getvect(8);

}

int DosBusy()
{
	if (indos_ptr && crit_err_ptr)
	{
		return (*indos_ptr || *crit_err_ptr);
	}
	else
	{
		return 0;
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

	disable();
		for (; i < TCB_MAX_NUM; i++)
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

void destroy(int id)
{
	if (id < 1 || id >= TCB_MAX_NUM)
	{
		printf("Error: There is no pthread %d\n", id);
		return;
	}
	tcb[id].state = FINISHED;
	memset(tcb[id].name, '\0', NAME_LENGTH);
	free(tcb[id].stack);
	tcb[id].stack = NULL;
}

/*
 * 先调用destroy，销毁当前线程，然后调用swtch进行重新调度
 */
void over()
{
	disable();
		destroy(current);
		my_swtch();
	enable();
}

/*void interrupt swtch()
{
	disable();
		ss1 = _SS;
		sp1 = _SP;
		_SS = ss2;
		_SP = sp2;
	enable();
}*/

void interrupt new_int8()
{
	(*old_int8)();
	timecount++;
	/*
	 * 当时间片已经用完而且INT 21H没有执行中,就进行重新调度
	 */
	if (timecount > TL && DosBusy() == 0)
	{
		my_swtch();
	}
}

void interrupt my_swtch()
{
	int ready_pthread;
	disable();
		/*printf("CURRENT pthread is %d ", current);*/
		tcb[current].ss = _SS;
		tcb[current].sp = _SP;
		if (tcb[current].state == RUNNING)
		{
			tcb[current].state = READY;
		}
		ready_pthread = find();
		/*printf(" NOW find a ready pthread %d, gonna to swtch\n", ready_pthread);*/
		_SS = tcb[ready_pthread].ss;
		_SP = tcb[ready_pthread].sp;
		tcb[ready_pthread].state = RUNNING;
		current = ready_pthread;
		timecount = 0;
	enable();
}

int find()
{
	int i, id = -1;
	for(i = current + 1; i < TCB_MAX_NUM; i++)
	{
		if (tcb[i].state == READY)
		{
			id = i;
			break;
		}
	}
	if (id == -1)
	{
		for(i = 0; i < current; i++)
		if (tcb[i].state == READY)
		{
			id = i;
			break;
		}
	}
	if (id == -1)
	{
		return current;
	}
	else
	{
		return id;
	}
}

int finished()
{
	int i = 1;
	for(; i < TCB_MAX_NUM; i++)
	{
		if (tcb[i].state != FINISHED)
		{
			return 0;
		}
	}
	return 1;
}

void p(struct semaphore *sem)
{
	struct TCB **qp;
	disable();
		sem->value = sem->value - 1;
		if (sem->value < 0)
		{
			qp = &(sem->wq);
			block(qp);
			if (*qp == NULL)
			{
				printf("the block qp is NULL in p operation\n");
			}
			else
			{
				printf("the block qp is not NULL in p operation, name is %s \n", (*qp)->name);
			}
		}
	enable();
}

void v(struct semaphore *sem)
{
	struct TCB **qp;
	disable();
		qp = &(sem->wq);
		sem->value = sem->value + 1;
		if (sem->value <= 0)
		{
			printf("start to wakeup_first\n");
			if (*qp == NULL)
			{
				printf("the block qp is NULL in v operation\n");
			}
			else
			{
				printf("the block qp is not NULL in v operation, name is %s \n", (*qp)->name);
			}
			wakeup_first(qp);
			
			printTCB();
		}
	enable();
}

void block(struct TCB **qp)
{
	struct TCB *temp;
	disable();
		temp = *qp;
		printf("the thread named %s has been blocked\n", tcb[current].name);
		tcb[current].state = BLOCKED;
		temp = &(tcb[current]);//TDO
	enable();
	my_swtch();
}

void wakeup_first(struct TCB **qp)
{
	disable();
		if (*qp != NULL)
		{
			printf("has wakeup a thread named %s\n", (*qp)->name);
			(*qp)->state = READY;
			*qp = (*qp)->next;
		}
		else
		{
			printf("the qp is null in the wakeup_first\n");
		}	
	enable();
}

void printTCB()
{
	int i;
	for(i = 0; i < TCB_MAX_NUM; i++)
	{
		printf("\n");
		printf("pthread %d[%s]: state-----%d\n", i, tcb[i].name, tcb[i].state);
	}
}

void f1()
{
	int i, j, k;
	for (i = 0; i < 30; i++)
	{
		printf("A");
		/*
		 * 这个是为了延时，否则打印操作在一个最短的时间片就执行完了，无法体现线程间的调度
		 */
		for(j = 0; j < 10000; j++)
		{
			for(k = 0; k < 10000; k++)
			{
			}
		}
	}
}

void f2()
{
	int i, j, k;
	for (i = 0; i < 30; i++)
	{
		printf("B");
		/*
		 * 同f1中的
		 */
		for(j = 0; j < 10000; j++)
		{
			for(k = 0; k < 10000; k++)
			{
			}
		}
	}
}

void producer()
{
	int i, j, k;
	for(i = 0; i < 10; i++)
	{
		v(&goods);
		printf("producer has produced a good. Now has %d pairs of goods\n", goods.value);
		for(j = 0; j < 10000; j++)
		{
			for(k = 0; k < 10000; k++)
			{
			}
		}
	}
	
}

void customer()
{
	int i, j, k;
	for(i = 0; i < 10; i++)
	{
		p(&goods);
		printf("customer has customed a good. Now has %d pairs of goods\n", goods.value);
		for(j = 0; j < 1000; j++)
		{
			for(k = 0; k < 1000; k++)
			{
			}
		}
	}
}