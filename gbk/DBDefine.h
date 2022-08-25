#ifndef __DBDEFINE__H__
#define __DBDEFINE__H__

#include "DBPool.h"
#include <type_traits>
#pragma pack(1) 

//需要添加，否则出现test.cpp:64：对‘MYSQLNAMESPACE::DBPool::~DBPool()’未定义的引用
using namespace MYSQLNAMESPACE;

/*
	select时, 数据库想要的字段值.并且类型必须和dbCol的size匹配.
	__attribute__代表属性，pack代表不进行字节对齐，而是按实际的内存进行读取.
*/
struct testDataStruct
{
	uint64_t userId;
	char name[32];
	unsigned char isOk;
} 
#if defined(__linux__) || defined(__linux)  
__attribute__ ((packed));
#else
;
#endif

/*
	insert时， 想要插入数据的结构.
*/
struct insertDataStruct
{
	uint32_t userId;
	char name[32];
	unsigned char isOk;
} 
#if defined(__linux__) || defined(__linux)  
__attribute__ ((packed));
#else
;
#endif

/*
	update时， 想要插入数据的结构.
*/
struct updateDataStruct
{
	char name[32];
	unsigned char isOk;
} 
#if defined(__linux__) || defined(__linux) 
__attribute__((packed));
#else
;
#endif



/*
	select时，想要从dbCol获取数据的结构.类型长度必须和dbCol匹配.
*/
struct testDataStruct1
{
	char camera_uid[50];
	char onvif_url[255];
	char onvif_username[255];
	char onvif_password[255];
} 
#if defined(__linux__) || defined(__linux)  
__attribute__ ((packed));
#else
;
#endif

/*
	insert时， 想要插入数据的结构.
*/
struct testDataStruct2
{
	uint32_t id;
	char camera_uid[50];
	unsigned char position_type;
	char onvif_url[255];
	char onvif_username[255];
	char onvif_password[255];
	char rtsp_url[255];
	char sub_rtsp_url[255];
	char third_rtsp_url[255];
	unsigned char has_third_rtsp;
	char play_rtmp_url[255];
	char play_hls_url[255];
	char play_httpflv_url[255];

	char play_create_time[8];
	char play_expire_time[8];
	char created_at[8];
	char updated_at[8];
	uint32_t retry_count;
	char retry_time[8];
	uint32_t label;
	unsigned char transmit_type;
}
#if defined(__linux__) || defined(__linux)  
__attribute__ ((packed));
#else
;
#endif

//<==test.cpp end==>

struct testfenzu
{
    int count;
    int dep_id;
}
#if defined(__linux__) || defined(__linux)  
__attribute__ ((packed));
#else
;
#endif

struct testConn
{
    long count;                 // 注意32位的long是4字节。test中的字节数也需要对应改成4.
    char city[30];
}
#if defined(__linux__) || defined(__linux) 
__attribute__((packed));
#else
	;
#endif

struct testNeiLian
{
    long count;                 // 注意32位的long是4字节。test中的字节数也需要对应改成4.
    char city[30];
}
#if defined(__linux__) || defined(__linux) 
__attribute__ ((packed));
#else
;
#endif

struct testWaiLian
{
    long beatyId;                 // 注意32位的long是4字节。test中的字节数也需要对应改成4.
    char beatyName[50];
    long boysId;
    char boysName[20];
    long boysuserCP;
}
#if defined(__linux__) || defined(__linux) 
__attribute__ ((packed));
#else
;
#endif

struct testSubQuery
{
    char last_name[25];
    char job_id[10];
    double salary;
}
#if defined(__linux__) || defined(__linux) 
__attribute__ ((packed));
#else
;
#endif

struct testUnion
{
	long employee_id;
	char first_name[20];
	char last_name[25];
    char email[25];

	char phone_number[20];
	char job_id[10];
	double salary;
	float commission_pct;
    // double commission_pct;
    // int commission_pct;

	long manager_id;
	int department_id;
	char hiredate[40];
}
#if defined(__linux__) || defined(__linux) 
__attribute__ ((packed));
#else
;
#endif

#endif