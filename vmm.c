#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "vmm.h"

/* 二级页表 */
PageTableItem pageTable[PAGE_LEVEL1_SIZE][PAGE_LEVEL2_SIZE];
/* 实存空间 */
BYTE actMem[ACTUAL_MEMORY_SIZE];
/* 用文件模拟辅存空间 */
FILE *ptr_auxMem;
/* 物理块使用标识 */
BOOL blockStatus[BLOCK_SUM];
/* 访存请求 */
Ptr_MemoryAccessRequest ptr_memAccReq;
/*总访存次数 for LRU*/
long cur_execNo;

/*初始化辅存文件*/
void initFile(){
	int i;
	char *content = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	char buffer[VIRTUAL_MEMORY_SIZE+1];
	ptr_auxMem = fopen(AUXILIARY_MEMORY,"w+");
	/*随机生成256个字符*/
	for(i = 0;i<VIRTUAL_MEMORY_SIZE-5;i++){
		buffer[i] = content[random()%62];
	}
	buffer[VIRTUAL_MEMORY_SIZE-5] = 'L';
	buffer[VIRTUAL_MEMORY_SIZE-4] = 'L';
	buffer[VIRTUAL_MEMORY_SIZE-3] = 'L';
	buffer[VIRTUAL_MEMORY_SIZE-2] = 'B';
	buffer[VIRTUAL_MEMORY_SIZE-1] = 'J';
	buffer[VIRTUAL_MEMORY_SIZE] = '\0';
	/*
	size_t fwrite(const void* buffer,size_t size,size_t count,FILE*stream)
	*/
	fwrite(buffer,sizeof(BYTE),VIRTUAL_MEMORY_SIZE,ptr_auxMem);
	printf("系统提示：初始化辅存文件完成\n");
	fclose(ptr_auxMem);
}

/* 初始化环境 */
void do_init()
{
	int i, j, k = 0;
	srandom(time(NULL));
	for(i = 0;i < PAGE_LEVEL1_SIZE; i++){
		for (j = 0; j < PAGE_LEVEL2_SIZE; j++){
			pageTable[i][j].pageNum = k;
			pageTable[i][j].filled = FALSE;
			pageTable[i][j].edited = FALSE;
			pageTable[i][j].count = 0;
			#ifdef MULTIPRO
			/*随机设定页面所属PID*/
			pageTable[i][j].pid = random() % PROCESS_NUM;
			#endif
			/* 使用随机数设置该页的保护类型 */
			switch (random() % 7){
				case 0:{
					pageTable[i][j].proType = READABLE;
					break;
				}
				case 1:{
					pageTable[i][j].proType = WRITABLE;
					break;
				}
				case 2:{
					pageTable[i][j].proType = EXECUTABLE;
					break;
				}
				case 3:{
					pageTable[i][j].proType = READABLE | WRITABLE;
					break;
				}
				case 4:{
					pageTable[i][j].proType = READABLE | EXECUTABLE;
					break;
				}
				case 5:{
					pageTable[i][j].proType = WRITABLE | EXECUTABLE;
					break;
				}
				case 6:{
					pageTable[i][j].proType = READABLE | WRITABLE | EXECUTABLE;
					break;
				}
				default:
					break;
			}
			/* 设置该页对应的辅存地址 */
			pageTable[i][j].auxAddr = k * PAGE_SIZE  ;
			k++;
		}
	}
	for (k = 0; k < BLOCK_SUM; k++){
		/* 随机选择一些物理块进行页面装入 */
		if (random() % 2 == 0)
		{
			i = k / PAGE_LEVEL2_SIZE;		//页目录号
			j = k % PAGE_LEVEL2_SIZE;		//页号
			do_page_in(&pageTable[i][j], k);
			pageTable[i][j].blockNum = k;
			pageTable[i][j].filled = TRUE;
			blockStatus[k] = TRUE;
		}
		else
			blockStatus[k] = FALSE;
	}
	cur_execNo = 0;
}


