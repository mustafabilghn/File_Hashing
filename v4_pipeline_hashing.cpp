#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>

namespace fs = std::filesystem;

struct FileData {
    fs::path path;
    std::string content;
};

struct HashResult {
    fs::path path;
    std::string hash;
};

std::queue<fs::path> file_queue;
std::queue<FileData> content_queue;
std::queue<HashResult> result_queue;

std::mutex file_mutex, content_mutex, result_mutex;
std::condition_variable file_cv, content_cv, result_cv;
std::atomic<bool> done_reading{false};
std::atomic<bool> done_hashing{false};

std::string compute_hash(const std::string& content) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(content));
}

void reader_thread() {
    while (true) {
        fs::path path;
        {
            std::unique_lock<std::mutex> lock(file_mutex);
            if (file_queue.empty()) break;
            path = std::move(file_queue.front());
            file_queue.pop();
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) continue;

        std::string content((std::istreambuf_iterator<char>(file)), {});

        {
            std::unique_lock<std::mutex> lock(content_mutex);
            content_queue.push({std::move(path), std::move(content)});
        }
        content_cv.notify_one();
    }
    done_reading = true;
    content_cv.notify_all();
}

void hasher_thread() {
    while (true) {
        FileData data;
        {
            std::unique_lock<std::mutex> lock(content_mutex);
            content_cv.wait(lock, [] { return !content_queue.empty() || done_reading.load(); });
            if (content_queue.empty() && done_reading) break;
            if (content_queue.empty()) continue;

            data = std::move(content_queue.front());
            content_queue.pop();
        }

        std::string hash = compute_hash(data.content);

        {
            std::unique_lock<std::mutex> lock(result_mutex);
            result_queue.push({std::move(data.path), std::move(hash)});
        }
        result_cv.notify_one();
    }
}

void writer_thread(std::ofstream& output_file) {
    while (true) {
        HashResult result;
        {
            std::unique_lock<std::mutex> lock(result_mutex);
            result_cv.wait(lock, [] { return !result_queue.empty() || done_hashing.load(); });
            if (result_queue.empty() && done_hashing) break;
            if (result_queue.empty()) continue;

            result = std::move(result_queue.front());
            result_queue.pop();
        }

        output_file << result.path << ": " << result.hash << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <directory>\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();

    try {
        for (auto& p : fs::recursive_directory_iterator(argv[1])) {
            if (fs::is_regular_file(p)) {
                file_queue.push(p.path());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Directory iteration error: " << e.what() << "\n";
        return 1;
    }

    std::ofstream output_file("hashes_v4.txt");
    if (!output_file) {
        std::cerr << "Error opening output file.\n";
        return 1;
    }

    std::thread reader(reader_thread);

    int num_hasher_threads = std::max(1u, std::thread::hardware_concurrency() - 2);
    std::vector<std::thread> hashers;
    for (int i = 0; i < num_hasher_threads; ++i) {
        hashers.emplace_back(hasher_thread);
    }

    std::thread writer(writer_thread, std::ref(output_file));

    reader.join();
    for (auto& t : hashers) t.join();
    done_hashing = true;
    result_cv.notify_all();
    writer.join();
    output_file.close();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds\n";
    std::cout << "Hash results written to hashes_v4.txt\n";

    return 0;
}
