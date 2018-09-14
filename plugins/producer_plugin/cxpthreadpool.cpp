#include "cxpthreadpool.h"
#include <fc/log/logger.hpp>

void CxpTaskQueue::push_Task(const CxpTask& task)
{
     m_taskQueue.push(task);
}
CxpTask CxpTaskQueue::pop_Task()
{
    //指向队列首部
    CxpTask task(m_taskQueue.front());
    //出队列
    m_taskQueue.pop();
    return task;
}
int CxpTaskQueue::get_size()
{
     return m_taskQueue.size();
}


void CxpThreadPool::run()
{

    while(is_run)
    {
        //一直处理线程池的任务
        //加上互斥锁
        {

            //ilog("run1 m_read_queue:${a} size:${b} write_queue:${c}",("a",m_read_queue)("b",m_taskQueue[m_read_queue].get_size())("c",m_taskQueue[(m_read_queue+1)%2].get_size()));
            boost::unique_lock<boost::mutex> lock(m_mutex);
            //如果队列中没有任务，则等待互斥锁
            if(m_taskQueue[0].get_size()==0 && m_taskQueue[1].get_size()==0)
            {
                m_cond.wait(lock);
            }

            if(is_suspend)
            {

                m_cond.wait(lock);
            }

            //ilog("run2 m_read_queue:${a} size:${b} write_queue:${c}",("a",m_read_queue)("b",m_taskQueue[m_read_queue].get_size())("c",m_taskQueue[(m_read_queue+1)%2].get_size()));
            //如果读取队列中没有任务，则切换队列
            if(m_taskQueue[m_read_queue].get_size()==0)
            {
                boost::unique_lock<boost::mutex> lock(m_switch_queue_mutex);
                m_read_queue=(++m_read_queue)%2;
            }

           //ilog("run3 m_read_queue:${a} size:${b} write_queue:${c}",("a",m_read_queue)("b",m_taskQueue[m_read_queue].get_size())("c",m_taskQueue[(m_read_queue+1)%2].get_size()));
           if(m_taskQueue[m_read_queue].get_size()==0)
               continue;

                CxpTask task = m_taskQueue[m_read_queue].pop_Task();
                m_run_thread++;

                lock.unlock();
                task();
                m_run_thread--;


        }

    }
}

void CxpThreadPool::wait()
{
    m_threadGroup.join_all();//等待线程池处理完成！
}

void CxpThreadPool::stop()
{
    is_run = false;
    is_suspend=false;
    m_cond.notify_all();
    wait();
}

void CxpThreadPool::init()
{
    if(m_threadNum <= 0) return;
    is_run = true;
    for (int i=0;i<m_threadNum;i++)
    {
        //生成多个线程，绑定run函数，添加到线程组
        m_threadGroup.add_thread(
            new boost::thread(boost::bind(&CxpThreadPool::run,this)));
    }
}

void CxpThreadPool::pause()
{

    ilog("push trxcount:${a}",("a",trxcount));
    trxcount=0;
    is_suspend=true;
    boost::unique_lock<boost::mutex> lock(m_mutex);


    while(m_run_thread>0)
    {

    }

}

void CxpThreadPool::resume()
{
    is_suspend=false;

    if(m_taskQueue[0].get_size()>0 || m_taskQueue[1].get_size()>0)
        m_cond.notify_all();

}

//添加任务
void CxpThreadPool::AddNewTask(const CxpTask& task)
{

    boost::unique_lock<boost::mutex> lock(m_switch_queue_mutex);
    m_taskQueue[(m_read_queue+1)%2].push_Task(task);
    trxcount++;
    lock.unlock();

    if(!is_suspend&& is_run)
        m_cond.notify_one();
}

int CxpThreadPool::get_thread_count(){return m_threadNum;}

uint64_t CxpThreadPool::get_readqueue_size()
{
    return m_taskQueue[m_read_queue].get_size();
}


uint64_t CxpThreadPool::get_writequeue_size()
{
    return m_taskQueue[(m_read_queue+1)%2].get_size();
}
