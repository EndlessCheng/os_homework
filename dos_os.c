#include <dos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* --------------
 *线程状态
 * READY 就绪
 * RUNNING 运行中
 * BLOCKED 阻塞
 * FINISHED 结束
 ---------------*/
#define READY 1
#define RUNNING 2
#define BLOCKED 3
#define FINISHED 4

struct TCB{
	unsigned char *stack; /*堆栈起始地址*/
	unsigned ss; /* 堆栈段址*/
	unsigned sp; /*堆栈指针*/
	char state; /*线程状态*/
	char name[10];
	struct TCB *next;
};

int main()
{
	return 0;
}