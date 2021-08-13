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
	bExit = 1;
}

/**
 * 测试通用的update语句。
 * sql语句：
 *  UPDATE departments SET department_name='Ttt', location_id='2500' WHERE department_id=280;
 * 注：Tyy是我自己额外添加的一行数据，避免改变之前的数据。
*/
void* UpdateFunc1(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    
    std::string sql = "UPDATE departments SET department_name='Ttt', location_id='2500' WHERE department_id=280;";

    uint32_t retCount = 0;
    retCount = dbPool->execUpdate(id, sql.c_str(), NULL, true);
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"execUpdate error"<<std::endl;
        sleep(2);
        dbPool->releaseConn(id);
        return NULL;
    }

    std::cout<<"tid= "<<pthread_self()<<" execUpdate success."<<" affected rows: "<<retCount<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

int main(){

    MYSQLNAMESPACE::DBPool::DBConnInfo connInfo;
    connInfo.dbName = "myemployees";//SelectFunc2
    connInfo.host = "192.168.1.185";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

    signal(SIGINT, signal_ctrlc);
    pthread_t tid1;

    while (!bExit)
    {
        pthread_create(&tid1, NULL, UpdateFunc1, (void*)&dbPool);
        sleep(10000);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
    }

    return 0;
}