#pragma once
#include <iostream>
#include <queue>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/abi_def.hpp>
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <fc/io/json.hpp>
#include <eosio/chain/asset.hpp>
#include "coinxp.match.abi.hpp"
#include "coinxp.bank.abi.hpp"

using namespace eosio::chain;
typedef boost::function<void(void)> CxpTask;


template <class T>
class CxpTaskQueue : boost::noncopyable
{
private:
    std::queue<T> m_taskQueue;

public:
    void push_Task(const T& task);

    T pop_Task();

    int get_size();

};

class CxpThreadPool : public boost::noncopyable
{
public:
    CxpThreadPool()
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
    CxpTaskQueue<CxpTask> m_taskQueue[2];
    CxpTaskQueue<fc::variant> m_taskvarQueue[2];
    CxpTaskQueue<string> m_taskstringQueue[2];
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

    void init(int num);

    void pause();
    void setsuspend(bool suspend){is_suspend=suspend;}
    void resume();
    //添加任务
    void AddNewTask(const CxpTask& task,const fc::variant var,string name);

    int get_thread_count();

    uint64_t get_readqueue_size();

    uint64_t get_writequeue_size();

};


struct CoinPair {
    symbol_code _currency;
    symbol_code _commodity;

//    bool operator ==(const CoinPair& coin)
//    {
//        return (coin._currency==_currency && coin._commodity==_commodity);

//    }

};



class CxpTransaction : public boost::noncopyable
{
public:
    CxpTransaction(int num):m_threadNum(num),is_run(false),is_suspend(false),m_run_thread(0),m_read_queue(0),trxcount(0)
    {

    }
    ~CxpTransaction(){

        stop();

    }
private:
    //任务队列

    boost::mutex m_mutex;//互斥锁

    boost::mutex m_switch_queue_mutex;

    boost::condition_variable_any m_cond;//条件变量
    CxpTaskQueue<CxpTask> m_taskQueue[2];
    CxpTaskQueue<packed_transaction_ptr> m_transactionptrQueue[2];
    std::atomic<uint32_t> m_read_queue;
    //线程组
    boost::thread_group m_threadGroup;
    int m_threadNum;

    std::atomic<bool> is_run;
    std::atomic<bool> is_suspend;
    std::atomic<uint32_t> m_run_thread;
    eosio::chain::abi_serializer  cxp_match_serializer;
    eosio::chain::abi_serializer  cxp_bank_serializer;
    fc::microseconds abi_serializer_max_time;

    void run();

    void wait();
    void dispatch_transaction(const CxpTask& task,const packed_transaction_ptr& trx);
    uint64_t trxcount;

    CxpThreadPool m_contractPool;

    std::map<CoinPair,std::unique_ptr<CxpThreadPool>> m_match_contractPool;

public:


    //停止线程池
    void stop();

    void init();

    void pause();

    void resume();
    //添加任务
    void AddNewTask(const CxpTask& task,const packed_transaction_ptr& trx);

    int get_thread_count();

    uint64_t get_readqueue_size();

    uint64_t get_writequeue_size();

};



static CxpTransaction threadpool(1);
