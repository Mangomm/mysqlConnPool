#ifndef __MYTIME__H__
#define __MYTIME__H__

#include <ctime>
#if defined(__linux__) || defined(__linux)
#include <sys/time.h>
#endif
#include <chrono>
#include <iostream>
namespace MYSQLNAMESPACE
{
     typedef struct {
        unsigned short year;
        unsigned char  mon;
        unsigned char  day;
    } DATE_T;               /* ���� */

    typedef struct {
        unsigned char  hour;
        unsigned char  min;
        unsigned char  sec;
        unsigned int   usec;
    } TIME_T;               /* ʱ�� */

    typedef struct {
        DATE_T         date;
        TIME_T         time;
    } SYSTIME_T;            /* ����+ʱ�� */

    class MyTime
    {
        public:
            MyTime(){
                time(&m_start);
            }
            ~MyTime(){
            }

            time_t elapse(){
                time_t now; 
                time(&now);
                return now - m_start;
            }

            void now()
            {
                time(&m_start);
            }
            
#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900)
            //"C++11 is supported"
            // C++��׼��chronoʱ���ȡʱ���
            // details see https://blog.csdn.net/m0_37263637/article/details/95059209

            // time_since_epoch�����õ���ǰʱ�䵽1970��1��1��00:00��ʱ�������������ǻ�ȡ1970��1��1�յ����ڵ�ʱ���
            std::time_t getTimeStampSec()
            {
                std::chrono::time_point<std::chrono::system_clock,std::chrono::seconds> tp = 
                    std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());//��ȡ��ǰʱ���
                std::time_t timestamp = tp.time_since_epoch().count(); //�������1970-1-1,00:00��ʱ�䳤��
                return timestamp;
            }

            std::time_t getTimeStampMillSec()
            {
                std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> tp = 
                    std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
                std::time_t timestamp = tp.time_since_epoch().count();
                return timestamp;
            }

            std::time_t getTimeStampMicroSec()
            {
                std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds> tp = 
                    std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
                std::time_t timestamp = tp.time_since_epoch().count();
                return timestamp;
            }

            void debugChronoTimeStamp(){
                auto sec = getTimeStampSec();// ��
                auto millsec = getTimeStampMillSec();// ����
                auto microsec = getTimeStampMicroSec();// ΢��

                //std::cout<<"debugChronoTimeStamp(): "<<"�룺"<<sec<<"�� ���룺"<<millsec<<"�� ΢�룺"<<microsec<<std::endl;
            }

            // C++��׼���ṩ��steady clock����ص���ʱ��.
            // ��Ϊ������ϵͳʱ�ӣ����ܻᱻ��Ϊ�޸����ʱ�ӻص�������Ϊ���ӳص�key���п��ܻ�������ӳصĻ���
            std::time_t getSteadyTimeStampSec()
            {
                std::chrono::time_point<std::chrono::steady_clock,std::chrono::seconds> tp = 
                    std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::steady_clock::now());//��ȡ��ǰʱ���
                std::time_t timestamp = tp.time_since_epoch().count(); //�������1970-1-1,00:00��ʱ�䳤��
                return timestamp;
            }

            std::time_t getSteadyTimeStampMillSec()
            {
                std::chrono::time_point<std::chrono::steady_clock,std::chrono::milliseconds> tp = 
                    std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
                std::time_t timestamp = tp.time_since_epoch().count();
                return timestamp;
            }

            std::time_t getSteadyTimeStampMicroSec()
            {
                std::chrono::time_point<std::chrono::steady_clock, std::chrono::microseconds> tp = 
                    std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
                std::time_t timestamp = tp.time_since_epoch().count();
                return timestamp;
            }

            void debugChronoSteadyTimeStamp(){
                auto sec = getSteadyTimeStampSec();// ��
                auto millsec = getSteadyTimeStampMillSec();// ����
                auto microsec = getSteadyTimeStampMicroSec();// ΢��

                //std::cout<<"debugChronoSteadyTimeStamp(): "<<"�룺"<<sec<<"�� ���룺"<<millsec<<"�� ΢�룺"<<microsec<<std::endl;
            }

#else
            //"C++11 is not supported";
#endif

#ifdef __linux__
            // linux�����£�gettimeofday��1970��1��1�յ����ڵ�ʱ����������涼�ǻ�ȡ1970��1��1�յ����ڵ�ʱ���.
            // ������ķ�����һ���ģ�ֻ��������ʹ��C++��׼��ʵ�֣�����ʹ��Linux����ʵ��.
            // ��ȡ���ʱ���
            std::time_t getTimeStampSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return tv.tv_sec;
            }

            // ��ȡ�����ʱ���
            // ����long longǿ��ת�����Ƿ�ֹ32λ������long��4�ֽڣ��������.
            std::time_t getTimeStampMillSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return (long long)(tv.tv_sec * 1000) + (long long)(tv.tv_usec / 1000);
            }

            // ��ȡ΢���ʱ���
            std::time_t getTimeStampMicroSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return (long long)(tv.tv_sec * 1000000) + (long long)tv.tv_usec;
            }

            void debugUnixTimeStamp(){
                auto sec = getTimeStampSecUnix();// ��
                auto millsec = getTimeStampMillSecUnix();// ����
                auto microsec = getTimeStampMicroSecUnix();// ΢��

                std::cout<<"debugUnixTimeStamp(): "<<"�룺"<<sec<<"�� ���룺"<<millsec<<"�� ΢�룺"<<microsec<<std::endl;
            }



            bool getUtcDate(SYSTIME_T *sys_t){
                if(!sys_t){
                    return false;
                }

                memset(sys_t, 0x00, sizeof(SYSTIME_T));
                struct timeval tv;
                struct tm tm;

                gettimeofday(&tv, NULL);
                localtime_r(&tv.tv_sec, &tm);

                sys_t->date.year = tm.tm_year + 1900;
                sys_t->date.mon = tm.tm_mon + 1;
                sys_t->date.day = tm.tm_mday;
                sys_t->time.hour = tm.tm_hour;
                sys_t->time.min = tm.tm_min;
                sys_t->time.sec = tm.tm_sec;
                sys_t->time.usec = tv.tv_usec;

                return true;
            }

            void debugUtcDate(){
                SYSTIME_T systime;
                if(!getUtcDate(&systime)){
                    return;
                }

                printf("year: %d, month: %d, day: %d, hour: %d, min: %d, sec: %d, usec: %lld\n", 
                    (int)systime.date.year, (int)systime.date.mon, (int)systime.date.day, (int)systime.time.hour, (int)systime.time.min, (int)systime.time.sec, (long long)systime.time.usec);
            }
#endif

        private:
            time_t m_start;
    };

}


#endif