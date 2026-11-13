#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "emailTask.h"

namespace notifyService {

/**
 * @brief 邮件异步发送工作类
 *
 * 工作线程持续从任务队列获取 EmailTask 并发送，避免阻塞 RPC 主线程；
 * 停止时先把队列中剩余任务发送完，再退出工作线程。
 */
class EmailWorker {
public:
    EmailWorker() = default;

    ~EmailWorker();

    EmailWorker(const EmailWorker&) = delete;
    EmailWorker& operator=(const EmailWorker&) = delete;

    /**
     * @brief 启动邮件发送线程
     */
    void start();

    /**
     * @brief 停止邮件发送线程（排空队列后退出）
     */
    void stop();

    /**
     * @brief 添加邮件发送任务到队列
     * @param task 邮件任务
     * @return 添加成功返回 true, 工作线程未启动返回 false
     */
    bool addTask(EmailTask task);

private:
    /**
     * @brief 工作线程主循环：从队列取任务并发送
     */
    void workerLoop();

private:
    std::deque<EmailTask> _taskQueue;        // 邮件任务队列
    std::mutex _mutex;                       // 保护任务队列的互斥锁
    std::condition_variable _condition;      // 通知工作线程有任务可处理或需要退出
    std::thread _workerThread;               // 邮件发送线程
    std::atomic<bool> _running{false};       // 工作线程运行标志
};

} // namespace notifyService
