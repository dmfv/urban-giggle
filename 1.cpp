#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <vector>
#include <list>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>


namespace fs = std::filesystem;

namespace textcolor {
    const std::string BLACK   {"\033[1;30m"};
    const std::string RED     {"\033[1;31m"};
    const std::string GREEN   {"\033[1;32m"};
    const std::string YELLOW  {"\033[1;33m"};
    const std::string BLUE    {"\033[1;34m"};
    const std::string MAGENTA {"\033[1;35m"};
    const std::string CYAN    {"\033[1;36m"};
    const std::string WHITE   {"\033[1;37m"};
    const std::string DEFAULT {"\033[0m"};
};

class FilePart {
public:
    FilePart() : start(0), end(0) {}
    FilePart(const fs::path& filePath, std::streamsize start, std::streamsize end) 
    : filePath(filePath)
    , file(filePath, std::ifstream::in)
    , start(start)
    , end(end)
    , curr(start) 
    { }

    FilePart(const FilePart&) = delete;  // because ifstream
    FilePart& operator=(const FilePart&) = delete; // because ifstream

    FilePart(FilePart&&) = default;
    FilePart& operator=(FilePart&&) = default;
    ~FilePart() { };

    std::optional<std::string> GetLine() {
        std::optional<std::string> line {std::nullopt};
        if (file.is_open()) {
            if (file.tellg() >= end) {
                return line;
            }
            file.seekg(curr, std::ios::beg);
            std::string tempLine;
            std::getline(file, tempLine);
            curr = file.tellg();

            // file.seekg(curr, std::ios::cur);
            line = tempLine;
        }
        return line;
    }

    const fs::path& GetFilePath() const { return filePath; }
    
private:
    fs::path filePath;
    std::ifstream file;
    std::streamsize start;
    std::streamsize end;
    std::streamsize curr;
};

class FilePartsBuilder {
public:
    static std::list<FilePart> Build(const fs::path& path, size_t filePartitionNum) {
        std::queue<fs::path> directories;
        std::list<FilePart> files;
        directories.push(path);
        while (!directories.empty()) {
            auto _path = directories.front();
            if (fs::is_directory(_path)) { 
                for (const auto& entry : fs::directory_iterator(_path)) {
                    if (fs::is_directory(entry)) {
                        directories.push(entry);
                    } else {
                        // TODO: maybe use mmap to faster process
                        std::cout << entry.path() << std::endl;
                        auto tempList = ProcessFile(entry.path(), filePartitionNum);
                        files.splice(std::prev(files.end()), tempList);
                        // files.emplace_back();
                    }
                }
            } else {
                // TODO: maybe use mmap to faster process
                std::cout << _path << std::endl;
                auto tempList = ProcessFile(_path, filePartitionNum);
                files.splice(std::prev(files.end()), tempList);
                // files.emplace_back();
            }
            directories.pop();
        }
        return files;
    }

    static std::list<FilePart> ProcessFile(const fs::path& path, size_t filePartitionNum) {
        auto fileSize = GetFileSize(path);
        std::list<FilePart> splittedFile;
        std::streamsize partSize = fileSize / filePartitionNum;

        for (size_t i = 0; i < filePartitionNum; ++i) {
            std::streamsize start = partSize * i;
            std::streamsize end = partSize * (i + 1) ;
            splittedFile.emplace_back(path, start, end);
        }
        return splittedFile;
    }

    static std::streamsize GetFileSize(const fs::path& path) {
        std::ifstream tempFile(path, std::ios::binary | std::ios::ate);
        return tempFile.tellg();
    }
};

// process files in parallel
class FileRegexProcessor {
public:
    FileRegexProcessor(size_t processCount, const std::string& regex) 
    : regex(std::move(regex)) 
    {
        for (auto i = 0; i < processCount; ++i) {
            threads.push_back(std::thread(&FileRegexProcessor::Job, this, i));
        }
    }

    ~FileRegexProcessor() {
        stop = true;
        for (auto& thread : threads) {
            thread.join();
        }
        std::cout << "Stopped all workers\n";
    }

    // TODO: fix this functions overload
    void AddFileSections(std::list<FilePart>&& fileParts) {
        std::lock_guard<std::mutex> lock{filePartsQueueMutex};
        std::cerr << "Adding file sections\n";
        for (auto& filePart : fileParts) {
            // std::cerr << filePart.GetFilePath() << std::endl;
            filePartsQueue.push(std::forward<FilePart>(filePart));
        }
    }

    void AddFileSections(std::list<FilePart>& fileParts) {
        AddFileSections(std::move(fileParts));
    }

private:
    void Job(size_t processId) {
        std::cout << "Processing \n";
        while (!stop) {
            // some busy work here
            std::lock_guard<std::mutex> lock(filePartsQueueMutex);
            FilePart filePart;
            if (!filePartsQueue.empty()) {
                // std::cout << "PROCESSING " << processId << std::endl;
                filePart = std::move(filePartsQueue.front());
                filePartsQueue.pop();
            }
            auto optLine = filePart.GetLine();
            while (optLine.has_value()) {
                auto matchedLine = ProcessLine(*optLine);
                if (matchedLine.has_value()) {
                    std::cout << filePart.GetFilePath() << ": " << *matchedLine << std::endl;
                }
                optLine = filePart.GetLine();
            }
        }
        std::cout << "Stopping \n";
    }

    std::optional<std::string> ProcessLine(const std::string& line) const {
        if (!regex_search(line, regex)) {
            return std::nullopt;
        }
        // "[$&]"
        static const std::string REPLACE_FORMAT {textcolor::GREEN + "$&" + textcolor::DEFAULT};
        return std::regex_replace(line, regex, REPLACE_FORMAT);
    }
    
    const std::regex regex;
    std::atomic<bool> stop {false};
    std::mutex filePartsQueueMutex;
    std::queue<FilePart> filePartsQueue;
    std::vector<std::thread> threads;
};


int main(int argc, char **argv) {
    if (argc <= 2) {
        std::cerr << "Usage: grep PATTERN PATH";
    }
    std::string regex = argv[1];
    fs::path path = argv[2];

    FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);
    // FileRegexProcessor fp(4, regex);
    fp.AddFileSections(FilePartsBuilder::Build(path, 4));

    // auto fps = FilePartsBuilder::Build(path, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
