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
//#include <queue>


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
    const std::string DEFAULT { "\033[0m" };
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
        std::optional<std::string> line{ std::nullopt };
        if (file.is_open()) {
            if (curr > end || curr == EOF ) {
                return line;
            }
            // TODO: maybe don't need all this curr end begin because ifstream has it's own
            std::string tempLine;
            std::getline(file, tempLine);
            auto oldCurr = curr;
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
        cvFilePartsQueue.notify_all();
        for (auto& thread : threads) {
            thread.join();
        }
        std::cout << "All workers stopped\n";
    }

    bool IsFinished() const { 
        std::lock_guard<std::mutex> lock(filePartsQueueMutex);
        return filePartsQueue.empty();
    }

    // TODO: fix this functions overload
    void AddFileSections(std::list<FilePart>&& fileParts) {
        std::unique_lock<std::mutex> lock{ filePartsQueueMutex };
        std::cout << "Adding file sections\n";
        for (auto& filePart : fileParts) {
            // std::cerr << filePart.GetFilePath() << std::endl;
            filePartsQueue.emplace_back(std::move(filePart));
        }
        cvFilePartsQueue.notify_all();
    }

    void AddFileSections(std::list<FilePart>& fileParts) {
        AddFileSections(std::move(fileParts));
    }

private:
    void Job(size_t threadId) {
        std::cout << "Processing \n";
        std::optional<FilePart> filePart;
        while (!stop) {
            // some busy work here
            std::unique_lock<std::mutex> lock(filePartsQueueMutex);
            cvFilePartsQueue.wait(lock, [&] { return stop || !filePartsQueue.empty(); });
            if (!filePartsQueue.empty()) {
                // std::cout << "PROCESSING " << processId << std::endl;
                filePart = std::move(filePartsQueue.front());
                filePartsQueue.pop_front();
            }
            if (!filePart.has_value())
                continue;
            auto optLine = filePart->GetLine();
            while (optLine.has_value()) {
                auto matchedLine = ProcessLine(*optLine);
                if (matchedLine.has_value()) {
                    std::cout << textcolor::MAGENTA << filePart->GetFilePath() << textcolor::DEFAULT << ": " << *matchedLine << std::endl;
                }
                optLine = filePart->GetLine();
            }
        }
        std::cout << "Stopping \n";
    }

    std::optional<std::string> ProcessLine(const std::string& line) const {
        if (!regex_search(line, regex)) {
            return std::nullopt;
        }
        // "[$&]"
        static const std::string REPLACE_FORMAT{ textcolor::GREEN + "$&" + textcolor::DEFAULT };
        return std::regex_replace(line, regex, REPLACE_FORMAT);
    }

    const std::regex regex;
    std::atomic<bool> stop{ false };
    mutable std::mutex filePartsQueueMutex;
    std::condition_variable cvFilePartsQueue;
    std::list<FilePart> filePartsQueue;
    std::list<std::thread> threads;
};


class FilePartsBuilder {
public:
    static std::list<FilePart> Build(const fs::path& path, size_t filePartitionNum, FileRegexProcessor& fp) {
        std::list<fs::path> directories;
        std::list<FilePart> files;
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
                        std::cout << entry.path() << std::endl;
                        auto tempList = ProcessFile(entry.path(), filePartitionNum);
                        fp.AddFileSections(std::move(tempList));
                        //files.splice(files.end(), tempList);
                    }
                }
            }
            else {
                // TODO: maybe use mmap that could be faster
                std::cout << _path << std::endl;
                auto tempList = ProcessFile(_path, filePartitionNum);
                fp.AddFileSections(std::move(tempList));
                //files.splice(files.end(), tempList);
            }
            directories.pop_front();
        }
        return files;
    }

    static std::list<FilePart> ProcessFile(const fs::path& path, size_t filePartitionNum) {
        auto fileSize = GetFileSize(path);
        std::list<FilePart> splittedFile;
        std::streamsize partSize = fileSize / filePartitionNum;

        for (size_t i = 0; i < filePartitionNum; ++i) {
            std::streamsize start = partSize * i;
            std::streamsize end = partSize * (i + 1);
            splittedFile.emplace_back(path, start, end);
        }
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

     //FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);
     FileRegexProcessor fp(1, regex);
     FilePartsBuilder::Build(path, 1, fp);
     //fp.AddFileSections(FilePartsBuilder::Build(path, 1));

     while (!fp.IsFinished()) {}
     // auto fps = FilePartsBuilder::Build(path, 2);
     //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
