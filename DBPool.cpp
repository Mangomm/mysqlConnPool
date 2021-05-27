
#include <mysql/mysql.h>
#include <unistd.h>
#include <sstream>
#include <string.h>
#include "MyTime.h"
#include "DBPool.h"

namespace MYSQLNAMESPACE
{

    class DBConn
    {
    public:

        /*连接状态*/
        enum CONN_STATUS
        {
            CONN_INVALID,       /*非法*/
            CONN_USING,         /*正在使用*/
            CONN_VALID,         /*合法，等待被使用*/
        };

    public:
        DBConn(DBPool::DBConnInfo connInfo):_mysql(NULL), _connInfo(connInfo), _timeout(20), 
                _connStatus(CONN_INVALID), _myId(0),  _threadId(0)
        {}

        ~DBConn()
        {
            fini();
        }

        CONN_STATUS getStatus(){
            return _connStatus;
        }
        int getMyId(){
            return _myId;
        }
        time_t getElapse(){
            return _myTime.elapse();
        }

        /*更新状态等信息*/
        void getConn()
        {
            _connStatus = CONN_USING;
            _threadId = pthread_self();
            _myTime.now();
            std::cout << "get (" << _threadId << ":" << _myId << ")" << std::endl;
        }

        /*释放连接，让其给其它线程使用，但不释放*/
        void releaseConn()
        {
            std::cout << "release (" << _threadId << ":" <<_myId << ")" << std::endl;
            _threadId = 0;
            _connStatus = CONN_VALID;
        }

        void fini()
        {
            if(_mysql){
                mysql_close(_mysql);
                _mysql = NULL;
            }
        }
        
        bool init()
        {
            if(_mysql){
                mysql_close(_mysql);
                _mysql = NULL;
            }
            if(connDB()){
                return true;
            }

            std::cerr << "init DB error" << std::endl;
            return false;
        }

        /* 执行sql语句,成功返回0，失败返回-1。*/
        int execSql(const char *sql, uint32_t sqlLen)
        {
            if (NULL == sql || 0 == sqlLen || NULL == _mysql)
            {
                std::cerr<<__FUNCTION__<<": "<<__LINE__<<"exec sql error sqlLen:"<<sqlLen<<" sql statement:"<<sql<<std::endl;
                return -1;
            }

            //mysql_real_query可以传二进制，而mysql_query()不能传二进制.例如二进制的Blob字段，由于可能存在\0，
            //其内部会调用strlen导致遇到\0而结束，而mysql_real_query不会调用strlen.
            int ret = mysql_real_query(_mysql, sql, sqlLen);
            if (0 != ret)
            {
                std::cerr<<__FUNCTION__<<": "<<__LINE__<<"line, sql exec error: "<<sql<<std::endl;
                std::cerr<<mysql_error(_mysql)<<std::endl;
            }
            return ret;
        }

        /*出错返回-1，成功返回传进col的所有字段类型所占的大小.*/
        uint32_t fetchSelectSql(const dbCol *col, const char *tbName, const char *where, 
            const char *order, uint32_t limit = 0, uint32_t limitFromNotZero = 0)
        {
            std::string sql;
            sql += "SELECT ";

            const dbCol *tmp = col;
            uint32_t colAllSize = 0;
            bool first = true;
            while (NULL != tmp->name)//遍历字段名
            {
                colAllSize += tmp->size;    //统计字段名的类型字节长度
                if(::strlen(tmp->name) > 0){//防止为空字符串,空字符串不做处理
                    if(first){
                        first = false;
                    }else{
                        sql += ", ";
                    }
                    sql += tmp->name;
                }
                tmp++;
            }

            if(NULL != tbName && ::strlen(tbName) > 0){
                sql += " FROM ";
                sql += tbName;
            }

            if(NULL != where && ::strlen(where) > 0){
                sql += " WHERE ";
                sql += where;
            }

            if(NULL != order && ::strlen(order) > 0){
                sql += " ORDER BY ";
                sql += order;
            }

            if(limitFromNotZero){//分页查询，开始下标不从0开始查询
                std::stringstream ss;
                ss << " LIMIT " << limitFromNotZero << "," << limit;
                sql += ss.str();
            }else if(limit){//分页查询，默认从0下标开始查询
                std::stringstream ss;
                ss << " LIMIT " << limit;
                sql += ss.str();
            }

            sql += ";";//这里不加好像也不会错
            std::cerr << "fetchSelectSql:" << sql << std::endl;

            if(0 != execSql(sql.c_str(), sql.length())){
                std::cerr<<"execSql failed."<<std::endl;
                return (uint32_t)-1;
            }

            return colAllSize;//成功返回所有字段类型所占的大小
        }

