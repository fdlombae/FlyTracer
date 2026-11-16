#pragma once

#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

// A path in the temporary directory that no other test or test run uses; the file is removed with it
class TempFile {
public:
    explicit TempFile(std::string_view extension)
        : m_path(std::filesystem::temp_directory_path() / std::format("flytracer_{:016x}{}", NextId(), extension)) {
    }
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return m_path; }
    [[nodiscard]] std::string String() const { return m_path.string(); }

private:
    static unsigned long long NextId() {
        static std::mt19937_64 generator{std::random_device{}()};
        return generator();
    }

    std::filesystem::path m_path;
};
