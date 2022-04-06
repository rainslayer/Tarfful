/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#pragma once

#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace Microtar {
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

    typedef struct {
        char name[100];
        char mode[8];
        char owner[8];
        char group[8];
        char size[12];
        char mtime[12];
        char checksum[8];
        unsigned char type;
        char linkname[100];
        char _padding[255];
    } raw_header_t;

    typedef struct {
        unsigned mode;
        unsigned owner;
        unsigned size;
        unsigned mtime;
        unsigned type;
        char name[100];
        char linkname[100];
    } header_t;

    class Tar {
    public:
        template<typename T>
        int file_write(const T &data, size_t size);

        int file_write(const std::string &data, size_t size);

        template<typename T>
        int file_read(T &data, size_t size);

        int file_read(std::string &data, size_t size);

        int file_seek(long offset);

        int file_close();

        std::fstream fstream;
        size_t pos;
        size_t remaining_data;
        size_t last_header;
    };

    class Microtar {
    private:
        Microtar() = default;

        ~Microtar() = default;

    public:
        static std::string strerror(int err);

        static int open(Tar &tar, const std::string &filename, const std::ios_base::openmode &mode);

        static int close(Tar &tar);

        static int seek(Tar &tar, long pos);

        static int rewind(Tar &tar);

        static int next(Tar &tar);

        static int find(Tar &tar, const std::string &name, header_t &h);

        static int read_header(Tar &tar, header_t &h);

        template<typename T>
        static int read_data(Tar &tar, T &data, size_t size);

        static int write_header(Tar &tar, const header_t &h);

        static int write_file_header(Tar &tar, const std::string &name, size_t size);

        static int write_dir_header(Tar &tar, const std::string &name);

        static int write_data(Tar &tar, const std::string &data, size_t size);

        static int finalize(Tar &tar);

    };
}