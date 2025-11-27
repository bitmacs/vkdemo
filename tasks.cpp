#include "tasks.h"

void push_task(TaskSystem *task_system, std::function<void()> &&task) {
    std::lock_guard<std::mutex> lock(task_system->mutex);
    task_system->tasks.emplace_back(std::move(task));
}

void exec_tasks(TaskSystem *task_system) {
    std::lock_guard<std::mutex> lock(task_system->mutex);
    for (const auto &task: task_system->tasks) {
        task();
    }
    task_system->tasks.clear();
}
