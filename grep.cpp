#include "grep.h"


std::optional<std::string> File::GetLine() {
    if (file.is_open()) {
        std::string tempStr;
        if (std::getline(file, tempStr)) {
            return tempStr;
        }
    }
    return std::nullopt;
}


FileRegexProcessor::FileRegexProcessor(
    size_t processCount,
    const std::string& regex
)
    : regex(std::move(regex))
{
    for (auto i = 0; i < processCount; ++i) {
        threads.emplace_back(std::thread(&FileRegexProcessor::Job, this, i));
    }
}

FileRegexProcessor::~FileRegexProcessor() {
    stop = true;
    cvFilesQueue.notify_all();
    for (auto& thread : threads) {
        thread.join();
    }
}

bool FileRegexProcessor::IsFinished() const { 
    std::lock_guard<std::mutex> lock(filesQueueMutex);
    return filesQueue.empty();
}

void FileRegexProcessor::AddFileSections(File&& file) {
    std::unique_lock<std::mutex> lock{ filesQueueMutex };
    filesQueue.emplace_back(std::move(file));
    cvFilesQueue.notify_all();
}

void FileRegexProcessor::AddFileSections(std::list<File>&& files) {
    std::unique_lock<std::mutex> lock{ filesQueueMutex };
    filesQueue.splice(filesQueue.end(), std::move(files));
    cvFilesQueue.notify_all();
}


void FileRegexProcessor::Job([[maybe_unused]] size_t threadId) {
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

inline void FileRegexProcessor::DumpMatchedLines(std::string& matchedLines) {
    std::cout << matchedLines;
    matchedLines = "";
}

inline std::string FileRegexProcessor::ProcessLine(const std::string& line) const {
    static const std::string REPLACE_FORMAT{ textcolor::GREEN + "$&" + textcolor::DEFAULT };
    return std::regex_replace(line, regex, REPLACE_FORMAT);
}


void FilesBuilder::Build(const fs::path& path, FileRegexProcessor& fp) {
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

std::list<File> FilesBuilder::ProcessFile(const fs::path& path) {
    auto fileSize = GetFileSize(path);
    std::list<File> splittedFile;

    return splittedFile;
}

std::streamsize FilesBuilder::GetFileSize(const fs::path& path) {
    std::ifstream tempFile(path, std::ios::binary | std::ios::ate);
    return tempFile.tellg();
}
