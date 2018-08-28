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
    CxpThreadPool(int num):m_threadNum(num),is_run(false),is_suspend(true),m_run_thread(0){}
private:
    //任务队列

    boost::mutex m_mutex;//互斥锁

    boost::condition_variable_any m_cond;//条件变量
    CxpTaskQueue m_taskQueue;
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
                if(is_suspend)
                {
                   // std::cout << "run is_suspend m_cond.wait"<<m_run_thread<<std::endl;
                    m_cond.wait(lock);
                }
                //如果队列中没有任务，则等待互斥锁
                if(m_taskQueue.get_size()>0)
                {
                    CxpTask task = m_taskQueue.pop_Task();
                    lock.unlock();

                    m_run_thread++;
                   // std::cout << "run m_run_thread1:"<<m_run_thread<<std::endl;
                    task();
                    m_run_thread--;

                   // std::cout << "run m_run_thread2:"<<m_run_thread<<std::endl;
                }
                 else
                {
                   // std::cout << "run size==0 m_cond.wait"<<m_run_thread<<std::endl;
                    m_cond.wait(lock);
                }


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

        //std::cout << "pause start m_run_thread:"<<m_run_thread<<std::endl;

        while(m_run_thread>0)
        {

        }



       // std::cout << "pause  end m_run_thread:"<<m_run_thread<<std::endl;
    }

    void resume()
    {
        is_suspend=false;
        boost::unique_lock<boost::mutex> lock(m_mutex);
        if(m_taskQueue.get_size()>=m_threadNum)
            m_cond.notify_all();
        else if(m_taskQueue.get_size()>0)
            m_cond.notify_one();
    }

    //添加任务
    void AddNewTask(const CxpTask& task)
    {
        boost::unique_lock<boost::mutex> lock(m_mutex);
        m_taskQueue.push_Task(task);
        if(!is_suspend&& is_run)
            m_cond.notify_one();

        // std::cout<<"AddNewTask taskQueue size="<<m_taskQueue.get_size()<<std::endl;
    }


};


static uint64_t loopcount=0;
CxpThreadPool threadpool(1);
