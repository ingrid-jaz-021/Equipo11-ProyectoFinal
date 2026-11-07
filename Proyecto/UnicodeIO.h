#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

// Assimp antiguo y stb_image usan fopen: convertir UTF-8 a rutas Windows Unicode.
inline FILE* OpenUtf8File(const char* filename, const wchar_t* mode)
{
    FILE* file = nullptr;
    const auto native = std::filesystem::u8path(filename);
    _wfopen_s(&file, native.c_str(), mode);
    return file;
}

class UnicodeStream final : public Assimp::IOStream
{
public:
    explicit UnicodeStream(FILE* file) : file(file) {}
    ~UnicodeStream() override { if (file) std::fclose(file); }
    size_t Read(void* data, size_t size, size_t count) override { return std::fread(data, size, count, file); }
    size_t Write(const void* data, size_t size, size_t count) override { return std::fwrite(data, size, count, file); }
    aiReturn Seek(size_t offset, aiOrigin origin) override
    {
        const int base = origin == aiOrigin_SET ? SEEK_SET : origin == aiOrigin_CUR ? SEEK_CUR : SEEK_END;
        const __int64 distance = origin == aiOrigin_END ? -static_cast<__int64>(offset) : static_cast<__int64>(offset);
        return _fseeki64(file, distance, base) == 0 ? aiReturn_SUCCESS : aiReturn_FAILURE;
    }
    size_t Tell() const override { return static_cast<size_t>(_ftelli64(file)); }
    size_t FileSize() const override
    {
        const auto current = _ftelli64(file);
        _fseeki64(file, 0, SEEK_END);
        const auto size = _ftelli64(file);
        _fseeki64(file, current, SEEK_SET);
        return static_cast<size_t>(size);
    }
    void Flush() override { std::fflush(file); }
private:
    FILE* file;
};

class UnicodeIO final : public Assimp::IOSystem
{
public:
    bool Exists(const char* filename) const override
    {
        std::error_code error;
        return std::filesystem::is_regular_file(std::filesystem::u8path(filename), error);
    }
    char getOsSeparator() const override { return '/'; }
    Assimp::IOStream* Open(const char* filename, const char* mode = "rb") override
    {
        const std::wstring wideMode(mode, mode + std::char_traits<char>::length(mode));
        FILE* file = OpenUtf8File(filename, wideMode.c_str());
        return file ? new UnicodeStream(file) : nullptr;
    }
    void Close(Assimp::IOStream* stream) override { delete stream; }
};
