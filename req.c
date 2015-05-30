#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include "vmm.h"

MemoryAccessRequest req;

/* 产生访存请求 */
void do_request()
{
	/* 随机产生请求地址 */
	req.virAddr = random() % VIRTUAL_MEMORY_SIZE;
	/*process number*/
	req.pid = random() % PROCESS_NUM;
	/* 随机产生请求类型 */
	switch (random() % 3)
	{
		case 0: //读请求
		{
			req.reqType = REQUEST_READ;
			printf("随机产生请求：\n地址：%lu\t类型：读取\tPID:%d\n", req.virAddr,req.pid);
			break;
		}
		case 1: //写请求
		{
			req.reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			req.value = random() % 0xFFu;
			printf("随机产生请求：\n地址：%lu\t类型：写入\t值：%02X\tPID:%d\n", req.virAddr, req.value,req.pid);
			break;
		}
		case 2:
		{
			req.reqType = REQUEST_EXECUTE;
			printf("随机产生请求：\n地址：%lu\t类型：执行\tPID:%d\n", req.virAddr,req.pid);
			break;
		}
		default:
			break;
	}	
}

/* 手动产生访存请求 */
void do_input_request()
{
	unsigned int virtAddr = 0;
	unsigned int pid = 0;
	unsigned int reqtype = 0;
	unsigned int writeValue = 0;

	/* 输入请求地址 */
	printf("请输入请求地址(0-255)与进程号,空格隔开：\n");
	scanf("%d %d",&virtAddr,&pid);
	while(virtAddr < 0 || virtAddr > 255|| pid < 0 || pid > PROCESS_NUM){
		printf("地址或进程号非法,请再次输入!\n");
		scanf("%d %d",&virtAddr,&pid);
	}
	req.virAddr = virtAddr;
	req.pid = pid;
	/* 输入请求类型 */
	printf("请输入请求类型(0-读请求,1-写请求,2-执行请求)：");
	scanf("%d",&reqtype);
	switch (reqtype){
		case 0: //读请求
		{
			req.reqType = REQUEST_READ;
			printf("输入产生的请求：\n地址：%lu\t类型：读取\tPID:%u\n", req.virAddr,pid);
			break;
		}
		case 1: //写请求
		{
			req.reqType = REQUEST_WRITE;
			/* 输入待写入的值 */
			printf("请输入待写入的值:");
			scanf("%d",&writeValue);
			req.value = writeValue % 0xFFu;
			printf("输入产生的请求：\n地址：%lu\t类型：写入\t值：%02X\tPID:%u\n", req.virAddr, req.value,pid);
			break;
		}
		case 2:
		{
			req.reqType = REQUEST_EXECUTE;
			printf("输入产生的请求：\n地址：%lu\t类型：执行\tPID:%u\n", req.virAddr,pid);
			break;
		}
		default:
			req.reqType = REQUEST_READ;
			printf("请求类型错误，将按照默认类型(读类型)产生请求!\n");
			printf("输入产生的请求：\n地址：%lu\t类型：读取\tPID:%u\n", req.virAddr,pid);
			break;
	}	
}



int main(int argc,char *argv[]){

	int fd;
	char c;

	srandom(time(NULL));
	
	while(TRUE){
		printf("按H将由手动输入访存请求,按X退出,按其他键由程序自动生成请求...\n");
		c = getchar();
		if(c == 'h'|| c == 'H'){
			do_input_request();
		}
		else if(c =='x'|| c == 'X'){
			break;
		}
		else{
			do_request();
		}
		while (c != '\n'){
			c = getchar();
		}
		if((fd = open(FIFO_FILE,O_WRONLY))<0){
			printf("Gen Request open fifo failed!\n");
		}
		if(write(fd,&req,REQ_DATALEN)<0){
			printf("Gen Request write failed!\n");
		}
		close(fd);
	}
	
}

/*if((fd=open("/tmp/server",O_WRONLY))<0)
			error_sys("enq open fifo failed");

		if(write(fd,&enqcmd,DATALEN)<0)
			error_sys("enq write failed");

		close(fd);*/
