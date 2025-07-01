#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace fs = std::filesystem;

struct FileData {
    std::string path;
    size_t fake_hash;
};

size_t computeFakeHash(const std::string& content) {
    return std::hash<std::string>{}(content);
}

std::vector<std::string> getTxtFiles(const std::string& directory) {
    std::vector<std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            files.push_back(entry.path().string());
        }
    }
    return files;
}

std::vector<FileData> processFiles(const std::vector<std::string>& files) {
    std::vector<FileData> results;

    for (const auto& path : files) {
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        size_t hash = computeFakeHash(content);

        results.push_back({path, hash});
    }

    return results;
}

void writeResultsToFile(const std::string& outputPath, const std::vector<FileData>& data) {
    std::ofstream out(outputPath);
    for (const auto& entry : data) {
        out << entry.path << ": " << entry.fake_hash << "\n";
    }
}

int main() {
    std::string directory = "test_files"; 
    std::string output_file = "hashes_v1.txt";

    auto start = std::chrono::high_resolution_clock::now();

    auto files = getTxtFiles(directory);
    auto hashed = processFiles(files);
    writeResultsToFile(output_file, hashed);

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "Elapsed time: " << elapsed << " seconds\n";
    std::cout << "Hash results written to " << output_file << std::endl;

    return 0;
}
