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
    const dbColConn column[] = 
	{
		{{' ', "COUNT(*) count"},  DB_DATA_TYPE::DB_INT,       4, NULL},       
		{{' ', "department_id"},   DB_DATA_TYPE::DB_INT,       4, NULL},
		{{' ', NULL},              DB_DATA_TYPE::DB_INVALID,   0, NULL}
	};

    uint32_t retCount = 0;

#define SELECT_LIMIT
#ifdef SELECT_LIMIT
#define SELECT_COUNT 2
	testfenzu *data = NULL;
    std::string sql = "SELECT COUNT(*) count, department_id FROM employees WHERE department_id>30 GROUP BY department_id HAVING COUNT(*)>5 ORDER BY count DESC LIMIT 2";
    retCount = dbPool->execSelect(id, sql.c_str(), column, (unsigned char **)&data, NULL);
#else
    testfenzu *data = NULL;
    std::string sql = "SELECT COUNT(*) count, department_id FROM employees WHERE department_id>30 GROUP BY department_id HAVING COUNT(*)>5 ORDER BY count DESC";
    retCount = dbPool->execSelect(id, sql.c_str(), column, (unsigned char**)&data, NULL);
#endif
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
    }
    else
    {
#ifdef SELECT_LIMIT
		for (uint32_t i = 0; i < retCount; ++i)
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
#endif
    }

    if(NULL != data){
        delete [] data;
        data = NULL;
    }
    std::cout<<"tid: "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

/**
 * 测试sql92的连接查询(92语法仅支持内连)：
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
        {{' ',"count"},                         DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{' ', "city"},                         DB_DATA_TYPE::DB_STR,           30,     NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}
    };
    
    testConn *data = NULL;
    uint32_t retCount = 0;
    std::string sql = "SELECT COUNT(*) count,l.city FROM departments d,locations l WHERE d.`location_id`=l.`location_id` GROUP BY city;";
    retCount = dbPool->execSelect(id, sql.c_str(), dbcol1, reinterpret_cast<unsigned char**>(&data), NULL, true);
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
    }

    if(NULL != data){
        delete [] data;
        data = NULL;
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
#define type 1
#if type == 0
    MYSQLNAMESPACE::dbColConn dbcol1[] = {
        {{' ',"count"},                         DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{' ', "city"},                         DB_DATA_TYPE::DB_STR,           30,     NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}
    };
    std::string sql1 = "SELECT COUNT(*) count, l.city FROM departments d INNER JOIN locations l ON d.`location_id`=l.`location_id` GROUP BY city;";

    testNeiLian *data = NULL;
    retCount = dbPool->execSelect(id, sql1.c_str(), dbcol1, reinterpret_cast<unsigned char**>(&data), NULL, true);

#elif type == 1
    // 2 外连
    MYSQLNAMESPACE::dbColConn dbcol1[] = {
        {{' ', "id"},                           DB_DATA_TYPE::DB_LONG,          8,      NULL},//注意一下这里，数据库是11字节的int，这里只用8字节去接，可能会丢失数据。
        {{' ', "name"},                         DB_DATA_TYPE::DB_STR,           50,     NULL},
        {{' ', "id"},                           DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{' ', "boyName"},                      DB_DATA_TYPE::DB_STR,           20,     NULL},
        {{' ', "userCP"},                       DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}
    };
    std::string sql2 = "SELECT b.id, b.name, bo.* FROM beauty b LEFT OUTER JOIN boys bo ON b.`boyfriend_id` = bo.`id` WHERE b.`id`>3;";

    testWaiLian *data = NULL;
    retCount = dbPool->execSelect(id, sql2.c_str(), dbcol1, reinterpret_cast<unsigned char**>(&data), NULL, true);
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
    }

    if(NULL != data){
        delete [] data;
        data = NULL;
    }
    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

/**
 * 测试子查询(这里以标量子查询为案例)：
 * 1）案例：返回job_id与141号员工相同，salary比143号员工多的员工 姓名，job_id 和工资。
 * sql语句：
    SELECT last_name,job_id,salary
    FROM employees
    WHERE job_id = (
        SELECT job_id
        FROM employees
        WHERE employee_id = 141
    ) AND salary>(
        SELECT salary
        FROM employees
        WHERE employee_id = 143
    );
    注：dbColConn的传入，可以在navicat先执行上面的语句看具体的结果类型及其大小，这样更好。或者看主查询SELECT后面想要查询的字段。
*/
void* SelectFunc4(void *arg)
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
        {{' ',"last_name"},                     DB_DATA_TYPE::DB_STR,           25,     NULL},
        {{' ', "job_id"},                       DB_DATA_TYPE::DB_STR,           10,     NULL},
        {{' ', "salary"},                       DB_DATA_TYPE::DB_DOUBLE,        8,      NULL},//数据库是10字节，这里是8字节，可能数据丢失，不过一般不会超过double
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}
    };
    testSubQuery *data = NULL;
    std::string sql = "SELECT last_name,job_id,salary \
    FROM employees \
    WHERE job_id = ( \
        SELECT job_id \
        FROM employees \
        WHERE employee_id = 141 \
    ) AND salary>( \
        SELECT salary \
        FROM employees \
        WHERE employee_id = 143 \
    );";

    uint32_t retCount = 0;
    retCount = dbPool->execSelect(id, sql.c_str(), dbcol1, reinterpret_cast<unsigned char**>(&data), NULL, true);
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
			std::cout << "last_name: " << data[i].last_name << "   job_id: " << data[i].job_id << "   salary: " << data[i].salary << std::endl;
		}
    }

    if(NULL != data){
        delete [] data;
        data = NULL;
    }
    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

