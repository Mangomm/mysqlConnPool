#ifndef __DBPOOL__H__
#define __DBPOOL__H__

#include <iostream>
#include <stdint.h>
#include <map>
#include <memory>

#include "MyLock.h"

namespace MYSQLNAMESPACE{

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
        unsigned char *data;	// 查询的数据
    } dbCol;

class DBConn;
class DBPool
{
    using MysqlConn = std::map<int, std::shared_ptr<DBConn>>;

    public:
        struct DBConnInfo
        {
            std::string host;	
            std::string user;
            std::string passwd;
            std::string dbName;
            int port;
            bool supportTransactions;
        };

    public:
        /* 默认最大连接池的个数为50 */
        DBPool(DBConnInfo connInfo) : m_connInfo(connInfo), m_MAX_CONN_COUNT(50){};
        ~DBPool(){};

    public:
        int getConn();
        int execSelect(uint32_t id, const dbCol *col, const char *tbName, const char *where, const char *order, unsigned char **data);
        
        uint32_t execSelectLimit(uint32_t id, const dbCol *col, const char *tbName, const char *where, 
            const char *order, uint32_t limit, unsigned char *data, uint32_t limitFrom = 0);
        void releaseConn(int connId);

        uint32_t execInsert(uint32_t id, const char *tbName, const dbCol *col);

        /*目前只支持单表更新,不支持多表的连接更新*/
        uint32_t execUpdate(uint32_t id, const char *tbName, const dbCol *col, const char *where);
        /*目前只支持单表的删除，不支持多表的连接删除*/
        uint32_t execDelete(uint32_t id, const char *tbName, const char *where);

        //显示事务的相关函数
        bool setTransactions(uint32_t id, bool bSupportTransactions);//设置该连接为显示事务
        bool commit(uint32_t id);
		bool rollback(uint32_t id);

    private:
        std::shared_ptr<DBConn> getConnById(uint32_t id);

    private:
        MysqlConn  m_connPool;                                      /*存放多个连接的容器*/
        DBConnInfo m_connInfo;                                      /*连接信息*/

        MyLock m_lock;                                              /*用于锁住连接池*/
        const uint32_t m_MAX_CONN_COUNT;                            /*最大连接数*/

};

}


#endif