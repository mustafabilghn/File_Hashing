#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <unordered_map>

namespace fs = std::filesystem;

std::mutex output_mutex;
std::ofstream output_file("hashes_v3.txt");
std::atomic<int> active_threads(0);

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers)
            worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

std::string compute_hash(const std::string& content) {
    std::hash<std::string> hasher;
    size_t hash_val = hasher(content);
    return std::to_string(hash_val);
}

void process_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string hash = compute_hash(content);

    std::lock_guard<std::mutex> lock(output_mutex);
    output_file << path << ": " << hash << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory>\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<fs::path> files;
    for (auto& p : fs::recursive_directory_iterator(argv[1])) {
        if (fs::is_regular_file(p)) {
            files.push_back(p.path());
        }
    }

    ThreadPool pool(std::thread::hardware_concurrency());
    for (const auto& file : files) {
        pool.enqueue([file]() {
            process_file(file);
        });
    }

    output_file.close();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
    std::cout << "Hash results written to hashes_v3.txt\n";

    return 0;
}
