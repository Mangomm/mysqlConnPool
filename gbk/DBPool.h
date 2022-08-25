#ifndef __DBPOOL__H__
#define __DBPOOL__H__

#include <iostream>
#include <stdint.h>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

//����ʹ�õ�mysql�汾�й�
#if defined(__linux__) || defined(__linux)
#include <mysql/mysql.h>
#else
#include <mysql.h>
#endif

#include "MyLock.h"
#include "MyTime.h"

namespace MYSQLNAMESPACE{

// �Ƿ�����ӳؽ���debug
#define _DEBUG_CONN_POOL
// �Ƿ�ʹ���Զ���1��Ϊ���ӵ�key������ʹ�ã�����ʹ��ʱ�����Ϊ���ӵ�key
//#define _USE_INCREASE_AS_KEY

    typedef long long llong;

    /* ��Ҫ��Ӧ���ݿ�����ͣ�����Ҳ���ԳƱ���������ͣ�����Ҫ������Ϊ�˶Խ����ݿ������. */
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

    /* ����ͬ�ϣ����������ʹ��. */
    typedef struct
    {
        const char *name;		// �ֶ���	
        DB_DATA_TYPE type;		// ���ݿ��ֶε�����
        uint32_t size;			// ���ݿ��ֶε����͵Ĵ�С
        unsigned char *data;	// ��������ݣ�select�ò�����һ����insert,updateʹ��
    } dbCol;

    typedef struct 
    {
        char alisa;             // ��ı���
        const char *name;       // ����ֶ����������Ǳ���
    }aliasItem;
    //�������Ӳ�ѯʱ���������ֶ�ǰ��Ӷ�Ӧ����
    typedef struct
    {
        aliasItem item;		    // �ֶ������ı���������m.id��m�Ǳ�ı�����id���ֶ���
        DB_DATA_TYPE type;		// ���ݿ��ֶε�����
        uint32_t size;			// ���ݿ��ֶε����͵Ĵ�С
        unsigned char *data;	// ��������ݣ�select�ò�����һ����insert,updateʹ��
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

    /* ����״̬. */
	enum CONN_STATUS
	{
		CONN_INVALID,       /* �Ƿ�. */
		CONN_USING,         /* ����ʹ��. */
		CONN_VALID,         /* �Ϸ�,�ȴ���ʹ��. */
	};

public:
#if defined(__linux__) || defined(__linux) 
	DBConn(const DBConnInfo connInfo) :
		_mysql(NULL), _connInfo(connInfo), _timeout(15), _connStatus(CONN_INVALID), _myId(-1), _threadId(0) {}
#else
	//_threadId��ʼ��Ĭ����0
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
	pthread_t getThreadId();// ��ȡ����ʹ�ø����ӵ��߳�id
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
    static int _connId;			    //���ӵ��ܴ�������Ϊmap��keyʹ��.��������ʹ��ʱ�������
#endif

    MYSQL *_mysql;					//���ڶ���һ��mysql����,���ں�������ȷ��Ҫ���������ݿ�����һ��
    DBConnInfo _connInfo;	        //������Ϣ
    const int _timeout;             //���ݿ�Ķ�д��ʱʱ������

    CONN_STATUS _connStatus;		//����״̬
    llong _myId;				    //���ӵ�id����������������һ�����

#if defined(__linux__) || defined(__linux) 
	pthread_t _threadId;            //ʹ�ø����ӵ��߳�id
#else
	std::thread::id _threadId;      //ʹ�ø����ӵ��߳�id
#endif

    MyTime _myTime;                 //���ڼ�¼���ӱ�ʹ�õĿ�ʼʱ���ʹ��ʱ��
};

//class DBConn;
class DBPool
{
    public:

    public:
        /* Ĭ�Ϲ��캯��. */
        DBPool(){}
        /* Ĭ��������ӳصĸ���Ϊ50. */
        DBPool(DBConnInfo connInfo) : 
            _connInfo(connInfo), _shutdown(false), 
            _minConnNum(0), _maxConnNum(0), _liveConnNum(0), _busyConnNum(0){}
        ~DBPool(){}//ע�⣺��������m_connPool����������second��ʹ��shared_ptr�����Ե��������ʱ�����Զ�����
                    //DBConn���������Ӷ�����fini();���յ����ӡ���������DBPool���������账���κζ�����

    public:
        // ��̬�ӿ�
        bool DbCreate(int minConnNum, int maxConnNum);
        llong DbGetConn();
        void DbReleaseConn(llong connId);

        int DbDestroy();
        int DbAdjustThread();// ��ֹ�ֶ�����

    public:
        /* ֧�ַ����ѯ(Group by,having)�����Ӳ�ѯ(92,99�﷨��֧��)���Ӳ�ѯ�ȶ��ֲ�ѯ. */
        uint32_t execSelect(llong id, const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode = true);

        /* ֧�ֵ�������������. */
        uint32_t execInsert(llong id, const char *sql, const char *encode, bool isEncode = true);

        /* ֧�����еĸ��£�ò�ƶ��ڲ��û�ȡ���ݵģ�Ҳ��ͨ�õġ�������롣ֻ��Ҫ��sql��䣬�����кϲ����롢���¡�ɾ���ı�Ҫ�𣬸��˾��ÿ�ʹ���ߣ�������Ͳ��ϲ���. */
        uint32_t execUpdate(llong id, const char *sql, const char *encode, bool isEncode = true);

        /* ֧�ֵ����ɾ��(�Ѳ���)����������ɾ��(��������ɾ����test.cpp�Ĳ��ԣ���������Ҳ���࣬������ʱ�����ٸ�һ�¼���). */
        uint32_t execDelete(llong id, const char *sql, const char *encode, bool isEncode = true);

        //��ʾ�������غ���
        bool setTransactions(llong id, bool bSupportTransactions);//���ø�����Ϊ��ʾ����
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
        MysqlConn  _connPool;                                       /* ��Ŷ�����ӵ�����. */
        DBConnInfo _connInfo;                                       /* ������Ϣ. */

/**
* @deprecated use the c++11 mutex instead
*/
#if defined(__linux__) || defined(__linux) 
		MyLock _lock;                                               /* ������ס���ӳ�. */
#else
		std::mutex _lock;
#endif

#if defined(__linux__) || defined(__linux) 
		pthread_t _adjust;                                          /* �����߳�. */
#else
		std::thread *_adjust;                                       /* �����߳�. */
#endif

        int _shutdown;                                              /* ��־λ�����ӳ�ʹ��״̬��true����Ҫ�ر����ӳأ�false������. */
        
        std::atomic<int> _minConnNum;                               /* ��С������. */
        std::atomic<int> _maxConnNum;                               /* ���������. */
        std::atomic<int> _liveConnNum;                              /* ��ǰ������Ӹ���. */
        std::atomic<int> _busyConnNum;                              /* æ״̬���Ӹ���. */

};

}


#endif