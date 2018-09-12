#pragma once
#include <iostream>
#include <queue>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>



typedef boost::function<void(void)> CxpTask;
class CxpTaskQueue : boost::noncopyable
{
private:
    std::queue<CxpTask> m_taskQueue;

public:
    void push_Task(const CxpTask& task)
    {
         m_taskQueue.push(task);
    }
    CxpTask pop_Task()
    {
        //指向队列首部
        CxpTask task(m_taskQueue.front());
        //出队列
        m_taskQueue.pop();
        return task;
    }
    int get_size()
    {
         return m_taskQueue.size();
    }

};

class CxpThreadPool : public boost::noncopyable
{
public:
    CxpThreadPool(int num):m_threadNum(num),is_run(false),is_suspend(true),m_run_thread(0),m_read_queue(0)
    {

    }
private:
    //任务队列

    boost::mutex m_mutex;//互斥锁

    boost::mutex m_switch_queue_mutex;

    boost::condition_variable_any m_cond;//条件变量
    CxpTaskQueue m_taskQueue[2];
    std::atomic<uint32_t> m_read_queue;
    //线程组
    boost::thread_group m_threadGroup;
    int m_threadNum;

    std::atomic<bool> is_run;
    std::atomic<bool> is_suspend;
    std::atomic<uint32_t> m_run_thread;


    void run()
    {

        while(is_run)
        {
            //一直处理线程池的任务
            //加上互斥锁
            {
                boost::unique_lock<boost::mutex> lock(m_mutex);
				 //如果队列中没有任务，则等待互斥锁
                if(m_taskQueue[0].get_size()==0 && m_taskQueue[1].get_size()==0)
                {
                    m_cond.wait(lock);
                }
                if(is_suspend)
                {
                   // std::cout << "run is_suspend m_cond.wait"<<m_run_thread<<std::endl;
                    m_cond.wait(lock);
                }
               
                //如果读取队列中没有任务，则切换队列
                if(m_taskQueue[m_read_queue].get_size()==0)
                {
                    boost::unique_lock<boost::mutex> lock(m_switch_queue_mutex);
                    m_read_queue=(++m_read_queue)%2;
                }

                    //std::cout << "run m_read_queue:"<<m_read_queue<<"queue size:"<<m_taskQueue[m_read_queue].get_size()<<std::endl;
                   // std::cout << "run m_write_queue:"<<(m_read_queue+1)%2<<"queue size:"<<m_taskQueue[(m_read_queue+1)%2].get_size()<<std::endl;
                    CxpTask task = m_taskQueue[m_read_queue].pop_Task();
                    m_run_thread++;
                    lock.unlock();
                    task();
                    m_run_thread--;

            }

        }
    }
    void wait()
    {
        m_threadGroup.join_all();//等待线程池处理完成！
    }

public:
    ~CxpThreadPool(){

        stop();

    }

    //停止线程池
    void stop()
    {
        is_run = false;
        is_suspend=false;
        m_cond.notify_all();
        wait();
    }

    void init()
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

    void pause()
    {
        is_suspend=true;

        boost::unique_lock<boost::mutex> lock(m_mutex);
       // std::cout << "pause start m_run_thread:"<<m_run_thread<<std::endl;

        while(m_run_thread>0)
        {

        }



       // std::cout << "pause  end m_run_thread:"<<m_run_thread<<std::endl;
    }

    void resume()
    {
        is_suspend=false;

        if(m_taskQueue[0].get_size()>0 || m_taskQueue[1].get_size()>0)
            m_cond.notify_all();

    }

    //添加任务
    void AddNewTask(const CxpTask& task)
    {

        boost::unique_lock<boost::mutex> lock(m_switch_queue_mutex);
        m_taskQueue[(m_read_queue+1)%2].push_Task(task);
        //std::cout << "AddNewTask m_write_queue:"<<(m_read_queue+1)%2<<"queue size:"<<m_taskQueue[(m_read_queue+1)%2].get_size()<<std::endl;
        lock.unlock();

        if(!is_suspend&& is_run)
            m_cond.notify_one();
    }

    int get_thread_count(){return m_threadNum;}

    uint64_t get_readqueue_size()
    {
        return m_taskQueue[m_read_queue].get_size();
    }


    uint64_t get_writequeue_size()
    {
        return m_taskQueue[(m_read_queue+1)%2].get_size();
    }


};



CxpThreadPool threadpool(1);