/* 响应请求 */
void do_response()
{
	Ptr_PageTableItem ptr_pageTabIt;
	unsigned int pageNum, page_lvl1No, page_lvl2No, offAddr;
	unsigned int actAddr;
	
	/* 检查地址是否越界 */
	if (ptr_memAccReq->virAddr < 0 || ptr_memAccReq->virAddr >= VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_OVER_BOUNDARY);
		return;
	}

	/* 计算页号和页内偏移值 */
	pageNum = ptr_memAccReq->virAddr / PAGE_SIZE;
	page_lvl1No = pageNum / PAGE_LEVEL2_SIZE;		//页目录号
	page_lvl2No = pageNum % PAGE_LEVEL2_SIZE;		//页号
	offAddr = ptr_memAccReq->virAddr % PAGE_SIZE;
	printf("页号为：%u\t页内偏移为：%u\n", pageNum, offAddr);

	/* 获取对应页表项 */
	ptr_pageTabIt = &pageTable[page_lvl1No][page_lvl2No];

	#ifdef MULTIPRO
	/* 检查请求PID与将要访问的页面所属PID是否匹配*/
	if (ptr_memAccReq->pid != ptr_pageTabIt->pid){
		do_error(ERROR_PID_NO_MATCH);
		return;
	}
	#endif
	
	/* 根据特征位决定是否产生缺页中断 */
	if (!ptr_pageTabIt->filled)
	{
		do_page_fault(ptr_pageTabIt);
	}
	
	actAddr = ptr_pageTabIt->blockNum * PAGE_SIZE + offAddr;
	ptr_pageTabIt->execNo = cur_execNo++;
	printf("实地址为：%u\n", actAddr);
	
	/* 检查页面访问权限并处理访存请求 */
	switch (ptr_memAccReq->reqType)
	{
		case REQUEST_READ: //读请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & READABLE)) //页面不可读
			{
				do_error(ERROR_READ_DENY);
				return;
			}
			/* 读取实存中的内容 */
			printf("读操作成功：值为%02X\n", actMem[actAddr]);
			break;
		}
		case REQUEST_WRITE: //写请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & WRITABLE)) //页面不可写
			{
				do_error(ERROR_WRITE_DENY);	
				return;
			}
			/* 向实存中写入请求的内容 */
			actMem[actAddr] = ptr_memAccReq->value;
			ptr_pageTabIt->edited = TRUE;			
			printf("写操作成功\n");
			break;
		}
		case REQUEST_EXECUTE: //执行请求
		{
			ptr_pageTabIt->count++;
			if (!(ptr_pageTabIt->proType & EXECUTABLE)) //页面不可执行
			{
				do_error(ERROR_EXECUTE_DENY);
				return;
			}			
			printf("执行成功\n");
			break;
		}
		default: //非法请求类型
		{	
			do_error(ERROR_INVALID_REQUEST);
			return;
		}
	}
}

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i;
	printf("产生缺页中断，开始进行调页...\n");
	for (i = 0; i < BLOCK_SUM; i++)
	{
		if (!blockStatus[i])
		{
			/* 读辅存内容，写入到实存 */
			do_page_in(ptr_pageTabIt, i);
			
			/* 更新页表内容 */
			ptr_pageTabIt->blockNum = i;
			ptr_pageTabIt->filled = TRUE;
			ptr_pageTabIt->edited = FALSE;
			ptr_pageTabIt->count = 0;
			
			blockStatus[i] = TRUE;
			return;
		}
	}
	/* 没有空闲物理块，进行页面替换 */
	do_LFU(ptr_pageTabIt);
}

