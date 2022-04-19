#pragma once

#include <chrono>
#include <functional>
#include <vector>


using namespace std::chrono_literals;
template <typename task_clock_t>
class TimedTaskManager {
    struct TimerTask {
        std::chrono::milliseconds execution_period;
        std::function<void(void)> task;
        typename task_clock_t::time_point last_execution{task_clock_t::now() - execution_period};
    };
public:
    size_t launch() {
        auto current_time = task_clock_t::now();
        size_t tasks_launched = 0;
        for (auto& task : m_tasks) {
            auto since_last_execution = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - task.last_execution
            );
            if (since_last_execution > task.execution_period) {
                task.task();
                ++tasks_launched;
            }          
        }
        return tasks_launched;
    }

    template<typename F>
    void add_task(
            F&& func, 
            std::chrono::milliseconds launch_interval, 
            std::chrono::milliseconds first_delay = 0ms
    ) {
        m_tasks.emplace_back(
                launch_interval, 
                std::forward<F>(func), 
                task_clock_t::now() + first_delay
        );
    }

private:
    std::vector<TimerTask> m_tasks;
};

