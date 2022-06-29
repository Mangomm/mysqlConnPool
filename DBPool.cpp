#include <unistd.h>
#include <sstream>
#include <string.h>
#include "DBPool.h"
#define DEFAULT_TIME 10                 /* xxxs检测一次，时间不建议太短 */
#define MIN_WAIT_BUSY_CONN_NUM 5        /* 至少存在多少个正在使用的连接才能扩容 */
#define DEFAULT_CONN_VARY 5             /* 每次创建和销毁连接的最大个数，因为当扩容超过最大限制时不会再扩容，销毁低于最小限制时不会再销毁 */

namespace MYSQLNAMESPACE
{
    int DBConn::_connId = 0;
    void DBConn::setStatus(CONN_STATUS connStatus){
        _connStatus = connStatus;
    }

    DBConn::CONN_STATUS DBConn::getStatus(){
        return _connStatus;
    }

    int DBConn::getMyId(){
        return _myId;
    }

    time_t DBConn::getElapse(){
        return _myTime.elapse();
    }

    /*更新状态等信息*/
    void DBConn::getConn()
    {
        _connStatus = CONN_USING;
        _threadId = pthread_self();
        _myTime.now();
#ifdef _DEBUG_CONN_POOL
        printf("get a connect, refresh status, connStatus: %d, tid: %lld, _myId: %d\n", _connStatus, _threadId, _myId);
        //std::cout << "get (" << _threadId << ":" << _myId << ")" << std::endl;
#endif
    }

    /*释放连接，让其给其它线程使用，但不释放*/
    void DBConn::releaseConn()
    {
        _threadId = 0;
        _connStatus = CONN_VALID;
#ifdef _DEBUG_CONN_POOL
        printf("releaseConn, _connStatus: %d, tid: %lld, _myId: %d\n", _connStatus, _threadId, _myId);
        //std::cout << "releaseConn (" << _threadId << ":" <<_myId << ")" << std::endl;
#endif
    }

    void DBConn::fini()
    {
        if(_mysql){
            mysql_close(_mysql);
            _mysql = NULL;
        }
    }
    
    bool DBConn::init()
    {
        if(_mysql){
            mysql_close(_mysql);
            _mysql = NULL;
        }
        if(connDB()){
            return true;
        }

        return false;
    }

    /* 执行sql语句,成功返回0，失败返回-1。*/
    int DBConn::execSql(const char *sql, uint32_t sqlLen)
    {
        if (NULL == sql || 0 == sqlLen || NULL == _mysql)
        {
            //std::cerr<<__FUNCTION__<<": "<<__LINE__<<"exec sql error sqlLen:"<<sqlLen<<" sql statement:"<<sql<<std::endl;
            return -1;
        }

        //mysql_real_query可以传二进制，而mysql_query()不能传二进制.例如二进制的Blob字段，由于可能存在\0，
        //其内部会调用strlen导致遇到\0而结束，而mysql_real_query不会调用strlen.
        int ret = mysql_real_query(_mysql, sql, sqlLen);
        if (0 != ret)
        {
#ifdef _DEBUG_CONN_POOL
            std::cerr<<__FUNCTION__<<": "<<__LINE__<<"line, sql exec error: "<<sql<<std::endl;
            std::cerr<<mysql_error(_mysql)<<std::endl;
#endif
        }

        return ret;
    }

    /* 支持子查询，出错返回-1，成功返回传进col的所有字段类型所占的大小.*/
    uint32_t DBConn::fetchSelectSql(const char *sql, const dbColConn *col)
    {
        const dbColConn *tmp = col;
        uint32_t colAllSize = 0;

        while(NULL != tmp->item.name){// 统计所有字段所占的字节数大小
            colAllSize += tmp->size;
            tmp++;
        }

        if(0 != execSql(sql, ::strlen(sql))){
            return (uint32_t)-1;
        }

        return colAllSize;//成功返回所有字段类型所占的大小
    }