/* 根据LRU算法进行页面替换,最近最久未使用法 */
void do_LRU(Ptr_PageTableItem ptr_pageTabIt){
	unsigned int i, j, min, page_i=0, page_j=0;
	printf("没有空闲物理块,开始进行LRU页面替换...\n");
	min = 0xFFFFFFFF;
	for(i = 0;i<PAGE_LEVEL1_SIZE; i++){
		for(j = 0;j<PAGE_LEVEL2_SIZE;j++)
			if(pageTable[page_i][page_j].filled && pageTable[i][j].execNo < min){
				min = pageTable[i][j].execNo;
				page_i = i;
				page_j = j;
			}
	}
	printf("选择第%u页进行替换\n",pageTable[page_i][page_j].pageNum);
	if(pageTable[page_i][page_j].edited){
		//页面内容已改变,写回辅存中
		printf("该页内容有修改,写回至辅存\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;
	pageTable[page_i][page_j].edited = FALSE;

	//从辅存中读取内容,写入实存
	do_page_in(ptr_pageTabIt,pageTable[page_i][page_j].blockNum);

	//更新页表相关信息
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	ptr_pageTabIt->execNo = cur_execNo;
	printf("页面替换成功!\n");
}


/* 根据LFU算法进行页面替换,最近最少使用法 */
void do_LFU(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int i, j, min, page_i=0, page_j=0;
	printf("没有空闲物理块，开始进行LFU页面替换...\n");
	for (i = 0, min = 0xFFFFFFFF; i < PAGE_LEVEL1_SIZE; i++)
	{
		for(j = 0; j < PAGE_LEVEL2_SIZE; j++){
			if (pageTable[i][j].filled && pageTable[i][j].count < min)
			{
				min = pageTable[i][j].count;
				page_i = i;
				page_j = j;
			}
		}
	}
	printf("选择第%u页进行替换\n", pageTable[page_i][page_j].pageNum);
	if (pageTable[page_i][page_j].edited)
	{
		/* 页面内容有修改，需要写回至辅存 */
		printf("该页内容有修改，写回至辅存\n");
		do_page_out(&pageTable[page_i][page_j]);
	}
	pageTable[page_i][page_j].filled = FALSE;
	pageTable[page_i][page_j].count = 0;
	pageTable[page_i][page_j].edited = FALSE;

	/* 读辅存内容，写入到实存 */
	do_page_in(ptr_pageTabIt, pageTable[page_i][page_j].blockNum);
	
	/* 更新页表内容 */
	ptr_pageTabIt->blockNum = pageTable[page_i][page_j].blockNum;
	ptr_pageTabIt->filled = TRUE;
	ptr_pageTabIt->edited = FALSE;
	ptr_pageTabIt->count = 0;
	printf("页面替换成功\n");
}

/* 将辅存内容写入实存 */
void do_page_in(Ptr_PageTableItem ptr_pageTabIt, unsigned int blockNum)
{
	unsigned int readNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%lu\tftell=%ld\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(actMem + blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%lu\tftell=%ld\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: blockNum=%u\treadNum=%u\n", blockNum, readNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("调页成功：辅存地址%lu-->>物理块%u\n", ptr_pageTabIt->auxAddr, blockNum);
}

/* 将被替换页面的内容写回辅存 */
void do_page_out(Ptr_PageTableItem ptr_pageTabIt)
{
	unsigned int writeNum;
	if (fseek(ptr_auxMem, ptr_pageTabIt->auxAddr, SEEK_SET) < 0)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%lu\tftell=%ld\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
#endif
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((writeNum = fwrite(actMem + ptr_pageTabIt->blockNum * PAGE_SIZE, 
		sizeof(BYTE), PAGE_SIZE, ptr_auxMem)) < PAGE_SIZE)
	{
#ifdef DEBUG
		printf("DEBUG: auxAddr=%lu\tftell=%ld\n", ptr_pageTabIt->auxAddr, ftell(ptr_auxMem));
		printf("DEBUG: writeNum=%u\n", writeNum);
		printf("DEGUB: feof=%d\tferror=%d\n", feof(ptr_auxMem), ferror(ptr_auxMem));
#endif
		do_error(ERROR_FILE_WRITE_FAILED);
		exit(1);
	}
	printf("写回成功：物理块%u-->>辅存地址%lu\n",ptr_pageTabIt->blockNum, ptr_pageTabIt->auxAddr);
}

/* 错误处理 */
void do_error(ERROR_CODE code)
{
	switch (code)
	{
		case ERROR_READ_DENY:
		{
			printf("访存失败：该地址内容不可读\n");
			break;
		}
		case ERROR_WRITE_DENY:
		{
			printf("访存失败：该地址内容不可写\n");
			break;
		}
		case ERROR_EXECUTE_DENY:
		{
			printf("访存失败：该地址内容不可执行\n");
			break;
		}
		case ERROR_PID_NO_MATCH:{
			printf("访存失败：该页面PID不匹配\n");
			break;
		}
		case ERROR_INVALID_REQUEST:
		{
			printf("访存失败：非法访存请求\n");
			break;
		}
		case ERROR_OVER_BOUNDARY:
		{
			printf("访存失败：地址越界\n");
			break;
		}
		case ERROR_FILE_OPEN_FAILED:
		{
			printf("系统错误：打开文件失败\n");
			break;
		}
		case ERROR_FILE_CLOSE_FAILED:
		{
			printf("系统错误：关闭文件失败\n");
			break;
		}
		case ERROR_FILE_SEEK_FAILED:
		{
			printf("系统错误：文件指针定位失败\n");
			break;
		}
		case ERROR_FILE_READ_FAILED:
		{
			printf("系统错误：读取文件失败\n");
			break;
		}
		case ERROR_FILE_WRITE_FAILED:
		{
			printf("系统错误：写入文件失败\n");
			break;
		}
		default:
		{
			printf("未知错误：没有这个错误代码\n");
		}
	}
}

/* 产生访存请求 */
void do_request()
{
	/* 随机产生请求地址 */
	ptr_memAccReq->virAddr = random() % VIRTUAL_MEMORY_SIZE;
	/*process number*/
	ptr_memAccReq->pid = random() % PROCESS_NUM;
	/* 随机产生请求类型 */
	switch (random() % 3)
	{
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%lu\t类型：读取\tPID:%d\n", ptr_memAccReq->virAddr,ptr_memAccReq->pid);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 随机产生待写入的值 */
			ptr_memAccReq->value = random() % 0xFFu;
			printf("产生请求：\n地址：%lu\t类型：写入\t值：%02X\tPID:%d\n", ptr_memAccReq->virAddr, ptr_memAccReq->value,ptr_memAccReq->pid);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%lu\t类型：执行\tPID:%d\n", ptr_memAccReq->virAddr,ptr_memAccReq->pid);
			break;
		}
		default:
			break;
	}	
}

#ifdef REQUIRE_HAND
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
		printf("地址或进程号非法!\n");
		scanf("%d %d",&virtAddr,&pid);
	}
	ptr_memAccReq->virAddr = virtAddr;
	ptr_memAccReq->pid = pid;
	/* 输入请求类型 */
	printf("请输入请求类型(0-读请求,1-写请求,2-执行请求)：");
	scanf("%d",&reqtype);
	switch (reqtype){
		case 0: //读请求
		{
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("产生请求：\n地址：%lu\t类型：读取\n", ptr_memAccReq->virAddr);
			break;
		}
		case 1: //写请求
		{
			ptr_memAccReq->reqType = REQUEST_WRITE;
			/* 输入待写入的值 */
			printf("请输入待写入的值:");
			scanf("%d",&writeValue);
			ptr_memAccReq->value = writeValue % 0xFFu;
			printf("产生请求：\n地址：%lu\t类型：写入\t值：%02X\n", ptr_memAccReq->virAddr, ptr_memAccReq->value);
			break;
		}
		case 2:
		{
			ptr_memAccReq->reqType = REQUEST_EXECUTE;
			printf("产生请求：\n地址：%lu\t类型：执行\n", ptr_memAccReq->virAddr);
			break;
		}
		default:
			ptr_memAccReq->reqType = REQUEST_READ;
			printf("请求类型错误，将按照默认类型(读类型)\n");
			break;
	}	
}
#endif