/**
 * 测试联合查询(能用普通查询就尽量别用联合查询)：
 * 1）案例：查询部门编号>90或邮箱包含a的员工信息。
 * sql语句：
    SELECT * FROM employees  WHERE email LIKE '%a%'
    UNION
    SELECT * FROM employees  WHERE department_id>90;
    实际上上面就是以下两个sql语句的合并：
    1）SELECT * FROM employees  WHERE email LIKE '%a%';
    2）SELECT * FROM employees  WHERE department_id>90;
    由于联合查询要求：1）多条查询语句的查询列数是一致的，2）且要求多条查询语句的查询的每一列的类型和顺序最好一致；
    所以SELECT后面的字段一般都要求相同。例如这里的 "*" 号。

    正常语句的实现：SELECT * FROM employees WHERE email LIKE '%a%' OR department_id>90;
    可以看到，正常语句比联合查询简单，故建议优先使用。
*/
void* SelectFunc5(void *arg)
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
        {{' ',"employee_id"},                   DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{' ', "first_name"},                   DB_DATA_TYPE::DB_STR,           20,     NULL},
        {{' ', "last_name"},                    DB_DATA_TYPE::DB_STR,           25,     NULL},
        {{' ', "email"},                        DB_DATA_TYPE::DB_STR,           25,     NULL},
        {{' ', "phone_number"},                 DB_DATA_TYPE::DB_STR,           20,     NULL},
        {{' ', "job_id"},                       DB_DATA_TYPE::DB_STR,           10,     NULL},
        {{' ', "salary"},                       DB_DATA_TYPE::DB_DOUBLE,        8,      NULL},
        {{' ', "commission_pct"},               DB_DATA_TYPE::DB_FLOAT,         4,      NULL},
        {{' ', "manager_id"},                   DB_DATA_TYPE::DB_LONG,          8,      NULL},
        {{' ', "department_id"},                DB_DATA_TYPE::DB_INT,           4,      NULL},
        {{' ', "hiredate"},                     DB_DATA_TYPE::DB_STR,           40,     NULL},//数据库的datetime类型，这里使用字符串40字节处理，够使用了
        {{'0', NULL},                           DB_DATA_TYPE::DB_INVALID,       0,      NULL}
    };
    testUnion *data = NULL;
    std::string sql = "SELECT * FROM employees  WHERE email LIKE '%a%' \
    UNION \
    SELECT * FROM employees  WHERE department_id>90;";

    uint32_t retCount = 0;
    retCount = dbPool->execSelect(id, sql.c_str(), dbcol1, reinterpret_cast<unsigned char**>(&data), NULL, true);
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
			std::cout << "employee_id: " << data[i].employee_id << "   first_name: " << data[i].first_name << "   last_name: " << data[i].last_name
            << "   email: " << data[i].email << "   phone_number: " << data[i].phone_number << "   job_id: " << data[i].job_id 
            << "   salary: " << data[i].salary << "   commission_pct: " << data[i].commission_pct << "   manager_id: " << data[i].manager_id 
            << "   department_id: " << data[i].department_id << "   hiredate: " << data[i].hiredate << std::endl;
		}
    }

    if(NULL != data){
        delete [] data;
        data = NULL;
    }
    std::cout<<"tid= "<<pthread_self()<<" select success."<<std::endl;
    sleep(2);
	dbPool->releaseConn(id);

	return NULL;
}

int main(){

    MYSQLNAMESPACE::DBPool::DBConnInfo connInfo;
    connInfo.dbName = "myemployees";    //SelectFunc1、2、3
    // connInfo.dbName = "girls";       //SelectFunc3
    connInfo.host = "127.0.0.1";
    connInfo.passwd = "6E6Zl4cy0z5phqjL";
    connInfo.port = 3872;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

    signal(SIGINT, signal_ctrlc);
    pthread_t tid1;
    pthread_t tid2;

    while (!bExit)
    {
        // pthread_create(&tid1, NULL, SelectFunc1, (void*)&dbPool);
        // pthread_create(&tid2, NULL, SelectFunc2, (void*)&dbPool);
        // pthread_create(&tid2, NULL, SelectFunc3, (void*)&dbPool);
        // pthread_create(&tid2, NULL, SelectFunc4, (void*)&dbPool);
        pthread_create(&tid2, NULL, SelectFunc5, (void*)&dbPool);
        sleep(10000);//使用usleep时，按下ctrl+c会报出段错误，用sleep不会
    }

    return 0;
}