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
    void push_Task(const CxpTask& task);

    CxpTask pop_Task();

    int get_size();

};

class CxpThreadPool : public boost::noncopyable
{
public:
    CxpThreadPool(int num):m_threadNum(num),is_run(false),is_suspend(true),m_run_thread(0),m_read_queue(0),trxcount(0)
    {

    }
    ~CxpThreadPool(){

        stop();

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


    void run();

    void wait();

    uint64_t trxcount;


public:


    //停止线程池
    void stop();

    void init();

    void pause();

    void resume();
    //添加任务
    void AddNewTask(const CxpTask& task);

    int get_thread_count();

    uint64_t get_readqueue_size();

    uint64_t get_writequeue_size();

};



static CxpThreadPool threadpool(16);
