#include <iostream>
#include <sstream>
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
    std::string sql = "SELECT USERID, MYNAME, ISOK FROM USERINFO;";
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

    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->DbReleaseConn(id);

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
    auto id = dbPool->DbGetConn();

    //1. 单条插入
    //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '马超', '1');"
    std::stringstream sql1;
    sql1 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '马超', '1');";
    if(dbPool->execInsert(id, sql1.str().c_str(), NULL) == (uint32_t)-1){
        std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;//退出之前必须使连接变为空闲可用
    }

    //2. 批量插入
    //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '黄忠', '1'),('231', '关羽', '1'),('232', '赵云', '1');"
    std::stringstream sql2;
    sql2 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '黄忠', '1'),('231', '关羽', '1'),('232', '赵云', '1');";
    if(dbPool->execInsert(id, sql2.str().c_str(), NULL) == (uint32_t)-1){
        std::cout<<"tid= "<<pthread_self()<<" insert failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;//退出之前必须使连接变为空闲可用
    }

    std::cout<<"tid= "<<pthread_self()<<" insert success."<<std::endl;
    sleep(2);
    dbPool->DbReleaseConn(id);
    
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
    auto id = dbPool->DbGetConn();

    //1. 更新单表的单条语句
    std::string sql1 = "UPDATE USERINFO SET MYNAME='瑶', ISOK='1' WHERE id=1;";
    auto ret = dbPool->execUpdate(id, sql1.c_str(), NULL);
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if(ret == (uint32_t)-1){
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    //2.更新单表的多条语句
    /*
        UPDATE USERINFO SET 
        MYNAME = CASE id
        WHEN 1 THEN '关1'
        WHEN 2 THEN '刘2'
        WHEN 3 THEN '张三丰'
        WHEN 4 THEN '李四'
        ELSE '负五'
        END,
        ISOK = CASE ISOK
        WHEN 0 THEN 1
        WHEN 1 THEN 0
        END
        WHERE USERID >= 223 AND USERID <= 227;
    #需要注意的是。case有两种语法形式(看控制流程26节)，当使用这种case后面带变量或表达式的：
    #1）该变量或表达式必须是一个数值，不能是字符串这种，否则报错。
    #2）想要更新多少个，WHERE后的条件就限制多少个，并且when有可能的情况都要列出来，否则会默认补充0或者报错。
    #以id为例，若WHEN 1 THEN '关1' WHEN 2 THEN '刘2'省略了，当然这种情况是正常的，填入负五。 
    #但是把ELSE '负五'也去掉，那么就会报错--->[Err] 1048 - Column 'MYNAME' cannot be null。
    #实际上只将ELSE '负五'去掉，也是报这样的错误，原因没有列出所有的id值的可能性，也就是如果你没有列出所有可能的值，
    #那么ELSE是必须写的，否则必会报错，这也是另一条case，即ISOK不用写ELSE的原因。
    #3）上面的MYNAME、id、ISOK、USERID都是数据库表的字段名。*/
    std::string sql2 = "UPDATE USERINFO SET \
        MYNAME = CASE id \
        WHEN 1 THEN '关1' \
        WHEN 2 THEN '刘2' \
        WHEN 3 THEN '张三丰' \
        WHEN 4 THEN '李四' \
        ELSE '负五' \
        END, \
        ISOK = CASE ISOK \
        WHEN 0 THEN 1 \
        WHEN 1 THEN 0 \
        END \
        WHERE USERID >= 223 AND USERID <= 227;";

    ret = dbPool->execUpdate(id, sql2.c_str(), NULL);
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if(ret == (uint32_t)-1){
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    //3. 更新多表的语句(不过执行这个语句需要把上面两个语句注释掉然后切换数据库为girls再执行，同理执行上面的语句，需要将本语句注释)
    /*
        修改张无忌的女朋友的手机号为114，结果看到：表beauty：周芷诺、小昭、赵敏的电话号码改变了；表boys：张无忌的userCP变成1000了。
        UPDATE boys bo
        INNER JOIN beauty b ON bo.`id`=b.`boyfriend_id`
        SET b.`phone`='114',bo.`userCP`=1000
        WHERE bo.`boyName`='张无忌';
    */
    // std::string sql3 = "UPDATE boys bo \
    //     INNER JOIN beauty b ON bo.`id`=b.`boyfriend_id` \
    //     SET b.`phone`='114',bo.`userCP`=1000 \
    //     WHERE bo.`boyName`='张无忌';";
    // auto ret = dbPool->execUpdate(id, sql3.c_str(), NULL);
    // std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    // if(ret == (uint32_t)-1){
    //     dbPool->DbReleaseConn(id);
    //     return (void*)-1;
    // }

    std::cout<<"update success"<<std::endl;
    sleep(2);
    dbPool->DbReleaseConn(id);
    
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
    auto id = dbPool->DbGetConn();

    /*
        1.单表的删除。
            #案例：删除手机号以9结尾的女神信息。
            DELETE FROM beauty WHERE phone LIKE '%9';
    */
    // std::string sql1 = "DELETE FROM beauty WHERE phone LIKE '%9';";
    // auto ret = dbPool->execDelete(id, sql1.c_str());
    // std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    // if((uint32_t)-1 == ret){
    //     std::cout<<"execDelete failed."<<std::endl;
    //     dbPool->DbReleaseConn(id);
    //     return (void*)-1;
    // }

    //sleep(2);

    /*
        2.多表的删除。(留一个未解决的问题：这里多表的删除时，虽然执行成功，但是影响行数返回0，数据库的数据并未被改变；但是直接在navicat执行又没有问题。)
            #案例：删除张无忌的女朋友的信息。
            DELETE b
            FROM beauty b
            INNER JOIN boys bo ON b.`boyfriend_id` = bo.`id`
            WHERE bo.`boyName`='张无忌';
    */
    std::string sql2 = "DELETE b FROM beauty b INNER JOIN boys bo ON b.`boyfriend_id` = bo.`id` WHERE bo.`boyName`='张无忌';";
    auto ret = dbPool->execDelete(id, sql2.c_str());
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if((uint32_t)-1 == ret){
        std::cout<<"execDelete failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    std::cout<<"execDelete success"<<std::endl;
    sleep(2);
    dbPool->DbReleaseConn(id);

    return NULL;    
}

int main(){

    MYSQLNAMESPACE::DBConnInfo connInfo;
    connInfo.dbName = "girls";
    connInfo.host = "127.0.0.1";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

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

    while (!bExit)
    {
        // pthread_create(&tid1, NULL, SelectFunc, (void*)&dbPool);
        // pthread_create(&tid2, NULL, InsertFunc, (void*)&dbPool);
        // pthread_create(&tid3, NULL, updateFunc, (void*)&dbPool);
        pthread_create(&tid4, NULL, deleteFunc, (void*)&dbPool);
        sleep(10000);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
    }

    return 0;
}