#ifdef PRINT_MEMORY
/* prin actMem */
void do_print_actMem()
{
	int i,j,k;
	printf("物理块号\t内容\t\n");
	for(i=0,k=0;i<BLOCK_SUM;i++){
		printf("%d\t",i);
		for(j=0;j<PAGE_SIZE;j++){
			printf("%c",actMem[k++]);
		}
		printf("\n");
	}
}

/*print auxmem*/
void do_print_auxMem()
{
	int i,j,k,readNum;
	BYTE temp[VIRTUAL_MEMORY_SIZE];
	if (fseek(ptr_auxMem, 0, SEEK_SET) < 0)//read from the begining
	{
		do_error(ERROR_FILE_SEEK_FAILED);
		exit(1);
	}
	if ((readNum = fread(temp, 
		sizeof(BYTE), VIRTUAL_MEMORY_SIZE, ptr_auxMem)) < VIRTUAL_MEMORY_SIZE)
	{
		do_error(ERROR_FILE_READ_FAILED);
		exit(1);
	}
	printf("页号\t内容\t\n");
	for(i=0,k=0;i<PAGE_SUM;i++)
	{
		printf("%d\t",i);
		for(j=0;j<PAGE_SIZE;j++){
			printf("%c",temp[k++]);
		}
		printf("\n");
	}
}
#endif

