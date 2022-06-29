#ifndef __MYTIME__H__
#define __MYTIME__H__

#include <ctime>
#include <sys/time.h>
#include <chrono>
#include <iostream>
namespace MYSQLNAMESPACE
{
     typedef struct {
        unsigned short year;
        unsigned char  mon;
        unsigned char  day;
    } DATE_T;               /* 日期 */

    typedef struct {
        unsigned char  hour;
        unsigned char  min;
        unsigned char  sec;
        unsigned int   usec;
    } TIME_T;               /* 时间 */

    typedef struct {
        DATE_T         date;
        TIME_T         time;
    } SYSTIME_T;            /* 日期+时间 */

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
            // C++标准库chrono时间获取时间戳
            // details see https://blog.csdn.net/m0_37263637/article/details/95059209

            // time_since_epoch用来得到当前时间到1970年1月1日00:00的时间戳，故下面均是获取1970年1月1日到现在的时间戳
            std::time_t getTimeStampSec()
            {
                std::chrono::time_point<std::chrono::system_clock,std::chrono::seconds> tp = 
                    std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());//获取当前时间点
                std::time_t timestamp = tp.time_since_epoch().count(); //计算距离1970-1-1,00:00的时间长度
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
                auto sec = getTimeStampSec();// 秒
                auto millsec = getTimeStampMillSec();// 毫秒
                auto microsec = getTimeStampMicroSec();// 微秒

                std::cout<<"debugChronoTimeStamp(): "<<"秒："<<sec<<"， 毫秒："<<millsec<<"， 微秒："<<microsec<<std::endl;
            }

            // C++标准库提供了steady clock不会回调的时钟.
            // 因为上面是系统时钟，可能会被人为修改造成时钟回调，若作为连接池的key，有可能会造成连接池的混乱
            std::time_t getSteadyTimeStampSec()
            {
                std::chrono::time_point<std::chrono::steady_clock,std::chrono::seconds> tp = 
                    std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::steady_clock::now());//获取当前时间点
                std::time_t timestamp = tp.time_since_epoch().count(); //计算距离1970-1-1,00:00的时间长度
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
                auto sec = getSteadyTimeStampSec();// 秒
                auto millsec = getSteadyTimeStampMillSec();// 毫秒
                auto microsec = getSteadyTimeStampMicroSec();// 微秒

                std::cout<<"debugChronoSteadyTimeStamp(): "<<"秒："<<sec<<"， 毫秒："<<millsec<<"， 微秒："<<microsec<<std::endl;
            }

#else
            //"C++11 is not supported";
#endif

#ifdef __linux__
            // linux环境下：gettimeofday是1970年1月1日到现在的时间戳，故下面都是获取1970年1月1日到现在的时间戳.
            // 与上面的方法是一样的，只不过上面使用C++标准库实现，这里使用Linux函数实现.
            // 获取秒的时间戳
            std::time_t getTimeStampSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return tv.tv_sec;
            }

            // 获取毫秒的时间戳
            // 加上long long强制转换，是防止32位机器的long是4字节，导致溢出.
            std::time_t getTimeStampMillSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return (long long)(tv.tv_sec * 1000) + (long long)(tv.tv_usec / 1000);
            }

            // 获取微秒的时间戳
            std::time_t getTimeStampMicroSecUnix(){
                struct timeval tv;
                gettimeofday(&tv, NULL);
                return (long long)(tv.tv_sec * 1000000) + (long long)tv.tv_usec;
            }

            void debugUnixTimeStamp(){
                auto sec = getTimeStampSecUnix();// 秒
                auto millsec = getTimeStampMillSecUnix();// 毫秒
                auto microsec = getTimeStampMicroSecUnix();// 微秒

                std::cout<<"debugUnixTimeStamp(): "<<"秒："<<sec<<"， 毫秒："<<millsec<<"， 微秒："<<microsec<<std::endl;
            }

#endif

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

        private:
            time_t m_start;
    };

}


#endif