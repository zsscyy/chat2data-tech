#include "emailWorker.h"

#include <utility>

#include <bite_scaffold/log.h>

namespace notifyService {

EmailWorker::~EmailWorker() {
    stop();
}

void EmailWorker::start() {
    if (_running.load()) {
        WRN("邮件发送线程已启动，忽略重复启动");
        return;
    }

    _running.store(true);
    _workerThread = std::thread(&EmailWorker::workerLoop, this);
    INF("邮件发送线程已启动");
}

void EmailWorker::stop() {
    if (!_running.load()) {
        if (_workerThread.joinable()) {
            _workerThread.join();
        }
        return;
    }

    // 置位退出标志后，工作线程会先把队列中剩余任务发送完再退出
    _running.store(false);
    _condition.notify_all();
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
    INF("邮件发送线程已停止");
}

bool EmailWorker::addTask(EmailTask task) {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_running.load()) {
            WRN("邮件发送线程未运行，拒绝邮件任务, toEmail: {}", task._toEmail);
            return false;
        }
        _taskQueue.push_back(std::move(task));
    }
    _condition.notify_one();
    return true;
}

void EmailWorker::workerLoop() {
    while (true) {
        EmailTask task;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.wait(lock, [this] { return !_running.load() || !_taskQueue.empty(); });

            // 已请求停止且队列排空，结束工作线程
            if (!_running.load() && _taskQueue.empty()) {
                return;
            }

            task = std::move(_taskQueue.front());
            _taskQueue.pop_front();
        }

        // 发送动作放在锁外执行，避免长时间持锁阻塞任务入队
        if (!task._sender) {
            ERR("邮件任务缺少发送器, toEmail: {}", task._toEmail);
            continue;
        }
        task._sender->sendEmail(task._toEmail, task._subject, task._content);
    }
}

} // namespace notifyService