/* 打印页表 */
void do_print_info()
{
	unsigned int i, j, k;
	char str[4];
	printf("页号\t块号\t装入\t修改\t保护\t计数\t辅存\tPID\n");
	for (i = 0; i < PAGE_LEVEL1_SIZE; i++){
		for(j = 0;j < PAGE_LEVEL2_SIZE; j++)
			printf("%u\t%u\t%u\t%u\t%s\t%lu\t%lu\t%u\n", pageTable[i][j].pageNum, pageTable[i][j].blockNum, pageTable[i][j].filled, 
				pageTable[i][j].edited, get_proType_str(str, pageTable[i][j].proType), 
				pageTable[i][j].count, pageTable[i][j].auxAddr,pageTable[i][j].pid);
	}
}

/* 获取页面保护类型字符串 */
char *get_proType_str(char *str, BYTE type)
{
	if (type & READABLE)
		str[0] = 'r';
	else
		str[0] = '-';
	if (type & WRITABLE)
		str[1] = 'w';
	else
		str[1] = '-';
	if (type & EXECUTABLE)
		str[2] = 'x';
	else
		str[2] = '-';
	str[3] = '\0';
	return str;
}

int main(int argc, char* argv[])
{
	char c;
	int i;
	initFile();
	if (!(ptr_auxMem = fopen(AUXILIARY_MEMORY, "r+")))
	{
		do_error(ERROR_FILE_OPEN_FAILED);
		exit(1);
	}
	
	do_init();
	do_print_info();
	ptr_memAccReq = (Ptr_MemoryAccessRequest) malloc(sizeof(MemoryAccessRequest));
	/* 在循环中模拟访存请求与处理过程 */
	while (TRUE)
	{
		printf("按H将由手动输入访存请求,按其他键由程序自动生成请求...\n");
		c = getchar();
		if(c == 'h'|| c == 'H')
			do_input_request();
		else
			do_request();
		while (c != '\n')
			c = getchar();
		do_response();
		printf("按Y打印页表，按A键打印辅存，按P键打印实存,按其他键不打印...\n");
		//while(c == '\n')
			c = getchar();
		if (c  == 'y' || c == 'Y')
			do_print_info();
		else if (c == 'A' || c == 'a')
		{
			printf("you print A/a!");
			do_print_auxMem();
		}
		else if (c == 'P' || c == 'p')
			do_print_actMem();
		while (c != '\n')
			c = getchar();
		printf("按X退出程序，按其他键继续...\n");
		if ((c = getchar()) == 'x' || c == 'X')
			break;
		while (c != '\n')
			c = getchar();
		//sleep(5000);
	}

	if (fclose(ptr_auxMem) == EOF)
	{
		do_error(ERROR_FILE_CLOSE_FAILED);
		exit(1);
	}
	return (0);
}
