#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sstream>
#include "DBPool.h"
#include "DBDefine.h"
#include "MyTime.h"
using namespace MYSQLNAMESPACE;


bool bExit = 0;
void signal_ctrlc(int sig)
{
	bExit = 1;
}

static int inc = 0;

void* UpdateFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());
    printf("UpdateFunc start, tid: %lld\n", pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool UpdateFunc is null！！！"<<std::endl;
        return NULL;
    }

    while(true){
        DBPool *dbPool = (DBPool*)arg;
        //int id = dbPool->getConn();
        auto id = dbPool->DbGetConn();
        
        std::stringstream ss;
        std::string t = "ttt";
        ss << "UPDATE USERINFO SET MYNAME='" << t << inc << "'" << ", ISOK='" << 1 << "'" << "WHERE id='" << 2 << "';";
        inc++;
        //std::string sql = "UPDATE userinfo SET USERNAME='Ttt', ISOK='3' WHERE id=2;";
        printf("sql: %s\n", ss.str().c_str());

        uint32_t retCount = 0;
        retCount = dbPool->execUpdate(id, ss.str().c_str(), NULL, true);
        if(retCount == (uint32_t)-1)
        {
            std::cout<<"execUpdate error"<<std::endl;
            sleep(2);
            dbPool->DbReleaseConn(id);
            return NULL;
        }

        std::cout<<"tid= "<<pthread_self()<<" execUpdate success."<<" affected rows: "<<retCount<<std::endl;
        sleep(5);
        dbPool->DbReleaseConn(id);
    }

	return NULL;
}

void* SelectFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());
    printf("SelectFunc start, tid: %lld\n", pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    while (true)
    {
        DBPool *dbPool = (DBPool*)arg;
        auto id = dbPool->DbGetConn();
        
        /*
            select注意点：
            1. 要select的字段，不一定写完数据库的所有字段.
            2. 并且dbCol的size可以大于或者等于数据库的类型所占字节长度.但dbCol的类型长度必须与testDataStruct一样，否则读取时会发生字节不对齐的问题(已测试).
            3. 并且，dbCol在内存中的大小我们不需要关心，我们只需要关心成员3(dbCol.size)即可，因为存储数据库的实际内存是new出来的(execSelect)或者数组(execSelectLimit),
                存储实际数据的内存布局最终和testDataStruct一样，这样就能确保完整无误的读取到数据，这也是testDataStruct使用__attribute__关键字的原因.
        */
        const dbColConn column[] = 
        {
            {{' ', "USERID"},  DB_DATA_TYPE::DB_ULONG,     8, NULL},
            {{' ', "MYNAME"},  DB_DATA_TYPE::DB_STR,       32, NULL},
            {{' ', "ISOK"},    DB_DATA_TYPE::DB_UCHAR,     1, NULL},
            {{'0', NULL} ,     DB_DATA_TYPE::DB_INVALID,   0, NULL}
        };

        // std::cout<<"struct :"<<sizeof(testDataStruct)<<std::endl;//41
        // std::cout<<"pthread :"<<pthread_self()<<" get Handle:"<<id<<"......operate db in here!"<<std::endl;

        uint32_t retCount = 0;

    //#define SELECT_LIMIT
    #ifdef SELECT_LIMIT
        int SELECT_COUNT = 5;
        testDataStruct *data = NULL;
        std::stringstream sql;
        sql << "SELECT USERID, MYNAME, ISOK FROM USERINFO LIMIT " << SELECT_COUNT << ";";
        std::cout<<"sql: "<<sql.str()<<std::endl;
        retCount = dbPool->execSelect(id, sql.str().c_str(), column, (unsigned char**)&data, NULL);
        std::cout<<"retCount: "<<retCount<<std::endl;
    #else
        testDataStruct *data = NULL;
        std::string sql = "SELECT USERID, MYNAME, ISOK FROM USERINFO WHERE id=9;";
        retCount = dbPool->execSelect(id, sql.c_str(), column, (unsigned char**)&data, NULL);
    #endif
        if(retCount == (uint32_t)-1)
        {
            std::cout<<"Select error"<<std::endl;
        }
        else
        {
    #ifdef SELECT_LIMIT
            // for (uint32_t i = 0; i < SELECT_COUNT; ++i)// error，当数据库数据不足SELECT_COUNT时，会多出来没意义的乱码。
            for (uint32_t i = 0; i < retCount; ++i)
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

        std::cout<<"tid= "<<pthread_self()<<" execSelect success."<<std::endl;
        sleep(5);
        dbPool->DbReleaseConn(id);

    }
    
	return NULL;
}

void* InsertFunc(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());
    printf("InsertFunc start, tid: %lld\n", pthread_self());

    if(NULL == arg)
    {
        std::cout<<"DBPool InsertFunc is null！！！"<<std::endl;
        return NULL;
    }

    while(true){
        auto dbPool = (DBPool*)arg;
        auto id = dbPool->DbGetConn();

        //1. 单条插入
        //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '马超', '1');"
        std::stringstream sql1;
        sql1 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '成风', '1');";
        if(dbPool->execInsert(id, sql1.str().c_str(), NULL) == (uint32_t)-1){
            std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
            dbPool->DbReleaseConn(id);
            return (void*)-1;//退出之前必须使连接变为空闲可用
        }

        //2. 批量插入
        //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '黄忠', '1'),('231', '关羽', '1'),('232', '赵云', '1');"
        // std::stringstream sql2;
        // sql2 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '黄忠', '1'),('231', '关羽', '1'),('232', '赵云', '1');";
        // if(dbPool->execInsert(id, sql2.str().c_str(), NULL) == (uint32_t)-1){
        //     std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
        //     dbPool->DbReleaseConn(id);
        //     return (void*)-1;//退出之前必须使连接变为空闲可用
        // }

        std::cout<<"tid= "<<pthread_self()<<" execInsert success."<<std::endl;
        sleep(5);
        dbPool->DbReleaseConn(id);
    }
    
    return NULL;
}

void testTimeStamp(){
    MyTime tt;
    tt.debugChronoTimeStamp();
    tt.debugChronoSteadyTimeStamp();
    tt.debugUnixTimeStamp();
}

int main(){

    MYSQLNAMESPACE::DBConnInfo connInfo;
    connInfo.dbName = "test";
    connInfo.host = "127.0.0.1";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

    // 1. 创建连接池
    auto ret = dbPool.DbCreate(10, 20);
    if(!ret){
        printf("DbCreate failed.\n");
        return -1;
    }


    signal(SIGINT, signal_ctrlc);
    pthread_t tid1;
    pthread_t tid2;
    pthread_t tid3;
    pthread_t tid4;
    pthread_t tid5;
    pthread_t tid6;

    while (!bExit)
    {
        // 这里建议不要学我多个线程使用一个回调，不然打印出来的内容乱七八糟，不太易看。
        // 如果一个线程对应一个不同的回调，使打印的内容不同，可以方便调试.
        // 经过debug，增加调整线程后，目前没有发现问题
        pthread_create(&tid1, NULL, UpdateFunc, (void*)&dbPool);
        pthread_create(&tid2, NULL, SelectFunc, (void*)&dbPool);
        pthread_create(&tid3, NULL, SelectFunc, (void*)&dbPool);
        pthread_create(&tid4, NULL, SelectFunc, (void*)&dbPool);
        pthread_create(&tid5, NULL, InsertFunc, (void*)&dbPool);
        pthread_create(&tid6, NULL, InsertFunc, (void*)&dbPool);
        sleep(1000000);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
        //break;
    }

    ret = dbPool.DbDestroy();

    printf("DbDestroy ok.\n");

    return 0;
}