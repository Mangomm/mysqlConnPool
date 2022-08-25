#include <iostream>
#include <sstream>
#include <thread>
#include <signal.h>
#include <string.h>
#include <vector>
#include <future>
#include "DBDefine.h"
#include "DBPool.h"

using namespace MYSQLNAMESPACE;

bool bExit = 0;
void signal_ctrlc(int sig)
{
	std::cout<<"get signal:"<<sig<<std::endl;
	bExit = 1;
}

void* SelectFunc(void *arg)
{
	std::cout << "SelectFunc tid= " << std::this_thread::get_id() << std::endl;
    if(NULL == arg){
        std::cout<<"DBPool SelectFunc is null!!!"<<std::endl;
        return NULL;
    }

    MYSQLNAMESPACE::DBPool *dbPool = (MYSQLNAMESPACE::DBPool*)arg;
    auto id = dbPool->DbGetConn();
    
    /*
        selectע��㣺
        1. Ҫselect���ֶΣ���һ��д�����ݿ�������ֶ�.
        2. ����dbCol��size���Դ��ڻ��ߵ������ݿ��������ռ�ֽڳ���.��dbCol�����ͳ��ȱ�����testDataStructһ���������ȡʱ�ᷢ���ֽڲ����������(�Ѳ���).
        3. ���ң�dbCol���ڴ��еĴ�С���ǲ���Ҫ���ģ�����ֻ��Ҫ���ĳ�Ա3(dbCol.size)���ɣ���Ϊ�洢���ݿ��ʵ���ڴ���new������(execSelect)��������(execSelectLimit),
            �洢ʵ�����ݵ��ڴ沼�����պ�testDataStructһ������������ȷ����������Ķ�ȡ�����ݣ���Ҳ��testDataStructʹ��__attribute__�ؼ��ֵ�ԭ��.
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
    retCount = dbPool->execSelect(id, sql.str().c_str(), column, (unsigned char**)&data, "set names gbk");
    std::cout<<"retCount: "<<retCount<<std::endl;
#else
    testDataStruct *data = NULL;
    std::string sql = "SELECT USERID, MYNAME, ISOK FROM USERINFO;";
    retCount = dbPool->execSelect(id, sql.c_str(), column, (unsigned char**)&data, "set names gbk");
	//retCount = dbPool->execSelect(id, sql.c_str(), column, (unsigned char**)&data, NULL);
#endif
    if(retCount == (uint32_t)-1)
    {
        std::cout<<"Select error"<<std::endl;
    }else
    {
#ifdef SELECT_LIMIT
        // for (uint32_t i = 0; i < SELECT_COUNT; ++i)// error�������ݿ����ݲ���SELECT_COUNTʱ��������û��������롣
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

	std::cout << "tid= " << std::this_thread::get_id() << " select success." << std::endl;
	//std::this_thread::sleep_for(std::chrono::milliseconds(30000));
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	dbPool->DbReleaseConn(id);
	std::cout << "id DbReleaseConn ok, id= " << id << std::endl;

	return NULL;
}

void* InsertFunc(void *arg)
{
    if(NULL == arg)
    {
        std::cout<<"DBPool InsertFunc is null!!!"<<std::endl;
        return NULL;
    }

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->DbGetConn();

    //1. ��������
    //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '��', '1');"
    std::stringstream sql1;
    sql1 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('229', '��', '1');";
    if(dbPool->execInsert(id, sql1.str().c_str(), "set names gbk") == (uint32_t)-1){
        std::cout<<"tid= "<< std::this_thread::get_id()<<" insert failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;//�˳�֮ǰ����ʹ���ӱ�Ϊ���п���
    }

    //2. ��������
    //"INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '����', '1'),('231', '����', '1'),('232', '����', '1');"
    std::stringstream sql2;
    sql2 << "INSERT INTO USERINFO(USERID, MYNAME, ISOK) VALUES('230', '����', '1'),('231', '����', '1'),('232', '����', '1');";
    //if(dbPool->execInsert(id, sql2.str().c_str(), NULL) == (uint32_t)-1){
	if (dbPool->execInsert(id, sql2.str().c_str(), "set names gbk") == (uint32_t)-1) {
        std::cout<<"tid= "<< std::this_thread::get_id()<<" insert failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;//�˳�֮ǰ����ʹ���ӱ�Ϊ���п���
    }

    std::cout<<"tid= "<< std::this_thread::get_id()<<" insert success."<<std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    dbPool->DbReleaseConn(id);
    
    return NULL;
}

void* updateFunc(void *arg)
{

    if(NULL == arg)
    {
        std::cout<<"DBPool updateFunc is null!!!"<<std::endl;
        return NULL;
    }

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->DbGetConn();

    //1. ���µ���ĵ������
    std::string sql1 = "UPDATE USERINFO SET MYNAME='��', ISOK='1' WHERE id=1;";
	//auto ret = dbPool->execUpdate(id, sql1.c_str(), NULL);
    auto ret = dbPool->execUpdate(id, sql1.c_str(), "set names gbk");
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if(ret == (uint32_t)-1){
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    //2.���µ���Ķ������
    /*
        UPDATE USERINFO SET 
        MYNAME = CASE id
        WHEN 1 THEN '��1'
        WHEN 2 THEN '��2'
        WHEN 3 THEN '������'
        WHEN 4 THEN '����'
        ELSE '����'
        END,
        ISOK = CASE ISOK
        WHEN 0 THEN 1
        WHEN 1 THEN 0
        END
        WHERE USERID >= 223 AND USERID <= 227;
    #��Ҫע����ǡ�case�������﷨��ʽ(����������26��)����ʹ������case�������������ʽ�ģ�
    #1���ñ�������ʽ������һ����ֵ���������ַ������֣����򱨴�
    #2����Ҫ���¶��ٸ���WHERE������������ƶ��ٸ�������when�п��ܵ������Ҫ�г����������Ĭ�ϲ���0���߱���
    #��idΪ������WHEN 1 THEN '��1' WHEN 2 THEN '��2'ʡ���ˣ���Ȼ��������������ģ����븺�塣 
    #���ǰ�ELSE '����'Ҳȥ������ô�ͻᱨ��--->[Err] 1048 - Column 'MYNAME' cannot be null��
    #ʵ����ֻ��ELSE '����'ȥ����Ҳ�Ǳ������Ĵ���ԭ��û���г����е�idֵ�Ŀ����ԣ�Ҳ���������û���г����п��ܵ�ֵ��
    #��ôELSE�Ǳ���д�ģ�����ػᱨ����Ҳ����һ��case����ISOK����дELSE��ԭ��
    #3�������MYNAME��id��ISOK��USERID�������ݿ����ֶ���. */
    std::string sql2 = "UPDATE USERINFO SET \
        MYNAME = CASE id \
        WHEN 1 THEN '��1' \
        WHEN 2 THEN '��2' \
        WHEN 3 THEN '������' \
        WHEN 4 THEN '����' \
        ELSE '����' \
        END, \
        ISOK = CASE ISOK \
        WHEN 0 THEN 1 \
        WHEN 1 THEN 0 \
        END \
        WHERE USERID >= 223 AND USERID <= 227;";

    ret = dbPool->execUpdate(id, sql2.c_str(), "set names gbk");
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if(ret == (uint32_t)-1){
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    //3. ���¶������(����ִ����������Ҫ�������������ע�͵�Ȼ���л����ݿ�Ϊgirls��ִ�У�ͬ��ִ���������䣬��Ҫ�������ע��)
    /*
        �޸����޼ɵ�Ů���ѵ��ֻ���Ϊ114�������������beauty������ŵ��С�ѡ������ĵ绰����ı��ˣ���boys�����޼ɵ�userCP���1000�ˡ�
        UPDATE boys bo
        INNER JOIN beauty b ON bo.`id`=b.`boyfriend_id`
        SET b.`phone`='114',bo.`userCP`=1000
        WHERE bo.`boyName`='���޼�';
    */
    // std::string sql3 = "UPDATE boys bo \
    //     INNER JOIN beauty b ON bo.`id`=b.`boyfriend_id` \
    //     SET b.`phone`='114',bo.`userCP`=1000 \
    //     WHERE bo.`boyName`='���޼�';";
    // auto ret = dbPool->execUpdate(id, sql3.c_str(), NULL);
    // std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    // if(ret == (uint32_t)-1){
    //     dbPool->DbReleaseConn(id);
    //     return (void*)-1;
    // }

    std::cout<<"update success"<<std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    dbPool->DbReleaseConn(id);
    
    return NULL;
}

void* deleteFunc(void *arg)
{
    if(NULL == arg)
    {
        std::cout<<"DBPool deleteFunc is null!!!"<<std::endl;
        return NULL;
    }

    /*
        deleteע��㣺
            û�����������󣬱Ƚϼ�.
    */

    auto dbPool = (DBPool*)arg;
    auto id = dbPool->DbGetConn();

    /*
        1.�����ɾ����
            #������ɾ���ֻ�����9��β��Ů����Ϣ��
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
        2.����ɾ����(��һ��δ��������⣺�������ɾ��ʱ����Ȼִ�гɹ�������Ӱ����������0�����ݿ�����ݲ�δ���ı䣻����ֱ����navicatִ����û�����⡣)
            #������ɾ�����޼ɵ�Ů���ѵ���Ϣ��
            DELETE b
            FROM beauty b
            INNER JOIN boys bo ON b.`boyfriend_id` = bo.`id`
            WHERE bo.`boyName`='���޼�';
    */
    std::string sql2 = "DELETE b FROM beauty b INNER JOIN boys bo ON b.`boyfriend_id` = bo.`id` WHERE bo.`boyName`='���޼�';";
    auto ret = dbPool->execDelete(id, sql2.c_str(), "set names gbk");
    std::cout<<"mysql_affected_rows: "<<ret<<std::endl;
    if((uint32_t)-1 == ret){
        std::cout<<"execDelete failed."<<std::endl;
        dbPool->DbReleaseConn(id);
        return (void*)-1;
    }

    std::cout<<"execDelete success"<<std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    dbPool->DbReleaseConn(id);

    return NULL;    
}

int main(){

	printf("main ������ʼ����...\n");

    MYSQLNAMESPACE::DBConnInfo connInfo;
    connInfo.dbName = "girls";
    connInfo.host = "127.0.0.1";
    connInfo.passwd = "123456";
    connInfo.port = 3306;
    connInfo.user = "root";
    DBPool dbPool(connInfo);

	//1.�������ӳ�
    auto ret = dbPool.DbCreate(10, 20);
    if(!ret){
        printf("DbCreate failed.\n");
        return -1;
    }


    signal(SIGINT, signal_ctrlc);

	//2.����װ�ش�����������
	std::vector < std::packaged_task<void*(void*)>> mytasks;
	std::packaged_task<void*(void*)> t1(SelectFunc);
	std::packaged_task<void*(void*)> t2(InsertFunc);
	std::packaged_task<void*(void*)> t3(updateFunc);
	std::packaged_task<void*(void*)> t4(deleteFunc);
	mytasks.push_back(std::move(t1));
	mytasks.push_back(std::move(t2));
	mytasks.push_back(std::move(t3));
	mytasks.push_back(std::move(t4));

	//3.ѭ�������߳�,������õ�������Ϊ�̺߳������е���
	for (auto it = mytasks.begin(); it != mytasks.end(); it++) {
		std::thread th(std::move(*it), (void*)&dbPool);//��ȡԭ���Ĵ������,ͬ����Ҫ��ȡ����Ȩ���ܳɹ���ȡ
		if (th.joinable()) {//���������������join����detach
			/* �����и�����,detach����߳�,�������߳��˳���,�������̻߳�������,
			��ô�߳�������������dbPool���Ե�ַ������,�᲻�ᱨ�δ���? 
			������������Թ�,������SelectFunc˯��30s(�����в���,debugģʽ�ᵼ���ж�),
			�������߳��˳�,detach�����߳�ûɶ��Ӧ,��û�����δ���.
			����ע��,��Ŀ�в�Ҫд�����ֲ�ȷ���ԵĴ���.
			����һ�㿪�����˶���ʹ��joinȥʵ��,��������һ�㲻���������. */
			th.detach();
			printf("�߳�����Ϊdetach����...\n");
		}
	}

	
	while (!bExit) {
		printf("sleep 1s, ctrl+c exit...\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	//4.�������ӳ�,��Ҫ�ǻ��յ����߳�
	ret = dbPool.DbDestroy();
	printf("DbDestroy finish.\n");
	printf("���߳��˳�\n");

    return 0;
}