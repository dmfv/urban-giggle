#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <list>
#include <optional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "timer.h"

namespace fs = std::filesystem;

namespace textcolor {
    const std::string BLACK   { "\033[1;30m" };
    const std::string RED     { "\033[1;31m" };
    const std::string GREEN   { "\033[1;32m" };
    const std::string YELLOW  { "\033[1;33m" };
    const std::string BLUE    { "\033[1;34m" };
    const std::string MAGENTA { "\033[1;35m" };
    const std::string CYAN    { "\033[1;36m" };
    const std::string WHITE   { "\033[1;37m" };
    const std::string DEFAULT { "\033[0m"    };
};


class File {
public:
    File() {}
    File(const fs::path& filePath)
        : filePath(filePath)
        , file(filePath, std::ifstream::in)
    { }

    File(const File&) = delete;  // because ifstream
    File& operator=(const File&) = delete;

    File(File&&) = default;
    File& operator=(File&&) = default;
    ~File() { };

    std::optional<std::string> GetLine() {
        if (file.is_open()) {
            std::string tempStr;
            if (std::getline(file, tempStr)) {
                return tempStr;
            }
        }
        return std::nullopt;
    }

    const fs::path& GetFilePath() const { return filePath; }

private:
    fs::path filePath;
    std::ifstream file;
};


// process files in parallel
class FileRegexProcessor {
public:
    FileRegexProcessor(size_t processCount, const std::string& regex)
        : regex(std::move(regex))
    {
        for (auto i = 0; i < processCount; ++i) {
            threads.emplace_back(std::thread(&FileRegexProcessor::Job, this, i));
        }
    }

    ~FileRegexProcessor() {
        stop = true;
        cvFilesQueue.notify_all();
        for (auto& thread : threads) {
            thread.join();
        }
        std::cout << "All " << threads.size() << " workers stopped\n";
    }

    bool IsFinished() const { 
        std::lock_guard<std::mutex> lock(filesQueueMutex);
        return filesQueue.empty();
    }

    // TODO: fix this functions overload
    void AddFileSections(File&& file) {
        std::unique_lock<std::mutex> lock{ filesQueueMutex };
        //std::cout << "Adding file sections\n";
        filesQueue.emplace_back(std::move(file));
        cvFilesQueue.notify_all();
    }

    void AddFileSections(File& file) {
        AddFileSections(std::move(file));
    }

private:
    void Job([[maybe_unused]] size_t threadId) {
        std::optional<File> file;
        while (!stop) {
            // some busy work here
            {
                std::unique_lock<std::mutex> lock(filesQueueMutex);
                cvFilesQueue.wait(lock, [&] { return stop || !filesQueue.empty(); });
                if (!filesQueue.empty()) {
                    file = std::move(filesQueue.front());
                    filesQueue.pop_front();
                }
            }
            if (!file.has_value())
                continue;
            auto optLine = file->GetLine();
            while (optLine.has_value()) {
                auto matchedLine = ProcessLine(*optLine);
                if (matchedLine != *optLine) {
                    thread_local std::string out{ textcolor::MAGENTA + file->GetFilePath().string() + textcolor::DEFAULT + ": " + matchedLine + '\n' };
                    std::cout << out;
                }
                optLine = file->GetLine();
            }
        }
    }

    inline std::string ProcessLine(const std::string& line) const {
        static const std::string REPLACE_FORMAT{ textcolor::GREEN + "$&" + textcolor::DEFAULT };
        return std::regex_replace(line, regex, REPLACE_FORMAT);
    }

    const std::regex regex;
    std::atomic<bool> stop{ false };
    mutable std::mutex filesQueueMutex;
    std::condition_variable cvFilesQueue;
    std::list<File> filesQueue;
    std::list<std::thread> threads;
};


class FilesBuilder {
public:
    static std::list<File> Build(const fs::path& path, FileRegexProcessor& fp) {
        std::list<fs::path> directories;
        std::list<File> files;
        directories.push_back(path);
        while (!directories.empty()) {
            auto _path = directories.front();
            if (fs::is_directory(_path)) {
                for (const auto& entry : fs::directory_iterator(_path)) {
                    if (fs::is_directory(entry)) {
                        directories.push_back(entry);
                    }
                    else {
                        // TODO: maybe use mmap that could be faster
                        // std::cout << entry.path() << std::endl;
                        fp.AddFileSections(File(entry.path()));
                    }
                }
            }
            else {
                fp.AddFileSections(File(_path));
            }
            directories.pop_front();
        }
        return files;
    }

    static std::list<File> ProcessFile(const fs::path& path) {
        auto fileSize = GetFileSize(path);
        std::list<File> splittedFile;

        return splittedFile;
    }

    static std::streamsize GetFileSize(const fs::path& path) {
        std::ifstream tempFile(path, std::ios::binary | std::ios::ate);
        return tempFile.tellg();
    }
};


int main(int argc, char** argv) {
    if (argc <= 2) {
        std::cerr << "Usage: grep PATTERN PATH\n"
                  << "Example: grep.out 123 ./grep.out";
    }
    std::string regex = argv[1];
    fs::path path = argv[2];

    double th1, th16;
    {
        Timer t;
        FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);
        //FileRegexProcessor fp(1, regex);

        FilesBuilder::Build(path, fp);

        while (!fp.IsFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        th16 = t.stop();
    }

    {
        Timer t;
        //FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);
        FileRegexProcessor fp(1, regex);

        FilesBuilder::Build(path, fp);

        while (!fp.IsFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        th1 = t.stop();
    }

    std::cout << "th1 : " << th1 << std::endl;
    std::cout << "th16 : " << th16 << std::endl;
}
