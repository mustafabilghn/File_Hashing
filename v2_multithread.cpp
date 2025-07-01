#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>

std::mutex file_mutex;

void hash_worker(const std::vector<std::filesystem::path>& files, int start, int end, std::ostringstream& local_result) {
    for (int i = start; i < end; ++i) {
        std::ifstream file(files[i], std::ios::binary);
        if (!file) continue;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        std::string contents = buffer.str();

        std::hash<std::string> hasher;
        size_t hash = hasher(contents);

        local_result << files[i].filename() << ": " << hash << '\n';
    }
}

int main(int argc, char* argv[]) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::string directory = (argc > 1) ? argv[1] : ".";
    std::vector<std::filesystem::path> files;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }

    int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4; 

    int chunk_size = files.size() / num_threads;
    std::vector<std::thread> threads;
    std::vector<std::ostringstream> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? files.size() : (i + 1) * chunk_size;
        threads.emplace_back(hash_worker, std::ref(files), start, end, std::ref(results[i]));
    }

    for (auto& t : threads) t.join();

    std::ofstream out("hashes_v2.txt");
    for (auto& res : results) {
        out << res.str();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    std::cout << "Elapsed time: " << elapsed << " seconds\n";
    std::cout << "Hash results written to hashes_v2.txt\n";

    return 0;
}
