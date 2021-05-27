#ifndef __MYTIME__H__
#define __MYTIME__H__

#include <ctime>
namespace MYSQLNAMESPACE
{
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

        private:
            time_t m_start;
    };
}


#endif