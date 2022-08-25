#ifndef __DBPOOL__H__
#define __DBPOOL__H__

#include <iostream>
#include <stdint.h>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

//与你使用的mysql版本有关
#if defined(__linux__) || defined(__linux)
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

#include "MyLock.h"
#include "MyTime.h"

namespace MYSQLNAMESPACE{

// 是否对连接池进行debug
#define _DEBUG_CONN_POOL
// 是否使用自动加1作为连接的key，定义使用，否则使用时间戳作为连接的key
//#define _USE_INCREASE_AS_KEY

    typedef long long llong;

    /* 主要对应数据库的类型，或者也可以称本程序的类型，但主要作用是为了对接数据库的类型. */
    enum class DB_DATA_TYPE
    {
        DB_CHAR,
        DB_UCHAR,
        DB_SHORT,
        DB_USHORT,
        DB_INT,
        DB_UINT,
        DB_LONG,
        DB_ULONG,
        DB_FLOAT,
        DB_DOUBLE,
        DB_STR,
        DB_BIN,
        //add new type before DB_INVALID

        DB_INVALID
    };

    /* 作用同上，和上面搭配使用. */
    typedef struct
    {
        const char *name;		// 字段名	
        DB_DATA_TYPE type;		// 数据库字段的类型
        uint32_t size;			// 数据库字段的类型的大小
        unsigned char *data;	// 填入的数据，select用不到，一般是insert,update使用
    } dbCol;

    typedef struct 
    {
        char alisa;             // 表的别名
        const char *name;       // 表的字段名，或者是表名
    }aliasItem;
    //用于连接查询时，方便在字段前添加对应别名
    typedef struct
    {
        aliasItem item;		    // 字段名与表的别名，例如m.id，m是表的别名，id是字段名
        DB_DATA_TYPE type;		// 数据库字段的类型
        uint32_t size;			// 数据库字段的类型的大小
        unsigned char *data;	// 填入的数据，select用不到，一般是insert,update使用
    } dbColConn;

    struct DBConnInfo
    {
        std::string host;	
        std::string user;
        std::string passwd;
        std::string dbName;
        int port;
        bool supportTransactions;
    };

//class DBPool;
class DBConn
{
public:

    /* 连接状态. */
	enum CONN_STATUS
	{
		CONN_INVALID,       /* 非法. */
		CONN_USING,         /* 正在使用. */
		CONN_VALID,         /* 合法,等待被使用. */
	};

public:
#if defined(__linux__) || defined(__linux) 
	DBConn(const DBConnInfo connInfo) :
		_mysql(NULL), _connInfo(connInfo), _timeout(15), _connStatus(CONN_INVALID), _myId(-1), _threadId(0) {}
#else
	//_threadId初始化默认是0
	DBConn(const DBConnInfo connInfo) :
		_mysql(NULL), _connInfo(connInfo), _timeout(15), _connStatus(CONN_INVALID), _myId(-1) {}
#endif

    ~DBConn()    
    {
#ifdef _DEBUG_CONN_POOL
        std::cout<<"test auto recycling"<<std::endl;
#endif
        fini();
    }

    bool init();
    void fini();

    void getConn();
    void releaseConn();

    void setStatus(CONN_STATUS connStatus);
    CONN_STATUS getStatus();
    llong getMyId();

#if defined(__linux__) || defined(__linux) 
	pthread_t getThreadId();// 获取正在使用该连接的线程id
#else
	std::thread::id getThreadId();
#endif

    time_t getElapse();
    
public: 
    int execSql(const char *sql, uint32_t sqlLen);
    uint32_t fetchSelectSql(const char *sql, const dbColConn *col);
    uint32_t fullSelectDataByRow(MYSQL_ROW row, unsigned long *lengths, const dbCol *temp, unsigned char *tempData);
    uint32_t fullSelectDataByRow(MYSQL_ROW row, unsigned long *lengths, const dbColConn *temp, unsigned char *tempData);
    uint32_t execSelect(const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode);
    uint32_t execInsert(const char *sql, const char *encode, bool isEncode);
    uint32_t execUpdate(const char *sql, const char *encode, bool isEncode);
    uint32_t execDelete(const char *sql, const char *encode, bool isEncode);
    bool setTransactions(bool bSupportTransactions);
    
private:
    bool connDB();

private:

#ifdef _USE_INCREASE_AS_KEY
    static int _connId;			    //连接的总次数，作为map的key使用.后续建议使用时间戳代替
#endif

