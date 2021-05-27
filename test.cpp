#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "DBPool.h"
#include "DBDefine.h"

using namespace MYSQLNAMESPACE;

bool bExit = 0;
void signal_ctrlc(int sig)
{
//	std::cout<<"get signal:"<<sig<<std::endl;
	bExit = 1;
}

void* SelectFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    
    /*
        select注意点：
        1. 要select的字段，不一定写完数据库的所有字段.
        2. 并且dbCol的size可以大于或者等于数据库的类型所占字节长度.但dbCol的类型长度必须与testDataStruct一样，否则读取时会发生字节不对齐的问题(已测试).
        3. 并且，dbCol在内存中的大小我们不需要关心，我们只需要关心成员3(dbCol.size)即可，因为存储数据库的实际内存是new出来的(execSelect)或者数组(execSelectLimit),
            存储实际数据的内存布局最终和testDataStruct一样，这样就能确保完整无误的读取到数据，这也是testDataStruct使用__attribute__关键字的原因.
    */
    const dbCol column[] = 
	{
		{"USERID",  DB_DATA_TYPE::DB_ULONG,     8, NULL},
		{"MYNAME",  DB_DATA_TYPE::DB_STR,       32, NULL},
		{"ISOK",    DB_DATA_TYPE::DB_UCHAR,     1, NULL},
		{NULL ,     DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

	// std::cout<<"struct :"<<sizeof(testDataStruct)<<std::endl;//41
    // std::cout<<"pthread :"<<pthread_self()<<" get Handle:"<<id<<"......operate db in here!"<<std::endl;

    uint32_t retCount = 0;

//#define SELECT_LIMIT
#ifdef SELECT_LIMIT
#define SELECT_COUNT 10
	testDataStruct data[SELECT_COUNT];
	memset(data, 0x00, sizeof(data));
	retCount = dbPool->execSelectLimit(id, column, "USERINFO", NULL, NULL, SELECT_COUNT, (unsigned char *)data);
#else
    testDataStruct *data = NULL;
    retCount = dbPool->execSelect(id, column, "USERINFO", NULL, NULL, (unsigned char**)&data);
#endif
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
    }
    else
    {
#ifdef SELECT_LIMIT
		for (uint32_t i = 0; i < SELECT_COUNT; ++i)
		{
            std::cout<<"li="<<i<<std::endl;
			std::cout<<"userId: " << data[i].userId << std::endl;
			std::cout<<"name: " << data[i].name << std::endl;
			std::cout<<"isOk: " << data[i].isOk << std::endl;
		}
#else
        for (uint32_t i = 0; i < retCount; ++i)
		{
			std::cout << "userId: " << data[i].userId << std::endl;
			std::cout << "name: " << data[i].name << std::endl;
			std::cout << "isOk: " << data[i].isOk << std::endl;
		}
        if(NULL != data){
            delete [] data;
            data = NULL;
        }
#endif
    }

    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

void* InsertFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg)
    {
        std::cout<<"DBPool InsertFunc is null！！！"<<std::endl;
        return NULL;
    }

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->getConn();


    /*
        insert注意点：
        1. 插入时与DBDefine中的数据结构没有关系,因为只需要插入即可，不需要读取,但是若直接传字符串给data作为插
         入值的话，转成部分无符号时就会出问题，造成插入失败但程序不崩溃,所以我们仍需要在DBDefine中自定义插入的数据类型.
        例如下面是错误写法:
    char c = '1';
    const dbCol colum[] = 
    {
        {"USERID", DB_DATA_TYPE::DB_ULONG,   8,  (unsigned char *)"33"},
        {"MYNAME", DB_DATA_TYPE::DB_STR,     32, (unsigned char *)"tyy"},
        {"ISOK",   DB_DATA_TYPE::DB_CHAR,    1,  (unsigned char *)&c},   //字符不能直接传临时对象，否则该字符常量(注意不是字符串)会在系统内存地址，造成非法访问程序崩溃.
        {NULL,     DB_DATA_TYPE::DB_INVALID, 0,   NULL}
    };
    std::cout<<"dbCol sizeof: "<<sizeof(dbCol)<<"DB_DATA_TYPE "<<sizeof(DB_DATA_TYPE)<<std::endl;
    */

    //正确写法
    int insertIndex = 0;
    srand(time(NULL));
    insertIndex = (rand() % 100) + 1;

	insertDataStruct temp;
    /*
        2. insertDataStruct作为传进的数据，尽量不要大于dbCol的数据长度，否则插入时会造成数据丢失，并且也不要小于dbCol的数据长度，不然调用
            mysql_real_escape_string(_mysql, strData, (const char*&)tmp->data, tmp->size)时，由于tmp->size比data大，导致拷贝超出了data的实际长度，sql语句就会不匹配。
        3. 若传进的tmp->data字符串长度大于tmp->size，会造成数据丢失.并且dbCol的size不能大于数据库的长度，否则插入会越界，可用mysql_error查看错误,会出现row out of等字眼.

        总结：也就说，插入时，dbCol尽量与数据库类型的长度保持一致，避免插入数据库时越界；然后传进时的数据insertDataStruct也尽量与dbCol一致，避免数据丢失和sql语句不匹配。
            所以定义时，先看数据库去定义dbCol的类型和大小，然后再定义insertDataStruct的大小.
    */
	memset(&temp, 0x00, sizeof(temp));
	temp.userId = insertIndex++;
	snprintf(temp.name, sizeof(temp.name), "tyy%u", temp.userId);//%d 有符号32位整数. %u 无符号32位整数. %lld 有符号64位整数. %llx有符号64位16进制整数. ll代表long long.
	temp.isOk = '1';
	const dbCol colum[] = 
	{
		{"USERID",  DB_DATA_TYPE::DB_UINT,      4, (unsigned char *)(&(temp.userId))},
		{"MYNAME",  DB_DATA_TYPE::DB_STR,       32, (unsigned char *)(temp.name)},
		{"ISOK",    DB_DATA_TYPE::DB_UCHAR,     1, (unsigned char *)(&(temp.isOk))},
		{NULL ,     DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

    if(dbPool->execInsert(id, "USERINFO", colum) == (uint32_t)-1){
        std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
        dbPool->releaseConn(id);
        return (void*)-1;//退出之前必须使连接变为空闲可用
    }

    std::cout<<"tid= "<<pthread_self()<<" insert success."<<std::endl;
    sleep(2);
    dbPool->releaseConn(id);
    
    return NULL;
}

void* updateFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg)
    {
        std::cout<<"DBPool updateFunc is null！！！"<<std::endl;
        return NULL;
    }

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->getConn();

    //构建方法类似插入
    updateDataStruct tmp;
    memset(tmp.name, 0x0, sizeof(tmp.name));
    //snprintf(tmp.name, sizeof(tmp.name), "tyy%llu", 0);
    memcpy(tmp.name, "tyy0", sizeof(tmp.name));
    tmp.isOk = '0';
    std::string w;
    w = "id=13";

    /*
        update注意点：
            1. char*与unsigned char*的字符串转换没有影响.例如"123".
            2. char类型是一个变量，需要传地址，而name数组名本身就是一个地址.
            3. 在传where时，我遇到一个错误，就是参数直接传字符串例如"id = 1"，
                然后update时，sql<<where报错，并且cout<<where<<endl;也报错，目前没复现没找到是啥情况，不过改了部分代码结果没报错了。
                所以这里最好建议大家传临时变量，而不直接传地址.
    */

    const dbCol colum[] = 
    {
        {"MYNAME", DB_DATA_TYPE::DB_STR,     32, (unsigned char *)tmp.name}, //char*与unsigned char*的字符串转换没有影响
        {"ISOK",   DB_DATA_TYPE::DB_CHAR,    1,  (unsigned char *)&tmp.isOk},//char类型是一个变量，需要传地址，而name数组名本身就是一个地址
        {NULL,     DB_DATA_TYPE::DB_INVALID, 0,  NULL}
    };

    if(dbPool->execUpdate(id, "USERINFO", colum, "id = 1") == (uint32_t)-1){
    //if(dbPool->execUpdate(id, "USERINFO", colum, w.c_str()) == -1){//最好用这种传参
        dbPool->releaseConn(id);
        return (void*)-1;
    }

    std::cout<<"update success"<<std::endl;
    sleep(2);
    dbPool->releaseConn(id);
    
    return NULL;
}

void* deleteFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg)
    {
        std::cout<<"DBPool deleteFunc is null！！！"<<std::endl;
        return NULL;
    }

    /*
        delete注意点：
            没有遇到过错误，比较简单.
    */

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->getConn();

    if((uint32_t)-1 == dbPool->execDelete(id, "USERINFO", "id = 10")){
        std::cout<<"execDelete failed."<<std::endl;
        dbPool->releaseConn(id);
        return (void*)-1;
    }

    std::cout<<"execDelete success"<<std::endl;
    sleep(2);
    dbPool->releaseConn(id);

    return NULL;    
}

/* SelectFunc1和InsertFunc1只是我自己代码的测试，大家不需要理会，只需要看上面四个线程的例子即可. */
void* SelectFunc1(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    //pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    
    /*
        select注意点：
        1. 要select的字段，不一定写完数据库的所有字段.
        2. 并且dbCol的size可以大于或者等于数据库的类型所占字节长度.但dbCol的类型长度必须与testDataStruct一样，否则读取时会发生字节不对齐的问题(已测试).
        3. 并且，dbCol在内存中的大小我们不需要关心，我们只需要关心成员3(dbCol.size)即可，因为存储数据库的实际内存是new出来的(execSelect)或者数组(execSelectLimit),
            存储实际数据的内存布局最终和testDataStruct一样，这样就能确保完整无误的读取到数据，这也是testDataStruct使用__attribute__关键字的原因.
    */
    const dbCol column[] = 
	{
		{"camera_uid",          DB_DATA_TYPE::DB_STR,     50,  NULL},
		{"onvif_url",           DB_DATA_TYPE::DB_STR,     255, NULL},
		{"onvif_username",      DB_DATA_TYPE::DB_STR,     255, NULL},
        {"onvif_password",      DB_DATA_TYPE::DB_STR,     255, NULL},

		{NULL ,                 DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

	// std::cout<<"struct :"<<sizeof(testDataStruct)<<std::endl;//41
    // std::cout<<"pthread :"<<pthread_self()<<" get Handle:"<<id<<"......operate db in here!"<<std::endl;

    uint32_t retCount = 0;

//#define SELECT_LIMIT
#ifdef SELECT_LIMIT
#define SELECT_COUNT 10
	testDataStruct data[SELECT_COUNT];
	memset(data, 0x00, sizeof(data));
	retCount = dbPool->execSelectLimit(id, column, "USERINFO", NULL, NULL, SELECT_COUNT, (unsigned char *)data);
#else
    testDataStruct1 *data = NULL;
    retCount = dbPool->execSelect(id, column, "vg_ms_push_info", NULL, NULL, (unsigned char**)&data);
#endif
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
    }
    else
    {
#ifdef SELECT_LIMIT
		for (uint32_t i = 0; i < SELECT_COUNT; ++i)
		{
            std::cout<<"li="<<i<<std::endl;
			std::cout<<"userId: " << data[i].userId << std::endl;
			std::cout<<"name: " << data[i].name << std::endl;
			std::cout<<"isOk: " << data[i].isOk << std::endl;
		}
#else
        for (uint32_t i = 0; i < retCount; ++i)
		{
			std::cout << "camera_uid: " << data[i].camera_uid << std::endl;
			std::cout << "onvif_url: " << data[i].onvif_url << std::endl;
			std::cout << "onvif_username: " << data[i].onvif_username << std::endl;
            std::cout << "onvif_password: " << data[i].onvif_password << std::endl;
		}
        if(NULL != data){
            delete [] data;
            data = NULL;
        }
#endif
    }

    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

void* InsertFunc1(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg)
    {
        std::cout<<"DBPool InsertFunc is null！！！"<<std::endl;
        return NULL;
    }

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->getConn();


    /*
        insert注意点：
        1. 插入时与DBDefine中的数据结构没有关系,因为只需要插入即可，不需要读取,但是若直接传字符串给data作为插
         入值的话，转成部分无符号时就会出问题，造成插入失败但程序不崩溃,所以我们仍需要在DBDefine中自定义插入的数据类型.
        例如下面是错误写法:
    char c = '1';
    const dbCol colum[] = 
    {
        {"USERID", DB_DATA_TYPE::DB_ULONG,   8,  (unsigned char *)"33"},
        {"MYNAME", DB_DATA_TYPE::DB_STR,     32, (unsigned char *)"tyy"},
        {"ISOK",   DB_DATA_TYPE::DB_CHAR,    1,  (unsigned char *)&c},   //字符不能直接传临时对象，否则该字符常量(注意不是字符串)会在系统内存地址，造成非法访问程序崩溃.
        {NULL,     DB_DATA_TYPE::DB_INVALID, 0,   NULL}
    };
    std::cout<<"dbCol sizeof: "<<sizeof(dbCol)<<"DB_DATA_TYPE "<<sizeof(DB_DATA_TYPE)<<std::endl;
    */

    //正确写法
    int insertIndex = 0;
    srand(time(NULL));
    insertIndex = (rand() % 100) + 1;

	testDataStruct2 temp;
    /*
        2. insertDataStruct作为传进的数据，尽量不要大于dbCol的数据长度，否则插入时会造成数据丢失，并且也不要小于dbCol的数据长度，不然调用
            mysql_real_escape_string(_mysql, strData, (const char*&)tmp->data, tmp->size)时，由于tmp->size比data大，导致拷贝超出了data的实际长度，sql语句就会不匹配。
        3. 若传进的tmp->data字符串长度大于tmp->size，会造成数据丢失.并且dbCol的size不能大于数据库的长度，否则插入会越界，可用mysql_error查看错误,会出现row out of等字眼.

        总结：也就说，插入时，dbCol尽量与数据库类型的长度保持一致，避免插入数据库时越界；然后传进时的数据insertDataStruct也尽量与dbCol一致，避免数据丢失和sql语句不匹配。
            所以定义时，先看数据库去定义dbCol的类型和大小，然后再定义insertDataStruct的大小.
    */
	memset(&temp, 0x00, sizeof(temp));
	temp.id = 2;
	snprintf(temp.camera_uid, sizeof(temp.camera_uid), "66e");
	temp.position_type = '1';
    snprintf(temp.onvif_url, sizeof(temp.onvif_url), "tyy%u", temp.id);
    snprintf(temp.onvif_username, sizeof(temp.onvif_username), "tyy%u", temp.id);
    snprintf(temp.onvif_password, sizeof(temp.onvif_password), "tyy%u", temp.id);
    snprintf(temp.rtsp_url, sizeof(temp.rtsp_url), "tyy%u", temp.id);
    snprintf(temp.sub_rtsp_url, sizeof(temp.sub_rtsp_url), "tyy%u", temp.id);
    snprintf(temp.third_rtsp_url, sizeof(temp.third_rtsp_url), "tyy%u", temp.id);
    temp.has_third_rtsp = '1';

    snprintf(temp.play_rtmp_url, sizeof(temp.play_rtmp_url), "tyy%d", 1);
    snprintf(temp.play_hls_url, sizeof(temp.play_hls_url), "tyy%d", 2);
    snprintf(temp.play_httpflv_url, sizeof(temp.play_httpflv_url), "tyy%d", 3);

    // snprintf(temp.play_create_time, sizeof(temp.play_create_time), "NULL", temp.id);
    // snprintf(temp.play_expire_time, sizeof(temp.play_expire_time), "tyy%lu", temp.id);
    // snprintf(temp.created_at, sizeof(temp.created_at), "tyy%lu", temp.id);
    // snprintf(temp.updated_at, sizeof(temp.updated_at), "tyy%lu", temp.id);
    char nu[4] = {0};
    strncpy(nu, "NULL", 4);
    std::cout<<"NU"<<nu<<std::endl;
    memcpy(temp.play_create_time, nu, 4);
    memcpy(temp.play_expire_time, nu, 4);
    memcpy(temp.created_at, nu, 4);
    memcpy(temp.updated_at, nu, 4);

    temp.retry_count = 21;
    // snprintf(temp.retry_time, sizeof(temp.retry_time), "tyy%lu", temp.id);
    memcpy(temp.retry_time, nu, 4);
    temp.label = 21;
    temp.transmit_type = '1';
	const dbCol colum[] = 
	{
		{"id",                  DB_DATA_TYPE::DB_UINT,      4, (unsigned char *)(&(temp.id))},
		{"camera_uid",          DB_DATA_TYPE::DB_STR,       50, (unsigned char *)(temp.camera_uid)},
		{"position_type",       DB_DATA_TYPE::DB_UCHAR,     1, (unsigned char *)(&(temp.position_type))},
        {"onvif_url",           DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.onvif_url))},
		{"onvif_username",      DB_DATA_TYPE::DB_STR,       255, (unsigned char *)(temp.onvif_username)},
		{"onvif_password",      DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.onvif_password))},
        {"rtsp_url",            DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.rtsp_url))},
		{"sub_rtsp_url",        DB_DATA_TYPE::DB_STR,       255, (unsigned char *)(temp.sub_rtsp_url)},
		{"third_rtsp_url",      DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.third_rtsp_url))},
        {"has_third_rtsp",      DB_DATA_TYPE::DB_UCHAR,     1, (unsigned char *)(&(temp.has_third_rtsp))},
		{"play_rtmp_url",       DB_DATA_TYPE::DB_STR,       255, (unsigned char *)(temp.play_rtmp_url)},
		{"play_hls_url",        DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.play_hls_url))},
        {"play_httpflv_url",    DB_DATA_TYPE::DB_STR,       255, (unsigned char *)((temp.play_httpflv_url))},
		{"play_create_time",    DB_DATA_TYPE::DB_STR,       8, (unsigned char *)(temp.play_create_time)},
		{"play_expire_time",    DB_DATA_TYPE::DB_STR,       8, (unsigned char *)((temp.play_expire_time))},
        {"created_at",          DB_DATA_TYPE::DB_STR,       8, (unsigned char *)((temp.created_at))},
		{"updated_at",          DB_DATA_TYPE::DB_STR,       8, (unsigned char *)(temp.updated_at)},
		{"retry_count",         DB_DATA_TYPE::DB_UINT,      4, (unsigned char *)(&(temp.retry_count))},
        {"retry_time",          DB_DATA_TYPE::DB_STR,       8, (unsigned char *)((temp.retry_time))},
		{"label",               DB_DATA_TYPE::DB_UINT,      4, (unsigned char *)(&temp.label)},
		{"transmit_type",       DB_DATA_TYPE::DB_UCHAR,     1, (unsigned char *)(&(temp.transmit_type))},

		{NULL ,                 DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

    if(dbPool->execInsert(id, "vg_ms_push_info", colum) == (uint32_t)-1){
        std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
        dbPool->releaseConn(id);
        return (void*)-1;//退出之前必须使连接变为空闲可用
    }

    std::cout<<"tid= "<<pthread_self()<<" insert success."<<std::endl;
    sleep(2);
    dbPool->releaseConn(id);
    
    return NULL;
}


int main(){

    MYSQLNAMESPACE::DBPool::DBConnInfo connInfo;
    connInfo.dbName = "test";
    connInfo.host = "127.0.0.1";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

    signal(SIGINT, signal_ctrlc);
    pthread_t tid1;
    pthread_t tid2;
    pthread_t tid3;
    pthread_t tid4;

    while (!bExit)
    {
        pthread_create(&tid1, NULL, SelectFunc, (void*)&dbPool);
        pthread_create(&tid2, NULL, InsertFunc, (void*)&dbPool);
        pthread_create(&tid3, NULL, updateFunc, (void*)&dbPool);
        pthread_create(&tid4, NULL, deleteFunc, (void*)&dbPool);
        sleep(1);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
    }

    return 0;
}