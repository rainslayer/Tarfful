#pragma once

#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <array>
#include <vector>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

namespace Microtar {
    using Byte = char;

    enum class EStatus {
        ESUCCESS = 0,
        EFAILURE = -1,
        EOPENFAIL = -2,
        EREADFAIL = -3,
        EWRITEFAIL = -4,
        ESEEKFAIL = -5,
        EBADCHKSUM = -6,
        ENULLRECORD = -7,
        ENOTFOUND = -8
    };

    enum class EType {
        TREG = '0',
        TLNK = '1',
        TSYM = '2',
        TCHR = '3',
        TBLK = '4',
        TDIR = '5',
        TFIFO = '6'
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
        unsigned char type = '\0';
        std::array<Byte, 100> linkname = {};
        std::array<Byte, 255> _padding = {};
    } raw_header_t;

    typedef struct header_t {
    public:
        unsigned mode = 0;
        unsigned owner = 0;
        unsigned size = 0;
        unsigned mtime = 0;
        unsigned type = 0;
        std::array<Byte, 100> name = {};
        std::array<Byte, 100> linkname = {};
    } header_t;

    class Tar {
    private:
        int write_header();

        int write_file_header(const std::string &name, size_t size);

        int write_dir_header(const std::string &name);

        template<typename T>
        int twrite(const T &data, size_t size);

        int write_data(const std::string &data, size_t size);

        int write_null_bytes(int n);

        header_t header;

    public:
        template<typename T>
        int file_write(const T &data, size_t size);

        int file_write(const std::string &data, size_t size);

        int file_read(std::ofstream &outputFile, size_t size);

        int file_read(raw_header_t &rh, size_t size);

        int file_seek(long offset);

        int file_close();

        std::fstream fstream;
        std::string archiveName;
        size_t pos = 0;
        size_t remaining_data = 0;
        size_t last_header = 0;

        explicit Tar(std::string archive) : archiveName(std::move(archive)) {};

        void Write(const std::vector<std::string> &filenames);

        void Extract(const std::string &filename);

        int read_header();

        int next();

        int find(const std::string &name);

        int seek(const long pos);

        int read_data(std::ofstream &outputFile, const size_t size);

        int rewind();

        int tread(std::ofstream &outputFile, const size_t size);

        int tread(raw_header_t &rh, const size_t size);
    };
}
