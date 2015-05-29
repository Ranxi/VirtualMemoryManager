#ifndef VMM_H
#define VMM_H

#ifndef DEBUG
#define DEBUG
#endif
#undef DEBUG
#define REQUIRE_HAND
#define PRINT_MEMORY
//#define MULTIPRO
/* 模拟辅存的文件路径 */
#define AUXILIARY_MEMORY "a.txt"		//进程0的辅存空间
#define AUXILIARY_MEMORY1 "b.txt"		//进程1的辅存空间

/* 页面大小（字节）*/
#define PAGE_SIZE 4

/* 虚存空间大小（字节） */
#define VIRTUAL_MEMORY_SIZE (64 * 4)
/* 实存空间大小（字节） */ 
#define ACTUAL_MEMORY_SIZE (32 * 4)
/* 总虚页数 */
#define PAGE_SUM (VIRTUAL_MEMORY_SIZE / PAGE_SIZE)
/* 总物理块数 */
#define BLOCK_SUM (ACTUAL_MEMORY_SIZE / PAGE_SIZE)
/* 二级页表表项数 */
#define PAGE_LEVEL2_SIZE 16
/*pageDir_Size*/
#define PAGE_LEVEL1_SIZE (PAGE_SUM/PAGE_LEVEL2_SIZE)
/*process number*/
#define PROCESS_NUM		2

/* 可读标识位 */
#define READABLE 0x01u
/* 可写标识位 */
#define WRITABLE 0x02u
/* 可执行标识位 */
#define EXECUTABLE 0x04u



/* 定义字节类型 */
#define BYTE unsigned char

typedef enum {
	TRUE = 1, FALSE = 0
} BOOL;



/* 页表项 */
typedef struct
{
	unsigned int pageNum;
	unsigned int blockNum; //物理块号	
	unsigned int pid;//所属进程号
	BOOL filled; //页面装入特征位
	BYTE proType; //页面保护类型
	BOOL edited; //页面修改标识
	unsigned long auxAddr; //外存地址
	unsigned long count; //页面使用计数器
	unsigned long execNo; //使用该页面时全局第几次访存次数,为LRU页面替换算法作支持
} PageTableItem, *Ptr_PageTableItem;


/* 访存请求类型 */
typedef enum { 
	REQUEST_READ, 
	REQUEST_WRITE, 
	REQUEST_EXECUTE 
} MemoryAccessRequestType;

/* 访存请求 */
typedef struct
{
	MemoryAccessRequestType reqType; //访存请求类型
	unsigned int pid;//进程号
	unsigned long virAddr; //虚地址
	BYTE value; //写请求的值
} MemoryAccessRequest, *Ptr_MemoryAccessRequest;


/* 访存错误代码 */
typedef enum {
	ERROR_READ_DENY, //该页不可读
	ERROR_WRITE_DENY, //该页不可写
	ERROR_EXECUTE_DENY, //该页不可执行
	ERROR_PID_NO_MATCH, //PID不匹配
	ERROR_INVALID_REQUEST, //非法请求类型
	ERROR_OVER_BOUNDARY, //地址越界
	ERROR_FILE_OPEN_FAILED, //文件打开失败
	ERROR_FILE_CLOSE_FAILED, //文件关闭失败
	ERROR_FILE_SEEK_FAILED, //文件指针定位失败
	ERROR_FILE_READ_FAILED, //文件读取失败
	ERROR_FILE_WRITE_FAILED //文件写入失败
} ERROR_CODE;

/* 产生访存请求 */
void do_request();

/* 响应访存请求 */
void do_response();

/* 处理缺页中断 */
void do_page_fault(Ptr_PageTableItem);

/* LFU页面替换 */
void do_LFU(Ptr_PageTableItem);

/* 装入页面 */
void do_page_in(Ptr_PageTableItem, unsigned in);

/* 写出页面 */
void do_page_out(Ptr_PageTableItem);

/* 错误处理 */
void do_error(ERROR_CODE);

/* 打印页表相关信息 */
void do_print_info();

/* 打印实存相关内容 */
void do_print_actMem();

/* 打印辅存相关内容*/
void do_print_auxMem();

/* 获取页面保护类型字符串 */
char *get_proType_str(char *, BYTE);


#endif
