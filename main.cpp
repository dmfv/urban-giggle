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

    File(const File&) = delete;  // because of ifstream
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

    void AddFileSections(File&& file) {
        std::unique_lock<std::mutex> lock{ filesQueueMutex };
        filesQueue.emplace_back(std::move(file));
        cvFilesQueue.notify_all();
    }

    void AddFileSections(std::list<File>&& files) {
        std::unique_lock<std::mutex> lock{ filesQueueMutex };
        filesQueue.splice(filesQueue.end(), std::move(files));
        cvFilesQueue.notify_all();
    }

private:
    void Job([[maybe_unused]] size_t threadId) {
        std::optional<File> file;
        thread_local std::string matchedLines{ []() {
            std::string temp;
            temp.reserve(THREAD_LOCAL_STORAGE_BYTES);
            return temp;
        }() };

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
            if (file.has_value()) {
                auto optLine = file->GetLine();
                while (optLine.has_value()) {
                    auto lineAfterRegexReplace = ProcessLine(*optLine);
                    if (lineAfterRegexReplace != *optLine) { // if regex_replaces did something
                        std::string newMatch{
                            "thread id: " +
                            std::to_string(threadId) +
                            " " +
                            textcolor::MAGENTA +
                            file->GetFilePath().string() +
                            textcolor::DEFAULT +
                            ": " +
                            lineAfterRegexReplace +
                            '\n' };
                        if (matchedLines.size() + newMatch.size() >= THREAD_LOCAL_STORAGE_BYTES) {
                            DumpMatchedLines(matchedLines);
                        }
                        matchedLines += std::move(newMatch);
                    }
                    optLine = file->GetLine();
                }
            }
            if (!matchedLines.empty()) {
                DumpMatchedLines(matchedLines);
            }
        }
    }

    inline void DumpMatchedLines(std::string& matchedLines) {
        std::cout << matchedLines;
        matchedLines = "";
    }

    inline std::string ProcessLine(const std::string& line) const {
        static const std::string REPLACE_FORMAT{ textcolor::GREEN + "$&" + textcolor::DEFAULT };
        return std::regex_replace(line, regex, REPLACE_FORMAT);
    }

    constexpr static inline size_t THREAD_LOCAL_STORAGE_BYTES { 1024 };

    const std::regex regex;
    std::atomic<bool> stop{ false };
    mutable std::mutex filesQueueMutex;
    std::condition_variable cvFilesQueue;
    std::list<File> filesQueue;
    std::list<std::thread> threads;
};


class FilesBuilder {
public:
    static void Build(const fs::path& path, FileRegexProcessor& fp) {
        std::list<fs::path> directories;
        std::list<File> files;
        static size_t FILES_SIZE_THRESHOLD { 10 };
        directories.push_back(path);
        while (!directories.empty()) {
            auto _path = directories.front();
            if (fs::is_directory(_path)) {
                for (const auto& entry : fs::directory_iterator(_path)) {
                    if (fs::is_directory(entry)) {
                        directories.push_back(entry);
                    }
                    else {
                        files.push_back(std::move(File(entry.path())));
                        if (files.size() >= FILES_SIZE_THRESHOLD) {
                            fp.AddFileSections(std::move(files));
                        }
                    }
                }
            }
            else {
                files.push_back(std::move(File(_path)));
            }
            fp.AddFileSections(std::move(files));
            directories.pop_front();
        }
        
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
    if (argc < 3) {
        std::cerr << "Usage: grep PATTERN PATH\n"
                  << "Example: grep.out 123 ./grep.out";
        return 0;
    }
    std::string regex = argv[1];
    fs::path path = argv[2];

    FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);

    FilesBuilder::Build(path, fp);

    while (!fp.IsFinished()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
