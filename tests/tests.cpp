#include <unordered_set>

#include "../grep.h"
#include <gtest/gtest.h>

#ifdef _WIN32
    // because cmake macros uses only one type of delimiters
    const char SYSTEM_PATH_DELIMITER = '\\';
#else
    const char SYSTEM_PATH_DELIMITER = '/';
#endif

const fs::path& GetTestFolderPath() {
#ifdef _WIN32
    const static fs::path BASE_TEST_FOLDER {[]() {
        std::string path = TEST_DIR_MACRO;
        path = std::regex_replace(path, std::regex("/"), "\\");
        return fs::path(path);
    }()};
#else
    const static fs::path BASE_TEST_FOLDER {[]() {
        std::string path = TEST_DIR_MACRO;
        return fs::path(path);
    }()};
#endif
    return BASE_TEST_FOLDER;
}


std::string replaceAll(std::string str, const std::string& from) {
    if (from.empty()) {
        return str; // No need to replace if the substring is empty
    }
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), "");
        start_pos += 0; // No need to advance by the length of the replacement (empty string)
    }
    return str;
}


class GrepTestFixture: public ::testing::Test {
protected:
    void SetUp() override {
        // Save cout's buffer...
        sbuf = std::cout.rdbuf();
        // Redirect cout to our stringstream buffer or any other ostream
        std::cout.rdbuf(buffer.rdbuf());
    }

    void TearDown() override {
        // When done redirect cout to its old self
        std::cout.rdbuf(sbuf);
        sbuf = nullptr;
    }

    static void RunGrep(const fs::path& path, const std::string& regex) {
        FileRegexProcessor fp(std::thread::hardware_concurrency(), regex);
        FilesBuilder::Build(path, fp);

        while (!fp.IsFinished()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // This can be an ofstream as well or any other ostream
    std::stringstream buffer{};
    // Save cout's buffer here
    std::streambuf *sbuf;
    
};


TEST_F(GrepTestFixture, TestOneFile) {
    const static std::unordered_set<std::string_view> EXPECTED_LINES {
        "100.txt: 123",
        "100.txt: 124",
    };

    const static fs::path ONE_FILE_PATH = GetTestFolderPath() / "test_folder_1" / "100.txt";
    RunGrep(ONE_FILE_PATH, "12[34]");
    std::string out {buffer.str()};
    EXPECT_FALSE(out.empty());
    std::istringstream outSS {out};
    
    std::string line;
    size_t linesCounter = 0;
    while (getline(outSS, line, '\n')) {
        line = replaceAll(line, textcolor::DEFAULT);
        line = replaceAll(line, textcolor::GREEN);

        ++linesCounter;
        const auto fileNameStart = line.find_last_of(SYSTEM_PATH_DELIMITER) + 1;
        const auto lineEnd = line.find_last_of('\r');  // only linux hack
        auto regexMatchLog = line.substr(fileNameStart, lineEnd - fileNameStart);

        EXPECT_TRUE(EXPECTED_LINES.contains(regexMatchLog));
    }
    EXPECT_EQ(linesCounter, EXPECTED_LINES.size());
}


TEST_F(GrepTestFixture, TestOneDirectory) {
    const static fs::path TEST_FOLDER_1 = GetTestFolderPath() / "test_folder_1";
    const static std::unordered_set<std::string_view> EXPECTED_LINES {
        "100.txt: 123",
        "200.txt: 223",
        "300.txt: 323",
    };

    RunGrep(TEST_FOLDER_1, ".23");
    std::string out {buffer.str()};
    EXPECT_FALSE(out.empty());
    std::istringstream outSS {out};
    
    std::string line;
    size_t linesCounter = 0;
    while (getline(outSS, line, '\n')) {
        line = replaceAll(line, textcolor::DEFAULT);
        line = replaceAll(line, textcolor::GREEN);

        ++linesCounter;
        const auto fileNameStart = line.find_last_of(SYSTEM_PATH_DELIMITER) + 1;
        const auto lineEnd = line.find_last_of('\r');
        auto regexMatchLog = line.substr(fileNameStart, lineEnd - fileNameStart);

        EXPECT_TRUE(EXPECTED_LINES.contains(regexMatchLog));
    }
    EXPECT_EQ(linesCounter, EXPECTED_LINES.size());
}


TEST_F(GrepTestFixture, TestRecursiveDirectory) {
    using namespace std::string_literals;
    const static fs::path TEST_FOLDER_2 = GetTestFolderPath() / "test_folder_2";
    const static std::unordered_set<std::string> EXPECTED_LINES {
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "100.txt: 111"s,
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "200.txt: 211"s,
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "300.txt: 311"s,
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "another_folder" + SYSTEM_PATH_DELIMITER + "700.txt: 711"s,
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "another_folder" + SYSTEM_PATH_DELIMITER + "800.txt: 811"s,
        "test_folder_2"s + SYSTEM_PATH_DELIMITER + "another_folder" + SYSTEM_PATH_DELIMITER + "900.txt: 911"s,
    };

    RunGrep(TEST_FOLDER_2, ".11");
    std::string out {buffer.str()};
    EXPECT_FALSE(out.empty());
    std::istringstream outSS {out};
    
    std::string line;
    size_t linesCounter = 0;
    while (getline(outSS, line, '\n')) {
        line = replaceAll(line, textcolor::DEFAULT);
        line = replaceAll(line, textcolor::GREEN);

        ++linesCounter;
        const auto fileNameStart = line.find(SYSTEM_PATH_DELIMITER + "test_folder_2"s) + 1;
        const auto lineEnd = line.find_last_of('\r');
        auto regexMatchLog = line.substr(fileNameStart, lineEnd - fileNameStart);

        EXPECT_TRUE(EXPECTED_LINES.contains(regexMatchLog));
    }
    EXPECT_EQ(linesCounter, EXPECTED_LINES.size());
}