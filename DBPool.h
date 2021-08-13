#ifndef __OVF__DBPOOL__H__
#define __OVF__DBPOOL__H__

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
    //用于99连接查询时，方便在字段前添加对应别名，并且需要添加ON条件，和支持多表连接
    typedef struct 
    {
        char alisa1;             // 表的别名1
        const char *name1;       // 表名1
        char alisa2;             // 表的别名2
        const char *name2;       // 表名2
        const char *connType;    // 连接类型
        const char *on;          // join on中，on的条件
    }aliasItem99;

class DBConn;
class DBPool
{
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
        /* 默认构造函数 */
        DBPool():m_MAX_CONN_COUNT(50){}
        /* 默认最大连接池的个数为50 */
        DBPool(DBConnInfo connInfo) : m_connInfo(connInfo), m_MAX_CONN_COUNT(50){};
        ~DBPool(){};

    public:
        int getConn();

        //支持分组查询Group by和Having分组后的筛选
        int execSelect(uint32_t id, const dbCol *col, const char *tbName, const char *where, const char *groupBy, const char* having,
            const char *order, unsigned char **data);
        //支持分组查询Group by和Having分组后的筛选
        uint32_t execSelectLimit(uint32_t id, const dbCol *col, const char *tbName, const char *where, const char *groupBy, const char* having,
            const char *order, uint32_t limit, unsigned char *data, uint32_t limitFrom = 0);
        //支持分组查询Group by和Having分组后的筛选，并且支持92的所有连接查询(实际92仅支持内连)
        int execSelectConn(uint32_t id, const dbColConn *col, const aliasItem *tbName, const char *where,  const char *groupBy, const char* having,
            const char *order, unsigned char **data, const char *encode, bool isEncode = true);
        //支持分组查询Group by和Having分组后的筛选，并且支持99的所有连接查询(内连外连交叉连)，和支持多表连接
        int execSelectConn99(uint32_t id, const dbColConn *col, const aliasItem99 *tbName, const char *where,  const char *groupBy, const char* having,
            const char *order, unsigned char **data, const char *encode, bool isEncode = true);
        //支持子查询，col里面的item.name,type,size是必传的。貌似所有查询都支持，若不支持，建议上面的分组、连接查询都按照这种方式设计，这样无需再考虑合并sql语句。函数名暂时不修改。
        int execSelectSubQuery(uint32_t id, const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode = true);

        void releaseConn(int connId);

        /*仅支持单条插入，多条插入需要循环*/
        uint64_t execInsert(uint32_t id, const char *tbName, const dbCol *col);
        /*支持批量插入*/
        uint64_t execInsert(uint32_t id, const char *tbName, const char *bulkFileds, const char *bulkValue);

        /*目前只支持单表更新,不支持多表的连接更新*/
        uint32_t execUpdate(uint32_t id, const char *tbName, const dbCol *col, const char *where);
        /* 支持所有的更新，貌似对于不用获取数据的，也是通用的。例如插入。只需要传sql语句。 */
        uint32_t execUpdate(uint32_t id, const char *sql, const char *encode, bool isEncode = true);

        /*目前只支持单表的删除，不支持多表的连接删除*/
        uint32_t execDelete(uint32_t id, const char *tbName, const char *where);

        //显示事务的相关函数
        bool setTransactions(uint32_t id, bool bSupportTransactions);//设置该连接为显示事务
        bool commit(uint32_t id);
		bool rollback(uint32_t id);

    public:
        void setConnInfo(DBConnInfo &connInfo);

    private:
        std::shared_ptr<DBConn> getConnById(uint32_t id);

    private:
        using MysqlConn = std::map<int, std::shared_ptr<DBConn>>;
        MysqlConn  m_connPool;                                      /*存放多个连接的容器*/
        DBConnInfo m_connInfo;                                      /*连接信息*/

        MyLock m_lock;                                              /*用于锁住连接池*/
        const uint32_t m_MAX_CONN_COUNT;                            /*最大连接数*/

};

}


#endif