        /*
            获取一行的数据到tempData.
            参12为数据与数据长度，参3为字段(主要用于遍历)，参4为要保存数据的内存.
            成功返回要选择的字段大小总和，失败返回0。0可能是某个参数为空,或者offset为空即传进的size有误.
        */
        uint32_t fullSelectDataByRow(MYSQL_ROW row, unsigned long *lengths, const dbCol *temp, unsigned char *tempData)
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
                            std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql type: "<<(int)temp->type<<" error"<<std::endl;
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

        /*成功：0代表查询结果为0，大于0代表查询行数.出错返回-1.*/
        uint32_t execSelect(const dbCol *col, const char *tbName, const char *where, const char *order, unsigned char **data)
        {
            if(NULL == _mysql || NULL == col || NULL == tbName){
                return (uint32_t)-1;
            }

            uint32_t retSize = 0;
            retSize = fetchSelectSql(col, tbName, where, order);//返回所有字段所占的字节数大小
            if(retSize == (uint32_t)-1){
                return (uint32_t)-1;
            }

            MYSQL_RES *store = NULL;
            store = mysql_store_result(_mysql);
            if(NULL == store){
                std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql error"<<mysql_error(_mysql)<<std::endl;
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
                std::cerr<<__FUNCTION__<<": "<<__LINE__<<"new error"<<std::endl;
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

        /*
            成功返回查询到的行数，0代表查询行数为0. 失败返回-1.
            当返回行数小于想要的查询行数时(此时也是返回查询到的行数)，说明从每一行中取数据出现错误了(fullSelectDataByRow)或
                者数据库中数据不足.但数据库中数据不足在test.cpp最后打印时，数值为0，字符串为空(memset为0，所以字符串遇到0会停止)，
                所以这点不需要理会。
        */
        uint32_t execSelectLimit(const dbCol *col, const char *tbName, const char *where, const char *order, 
            unsigned char *data, uint32_t limit, uint32_t limitFromNotZero = 0)
        {
			uint32_t retError = (uint32_t)-1;

            if (NULL == tbName || NULL == col || NULL == _mysql)
            {
                std::cerr << __FUNCTION__ << ": " << __LINE__ << "nullptr error" << std::endl;
                return retError;
            }

            //合并select语句并执行
            uint32_t retSize = 0;
            retSize = fetchSelectSql(col, tbName, where, order, limit, limitFromNotZero);
            if ((uint32_t)-1 == retSize)
            {
                return retError;
            }

            MYSQL_RES *store = NULL;
            {
                store = mysql_store_result(_mysql);//获取结果集
                if (NULL == store)
                {
                    std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql error"<<mysql_error(_mysql)<<std::endl;
                    return retError;
                }
                //mysql_num_rows() 返回结果集中行的数目。此命令仅对 SELECT 语句有效。要取得被 INSERT，UPDATE 或者 DELETE 查询所影
                //响到的行的数目，用 mysql_affected_rows()。因为mysql_affected_rows() 函数返回前一次 MySQL 操作所影响的记录行数。
                uint32_t retCount = 0;
                retCount = mysql_num_rows(store);
                std::cout<<"retCOunt :"<<retCount<<std::endl;
                if (0 == retCount)
                {
                    mysql_free_result(store);
                    return 0;
                }
            }

            //selectLimit是没有new内存的,因为它已经知道想要的行数，
            //调用时传一个局部对象即可.
            MYSQL_ROW row;
            unsigned char *tempData = data;
            uint32_t count = 0;
            while ((NULL != (row = mysql_fetch_row(store))) && count < limit)//区别在于count，获取一定行数
            {
                unsigned long *lengths = mysql_fetch_lengths(store);//获取本行的长度
                uint32_t fullSize = fullSelectDataByRow(row, lengths, col, tempData);//获取一行的每个字段内容
                if (0 == fullSize)
                {
                    std::cout<<"fullSize=0 in "<<__FUNCTION__<<std::endl;
                    mysql_free_result(store);
                    return count;//合并过程中出错，前面的直接返回，这也是导致返回的行数小于要分页查询的个数
                }
                tempData += fullSize;
                ++count;
                //std::cout<<"count :"<<count<<std::endl;
            }

            mysql_free_result(store);
            return count;//返回分页查询的行数
        }

        /*失败返回-1，成功返回插入成功的主键id*/
        uint32_t execInsert(const char *tbName, const dbCol *col)
        {
            if(NULL == _mysql || NULL == tbName || NULL == col){
                return (uint32_t)-1;
            }

            std::stringstream sql;
            sql << "INSERT INTO ";
            sql << tbName;
            sql << "(";
            auto *tmp = col;
            bool first = true;
            while(NULL != tmp->name)
            {
                if(::strlen(tmp->name) > 0)//传进的字段名为空的一律不处理
                {
                    if(first){
                        first = false;
                    }else{
                        sql << ", ";
                    }
                    sql << tmp->name;
                }
                tmp++;
            }

            sql << ") VALUES(";
            std::cout<<"sql :"<<sql.str()<<std::endl;

            tmp = col;
            first = true;
            while(NULL != tmp->name)
            {
                if(::strlen(tmp->name) > 0)//同样只插入有字段名的字段值
                {
                    if(first)
                    {
                        first = false;
                    }else{
                        sql << ",";
                    }

                    std::cout<<"tmp->type :"<<static_cast<int>(tmp->type)<<std::endl;
                    switch (tmp->type)
                    {
                        case DB_DATA_TYPE::DB_CHAR :
                            {
                                if(NULL != tmp->data && ::strlen((char*)(tmp->data)) > 0)
                                {
                                    char c = *(char *)(tmp->data);//若mp->data大于1字节，只会取首字节
                                    sql << c;
                                }else{
                                    sql << "";
                                }
                            }
                            break;
                        case DB_DATA_TYPE::DB_UCHAR :
                            {
                                if(NULL != tmp->data && ::strlen((char*)(tmp->data)) > 0)
                                {
                                    unsigned char uc = *(tmp->data);
                                    sql << uc;
                                }else{
                                    sql << "";
                                }
                            }
                            break;
                        case DB_DATA_TYPE::DB_SHORT :
                            {
                                if(NULL != tmp->data)
                                {
                                    short s = *(short*)(tmp->data);//若tmp->data长度大于short，则会丢失数据
                                    sql << s;
                                }else{
                                    //sql << 0;//数值为空不需要处理,sql语句报错让执行者排查
                                }
                            }
                            break;
                        case DB_DATA_TYPE::DB_USHORT :
                            {
                                if(NULL != tmp->data)
                                {
                                    unsigned short us = *(unsigned short*)(tmp->data);
                                    sql << us;
                                }else{
                                    //sql << 0;
                                }
                            }
                            break;
                        case DB_DATA_TYPE::DB_INT :
                            {
                                if(NULL != tmp->data)
                                {
                                    int i = *(int*)(tmp->data);//若tmp->data长度大于int，则会丢失数据
                                    sql << i;
                                }else{
                                    //sql << 0;
                                }
                            }
                            break; 
                        case DB_DATA_TYPE::DB_UINT :
                            {
                                if(NULL != tmp->data)
                                {
                                    unsigned int ui = *(unsigned int*)(tmp->data);
                                    sql << ui;
                                }else{
                                    //sql << 0;
                                }
                            }
                            break;                                                        

                        case DB_DATA_TYPE::DB_LONG :
                            {
                                if(NULL != tmp->data)
                                {
                                    long l = *(long*)(tmp->data);//若tmp->data长度大于long，则会丢失数据
                                    sql << l;
                                }else{
                                    //sql << 0;
                                }
                            }
                            break; 
                        case DB_DATA_TYPE::DB_ULONG :
                            {
                                if(NULL != tmp->data)
                                {
                                    //std::cout<<"tmp->data :"<<tmp->data<<std::endl;
                                    unsigned long ul = *(unsigned long*)(tmp->data);
                                    sql << ul;
                                }else{
                                    //sql << 0;
                                }
                            }
                            break; 

                       case DB_DATA_TYPE::DB_STR :
                            {
                                if(NULL != tmp->data)
                                {
                                    //插入诸如datetime这些NULL值
                                    if(strncmp("NULL", (const char*)tmp->data, 4) == 0)
                                    {
                                        // char strData[2 * tmp->size + 1] = {0};
                                        //若传进的tmp->data字符串长度大于tmp->size，会造成数据丢失.并且dbCol的size不能大于数据库的长度，否则插入会越界，可用mysql_error查看错误.
                                        // mysql_real_escape_string(_mysql, strData, (const char*&)tmp->data, tmp->size);
                                        sql << "NULL" ;
                                    }else{
                                        char strData[2 * tmp->size + 1] = {0};
                                        //若传进的tmp->data字符串长度大于tmp->size，会造成数据丢失.并且dbCol的size不能大于数据库的长度，否则插入会越界，可用mysql_error查看错误.
                                        mysql_real_escape_string(_mysql, strData, (const char*&)tmp->data, tmp->size);
                                        //std::cout<<"strData :"<<strData<<std::endl;//出现多个\0，是因为mysql_real_escape_string会进行相关转义，添加相应的转义字符
                                        sql << "\'" << strData << "\'";
                                    }
                                }else{
                                    //sql << 0;//同样不处理，让数据库报错，执行者自行排查
                                }
                            }
                            break; 
                        case DB_DATA_TYPE::DB_BIN:
                            {
                                if(NULL != tmp->data)
                                {
									char strData[2 * tmp->size + 1] = {0};
									mysql_real_escape_string(_mysql, strData, (const char*&)tmp->data, tmp->size);
                                    std::cout<<"strData :"<<strData<<std::endl;//出现多个\0，是因为mysql_real_escape_string会进行相关转义，添加相应的转义字符
									sql << "\'" << strData << "\'";
                                }else{
                                    //sql << 0;
                                }
                            }
                            break;
                        default:
                            {
                                std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql type: "<<(int)tmp->type<<" error"<<std::endl;
                            }
                            break;
                    }
                }

                tmp++;
            }

            sql << ");";
            std::cout<<"insert sql: "<<sql.str()<<std::endl;

            if(0 != execSql(sql.str().c_str(), sql.str().length()))
            {
                return -1;
            }

            //mysql_insert_id返回当前连接最后insert的主键一列(AUTO_INCREMENT)的id，
            //没有使用insert语句或没有auto_increment类型的数据，或返回结果恰好为空值时，会导致mysql_insert_id()返回空)。空值一般指0.
            //但是需要注意，C中mysql_insert_id的返回值为int64_t，数据库返回的id最大为unsigned bigint，也是无符号64位。
            //但在PHP中mysql_insert_id的返回值为int,int实际在PHP由long存储，所以在64位机器中，
            //无符号的64位转成有符号的64位必然会导致溢出.但我们C/C++不需要担心溢出问题.
            return (uint32_t)mysql_insert_id(_mysql);//这里64转32可能出现问题
        }

        /*成功返回update受影响的行数，失败返回-1.*/
        uint32_t execUpdate(const char *tbName, const dbCol *col, const char *where)
        {
            if(NULL == tbName || NULL == col || NULL == _mysql){
                return -1;
            }

            std::stringstream sql;
            sql << "UPDATE " << tbName << " SET ";
            
            bool first = true;
            auto tmp = col;
            while(NULL != tmp->name)
            {
                if(::strlen(tmp->name) > 0)//只处理字段名字符串长度大于0的字段
                {
                    if(first){
                        first = false;
                    }else{
                        sql << ", ";
                    }

                    //std::cout<<"type :"<<(int)tmp->type<<std::endl;
                    switch(tmp->type)
                    {
                        case DB_DATA_TYPE::DB_CHAR ://char类型必须传地址，别传错，否则段错误
                        {
                            if(NULL != tmp->data)
                            {
                                char c = *((char *)tmp->data);//只取第一个字节
                                sql << tmp->name;
                                sql << "=";
                                sql << c;
                            }else{
                                //data为空打印错误即可。注意：并非data=""而是data=NULL，data为""时上面也会插入
                            }
                        }
                        break;
                        case DB_DATA_TYPE::DB_UCHAR :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned char uc = *((unsigned char *)tmp->data);//只取第一个字节
                                sql << tmp->name;
                                sql << "=";
                                sql << uc;
                            }
                        }
                        break;           
                        case DB_DATA_TYPE::DB_SHORT :
                        {
                            if(NULL != tmp->data)
                            {
                                short s = *((short *)tmp->data);//凡是传进的data大于16位，只取低16位
                                sql << tmp->name;
                                sql << "=";
                                sql << s;
                            }
                        }
                        break; 
                        case DB_DATA_TYPE::DB_USHORT :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned short us = *((unsigned short *)tmp->data);
                                sql << tmp->name;
                                sql << "=";
                                sql << us;
                            }
                        }
                        break;  
                        case DB_DATA_TYPE::DB_INT :
                        {
                            if(NULL != tmp->data)
                            {
                                int i = *((int *)tmp->data);
                                sql << tmp->name;
                                sql << "=";
                                sql << i;
                            }
                        }
                        break; 
                        case DB_DATA_TYPE::DB_UINT :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned int ui = *((unsigned int *)tmp->data);
                                sql << tmp->name;
                                sql << "=";
                                sql << ui;
                            }
                        }
                        break;  
                      case DB_DATA_TYPE::DB_LONG :
                        {
                            if(NULL != tmp->data)
                            {
                                long l = *((long *)tmp->data);
                                sql << tmp->name;
                                sql << "=";
                                sql << l;
                            }
                        }
                        break; 
                        case DB_DATA_TYPE::DB_ULONG :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned long ul = *((unsigned long *)tmp->data);
                                sql << tmp->name;
                                sql << "=";
                                sql << ul;
                            }
                        }
                        break;

                      case DB_DATA_TYPE::DB_STR :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned long len = 2 * tmp->size + 1;
                                char strData[len] = {0};
                                //mysql_escape_string(strData, (const char *)tmp->data, len);//error
                                mysql_escape_string(strData, (const char *)tmp->data, tmp->size);//若传进的tmp->data太大，则最大只会转义tmp->size个内容。
                                sql << tmp->name;
                                sql << "=";
                                sql << "\'" << strData << "\'";
                            }
                        }
                        break; 
                        case DB_DATA_TYPE::DB_BIN :
                        {
                            if(NULL != tmp->data)
                            {
                                unsigned long len = 2 * tmp->size + 1;
                                char strData[len] = {0};
                                mysql_escape_string(strData, (const char *)tmp->data, tmp->size);//若传进的tmp->data太大，则最大只会转义tmp->size个内容。
                                sql << tmp->name;
                                sql << "=";
                                sql << "\'" << strData << "\'";
                            }
                        }
                        break;

                        default :
                        {
                            std::cerr<<__FUNCTION__<<": "<<__LINE__<<"sql type: "<<(int)tmp->type<<" error"<<std::endl;
                        }
                    }
                }

                tmp++;
            }

            if(NULL != where)
            {
                //这里可能会由于STR的处理而出现段错误？有可能是长度问题?个人觉得不是，因为连打印where也会出现段错误，关键我没处理过where，
                //只能等下次重现再找问题，不过传地址必不会出现这个问题)
                //解决：调用时，最好传有地址的，而不传字符串常量就不会出现段错误,所以我感觉可能与系统的内存分配有关.所以这个问题可能忽略
                std::cout<<"conndb w :"<<where<<std::endl;   
                sql << " WHERE ";
                sql << where;
            }
            sql << ";";

            std::cout<<"update sql :"<<sql.str()<<std::endl;
            if(0 != execSql(sql.str().c_str(), sql.str().length()))
            {
                return -1;
            }

            return mysql_affected_rows(_mysql);
        }

        /*成功返回受影响的行数，失败返回-1*/
        uint32_t execDelete(const char *tbName, const char *where)
        {
            if(NULL == tbName || NULL == _mysql){
                return -1;
            }

            std::stringstream sql;
            sql << "DELETE FROM " << tbName;

            if(NULL != where){
                sql << " WHERE " << where;
            }
            sql << ";";

            std::cout<<"delete sql :"<<sql.str().c_str()<<std::endl;
            if(0 != execSql(sql.str().c_str(), sql.str().length())){
                return -1;
            }

            return mysql_affected_rows(_mysql);
        }

        /*设置显示事务.true设置成功，false设置失败*/
        bool setTransactions(bool bSupportTransactions)
        {
            if(bSupportTransactions){
                return (0 == execSql("SET AUTOCOMMIT = 0", ::strlen("SET AUTOCOMMIT = 0")));

            }else{
                return (0 == execSql("SET AUTOCOMMIT = 1", ::strlen("SET AUTOCOMMIT = 1")));
            }
        }

    private:
        /*失败返回false，成功返回true*/
        bool connDB()
        {
            _mysql = mysql_init(NULL);
            if (NULL == _mysql)
            {
                std::cerr<<"mysql init error:"<<std::endl;
                return false;
            }
            mysql_options(_mysql, MYSQL_OPT_READ_TIMEOUT, (const char *)&_timeout);
            mysql_options(_mysql, MYSQL_OPT_WRITE_TIMEOUT, (const char *)&_timeout);

            int opt = 1;
            mysql_options(_mysql, MYSQL_OPT_RECONNECT, (char *)&opt);
            if (!mysql_real_connect(_mysql, _connInfo.host.c_str(), _connInfo.user.c_str(), 
                _connInfo.passwd.c_str(), _connInfo.dbName.c_str(), _connInfo.port, 0, 0))
            {
                std::cerr<<"mysql connect error:"<<mysql_error(_mysql)<<std::endl;
                mysql_close(_mysql);
                _mysql = NULL;
                return false;
            }

            _myId = _connId++;              //增加连接总次数和记录本次id序号,但_myId仍从0开始
            _connStatus = CONN_VALID;       //连接设为有效
            std::cout << "DB:" << _myId << " conn ok!" << std::endl;

            return true;
        }

    private:
        static int _connId;				//连接的总次数

        MYSQL *_mysql;					//用于定义一个mysql对象,便于后续操作确定要操作的数据库是哪一个
        DBPool::DBConnInfo _connInfo;	//连接信息
        const int _timeout;             //数据库的读写超时时长限制

        CONN_STATUS _connStatus;		//连接状态
        int _myId;						//连接的id，即所有连接数的一个序号

        pthread_t _threadId;            //使用该连接的线程id
        MyTime _myTime;                 //用于记录连接被使用的开始时间和使用时长
    };
    int DBConn::_connId = 0; 


    /*最重要的函数，获取连接*/
    int DBPool::getConn()
    {
        while (true)
        {
            RallLock lock(m_lock);
        
            std::shared_ptr<DBConn> invaild;
            for(auto it = m_connPool.begin(); it != m_connPool.end(); ){
                if(!it->second){
                    m_connPool.erase(it++);//空的不能再使用，也不能通过非法转合法，因为是空所以无法调用函数
                    continue;
                }

                //1 首先判断连接是否合法
                auto tmp = it->second;
                switch (tmp->getStatus())
                {
                case DBConn::CONN_INVALID://非法
                    invaild = tmp;        //先记录下来非法的连接，不直接转
                    break;
                case DBConn::CONN_USING://正在被使用
                    std::cout<<"Used time: "<<tmp->getElapse()<<std::endl;
                    break;
                case DBConn::CONN_VALID://合法
                    tmp->getConn();
                    return tmp->getMyId();
                default:
                    std::cerr<<"unkown option..."<<std::endl;
                    break;
                }

                it++;
            }

            //若上面一直找不到有效的连接(for能出来肯定是最后一个非法连接或者没有非法全是有效连接)，
            //2 先将非法的连接重新初始化成一个有效连接返回
            if(!invaild){
                //std::cout<<"invaild is zero"<<std::endl;
            }else{
                if(invaild->init()){
                    invaild->getConn();//更新内容
                    return invaild->getMyId();
                }else{
                    std::cout<<"invaild init failed."<<std::endl;//重连失败让其继续往下走即可
                }
            }

            //3 到这一步，说明全是合法连接或者非法转合法失败，那么我们需要判断能否再创建新连接
            if(m_connPool.size() < m_MAX_CONN_COUNT)
            {
                std::shared_ptr<DBConn> newConn(new DBConn(m_connInfo));
                if(NULL != newConn)
                {
                    if(newConn->init())//里面的connDB自动将_connId自增
                    {
                        // int id = m_connPool.size();
                        // id++;
                        auto ret = m_connPool.insert(std::make_pair(newConn->getMyId(), newConn));//有效连接从0开始插入
                        if(ret.second)
                        {
                            newConn->getConn();
                            return newConn->getMyId();
                        }else
                        {
                            std::cout<<"newConn insert failed."<<std::endl;
                        }
                    }else
                    {
                        std::cout<<"newConn init failed."<<std::endl;//失败不做处理，让其进行下一次获取
                    }
                }else
                {
                    std::cout<<"newConn new failed."<<std::endl;//失败不做处理，让其自动释放内存
                }
            }

            usleep(1000);
            std::cout<<"sleep 1s..."<<std::endl;
        }

    }

    /*根据id获取一个已存在的连接，该id由DBPool::getConn获取.*/
    std::shared_ptr<DBConn> DBPool::getConnById(uint32_t id)
    {
        auto ret = m_connPool.find(id);
        if(ret == m_connPool.end()){
            std::cout<<"id: "<<id<<"is not found."<<std::endl;
            std::shared_ptr<DBConn> nullConn;
            return nullConn;
        }

        return ret->second;
    }

    /*成功：大于0代表查询行数,0代表查询结果为0.出错返回-1.*/
    int DBPool::execSelect(uint32_t id, const dbCol *col, const char *tbName, const char *where, const char *order, unsigned char **data)
    {
        if(NULL == col || NULL == tbName){//无法组成最基本的select语句直接返回
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            std::cerr<<"conn is null."<<std::endl;
            return -1;
        }else{
            return conn->execSelect(col, tbName, where, order, data);
        }
    }

    /*成功返回查询到的行数，失败返回-1，0代表查询行数为0.与ConnDB::execSelectLimit一样*/
    uint32_t DBPool::execSelectLimit(uint32_t id, const dbCol *col, const char *tbName, 
        const char *where, const char *order, uint32_t limit, unsigned char *data, uint32_t limitFrom)
    {
        if(NULL == col || NULL == tbName){//无法组成最基本的select语句直接返回
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            std::cerr<<"conn is null."<<std::endl;
            return -1;
        }
            
        return conn->execSelectLimit(col, tbName, where, order, data, limit, limitFrom);
    }

    /*成功返回插入成功的主键id行,失败返回-1.*/
    uint32_t DBPool::execInsert(uint32_t id, const char *tbName, const dbCol *col)
    {
        if(NULL == tbName || NULL == col){
            return (uint32_t)-1;
        }
        auto conn = getConnById(id);
        if(NULL == conn){
            std::cerr<<"conn is null in execInsert."<<std::endl;
            return -1;
        }

        return conn->execInsert(tbName, col);
    }

    /*成功返回受影响的行数，失败返回-1*/
    uint32_t DBPool::execUpdate(uint32_t id, const char *tbName, const dbCol *col, const char *where)
    {
        if(NULL == tbName || NULL == col){
            return (uint32_t)-1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            std::cerr<<"conn is null in execUpdate."<<std::endl;
            return -1;
        }

        return conn->execUpdate(tbName, col, where);
    }

    /*成功返回受影响的行数，失败返回-1*/
    uint32_t DBPool::execDelete(uint32_t id, const char *tbName, const char *where)
    {
        if(NULL == tbName){
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            std::cerr<<"conn is null in execDelete."<<std::endl;
            return -1;
        }

        return conn->execDelete(tbName, where);
    }

    /*
        设置显示事务.true设置成功，false设置失败.
        注意：查看mysql支持的数据库引擎(SHOW ENGINES;)。默认innodb是支持事务。
    */
    bool DBPool::setTransactions(uint32_t id, bool bSupportTransactions)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            std::cout<<"conn is null in setTransactions"<<std::endl;
            return false;
        }

        return conn->setTransactions(bSupportTransactions);
    }

    /*显示事务提交成功返回true，失败返回false*/
    bool DBPool::commit(uint32_t id)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            std::cout<<"conn is null in commit"<<std::endl;
            return false;
        }

        return (0 == conn->execSql("COMMIT;", ::strlen("COMMIT;")));
    }

    /*回滚事务*/
    bool DBPool::rollback(uint32_t id)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            std::cout<<"conn is null in rollback"<<std::endl;
            return false;
        }

        return (0 == conn->execSql("ROLLBACK;", ::strlen("ROLLBACK;")));
    }

    /*释放连接*/
	void DBPool::releaseConn(int id)
	{
		std::shared_ptr<DBConn> ret = getConnById(id);
		if (!ret)
		{
			std::cerr<<"release conn error not found("<<id<<"in pool"<<std::endl;
            return;
		}
		ret->releaseConn();
	}
}
