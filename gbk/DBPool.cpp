#if defined(__linux__) || defined(__linux) 
#include <unistd.h>
#endif
#include <sstream>
#include <string.h>
#include "DBPool.h"
#define DEFAULT_TIME 10                 /* xxxs���һ�Σ�ʱ�䲻����̫��. */
#define MIN_WAIT_BUSY_CONN_NUM 5        /* ���ٴ��ڶ��ٸ�����ʹ�õ����Ӳ�������. */
#define DEFAULT_CONN_VARY 5             /* ÿ�δ������������ӵ�����������Ϊ�����ݳ����������ʱ���������ݣ����ٵ�����С����ʱ����������. */

#if defined(__linux__) || defined(__linux)
#else
#define bzero(s, n) memset((void*)(s), 0, (n))
#define bcopy(src, dst, n) memmove((void*)(dst), src, n)
#endif

namespace MYSQLNAMESPACE
{

#ifdef _USE_INCREASE_AS_KEY
    int DBConn::_connId = 0;
#endif

    void DBConn::setStatus(CONN_STATUS connStatus){
        _connStatus = connStatus;
    }

    DBConn::CONN_STATUS DBConn::getStatus(){
        return _connStatus;
    }

    llong DBConn::getMyId(){
        return _myId;
    }

#if defined(__linux__) || defined(__linux) 
	pthread_t DBConn::getThreadId() {
		return _threadId;
	}
#else
	std::thread::id DBConn::getThreadId() {
		return _threadId;
	}
#endif

    time_t DBConn::getElapse(){
        return _myTime.elapse();
    }

    /* ����״̬����Ϣ. */
    void DBConn::getConn()
    {
        _connStatus = CONN_USING;
#if defined(__linux__) || defined(__linux) 
		_threadId = pthread_self();
#else
		_threadId = std::this_thread::get_id();
#endif
        _myTime.now();
    }

