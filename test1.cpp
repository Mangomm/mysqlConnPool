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
 * 测试分组查询(带分组函数)：
 * 1）#案例：查询哪个部门的员工个数>5。
 * 
 * SELECT COUNT(*) count, department_id FROM employees GROUP BY department_id HAVING COUNT(*)>5;
 * 其中，带有HAVING的叫分组后查询，不带则是分组前查询(HAVING可以省略的)，因为分组后的数据放在结果集。 实战中尽量使用分组前查询，效率高
 * department_id叫分组字段，GROUP BY后面的字段必须是分组字段。
 * 2）或者支持完整一点的：
 * SELECT COUNT(*) count, department_id FROM employees WHERE department_id>30 GROUP BY department_id HAVING COUNT(*)>5 ORDER BY count DESC LIMIT 2;
*/
void* SelectFunc1(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    
    // 这里的COUNT(*)至少需要比数据库的类型大，因为不知道数据库对COUNT函数返回的结果集取什么，我们最好去无符号的64位，防止数据丢失，这里取了int，32位4字节
    const dbCol column[] = 
	{
		{"COUNT(*) count",  DB_DATA_TYPE::DB_INT,       4, NULL},       
		{"department_id",   DB_DATA_TYPE::DB_INT,       4, NULL},
		{NULL ,             DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

    uint32_t retCount = 0;

#define SELECT_LIMIT
#ifdef SELECT_LIMIT
#define SELECT_COUNT 2
	testfenzu data[SELECT_COUNT];
	memset(data, 0x00, sizeof(data));
	//retCount = dbPool->execSelectLimit(id, column, "employees", NULL, "department_id", "COUNT(*)>5", NULL, SELECT_COUNT, (unsigned char *)data);
    /*//可以增加where和order by，但需要注意where不能使用分组后的结果集的字段，因为where后跟原表字段，而order by可以跟结果集的内容
    //例如：SELECT COUNT(*) count, department_id FROM employees WHERE department_id>30 GROUP BY department_id HAVING COUNT(*)>5 ORDER BY count DESC LIMIT 2;
    */
    retCount = dbPool->execSelectLimit(id, column, "employees", "department_id>30", "department_id", "COUNT(*)>5", "count DESC", SELECT_COUNT, (unsigned char *)data);
#else
    testfenzu *data = NULL;
    retCount = dbPool->execSelect(id, column, "employees", NULL, "department_id", "COUNT(*)>5", NULL, (unsigned char**)&data);
    /*//可以增加where和order by，但需要注意where不能使用分组后的结果集的字段，因为where后跟原表字段，而order by可以跟结果集的内容
    //例如：SELECT COUNT(*) count, department_id FROM employees WHERE department_id>30 GROUP BY department_id HAVING COUNT(*)>5 ORDER BY count DESC;
    //retCount = dbPool->execSelect(id, column, "employees", "department_id>30", "department_id", "COUNT(*)>5", "count DESC", (unsigned char**)&data);
    */
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
			std::cout << "count: " << data[i].count << std::endl;
			std::cout << "dep_id: " << data[i].dep_id << std::endl;
		}
#else
        for (uint32_t i = 0; i < retCount; ++i)
		{
			std::cout << "count: " << data[i].count << std::endl;
			std::cout << "dep_id: " << data[i].dep_id << std::endl;
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

/**
 * 测试sql92的连接查询(92仅支持内连)：
 * 1. 测试内连：
 *  #案例：查询每个城市的部门个数
 *  sql语句：SELECT COUNT(*) count,l.city FROM departments d,locations l WHERE d.`location_id`=l.`location_id` GROUP BY city;
 * 
*/
void* SelectFunc2(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    
    MYSQLNAMESPACE::dbColConn dbcol1[] = {
        {{' ',"COUNT(*) count"},                DB_DATA_TYPE::DB_LONG,          8,      NULL},//不能加别名的传空字符即可
        {{'l', "city"},                         DB_DATA_TYPE::DB_STR,           30,     NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}//name为NULL时，alias无作用，故可以任意
    };
    
    //此时aliasItem的name代表表名
    std::string t1 = "departments";
    std::string t2 = "locations";
    MYSQLNAMESPACE::aliasItem tbList1[] = {
        {'d', t1.c_str()},
        {'l', t2.c_str()},
        {'0', NULL}
    };
    std::string where1 = "d.`location_id`=l.`location_id`";
    testConn *data = NULL;

    uint32_t retCount = 0;
    retCount = dbPool->execSelectConn(id, dbcol1, tbList1, where1.c_str(), "city", NULL, NULL, reinterpret_cast<unsigned char**>(&data), NULL, true);
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
        sleep(2);
        dbPool->releaseConn(id);
        return NULL;
    }
    else
    {
        for (uint32_t i = 0; i < retCount; ++i)
		{
			std::cout << "count: " << data[i].count << std::endl;
			std::cout << "city: " << data[i].city << std::endl;
		}
        if(NULL != data){
            delete [] data;
            data = NULL;
        }
    }

    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

/**
 * 测试sql99的连接查询(包括内连、外连、交叉连接)：
 * 1. 测试内连：
 *  #案例：查询每个城市的部门个数（上面92的例子）
 *  sql语句：SELECT COUNT(*) count,l.city FROM departments d INNER JOIN locations l ON d.`location_id`=l.`location_id` GROUP BY city;
 *  INNER不写，默认也是INNER代表内连。
 *
 * 2. 测试外连：
 *  #案例：查询编号>3的女神的男朋友信息，如果有则列出详细，如果没有，用null填充。
 * sql语句：SELECT b.id, b.name, bo.* FROM beauty b LEFT OUTER JOIN boys bo ON b.`boyfriend_id` = bo.`id` WHERE b.`id`>3;
 * 
 * 3. 测试交叉连接：
 *  #案例：选择A和B表的所有集合。
 * sql语句：SELECT * FROM `beauty`  CROSS JOIN `boys`;
 * 实际上交叉连接就是笛卡尔乘积AB表的所有集合，所以这种连接在实际并没有太大作用。
 * 
*/
void* SelectFunc3(void *arg)
{
    //分离线程，让系统回收线程结束后的资源
    pthread_detach(pthread_self());

    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null！！！"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    int id = dbPool->getConn();
    uint32_t retCount = 0;
    
    // 1 内连
#define type 0
#if type == 0
    MYSQLNAMESPACE::dbColConn dbcol1[] = {
        {{' ',"COUNT(*) count"},                DB_DATA_TYPE::DB_LONG,          8,      NULL},//不能加别名的传空字符即可
        {{'l', "city"},                         DB_DATA_TYPE::DB_STR,           30,     NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}//name为NULL时，alias无作用，故可以任意
    };
    std::string t1 = "departments";
    std::string t2 = "locations";
    MYSQLNAMESPACE::aliasItem99 tbList1[] = {
        {'d',   t1.c_str(),     'l',    t2.c_str(),     NULL,   "d.`location_id`=l.`location_id`"},
        {' ',   NULL,           ' ',    NULL,           NULL,   NULL}//结尾标志
    };
    testNeiLian *data = NULL;
    retCount = dbPool->execSelectConn99(id, dbcol1, tbList1, NULL, "city", NULL, NULL, reinterpret_cast<unsigned char**>(&data), NULL, true);

#elif type == 1
    // 2 外连
    MYSQLNAMESPACE::dbColConn dbcol1[] = {
        {{'b', "id"},                           DB_DATA_TYPE::DB_LONG,          8,      NULL},//注意一下这里，数据库是11字节的int，这里只用8字节去接，可能会丢失数据。
        {{'b', "name"},                         DB_DATA_TYPE::DB_STR,           50,     NULL},
        {{'o', "id"},                           DB_DATA_TYPE::DB_LONG,          8,      NULL},//注意，由于别名只有一个字节，所以不能写成两个字符的bo。
        {{'o', "boyName"},                      DB_DATA_TYPE::DB_STR,           20,     NULL},
        {{'o', "userCP"},                       DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}//name为NULL时，alias无作用，故可以任意
    };
    std::string t1 = "beauty";
    std::string t2 = "boys";
    MYSQLNAMESPACE::aliasItem99 tbList1[] = {
        {'b',   t1.c_str(),     'o',    t2.c_str(),     "LEFT OUTER",   "b.`boyfriend_id` = o.`id`"},
        {' ',   NULL,           ' ',    NULL,           NULL,   NULL}//结尾标志
    };
    testWaiLian *data = NULL;
    std::string where1 = "b.`id`>3";
    retCount = dbPool->execSelectConn99(id, dbcol1, tbList1, where1.c_str(), NULL, NULL, NULL, reinterpret_cast<unsigned char**>(&data), NULL, true);
#else
    // 3 交叉连
    交叉连接很简单，这里就不写了，注意：由于*是没有类型的，所以只能列出两表的所有字段，自己也可以想想如何优化适配。
#endif

    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
        sleep(2);
        dbPool->releaseConn(id);
        return NULL;
    }
    else
    {
        for (uint32_t i = 0; i < retCount; ++i)
		{
#if type == 0
			std::cout << "count: " << data[i].count << std::endl;
			std::cout << "city: " << data[i].city << std::endl;
#elif type == 1
			std::cout << "beatyId: " << data[i].beatyId << std::endl;
			std::cout << "beatyName: " << data[i].beatyName << std::endl;
            std::cout << "boysId: " << data[i].boysId << std::endl;
			std::cout << "boysName: " << data[i].boysName << std::endl;
            std::cout << "boysuserCP: " << data[i].boysuserCP << std::endl;
#else
            std::cout << "交叉连接" << std::endl;
#endif
		}
        if(NULL != data){
            delete [] data;
            data = NULL;
        }
    }

    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

int main(){

    MYSQLNAMESPACE::DBPool::DBConnInfo connInfo;
    connInfo.dbName = "myemployees";//SelectFunc2
    // connInfo.dbName = "girls";//SelectFunc3
    connInfo.host = "192.168.1.185";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

    signal(SIGINT, signal_ctrlc);
    pthread_t tid1;
    pthread_t tid2;

    while (!bExit)
    {
        // pthread_create(&tid1, NULL, SelectFunc1, (void*)&dbPool);
        // pthread_create(&tid2, NULL, SelectFunc2, (void*)&dbPool);
        pthread_create(&tid2, NULL, SelectFunc3, (void*)&dbPool);
        sleep(10000);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
    }

    return 0;
}