    /*
        获取一行的数据到tempData.
        参12为数据与数据长度，参3为字段(主要用于遍历)，参4为要保存数据的内存.
        成功返回要选择的字段大小总和，失败返回0。0可能是某个参数为空,或者offset为空即传进的size有误.
    */
    uint32_t DBConn::fullSelectDataByRow(MYSQL_ROW row, unsigned long *lengths, const dbCol *temp, unsigned char *tempData)
    {
        if(NULL == lengths || NULL == temp || NULL == tempData){
            return (uint32_t)0;
        }

        int offset = 0;
        int i = 0;
        while (NULL != temp->name)
        {
            //传进的字段为空是select不出来的，所以必须过滤，组sql语句也需要过滤。当然也可以写在其它位置，但是这里最好
            if(::strlen(temp->name) > 0)
            {
                switch (temp->type)
                {
                    case DB_DATA_TYPE::DB_CHAR:
                    {
                        if (row[i])//必须保证字段不为空才能使用，否则会程序崩溃
                        {
                            *(char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(char *)(tempData + offset) = 0;//如果该字段为空，默认赋0，然后offset+=tmp->size
                        }
                        break;
                    }
                        
                    case DB_DATA_TYPE::DB_UCHAR:
                    {
                        if(row[i])
                        {
                            *(unsigned char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(unsigned char *)(tempData + offset) = 0;
                        }
                        break;
                    }
                        
                    case DB_DATA_TYPE::DB_SHORT:
                    {
                        if(row[i])
                        {
                            //*(short *)(tempData + offset) = *row[i];//数值型不转无符号会出问题，数字对不上
                            *(int16_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(short *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                        
                    case DB_DATA_TYPE::DB_USHORT:
                    {
                        if(row[i])
                        {
                            //*(unsigned short *)(tempData + offset) = *row[i];
                            *(uint16_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned short *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_INT:
                    {
                        if(row[i])
                        {
                            //*(int *)(tempData + offset) = *row[i];
                            *(int32_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(int *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_UINT:
                    {
                        if(row[i])
                        {
                            //*(unsigned int *)(tempData + offset) = *row[i];
                            *(uint32_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned int *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_LONG:
                    {
                        if(row[i])
                        {
                            //*(long *)(tempData + offset) = *row[i];
                            *(int64_t *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(long *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_ULONG:
                    {
                        if(row[i])
                        {
                            //*(unsigned long *)(tempData + offset) = *row[i];
                            *(uint64_t *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned long *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_FLOAT:
                    {
                        if(row[i])
                        {
                            *(float *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(float *)(tempData + offset) = 0;
                        }
                        break;                             
                    }
                    case DB_DATA_TYPE::DB_DOUBLE:
                    {
                        if(row[i])
                        {
                            *(double *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(double *)(tempData + offset) = 0;
                        }
                        break;                             
                    }
                    case DB_DATA_TYPE::DB_STR:
                    {
                        //使用二进制的方式处理字符串,不要break即可
                        // if(row[i]){
                        //     bcopy(row[i], tempData + offset, temp->size);
                        // }else
                        // {
                        //     bcopy(0x00, tempData + offset, temp->size);
                        // }  
                        // break;
                    }                
                    case DB_DATA_TYPE::DB_BIN:
                    {
                        bzero(tempData + offset, temp->size);
                        if(row[i] && lengths[i]){
                            //bcopy(row[i], tempData + offset, temp->size);
                            //参3是防止当传进的struceTestData中的某个成员的字节数小于数据库的字节数时，造成内存越界发生数据混乱,严
                            //重可能程序崩溃,所以当小于时，只能拷贝数据库的部分数据
                            bcopy(row[i], tempData + offset, temp->size > lengths[i] ? lengths[i] : temp->size);
                        }
                        // else
                        // {
                        //     bcopy(0x00, tempData + offset, temp->size);
                        // }  
                        break;
                    }
                    
                    default:
                    {
#ifdef _DEBUG_CONN_POOL
                        std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql type: "<<(int)temp->type<<" error"<<std::endl;
#endif
                        break;
                    }
                }//switch

                i++;//下一字段和其长度(row[i], lengths[i]),只有进入了if才i++，否则不需要,不然存在空name时会造成数据获取混乱
            }   
            
            offset += temp->size;
            temp++;
        }

        return offset;//返回要选择的字段大小总和
    }

    /*
        获取一行的数据到tempData.
        参12为数据与数据长度，参3为字段(主要用于遍历)，参4为要保存数据的内存.
        成功返回要选择的字段大小总和，失败返回0。0可能是某个参数为空,或者offset为空即传进的size有误.
        
        注，下面的合并数据库数据的函数(fullSelectDataByRow)： 
        1）当类型是CHAR+UCHAR或者是数值型(short~double)时，若值为0，可能代表数据库的值是null或者是真的0。
        2）strtoul与strtoull是一样的，都是将传入的字符串参1转成对应的进制，只不过一个是unsigned long返回，一个是unsigned long long返回，
        所以short、unsigned short、int、unsigned int这些使用都没问题，不存在访问越界的问题，因为都是通过传入的参1字符串进行处理，只是强转的问题而已，但是至少需要保证
        返回值大于要保存的值，例如要转的字符串大于long，但是你的返回值类型仍用long，那么就会返回时出现数据丢失，所以此时对一个已经数据丢失的数据(即返回值)进行强转还是数据丢失。
        3）float、double存在小数的不能使用整数的方法转，否则遇到小数点即非法字符后，函数返回，例如1.3，则返回1，0.3则返回0。
    */
    uint32_t DBConn::fullSelectDataByRow(MYSQL_ROW row, unsigned long *lengths, const dbColConn *temp, unsigned char *tempData)
    {
        if(NULL == lengths || NULL == temp || NULL == tempData){
            return (uint32_t)0;
        }

        int offset = 0;
        int i = 0;
        
        while (NULL != temp->item.name)
        {
            //传进的字段为空是select不出来的，所以必须过滤，组sql语句也需要过滤。当然也可以写在其它位置，但是这里最好
            if(::strlen(temp->item.name) > 0)
            {
                switch (temp->type)
                {
                    case DB_DATA_TYPE::DB_CHAR:
                    {
                        if (row[i])//必须保证字段不为空才能使用，否则会程序崩溃
                        {
                            *(char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(char *)(tempData + offset) = 0;//如果该字段为空，默认赋0，然后offset+=tmp->size
                        }
                        break;
                    }
                        
                    case DB_DATA_TYPE::DB_UCHAR:
                    {
                        if(row[i])
                        {
                            *(unsigned char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(unsigned char *)(tempData + offset) = 0;
                        }
                        break;
                    }
                        
                    case DB_DATA_TYPE::DB_SHORT:
                    {
                        if(row[i])
                        {
                            //*(short *)(tempData + offset) = *row[i];//数值型不转无符号会出问题，数字对不上
                            *(int16_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(short *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                        
                    case DB_DATA_TYPE::DB_USHORT:
                    {
                        if(row[i])
                        {
                            //*(unsigned short *)(tempData + offset) = *row[i];
                            *(uint16_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned short *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_INT:
                    {
                        if(row[i])
                        {
                            //*(int *)(tempData + offset) = *row[i];
                            *(int32_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(int *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_UINT:
                    {
                        if(row[i])
                        {
                            //*(unsigned int *)(tempData + offset) = *row[i];
                            *(uint32_t *)(tempData + offset) = ::strtoul(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned int *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_LONG:
                    {
                        if(row[i])
                        {
                            //*(long *)(tempData + offset) = *row[i];
                            *(int64_t *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(long *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_ULONG:
                    {
                        if(row[i])
                        {
                            //*(unsigned long *)(tempData + offset) = *row[i];
                            *(uint64_t *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                        }else
                        {
                            *(unsigned long *)(tempData + offset) = 0;
                        }
                        break; 
                    }
                    case DB_DATA_TYPE::DB_FLOAT:
                    {
                        if(row[i])
                        {
                            // float、double存在小数的不能使用整数的方法转，否则遇到小数点即非法字符后，函数返回，例如1.3，则返回1，0.3则返回0。
                            // *(float *)(tempData + offset) = ::strtoull(row[i], (char **)NULL, 10);
                            *(float *)(tempData + offset) = ::strtof(row[i], (char **)NULL);
                        }else
                        {
                            *(float *)(tempData + offset) = 0;
                        }
                        break;                             
                    }
                    case DB_DATA_TYPE::DB_DOUBLE:
                    {
                        if(row[i])
                        {
                            *(double *)(tempData + offset) = ::strtold(row[i], (char **)NULL);
                        }else
                        {
                            *(double *)(tempData + offset) = 0;
                        }
                        break;                             
                    }
                    case DB_DATA_TYPE::DB_STR:
                    {
                        //使用二进制的方式处理字符串,不要break即可
                        // if(row[i]){
                        //     bcopy(row[i], tempData + offset, temp->size);
                        // }else
                        // {
                        //     bcopy(0x00, tempData + offset, temp->size);
                        // }  
                        // break;
                    }                
                    case DB_DATA_TYPE::DB_BIN:
                    {
                        bzero(tempData + offset, temp->size);
                        if(row[i] && lengths[i]){
                            //bcopy(row[i], tempData + offset, temp->size);
                            //参3是防止当传进的struceTestData中的某个成员的字节数小于数据库的字节数时，造成内存越界发生数据混乱,严
                            //重可能程序崩溃,所以当小于时，只能拷贝数据库的部分数据
                            bcopy(row[i], tempData + offset, temp->size > lengths[i] ? lengths[i] : temp->size);
                        }
                        // else
                        // {
                        //     bcopy(0x00, tempData + offset, temp->size);
                        // }  
                        break;
                    }
                    
                    default:
                    {
#ifdef _DEBUG_CONN_POOL
                        std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql type: "<<(int)temp->type<<" error"<<std::endl;
#endif
                        break;
                    }
                }//switch

                i++;//下一字段和其长度(row[i], lengths[i]),只有进入了if才i++，否则不需要,不然存在空name时会造成数据获取混乱
            }   
            
            offset += temp->size;
            temp++;
        }

        return offset;//返回要选择的字段大小总和
    }

    /* 成功：0代表查询结果为0，大于0代表查询行数.出错返回-1. */
    uint32_t DBConn::execSelect(const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == col){
            return (uint32_t)-1;
        }

        // 为空，默认utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // 默认设置该参数
        if(isEncode){
            if (mysql_query(_mysql, encode) != 0) {
                return (uint32_t)-1;
            }
        }

        uint32_t retSize = 0;
        retSize = fetchSelectSql(sql, col);//返回所有字段所占字节大小
        if(retSize == (uint32_t)-1){
            return (uint32_t)-1;
        }

        MYSQL_RES *store = NULL;
        store = mysql_store_result(_mysql);
        if(NULL == store){
#ifdef _DEBUG_CONN_POOL
            std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql error"<<mysql_error(_mysql)<<std::endl;
#endif
            return (uint32_t)-1;
        }

        //mysql_num_rows() 返回结果集中行的数目。此命令仅对 SELECT 语句有效。要取得被 INSERT，UPDATE 或者 DELETE 查询所影
        //响到的行的数目，用 mysql_affected_rows()。因为mysql_affected_rows() 函数返回前一次 MySQL 操作所影响的记录行数。
        int numRows = 0;
        numRows = mysql_num_rows(store);
        if (0 == numRows)
        {
            mysql_free_result(store);
            return 0;
        }

        *data = new unsigned char[numRows * retSize];//开辟行数与想要select的字段总长度
        if (NULL == *data)
        {
            mysql_free_result(store);
            return (uint32_t)-1;
        }

        bzero(*data, numRows * retSize);

        MYSQL_ROW row;
        unsigned char *tempData = *data;
        while (NULL != (row = mysql_fetch_row(store)))
        {
            unsigned long *lengths = mysql_fetch_lengths(store);//获取本行的长度,一个数组，里面包含每一个字段的长度
            uint32_t fullSize = fullSelectDataByRow(row, lengths, col, tempData);
            if (0 == fullSize)
            {
                delete [] (*data);
                mysql_free_result(store);
                return (uint32_t)-1;
            }
            tempData += fullSize;
        }

        mysql_free_result(store);
        
        return numRows;
    }

    /*通用的Insert，成功返回Insert受影响的行数，失败返回-1.*/
    uint32_t DBConn::execInsert(const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == sql){
            return (uint32_t)-1;
        }

        // 为空，默认utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // 默认设置该参数
        if(isEncode){
            if (mysql_query(_mysql, encode) != 0) {
                return (uint32_t)-1;
            }
        }

        if(0 != execSql(sql, ::strlen(sql)))
        {
            return -1;
        }

        //mysql_insert_id返回当前连接最后insert的主键一列(AUTO_INCREMENT)的id，
        //没有使用insert语句或没有auto_increment类型的数据，或返回结果恰好为空值时，会导致mysql_insert_id()返回空)。空值一般指0.
        //但是需要注意，C中mysql_insert_id的返回值为int64_t，数据库返回的id最大为unsigned bigint，也是无符号64位。
        //但在PHP中mysql_insert_id的返回值为int,int实际在PHP由long存储，所以在64位机器中，
        //无符号的64位转成有符号的64位必然会导致溢出.但我们C/C++不需要担心溢出问题.
        // 插入时选择返回了受影响的行数。而不返回(uint64_t)mysql_insert_id(_mysql);是为了统一insert、update这些接口
        //return (uint64_t)mysql_insert_id(_mysql);
        return mysql_affected_rows(_mysql);
    }

    /*通用的Update，成功返回update受影响的行数，失败返回-1.*/
    uint32_t DBConn::execUpdate(const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == sql){
            return (uint32_t)-1;
        }

        // 为空，默认utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // 默认设置该参数
        if(isEncode){
            if (mysql_query(_mysql, encode) != 0) {
                return (uint32_t)-1;
            }
        }

        if(0 != execSql(sql, ::strlen(sql)))
        {
            return -1;
        }

        return mysql_affected_rows(_mysql);
    }

    /*成功返回受影响的行数，失败返回-1*/
    uint32_t DBConn::execDelete(const char *sql)
    {
        if(NULL == sql || NULL == _mysql){
            return -1;
        }

        if(0 != execSql(sql, ::strlen(sql))){
            return -1;
        }
        
        return mysql_affected_rows(_mysql);
    }

    /*设置显示事务.true设置成功，false设置失败*/
    bool DBConn::setTransactions(bool bSupportTransactions)
    {
        if(bSupportTransactions){
            return (0 == execSql("SET AUTOCOMMIT = 0", ::strlen("SET AUTOCOMMIT = 0")));

        }else{
            return (0 == execSql("SET AUTOCOMMIT = 1", ::strlen("SET AUTOCOMMIT = 1")));
        }
    }

    /*失败返回false，成功返回true*/
    bool DBConn::connDB()
    {
        _mysql = mysql_init(NULL);
        if (NULL == _mysql)
        {
            return false;
        }
        mysql_options(_mysql, MYSQL_OPT_READ_TIMEOUT, (const char *)&_timeout);
        mysql_options(_mysql, MYSQL_OPT_WRITE_TIMEOUT, (const char *)&_timeout);
        mysql_options(_mysql, MYSQL_OPT_CONNECT_TIMEOUT, (const char*)&_timeout);

        int opt = 1;
        mysql_options(_mysql, MYSQL_OPT_RECONNECT, (char *)&opt);
        if (!mysql_real_connect(_mysql, _connInfo.host.c_str(), _connInfo.user.c_str(), 
            _connInfo.passwd.c_str(), _connInfo.dbName.c_str(), _connInfo.port, 0, 0))
        {
#ifdef _DEBUG_CONN_POOL
            std::cerr<<"mysql connect error:"<<mysql_error(_mysql)<<std::endl;
#endif
            mysql_close(_mysql);
            _mysql = NULL;
            return false;
        }

        _connStatus = CONN_VALID;       //连接设为有效
        _myId = _connId;                //增加连接总次数和记录本次id序号,但_myId仍从0开始
        _connId++;

        return true;
    }

    // <-- DBConn end -->

    int DBPool::DbAdjustThread(){

        while (_shutdown == false) 
        {
            sleep(DEFAULT_TIME);                                      /*定时 对连接池管理*/

            // 这里获取某个时刻的存活连接数、忙连接数，是为了用同样的值去判断扩容与销毁
            // 满足条件后，应当使用具体的成员变量判断，类似单例的双重否定处理
            int tmp_live_conn_num = _liveConnNum;                  /* 存活 连接数. */
            int tmp_busy_conn_num = _busyConnNum;                  /* 忙着的线程数 */
#ifdef _DEBUG_CONN_POOL
            printf("tmp_live_conn_num: %d, tmp_busy_conn_num: %d\n", tmp_live_conn_num, tmp_busy_conn_num);
#endif

            /* 创建新连接 算法： 正在使用的连接大于最少正在连接的个数, 且存活的连接数少于最大连接个数时 如：10>=5 && 30<50*/
            if (tmp_busy_conn_num >= MIN_WAIT_BUSY_CONN_NUM && tmp_live_conn_num < _maxConnNum) {
                RallLock rallLock(_lock);
                int add = 0;

                /*
                    一次增加 DEFAULT_CONN_VARY 个连接.
                */  
                for (int i = 0; add < DEFAULT_CONN_VARY && _liveConnNum < _maxConnNum; i++) {
                    std::shared_ptr<DBConn> newConn(new DBConn(_connInfo));
                    if (NULL == newConn) {
#ifdef _DEBUG_CONN_POOL
                        printf("incre conn failed, it will continue to incre.\n");
#endif
                        continue;//若全部new失败，则增加0个连接,不push进连接数组，无需处理错误
				    }
                    if(false == newConn->init()){
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"incre newConn init failed."<<std::endl;
#endif
                        continue;
                    }

                    auto ret = _connPool.insert(std::make_pair(newConn->getMyId(), newConn));
                    if(!ret.second){
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"incre newConn insert failed."<<std::endl;
#endif
                        newConn->fini();
                        continue;
                    }
                    
                    add++;
                    _liveConnNum++;
#ifdef _DEBUG_CONN_POOL
                    std::cout<<"add: "<<add<<", _liveConnNum: "<<_liveConnNum<<", _minConnNum: "<<_minConnNum
                    <<", _connPool.size: "<<_connPool.size()<<std::endl;
#endif
                }
            }

            /* 
                销毁多余的空闲连接 算法：忙连接X2 小于 存活的连接数 且 存活的连接数 大于 最小连接数时.
                例，存活的连接=30，而忙的连接12，那么还剩18个是多余的，可以退出.乘以2是防止后续获取连接的请求增多后又要重新创建连接.
            */
            if ((tmp_live_conn_num - (tmp_busy_conn_num * 2)) > 0 &&  tmp_live_conn_num > _minConnNum) {
                RallLock rallLock(_lock);

                /* 一次销毁DEFAULT_THREAD个连接 */
                int del = 0;
                for(auto it = _connPool.begin(); it != _connPool.end(); ){
                    if(it->second == NULL){
                        // 删除时，如果为空，直接从这里删除即可.
                        it = _connPool.erase(it);
                        _liveConnNum--;
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"del null: "<<del<<", _liveConnNum: "<<_liveConnNum<<", _minConnNum: "<<_minConnNum
                        <<", _connPool.size: "<<_connPool.size()<<std::endl;
#endif
                        continue;
                    }
                    if(DBConn::CONN_STATUS::CONN_VALID == it->second->getStatus()){
                        if(del < DEFAULT_CONN_VARY && _liveConnNum > _minConnNum){
                            auto delConn = it->second;
                            it = _connPool.erase(it);
                            delConn->fini();

                            del++;
                            _liveConnNum--;
#ifdef _DEBUG_CONN_POOL
                            std::cout<<"del: "<<del<<", _liveConnNum: "<<_liveConnNum<<", _minConnNum: "<<_minConnNum
                            <<", _connPool.size: "<<_connPool.size()<<std::endl;
#endif
                            // 这里delConn生命周期结束后，shared的引用计数为0，会自动析构
                        }else{
                            // 本次删除完成
                            break;
                        }
                    }else{
                        // 不是空闲的连接则继续.
                        it++;
                        continue;
                    }

                }

            }

        }// while end

        return 0;
    }

    void *adjust_thread(void *dbpool){
        if(NULL == dbpool){
            //std::cout<<"dbpool is null in adjust_thread"<<std::endl;
            return (void*)-1;
        }

        DBPool *pool = (DBPool *)dbpool;
        int ret = pool->DbAdjustThread();
#ifdef _DEBUG_CONN_POOL
        std::cout<<"adjust_thread exit, ret: "<<ret<<std::endl;
#endif

        return NULL;
    }

    bool DBPool::DbCreate(int minConnNum, int maxConnNum){
        if (minConnNum <= 0 
            || maxConnNum <= 0
            || minConnNum > maxConnNum) 
        {
            return false;
        }

#ifdef _DEBUG_CONN_POOL
        printf("minConnNum: %d, maxConnNum: %d\n", minConnNum, maxConnNum);
#endif
        _minConnNum = minConnNum;
        _maxConnNum = maxConnNum;
        _busyConnNum = 0;
        _liveConnNum = minConnNum;
        _shutdown = false;

        for(int i = 0; i < minConnNum; i++){
            std::shared_ptr<DBConn> newConn(new DBConn(_connInfo));
            if(NULL == newConn){
                //失败不做处理，让其自动释放内存
                return false;
            }

            if(false == newConn->init())//里面的connDB自动将_connId自增
            {
                return false;
            }

            auto ret = _connPool.insert(std::make_pair(newConn->getMyId(), newConn));//有效连接从0开始插入
            if(!ret.second)
            {
                newConn->fini();
                return false;
            }

        }

        // 是否需要回收内存，不需要，因为这里使用的是shared_ptr

        // 创建调整线程
        pthread_create(&_adjust, NULL, adjust_thread, (void *)this);
        return true;
    }

    int DBPool::DbGetConn(){
        while (_shutdown == false)
        {
            RallLock lock(_lock);

            for(auto it = _connPool.begin(); it != _connPool.end(); ){
                if(!it->second){
                    // 实际上应该是不存在空的，如果存在该连接是空的应该报错或者恢复该连接的有效性是最好的,
                    // 因为如果直接删掉该连接的话，m_live_conn_num--后，可能会低于m_min_conn_num，当然可能先判断是否小于最小连接后再删除。
                    //it = _connPool.erase(it);
                    //_liveConnNum--;
                    //continue;

#ifdef _DEBUG_CONN_POOL
                    std::cout<<"DbGetConn conn is null"<<std::endl;                            
#endif

                    std::shared_ptr<DBConn> newConn(new DBConn(_connInfo));
                    if(NULL == newConn)
                    {
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"newConn new failed."<<std::endl;
#endif
                        it++;
                        continue;// 开辟失败说明此时内存不足，我们此时暂不对该空连接处理，跳过该连接去获取后面的空闲连接.当然也可以报错处理
                    }

                    if(false == newConn->init())//里面的connDB自动将_connId自增
                    {
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"newConn init failed."<<std::endl;
#endif
                        it++;
                        continue;// 同样，若恢复空连接失败，则先跳过该连接去获取后面的空闲连接，等待下一次获取再恢复。当然也可以报错处理
                    }

                    // 恢复连接、更新信息并返回
                    it->second = newConn;
                    newConn->getConn();
                    _busyConnNum++;
                    return newConn->getMyId();
                }

                auto tmpConn = it->second;
                switch (tmpConn->getStatus())
                {
                case DBConn::CONN_INVALID:
                    // 动态时不需要用到非法的类型
                    break;
                case DBConn::CONN_USING:
                    //正在被使用.
                    //这个使用时间可能是0-n秒都有可能，因为一个线程拿到一个连接后，其它线程上锁会遍历到刚刚那个被拿的连接的使用时间
#ifdef _DEBUG_CONN_POOL
                    std::cout<<"tmpConn: "<<pthread_self()<<", Used time: "<<tmpConn->getElapse()<<std::endl;
#endif
                    break;
                case DBConn::CONN_VALID:
                    //未被使用
                    tmpConn->getConn();
                    _busyConnNum++;
                    return tmpConn->getMyId();
                default:
#ifdef _DEBUG_CONN_POOL
                    std::cerr<<"unkown option..."<<std::endl;
#endif
                    break;
                }

                it++;
            }

            usleep(1000);
#ifdef _DEBUG_CONN_POOL
            std::cout<<"sleep 1s..."<<std::endl;
#endif

        }// <== while (_shutdown == false) end ==>
        
    }

    void DBPool::DbReleaseConn(int connId){
		std::shared_ptr<DBConn> ret = getConnById(connId);
		if (!ret){
#ifdef _DEBUG_CONN_POOL
			std::cerr<<"DbReleaseConn error not found, connId: "<<connId<<std::endl;
#endif
            return;
		}

		ret->releaseConn();
        _busyConnNum--;
    }

    int DBPool::DbDestroy(){
        if(_shutdown){
            return 0;
        }

        _shutdown = true;
        pthread_join(_adjust, NULL);

#ifdef _DEBUG_CONN_POOL
        printf("adjust thread has join\n");
#endif

        return 0;
    }

    /*根据id获取一个已存在的连接，该id由DBPool::getConn获取.*/
    std::shared_ptr<DBConn> DBPool::getConnById(uint32_t id)
    {
        auto ret = _connPool.find(id);
        if(ret == _connPool.end()){
            std::shared_ptr<DBConn> nullConn;
            return nullConn;
        }

        return ret->second;
    }


    /* 支持分组查询(Group by,having)，连接查询(92,99语法都支持)，支持子查询，并且可以添加设置字符集，成功：大于0代表查询行数,0代表查询结果为0.出错返回-1. */
    uint32_t DBPool::execSelect(uint32_t id, const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode)
    {
        if(NULL == sql){
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            return -1;
        }else{
            return conn->execSelect(sql, col, data, encode, isEncode);
        }   
    }

    /*成功返回受影响的行数，失败返回-1*/
    uint32_t DBPool::execInsert(uint32_t id, const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == sql){
            return (uint32_t)-1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            return -1;
        }

        return conn->execInsert(sql, encode, isEncode);
    }

    /*成功返回受影响的行数，失败返回-1*/
    uint32_t DBPool::execUpdate(uint32_t id, const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == sql){
            return (uint32_t)-1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            return -1;
        }

        return conn->execUpdate(sql, encode, isEncode);
    }

    uint32_t DBPool::execDelete(uint32_t id, const char *sql)
    {
        if(NULL == sql){
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            return -1;
        }

        return conn->execDelete(sql); 
    }

    /*
        设置显示事务.true设置成功，false设置失败.
        注意：查看mysql支持的数据库引擎(SHOW ENGINES;)。默认innodb是支持事务。
    */
    bool DBPool::setTransactions(uint32_t id, bool bSupportTransactions)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            return false;
        }

        return conn->setTransactions(bSupportTransactions);
    }

    /*显示事务提交成功返回true，失败返回false*/
    bool DBPool::commit(uint32_t id)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            return false;
        }

        return (0 == conn->execSql("COMMIT;", ::strlen("COMMIT;")));
    }

    /*回滚事务*/
    bool DBPool::rollback(uint32_t id)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            return false;
        }

        return (0 == conn->execSql("ROLLBACK;", ::strlen("ROLLBACK;")));
    }

    void DBPool::setConnInfo(DBConnInfo &connInfo)
    {
        _connInfo = connInfo;
    }

}