    /* �ͷ����ӣ�����������߳�ʹ�ã������ͷ�. */
    void DBConn::releaseConn()
    {
#if defined(__linux__) || defined(__linux) 
		_threadId = 0;
#else
		std::thread::id tmpTid;//������ʱ������0
		_threadId = tmpTid;
#endif
        _connStatus = CONN_VALID;
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

    /* ִ��sql���,�ɹ�����0��ʧ�ܷ���-1. */
    int DBConn::execSql(const char *sql, uint32_t sqlLen)
    {
        if (NULL == sql || 0 == sqlLen || NULL == _mysql)
        {
            //std::cerr<<__FUNCTION__<<": "<<__LINE__<<"exec sql error sqlLen:"<<sqlLen<<" sql statement:"<<sql<<std::endl;
            return -1;
        }

        //mysql_real_query���Դ������ƣ���mysql_query()���ܴ�������.��������Ƶ�Blob�ֶΣ����ڿ��ܴ���\0��
        //���ڲ������strlen��������\0����������mysql_real_query�������strlen.
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

    /* ֧���Ӳ�ѯ��������-1���ɹ����ش���col�������ֶ�������ռ�Ĵ�С. */
    uint32_t DBConn::fetchSelectSql(const char *sql, const dbColConn *col)
    {
        const dbColConn *tmp = col;
        uint32_t colAllSize = 0;

        while(NULL != tmp->item.name){// ͳ�������ֶ���ռ���ֽ�����С
            colAllSize += tmp->size;
            tmp++;
        }

        if(0 != execSql(sql, ::strlen(sql))){
            return (uint32_t)-1;
        }

        return colAllSize;//�ɹ����������ֶ�������ռ�Ĵ�С
    }


    /*
        ��ȡһ�е����ݵ�tempData.
        ��12Ϊ���������ݳ��ȣ���3Ϊ�ֶ�(��Ҫ���ڱ���)����4ΪҪ�������ݵ��ڴ�.
        �ɹ�����Ҫѡ����ֶδ�С�ܺͣ�ʧ�ܷ���0��0������ĳ������Ϊ��,����offsetΪ�ռ�������size����.
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
            //�������ֶ�Ϊ����select�������ģ����Ա�����ˣ���sql���Ҳ��Ҫ���ˡ���ȻҲ����д������λ�ã������������
            if(::strlen(temp->name) > 0)
            {
                switch (temp->type)
                {
                    case DB_DATA_TYPE::DB_CHAR:
                    {
                        if (row[i])//���뱣֤�ֶβ�Ϊ�ղ���ʹ�ã������������
                        {
                            *(char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(char *)(tempData + offset) = 0;//������ֶ�Ϊ�գ�Ĭ�ϸ�0��Ȼ��offset+=tmp->size
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
                            //*(short *)(tempData + offset) = *row[i];//��ֵ�Ͳ�ת�޷��Ż�����⣬���ֶԲ���
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
                        //ʹ�ö����Ƶķ�ʽ�����ַ���,��Ҫbreak����
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
                            //��3�Ƿ�ֹ��������struceTestData�е�ĳ����Ա���ֽ���С�����ݿ���ֽ���ʱ������ڴ�Խ�緢�����ݻ���,��
                            //�ؿ��ܳ������,���Ե�С��ʱ��ֻ�ܿ������ݿ�Ĳ�������
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

                i++;//��һ�ֶκ��䳤��(row[i], lengths[i]),ֻ�н�����if��i++��������Ҫ,��Ȼ���ڿ�nameʱ��������ݻ�ȡ����
            }   
            
            offset += temp->size;
            temp++;
        }

        return offset;//����Ҫѡ����ֶδ�С�ܺ�
    }

    /*
        ��ȡһ�е����ݵ�tempData.
        ��12Ϊ���������ݳ��ȣ���3Ϊ�ֶ�(��Ҫ���ڱ���)����4ΪҪ�������ݵ��ڴ�.
        �ɹ�����Ҫѡ����ֶδ�С�ܺͣ�ʧ�ܷ���0��0������ĳ������Ϊ��,����offsetΪ�ռ�������size����.
        
        ע������ĺϲ����ݿ����ݵĺ���(fullSelectDataByRow)�� 
        1����������CHAR+UCHAR��������ֵ��(short~double)ʱ����ֵΪ0�����ܴ������ݿ��ֵ��null���������0��
        2��strtoul��strtoull��һ���ģ����ǽ�������ַ�����1ת�ɶ�Ӧ�Ľ��ƣ�ֻ����һ����unsigned long���أ�һ����unsigned long long���أ�
        ����short��unsigned short��int��unsigned int��Щʹ�ö�û���⣬�����ڷ���Խ������⣬��Ϊ����ͨ������Ĳ�1�ַ������д���ֻ��ǿת��������ѣ�����������Ҫ��֤
        ����ֵ����Ҫ�����ֵ������Ҫת���ַ�������long��������ķ���ֵ��������long����ô�ͻ᷵��ʱ�������ݶ�ʧ�����Դ�ʱ��һ���Ѿ����ݶ�ʧ������(������ֵ)����ǿת�������ݶ�ʧ��
        3��float��double����С���Ĳ���ʹ�������ķ���ת����������С���㼴�Ƿ��ַ��󣬺������أ�����1.3���򷵻�1��0.3�򷵻�0��
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
            //�������ֶ�Ϊ����select�������ģ����Ա�����ˣ���sql���Ҳ��Ҫ���ˡ���ȻҲ����д������λ�ã������������
            if(::strlen(temp->item.name) > 0)
            {
                switch (temp->type)
                {
                    case DB_DATA_TYPE::DB_CHAR:
                    {
                        if (row[i])//���뱣֤�ֶβ�Ϊ�ղ���ʹ�ã������������
                        {
                            *(char *)(tempData + offset) = *row[i];
                        }else
                        {
                            *(char *)(tempData + offset) = 0;//������ֶ�Ϊ�գ�Ĭ�ϸ�0��Ȼ��offset+=tmp->size
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
                            //*(short *)(tempData + offset) = *row[i];//��ֵ�Ͳ�ת�޷��Ż�����⣬���ֶԲ���
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
                            // float��double����С���Ĳ���ʹ�������ķ���ת����������С���㼴�Ƿ��ַ��󣬺������أ�����1.3���򷵻�1��0.3�򷵻�0��
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
                        //ʹ�ö����Ƶķ�ʽ�����ַ���,��Ҫbreak����
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
                            //��3�Ƿ�ֹ��������struceTestData�е�ĳ����Ա���ֽ���С�����ݿ���ֽ���ʱ������ڴ�Խ�緢�����ݻ���,��
                            //�ؿ��ܳ������,���Ե�С��ʱ��ֻ�ܿ������ݿ�Ĳ�������
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

                i++;//��һ�ֶκ��䳤��(row[i], lengths[i]),ֻ�н�����if��i++��������Ҫ,��Ȼ���ڿ�nameʱ��������ݻ�ȡ����
            }   
            
            offset += temp->size;
            temp++;
        }

        return offset;//����Ҫѡ����ֶδ�С�ܺ�
    }

    /* �ɹ���0�����ѯ���Ϊ0������0�����ѯ����.������-1. */
    uint32_t DBConn::execSelect(const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == col){
            return (uint32_t)-1;
        }

        // Ϊ�գ�Ĭ��utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // Ĭ�����øò���
        if(isEncode){
            if (mysql_query(_mysql, encode) != 0) {
                return (uint32_t)-1;
            }
        }

        uint32_t retSize = 0;
        retSize = fetchSelectSql(sql, col);//���������ֶ���ռ�ֽڴ�С
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

        //mysql_num_rows() ���ؽ�������е���Ŀ����������� SELECT �����Ч��Ҫȡ�ñ� INSERT��UPDATE ���� DELETE ��ѯ��Ӱ
        //�쵽���е���Ŀ���� mysql_affected_rows()����Ϊmysql_affected_rows() ��������ǰһ�� MySQL ������Ӱ��ļ�¼������
        int numRows = 0;
        numRows = mysql_num_rows(store);
        if (0 == numRows)
        {
            mysql_free_result(store);
            return 0;
        }

        *data = new unsigned char[numRows * retSize];//������������Ҫselect���ֶ��ܳ���
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
            unsigned long *lengths = mysql_fetch_lengths(store);//��ȡ���еĳ���,һ�����飬�������ÿһ���ֶεĳ���
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

    /*ͨ�õ�Insert���ɹ�����Insert��Ӱ���������ʧ�ܷ���-1.*/
    uint32_t DBConn::execInsert(const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == sql){
            return (uint32_t)-1;
        }

        // Ϊ�գ�Ĭ��utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // Ĭ�����øò���
        if(isEncode){
            if (mysql_query(_mysql, encode) != 0) {
                return (uint32_t)-1;
            }
        }

        if(0 != execSql(sql, ::strlen(sql)))
        {
            return -1;
        }

        //mysql_insert_id���ص�ǰ�������insert������һ��(AUTO_INCREMENT)��id��
        //û��ʹ��insert����û��auto_increment���͵����ݣ��򷵻ؽ��ǡ��Ϊ��ֵʱ���ᵼ��mysql_insert_id()���ؿ�)����ֵһ��ָ0.
        //������Ҫע�⣬C��mysql_insert_id�ķ���ֵΪint64_t�����ݿⷵ�ص�id���Ϊunsigned bigint��Ҳ���޷���64λ��
        //����PHP��mysql_insert_id�ķ���ֵΪint,intʵ����PHP��long�洢��������64λ�����У�
        //�޷��ŵ�64λת���з��ŵ�64λ��Ȼ�ᵼ�����.������C/C++����Ҫ�����������.
        // ����ʱѡ�񷵻�����Ӱ�����������������(uint64_t)mysql_insert_id(_mysql);��Ϊ��ͳһinsert��update��Щ�ӿ�
        //return (uint64_t)mysql_insert_id(_mysql);
        return mysql_affected_rows(_mysql);
    }

    /*ͨ�õ�Update���ɹ�����update��Ӱ���������ʧ�ܷ���-1.*/
    uint32_t DBConn::execUpdate(const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == _mysql || NULL == sql){
            return (uint32_t)-1;
        }

        // Ϊ�գ�Ĭ��utf8 
        if(!encode){
            encode = "set names utf8";
        }
        // Ĭ�����øò���
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

    /* �ɹ�������Ӱ���������ʧ�ܷ���-1. */
    uint32_t DBConn::execDelete(const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == sql || NULL == _mysql){
            return -1;
        }

		// Ϊ�գ�Ĭ��utf8 
		if (!encode) {
			encode = "set names utf8";
		}
		// Ĭ�����øò���
		if (isEncode) {
			if (mysql_query(_mysql, encode) != 0) {
				return (uint32_t)-1;
			}
		}

        if(0 != execSql(sql, ::strlen(sql))){
            return -1;
        }
        
        return mysql_affected_rows(_mysql);
    }

