#pragma once

#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <experimental/filesystem>
#include <chrono>

#ifdef __linux__
    #include <pwd.h>
    #include <grp.h>
    #include <sys/stat.h>
#endif

namespace Microtar {
    namespace fs = std::experimental::filesystem;

    using Byte = char;

    enum class EStatus {
        ESUCCESS = 0,
        EOPENFAIL = -1,
        EREADFAIL = -2,
        EWRITEFAIL = -3,
        ESEEKFAIL = -4,
        EBADCHKSUM = -5,
        ENULLRECORD = -6,
        ENOTFOUND = -7
    };

    typedef struct raw_header_t {
    public:
        std::array<Byte, 100> name = {};
        std::array<Byte, 8> mode = {};
        std::array<Byte, 8> owner = {};
        std::array<Byte, 8> group = {};
        std::array<Byte, 12> size = {};
        std::array<Byte, 12> mtime = {};
        std::array<Byte, 8> checksum = {};
        unsigned char type = 0;
        std::array<Byte, 100> linkname = {};
        std::array<Byte, 255> _padding = {};
    } raw_header_t;

    typedef struct header_t {
    public:
        size_t mode = 0;
        size_t owner = 0;
        size_t group = 0;
        size_t size = 0;
        size_t mtime = 0;
        unsigned char type = 0;
        std::array<Byte, 100> name = {};
        std::array<Byte, 100> linkname = {};
    } header_t;

    class Tar {
    private:
        header_t header;
        std::fstream fstream;
        std::string archive_name;
        size_t pos = 0;
        size_t remaining_data = 0;
        size_t last_header = 0;

    private:
        int write_header();

        int write_file_header(const std::string &name, const size_t &size);

        template<typename T>
        int twrite(const T &data, const size_t &size);

        int write_data(const std::string &data, const size_t &size);

        int write_null_bytes(const size_t &n);

        int raw_to_header(Microtar::raw_header_t &rh);

        template<typename T>
        int file_write(const T &data, const size_t &size);

        int file_write(const std::string &data, const size_t &size);

        int file_read(std::ofstream &outputFile, const size_t &size);

        int file_read(raw_header_t &rh, const size_t &size);

        int file_seek(const size_t &offset);

        int read_header();

        int next();

        int rewind();

        int find(const std::string &name);

        int seek(const size_t &newPos);

        int read_data(std::ofstream &outputFile, const size_t &size);

        int tread(std::ofstream &outputFile, const size_t &size);

        int tread(raw_header_t &rh);

    public:
        explicit Tar(std::string archive) : archive_name(std::move(archive)) {};

        int Archive(const std::string &path);

        int Extract(const std::string &filename);

        int ExtractAll();
    };
}
