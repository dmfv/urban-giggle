#include "grep.h"

int main(int argc, char** argv) {
    if (argc != 3) {
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