    /* ������ʾ����.true���óɹ���false����ʧ��. */
    bool DBConn::setTransactions(bool bSupportTransactions)
    {
        if(bSupportTransactions){
            return (0 == execSql("SET AUTOCOMMIT = 0", ::strlen("SET AUTOCOMMIT = 0")));

        }else{
            return (0 == execSql("SET AUTOCOMMIT = 1", ::strlen("SET AUTOCOMMIT = 1")));
        }
    }

    /* ʧ�ܷ���false���ɹ�����true. */
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
        //����ssl�������е�mysql����ssl(show variables like "%ssl%")����������ʧ��
        mysql_ssl_mode sslmode = SSL_MODE_DISABLED;
        mysql_options(_mysql, MYSQL_OPT_SSL_MODE, (void *)&sslmode);

        if (!mysql_real_connect(_mysql, _connInfo.host.c_str(), _connInfo.user.c_str(), 
            _connInfo.passwd.c_str(), _connInfo.dbName.c_str(), _connInfo.port, 0, 0))
        {
#ifdef _DEBUG_CONN_POOL
            std::cerr<<"mysql connect error: "<<mysql_error(_mysql)<<std::endl;
#endif
            mysql_close(_mysql);
            _mysql = NULL;
            return false;
        }

        _connStatus = CONN_VALID;       //������Ϊ��Ч
#ifdef _USE_INCREASE_AS_KEY
        _myId = _connId;                //���������ܴ����ͼ�¼����id���,��_myId�Դ�0��ʼ
        _connId++;
#else
        _myId = _myTime.getSteadyTimeStampMicroSec();
#endif

