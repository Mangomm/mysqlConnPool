#ifndef __DBDEFINE__H__
#define __DBDEFINE__H__

#include "DBPool.h"
#include <type_traits>
#pragma pack(1) 

//��Ҫ��ӣ��������test.cpp:64���ԡ�MYSQLNAMESPACE::DBPool::~DBPool()��δ���������
using namespace MYSQLNAMESPACE;

/*
	selectʱ, ���ݿ���Ҫ���ֶ�ֵ.�������ͱ����dbCol��sizeƥ��.
	__attribute__�������ԣ�pack���������ֽڶ��룬���ǰ�ʵ�ʵ��ڴ���ж�ȡ.
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
	insertʱ�� ��Ҫ�������ݵĽṹ.
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
	updateʱ�� ��Ҫ�������ݵĽṹ.
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
	selectʱ����Ҫ��dbCol��ȡ���ݵĽṹ.���ͳ��ȱ����dbColƥ��.
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
	insertʱ�� ��Ҫ�������ݵĽṹ.
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
    long count;                 // ע��32λ��long��4�ֽڡ�test�е��ֽ���Ҳ��Ҫ��Ӧ�ĳ�4.
    char city[30];
}
#if defined(__linux__) || defined(__linux) 
__attribute__((packed));
#else
	;
#endif

struct testNeiLian
{
    long count;                 // ע��32λ��long��4�ֽڡ�test�е��ֽ���Ҳ��Ҫ��Ӧ�ĳ�4.
    char city[30];
}
#if defined(__linux__) || defined(__linux) 
__attribute__ ((packed));
#else
;
#endif

struct testWaiLian
{
    long beatyId;                 // ע��32λ��long��4�ֽڡ�test�е��ֽ���Ҳ��Ҫ��Ӧ�ĳ�4.
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