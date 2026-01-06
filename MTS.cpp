#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

/* -------------------- TASK BASE CLASS -------------------- */
class Task {
protected:
    std::string name;
    int delayMs = 0;
    bool repeating = false;
    int repeatIntervalMs = 0;

public:
    Task(const std::string& _name) : name(_name) {}
    virtual ~Task() {}

    std::string getName() const { return name; }

    void setDelay(int ms) { delayMs = ms; }
    int getDelay() const { return delayMs; }

    void setRepeating(bool flag) { repeating = flag; }
    bool isRepeating() const { return repeating; }

    void setRepeatInterval(int ms) { repeatIntervalMs = ms; }
    int getRepeatInterval() const { return repeatIntervalMs; }

    virtual void execute() = 0;
};

/* -------------------- TASK TYPES -------------------- */
class PrintTask : public Task {
public:
    PrintTask(const std::string& name) : Task(name) {}
    void execute() override {
        std::cout << "PrintTask: Hello World" << std::endl;
    }
};

class WaitTask : public Task {
    int seconds;
public:
    WaitTask(int sec, const std::string& name)
        : Task(name), seconds(sec) {}

    void execute() override {
        std::cout << "WaitTask: Sleeping for " << seconds << " seconds" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
    }
};

class ComputeTask : public Task {
    int limit;
public:
    ComputeTask(int n, const std::string& name)
        : Task(name), limit(n) {}

    void execute() override {
        int sum = 0;
        for (int i = 1; i <= limit; ++i)
            sum += i;

        std::cout << "ComputeTask: Sum = " << sum << std::endl;
    }
};

/* -------------------- TASK EXECUTOR -------------------- */
class TaskExecutor {
private:
    std::vector<std::shared_ptr<Task>> tasks;
    std::atomic<bool> stopRequested{false};
    std::mutex coutMutex;

public:
    void addTask(const std::shared_ptr<Task>& task) {
        tasks.push_back(task);
    }

    void requestStop() {
        stopRequested.store(true);
    }

    void runAll() {
        std::vector<std::thread> workers;

        for (auto& task : tasks) {
            workers.emplace_back([this, task]() {
                {
                    std::lock_guard<std::mutex> lock(coutMutex);
                    std::cout << "[START] " << task->getName() << std::endl;
                }

                if (task->getDelay() > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(task->getDelay()));
                }

                if (task->isRepeating()) {
                    while (!stopRequested.load()) {
                        task->execute();
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(task->getRepeatInterval()));
                    }
                } else {
                    task->execute();
                }

                {
                    std::lock_guard<std::mutex> lock(coutMutex);
                    std::cout << "[END] " << task->getName() << std::endl;
                }
            });
        }

        for (auto& t : workers) {
            if (t.joinable())
                t.join();
        }
    }
};

/* -------------------- MAIN -------------------- */
int main() {
    auto executor = std::make_unique<TaskExecutor>();

    auto printTask = std::make_shared<PrintTask>("Printer");
    printTask->setDelay(1000);
    printTask->setRepeating(true);
    printTask->setRepeatInterval(500);

    auto waitTask = std::make_shared<WaitTask>(2, "Waiter");
    waitTask->setDelay(2000);

    auto computeTask = std::make_shared<ComputeTask>(100, "Calculator");
    computeTask->setDelay(3000);

    executor->addTask(printTask);
    executor->addTask(waitTask);
    executor->addTask(computeTask);

    // Run scheduler in separate thread
    std::thread schedulerThread([&executor]() {
        executor->runAll();
    });

    // Let tasks run for some time
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Request graceful shutdown
    executor->requestStop();

    if (schedulerThread.joinable())
        schedulerThread.join();

    std::cout << "Scheduler stopped gracefully." << std::endl;
    return 0;
}