        return true;
    }

    // <-- DBConn end -->

    int DBPool::DbAdjustThread(){

        while (_shutdown == false) 
        {
            //sleep(DEFAULT_TIME);                                      /* ��ʱ �����ӳع���. */
			std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_TIME * 1000));

            // �����ȡĳ��ʱ�̵Ĵ����������æ����������Ϊ����ͬ����ֵȥ�ж�����������
            // ����������Ӧ��ʹ�þ���ĳ�Ա�����жϣ����Ƶ�����˫�ط񶨴���
            int tmp_live_conn_num = _liveConnNum;                  /* ��� ������. */
            int tmp_busy_conn_num = _busyConnNum;                  /* æ�ŵ��߳���. */
#ifdef _DEBUG_CONN_POOL
            printf("tmp_live_conn_num: %d, tmp_busy_conn_num: %d\n", tmp_live_conn_num, tmp_busy_conn_num);
#endif

            /* ���������� �㷨�� ����ʹ�õ����Ӵ��������������ӵĸ���, �Ҵ�������������������Ӹ���ʱ �磺10>=5 && 30<50. */
            if (tmp_busy_conn_num >= MIN_WAIT_BUSY_CONN_NUM && tmp_live_conn_num < _maxConnNum) {
#if defined(__linux__) || defined(__linux) 
				RallLock rallLock(_lock);
#else
				std::lock_guard<std::mutex> lguard(_lock);
#endif
                int add = 0;

                /*
                    һ������ DEFAULT_CONN_VARY ������.
                */  
                for (int i = 0; add < DEFAULT_CONN_VARY && _liveConnNum < _maxConnNum; i++) {
                    std::shared_ptr<DBConn> newConn(new DBConn(_connInfo));
                    if (NULL == newConn) {
#ifdef _DEBUG_CONN_POOL
                        printf("incre conn failed, it will continue to incre.\n");
#endif
                        continue;//��ȫ��newʧ�ܣ�������0������,��push���������飬���账�����
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
                ���ٶ���Ŀ������� �㷨��æ����X2 С�� ���������� �� ���������� ���� ��С������ʱ.
                ������������=30����æ������12����ô��ʣ18���Ƕ���ģ������˳�.����2�Ƿ�ֹ������ȡ���ӵ������������Ҫ���´�������.
            */
            if ((tmp_live_conn_num - (tmp_busy_conn_num * 2)) > 0 &&  tmp_live_conn_num > _minConnNum) {
#if defined(__linux__) || defined(__linux) 
				RallLock rallLock(_lock);
#else
				std::lock_guard<std::mutex> lguard(_lock);
#endif

                /* һ������DEFAULT_THREAD������. */
                int del = 0;
                for(auto it = _connPool.begin(); it != _connPool.end(); ){
                    if(it->second == NULL){
                        // ɾ��ʱ�����Ϊ�գ�ֱ�Ӵ�����ɾ������.
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
                            // ����delConn�������ڽ�����shared�����ü���Ϊ0�����Զ�����
                        }else{
                            // ����ɾ�����
                            break;
                        }
                    }else{
                        // ���ǿ��е����������.
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
                //ʧ�ܲ������������Զ��ͷ��ڴ�
                return false;
            }

            if(false == newConn->init())//�����connDB�Զ���_connId����
            {
                return false;
            }

            auto ret = _connPool.insert(std::make_pair(newConn->getMyId(), newConn));//��Ч���Ӵ�0��ʼ����
            if(!ret.second)
            {
                newConn->fini();
                return false;
            }

        }

        // �Ƿ���Ҫ�����ڴ棬����Ҫ����Ϊ����ʹ�õ���shared_ptr

        // ���������߳�
#if defined(__linux__) || defined(__linux) 
		pthread_create(&_adjust, NULL, adjust_thread, (void *)this);
#else
		_adjust = (std::thread*)new std::thread(adjust_thread, this);
		if (_adjust == NULL) {
			printf("new adjust_thread fail.\n");
			return NULL;
		}
#endif
        return true;
    }

    llong DBPool::DbGetConn(){
        while (_shutdown == false)
        {
#if defined(__linux__) || defined(__linux) 
			RallLock rallLock(_lock);
#else
			std::lock_guard<std::mutex> lguard(_lock);
#endif

            for(auto it = _connPool.begin(); it != _connPool.end(); ){
                if(!it->second){
                    // ʵ����Ӧ���ǲ����ڿյģ�������ڸ������ǿյ�Ӧ�ñ�����߻ָ������ӵ���Ч������õ�,
                    // ��Ϊ���ֱ��ɾ�������ӵĻ���m_live_conn_num--�󣬿��ܻ����m_min_conn_num����Ȼ�������ж��Ƿ�С����С���Ӻ���ɾ����
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
                        continue;// ����ʧ��˵����ʱ�ڴ治�㣬���Ǵ�ʱ�ݲ��Ըÿ����Ӵ�������������ȥ��ȡ����Ŀ�������.��ȻҲ���Ա�����
                    }

                    if(false == newConn->init())//�����connDB�Զ���_connId����
                    {
#ifdef _DEBUG_CONN_POOL
                        std::cout<<"newConn init failed."<<std::endl;
#endif
                        it++;
                        continue;// ͬ�������ָ�������ʧ�ܣ���������������ȥ��ȡ����Ŀ������ӣ��ȴ���һ�λ�ȡ�ٻָ�����ȻҲ���Ա�����
                    }

                    // �ָ����ӡ�������Ϣ������
                    it->second = newConn;
                    newConn->getConn();
                    _busyConnNum++;
                    return newConn->getMyId();
                }

                auto tmpConn = it->second;
                switch (tmpConn->getStatus())
                {

                case DBConn::CONN_INVALID:
                    // ��̬ʱ����Ҫ�õ��Ƿ�������
                    break;

                case DBConn::CONN_USING:
                    //���ڱ�ʹ��.
                    //���ʹ��ʱ�������0-n�붼�п��ܣ���Ϊһ���߳��õ�һ�����Ӻ������߳�������������ո��Ǹ����õ����ӵ�ʹ��ʱ��
#ifdef _DEBUG_CONN_POOL
                    // ע�ⲻ�ܴ�ӡ�߳�idʱ������ֱ��ʹ��pthread_self()��ӡ������ᷢ�ֳ���ͬ����id������Ӧ�ô�ӡ������ʹ�õ������ڲ�������߳�id������ȷ�ġ�
                    //std::cout<<"tmpConn is used, tid: "<<tmpConn->getThreadId()<<", Used time: "<<tmpConn->getElapse()<<std::endl;
                    printf("tmpConn is used, connStatus: %d, tid: %lld, _myId: %lld, Used time: %lld\n", 
                            tmpConn->getStatus(), tmpConn->getThreadId(), tmpConn->getMyId(), tmpConn->getElapse());
#endif
                    break;

                case DBConn::CONN_VALID:
                    //δ��ʹ��
                    tmpConn->getConn();
                    _busyConnNum++;
#ifdef _DEBUG_CONN_POOL
                    printf("get a connect, refresh status, connStatus: %d, tid: %lld, _myId: %lld\n", 
                            tmpConn->getStatus(), tmpConn->getThreadId(), tmpConn->getMyId());
#endif
                    return tmpConn->getMyId();

                default:
#ifdef _DEBUG_CONN_POOL
                    std::cerr<<"unkown option..."<<std::endl;
#endif
                    break;
                }

                it++;
            }

			std::this_thread::sleep_for(std::chrono::milliseconds(100));

#ifdef _DEBUG_CONN_POOL
            std::cout<<"sleep 0.1s..."<<std::endl;
#endif

        }// <== while (_shutdown == false) end ==>
        
    }

    void DBPool::DbReleaseConn(llong connId){
		std::shared_ptr<DBConn> releaConn = getConnById(connId);
		if (!releaConn){
#ifdef _DEBUG_CONN_POOL
			std::cerr<<"DbReleaseConn error not found, connId: "<<connId<<std::endl;
#endif
            return;
		}

		releaConn->releaseConn();
        _busyConnNum--;

#ifdef _DEBUG_CONN_POOL
        printf("DbReleaseConn, _connStatus: %d, tid: %lld, _myId: %lld\n", releaConn->getStatus(), releaConn->getThreadId(), releaConn->getMyId());
#endif
    }

    int DBPool::DbDestroy(){
        if(_shutdown){
            return 0;
        }

        _shutdown = true;
#if defined(__linux__) || defined(__linux)
		pthread_join(_adjust, NULL);
#else
		if (NULL != _adjust) {
			if (_adjust->joinable()) {
				_adjust->join();
#ifdef _DEBUG_CONN_POOL
				printf("adjust thread(class) has join\n");
#endif
			}
			delete _adjust;
			_adjust = NULL;
		}
#endif

#ifdef _DEBUG_CONN_POOL
        printf("adjust thread has join\n");
#endif

        return 0;
    }

    /* ����id��ȡһ���Ѵ��ڵ����ӣ���id��DBPool::getConn��ȡ. */
    std::shared_ptr<DBConn> DBPool::getConnById(llong id)
    {
        auto ret = _connPool.find(id);
        if(ret == _connPool.end()){
            std::shared_ptr<DBConn> nullConn;
            return nullConn;
        }

        return ret->second;
    }


    /* ֧�ַ����ѯ(Group by,having)�����Ӳ�ѯ(92,99�﷨��֧��)��֧���Ӳ�ѯ�����ҿ�����������ַ������ɹ�������0�����ѯ����,0�����ѯ���Ϊ0.������-1. */
    uint32_t DBPool::execSelect(llong id, const char *sql, const dbColConn *col, unsigned char **data, const char *encode, bool isEncode)
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

    /* �ɹ�������Ӱ���������ʧ�ܷ���-1. */
    uint32_t DBPool::execInsert(llong id, const char *sql, const char *encode, bool isEncode)
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

    /* �ɹ�������Ӱ���������ʧ�ܷ���-1. */
    uint32_t DBPool::execUpdate(llong id, const char *sql, const char *encode, bool isEncode)
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

    uint32_t DBPool::execDelete(llong id, const char *sql, const char *encode, bool isEncode)
    {
        if(NULL == sql){
            return -1;
        }

        auto conn = getConnById(id);
        if(NULL == conn){
            return -1;
        }

        return conn->execDelete(sql, encode, isEncode);
    }

    /*
        ������ʾ����.true���óɹ���false����ʧ��.
        ע�⣺�鿴mysql֧�ֵ����ݿ�����(SHOW ENGINES;)��Ĭ��innodb��֧������.
    */
    bool DBPool::setTransactions(llong id, bool bSupportTransactions)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            return false;
        }

        return conn->setTransactions(bSupportTransactions);
    }

    /* ��ʾ�����ύ�ɹ�����true��ʧ�ܷ���false. */
    bool DBPool::commit(llong id)
    {
        auto conn = getConnById(id);
        if(NULL == conn){
            return false;
        }

        return (0 == conn->execSql("COMMIT;", ::strlen("COMMIT;")));
    }

    /* �ع�����. */
    bool DBPool::rollback(llong id)
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