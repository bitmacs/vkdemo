#pragma once

#include <functional>
#include <mutex>
#include <vector>

struct TaskSystem {
    std::vector<std::function<void()>> tasks;
    std::mutex mutex;
};

void push_task(TaskSystem *task_system, std::function<void()> &&task);

void exec_tasks(TaskSystem *task_system);
