#ifndef __MYLOCK__H__
#define __MYLOCK__H__

//���ļ����ڻ�ɾ��,���Ժ�ʹ��C++11����,�����ƽ̨ʹ��
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
            //Ҳ����ʹ������,�����вι��츳ֵ.������д����lock_guard��unique_lock
            //MyLock m_lock;
            MyLock &m_lock;
    };

    //ʹ��ָ��ķ�ʽʵ��RallLock
    class CLock
    {
        public:
            CLock(pthread_mutex_t *pMutex)
            {
                m_pMutex = pMutex;
                pthread_mutex_lock(m_pMutex); //����������
            }
            ~CLock()
            {
                pthread_mutex_unlock(m_pMutex); //����������
            }
        private:
            pthread_mutex_t *m_pMutex;
        
    };

#endif

}

#endif