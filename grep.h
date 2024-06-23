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
    const inline std::string BLACK   { "\033[1;30m" };
    const inline std::string RED     { "\033[1;31m" };
    const inline std::string GREEN   { "\033[1;32m" };
    const inline std::string YELLOW  { "\033[1;33m" };
    const inline std::string BLUE    { "\033[1;34m" };
    const inline std::string MAGENTA { "\033[1;35m" };
    const inline std::string CYAN    { "\033[1;36m" };
    const inline std::string WHITE   { "\033[1;37m" };
    const inline std::string DEFAULT { "\033[0m"    };
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

    std::optional<std::string> GetLine();
    const fs::path& GetFilePath() const { return filePath; }

private:
    fs::path filePath;
    std::ifstream file;
};


// process files in parallel
class FileRegexProcessor {
public:
    FileRegexProcessor(
        size_t processCount,
        const std::string& regex
    );

    ~FileRegexProcessor();

    bool IsFinished() const;

    void AddFileSections(File&& file);

    void AddFileSections(std::list<File>&& files);

private:
    void Job([[maybe_unused]] size_t threadId);

    inline void DumpMatchedLines(std::string& matchedLines);

    inline std::string ProcessLine(const std::string& line) const;

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
    static void Build(const fs::path& path, FileRegexProcessor& fp);

    static std::list<File> ProcessFile(const fs::path& path);

    static std::streamsize GetFileSize(const fs::path& path);
};
