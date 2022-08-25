#ifndef __MYLOCK__H__
#define __MYLOCK__H__

//本文件后期会删除,锁以后使用C++11的锁,方便跨平台使用
#if defined(__linux__) || defined(__linux)
#include <pthread.h>
#endif


namespace MYSQLNAMESPACE{

#if defined(__linux__) || defined(__linux)
    class MyLock{
        public:
            MyLock(){
                pthread_mutex_init(&m_mutex, NULL);
            }
            ~MyLock(){
                pthread_mutex_destroy(&m_mutex);
            }

        public:
            void lock(){
                pthread_mutex_lock(&m_mutex);
            }    
            void unlock(){
                pthread_mutex_unlock(&m_mutex);
            }
        private:
            pthread_mutex_t m_mutex;
    };

    class RallLock{
        public:
            RallLock(MyLock &lock):m_lock(lock){
                m_lock.lock();
            }
            ~RallLock(){
                m_lock.unlock();
            }

        private:
            //也可以使用引用,利用有参构造赋值.我这样写类似lock_guard和unique_lock
            //MyLock m_lock;
            MyLock &m_lock;
    };

    //使用指针的方式实现RallLock
    class CLock
    {
        public:
            CLock(pthread_mutex_t *pMutex)
            {
                m_pMutex = pMutex;
                pthread_mutex_lock(m_pMutex); //加锁互斥量
            }
            ~CLock()
            {
                pthread_mutex_unlock(m_pMutex); //解锁互斥量
            }
        private:
            pthread_mutex_t *m_pMutex;
        
    };

#endif

}

#endif