    MYSQL *_mysql;					//用于定义一个mysql对象,便于后续操作确定要操作的数据库是哪一个
    DBConnInfo _connInfo;	        //连接信息
    const int _timeout;             //数据库的读写超时时长限制

    CONN_STATUS _connStatus;		//连接状态
    llong _myId;				    //连接的id，即所有连接数的一个序号

#if defined(__linux__) || defined(__linux) 
	pthread_t _threadId;            //使用该连接的线程id
#else
	std::thread::id _threadId;      //使用该连接的线程id
#endif

    MyTime _myTime;                 //用于记录连接被使用的开始时间和使用时长
};

//class DBConn;
class DBPool
{
    public:

    public:
        /* 默认构造函数. */
        DBPool(){}
        /* 默认最大连接池的个数为50. */
        DBPool(DBConnInfo connInfo) : 
            _connInfo(connInfo), _shutdown(false), 
            _minConnNum(0), _maxConnNum(0), _liveConnNum(0), _busyConnNum(0){}
        ~DBPool(){}//注意：由于我们m_connPool连接容器的second是使用shared_ptr，所以当程序结束时，能自动调用
                    //DBConn的析构，从而调用fini();回收掉连接。所以这里DBPool的析构无需处理任何东西。

    public:
        // 动态接口
        bool DbCreate(int minConnNum, int maxConnNum);
        llong DbGetConn();
        void DbReleaseConn(llong connId);

        int DbDestroy();
        int DbAdjustThread();// 禁止手动调用

    public:
        /* 支持分组查询(Group by,having)，连接查询(92,99语法都支持)，子查询等多种查询. */
        uint32_t execSelect(llong id, const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode = true);

        /* 支持单条、批量插入. */
        uint32_t execInsert(llong id, const char *sql, const char *encode, bool isEncode = true);

        /* 支持所有的更新，貌似对于不用获取数据的，也是通用的。例如插入。只需要传sql语句，但是有合并插入、更新、删除的必要吗，个人觉得看使用者，我这里就不合并了. */
        uint32_t execUpdate(llong id, const char *sql, const char *encode, bool isEncode = true);

        /* 支持单表的删除(已测试)，多表的连接删除(多表的连接删除看test.cpp的测试，而且需求也不多，遇到的时候大家再搞一下即可). */
        uint32_t execDelete(llong id, const char *sql, const char *encode, bool isEncode = true);

        //显示事务的相关函数
        bool setTransactions(llong id, bool bSupportTransactions);//设置该连接为显示事务
        bool commit(llong id);
		bool rollback(llong id);

    public:
        void setConnInfo(DBConnInfo &connInfo);

    private:
        std::shared_ptr<DBConn> getConnById(llong id);

    private:
    #ifdef _USE_INCREASE_AS_KEY
        using MysqlConn = std::map<int, std::shared_ptr<DBConn>>;
    #else
        using MysqlConn = std::map<llong, std::shared_ptr<DBConn>>;
        
    #endif
        MysqlConn  _connPool;                                       /* 存放多个连接的容器. */
        DBConnInfo _connInfo;                                       /* 连接信息. */

/**
* @deprecated use the c++11 mutex instead
*/
#if defined(__linux__) || defined(__linux) 
		MyLock _lock;                                               /* 用于锁住连接池. */
#else
		std::mutex _lock;
#endif

#if defined(__linux__) || defined(__linux) 
		pthread_t _adjust;                                          /* 调整线程. */
#else
		std::thread *_adjust;                                       /* 调整线程. */
#endif

        int _shutdown;                                              /* 标志位，连接池使用状态，true代表将要关闭连接池，false代表不关. */
        
        std::atomic<int> _minConnNum;                               /* 最小连接数. */
        std::atomic<int> _maxConnNum;                               /* 最大连接数. */
        std::atomic<int> _liveConnNum;                              /* 当前存活连接个数. */
        std::atomic<int> _busyConnNum;                              /* 忙状态连接个数. */

};

}


#endif