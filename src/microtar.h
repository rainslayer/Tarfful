/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `microtar.c` for details.
 */

#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        char type;
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

    typedef struct type {
        int (*read)(type *tar, void *data, unsigned size);

        int (*write)(type *tar, const void *data, unsigned size);

        int (*seek)(type *tar, unsigned pos);

        int (*close)(type *tar);

        FILE *stream;
        unsigned pos;
        unsigned remaining_data;
        unsigned last_header;
    } type;

    class Microtar {
    private:
        Microtar() = default;

        ~Microtar() = default;

    public:
        static const char *strerror(int err);

        static int open(type *tar, const char *filename, const char *mode);

        static int close(type *tar);

        static int seek(type *tar, unsigned pos);

        static int rewind(type *tar);

        static int next(type *tar);

        static int find(type *tar, const char *name, header_t *h);

        static int read_header(type *tar, header_t *h);

        static int read_data(type *tar, void *ptr, unsigned size);

        static int write_header(type *tar, const header_t *h);

        static int write_file_header(type *tar, const char *name, unsigned size);

        static int write_dir_header(type *tar, const char *name);

        static int write_data(type *tar, const void *data, unsigned size);

        static int finalize(type *tar);

    };
}