/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "microtar.h"

unsigned round_up(const unsigned &n, const unsigned &incr) {
    return n + (incr - n % incr) % incr;
}

unsigned checksum(Microtar::raw_header_t &rh) {
    auto *p = reinterpret_cast<unsigned char *>(&rh);

    unsigned res = 256;
    for (size_t i = 0, offset = offsetof(Microtar::raw_header_t, checksum); i < offset; ++i) {
        res += p[i];
    }

    for (size_t i = offsetof(Microtar::raw_header_t, type), size = sizeof(rh); i < size; ++i) {
        res += p[i];
    }
    return res;
}

template<typename T>
int tread(Microtar::Tar &tar, T &data, const size_t size) {
    const int err = tar.file_read(data, size);
    tar.pos += size;
    return err;
}

template<typename T>
int twrite(Microtar::Tar &tar, const T &data, const size_t size) {
    const int err = tar.file_write(data, size);
    tar.pos += size;
    return err;
}


static int write_null_bytes(Microtar::Tar &tar, const int n) {
    for (size_t i = 0; i < n; ++i) {
        const int err = twrite(tar, '\0', 1);
        if (err) {
            return err;
        }
    }
    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


static int raw_to_header(Microtar::header_t &h, Microtar::raw_header_t &rh) {
    /* If the checksum starts with a null byte we assume the record is NULL */
    if (*rh.checksum == '\0') {
        return static_cast<int>(Microtar::EStatus::ENULLRECORD);
    }

    /* Build and compare checksum */
    const unsigned chksum1 = checksum(rh);
    unsigned chksum2;
    sscanf(rh.checksum, "%o", &chksum2);
    if (chksum1 != chksum2) {
        return static_cast<int>(Microtar::EStatus::EBADCHKSUM);
    }

    /* Load raw header into header */
    sscanf(rh.mode, "%o", &h.mode);
    sscanf(rh.owner, "%o", &h.owner);
    sscanf(rh.size, "%o", &h.size);
    sscanf(rh.mtime, "%o", &h.mtime);
    h.type = rh.type;
    strncpy(h.name, rh.name, sizeof(h.name));
    strncpy(h.linkname, rh.linkname, sizeof(h.linkname));

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


static int header_to_raw(Microtar::raw_header_t &rh, const Microtar::header_t &h) {
    /* Load header into raw header */
    sprintf(rh.mode, "%o", h.mode);
    sprintf(rh.owner, "%o", h.owner);
    sprintf(rh.size, "%o", h.size);
    sprintf(rh.mtime, "%o", h.mtime);
    rh.type = h.type ? h.type : static_cast<char>(Microtar::EType::TREG);
    strncpy(rh.name, h.name, sizeof(h.name));
    strncpy(rh.linkname, h.linkname, sizeof(h.name));

    /* Calculate and write checksum */
    const unsigned chksum = checksum(rh);
    sprintf(rh.checksum, "%06o", chksum);
    rh.checksum[7] = ' ';

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}


std::string Microtar::Microtar::strerror(const int err) {
    switch (err) {
        case static_cast<int>(EStatus::ESUCCESS):
            return "success";
        case static_cast<int>(EStatus::EFAILURE):
            return "failure";
        case static_cast<int>(EStatus::EOPENFAIL):
            return "could not open";
        case static_cast<int>(EStatus::EREADFAIL):
            return "could not read";
        case static_cast<int>(EStatus::EWRITEFAIL):
            return "could not write";
        case static_cast<int>(EStatus::ESEEKFAIL):
            return "could not seek";
        case static_cast<int>(EStatus::EBADCHKSUM):
            return "bad checksum";
        case static_cast<int>(EStatus::ENULLRECORD):
            return "null record";
        case static_cast<int>(EStatus::ENOTFOUND):
            return "file not found";
        default:
            return "unknown error";
    }
}

template<typename T>
int Microtar::Tar::file_write(const T &data, const size_t size) {
//    const unsigned res = fwrite(static_cast<const void *>(&data), 1, size, this->stream);
    this->fstream.write(reinterpret_cast<const char *>(&data), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_write(const std::string &data, const size_t size) {
    this->fstream.write(data.data(), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

template<typename T>
int Microtar::Tar::file_read(T &data, const size_t size) {
    this->fstream.read(reinterpret_cast<char *>(&data), size);
//    const unsigned res = fread(static_cast<void *>(&data), 1, size, this->stream);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_read(std::string &data, const size_t size) {
    this->fstream.read(static_cast<char *>(std::remove_const<char *>::type(data.c_str())), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_seek(const long offset) {
    this->fstream.seekg(offset, std::ios_base::beg);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_close() {
    this->fstream.close();
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::open(Tar &tar, const std::string &filename, const std::ios_base::openmode &mode) {
    header_t h = {};
    /* Open file */
    tar.fstream.open(filename.data(), mode | std::ios_base::binary);
    if (!tar.fstream) {
        return static_cast<int>(EStatus::EOPENFAIL);
    }

    /* Read first header to check it is valid if mode is `r` */
    if (mode == std::ios_base::in) {
        const int err = Microtar::read_header(tar, h);
        if (err != static_cast<int>(EStatus::ESUCCESS)) {
            Microtar::close(tar);
            return err;
        }
    }

    /* Return ok */
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::close(Tar &tar) {
    return tar.file_close();
}


int Microtar::Microtar::seek(Tar &tar, const long pos) {
    int err = tar.file_seek(pos);
    tar.pos = pos;
    return err;
}

int Microtar::Microtar::rewind(Tar &tar) {
    tar.remaining_data = 0;
    tar.last_header = 0;
    return Microtar::Microtar::seek(tar, 0);
}


int Microtar::Microtar::next(Tar &tar) {
    header_t h;
    /* Load header */
    const int err = Microtar::Microtar::read_header(tar, h);
    if (err) {
        return err;
    }
    /* Seek to next record */
    const int n = round_up(h.size, 512) + sizeof(raw_header_t);
    return Microtar::Microtar::seek(tar, tar.pos + n);
}


int Microtar::Microtar::find(Tar &tar, const std::string &name, header_t &h) {
    header_t header;
    /* Start at beginning */
    int err = Microtar::Microtar::rewind(tar);
    if (err) {
        return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while ((err = Microtar::Microtar::read_header(tar, header)) == static_cast<int>(EStatus::ESUCCESS)) {
        if (!strcmp(header.name, name.c_str())) {
            h = header;
            return static_cast<int>(EStatus::ESUCCESS);
        }
        Microtar::Microtar::next(tar);
    }
    /* Return error */
    return err;
}


int Microtar::Microtar::read_header(Tar &tar, header_t &h) {
    raw_header_t rh{};
    /* Save header position */
    tar.last_header = tar.pos;
    /* Read raw header */
    int err = tread(tar, rh, sizeof(rh));
    if (err) {
        return static_cast<int>(err);
    }
    /* Seek back to start of header */
    err = Microtar::Microtar::seek(tar, tar.last_header);
    if (err) {
        return static_cast<int>(err);
    }
    /* Load raw header into header struct and return */
    return raw_to_header(h, rh);
}

template<typename T>
int Microtar::Microtar::read_data(Tar &tar, T &data, const size_t size) {
    /* If we have no remaining data then this is the first read, we get the size,
     * set the remaining data and seek to the beginning of the data */
    if (tar.remaining_data == 0) {
        header_t h;
        /* Read header */
        int err = Microtar::Microtar::read_header(tar, h);
        if (err) {
            return err;
        }
        /* Seek past header and init remaining data */
        err = Microtar::Microtar::seek(tar, tar.pos + sizeof(raw_header_t));
        if (err) {
            return err;
        }
        tar.remaining_data = h.size;
    }
    /* Read data */
    int err = tread(tar, data, size);
    if (err) {
        return err;
    }
    tar.remaining_data -= size;
    /* If there is no remaining data we've finished reading and seek back to the
     * header */
    if (tar.remaining_data == 0) {
        return Microtar::Microtar::seek(tar, tar.last_header);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::write_header(Tar &tar, const header_t &h) {
    raw_header_t rh = {};
    /* Build raw header and write */
    header_to_raw(rh, h);
    tar.remaining_data = h.size;
    return twrite(tar, rh, sizeof(rh));
}


int Microtar::Microtar::write_file_header(Tar &tar, const std::string &name, const size_t size) {
    header_t h = {};
    /* Build header */
    strcpy(h.name, name.c_str());
    h.size = size;
    h.type = '0'; // EType::TREG
    h.mode = 0664;
    /* Write header */
    return Microtar::Microtar::write_header(tar, h);
}


int Microtar::Microtar::write_dir_header(Tar &tar, const std::string &name) {
    header_t h = {};
    /* Build header */
    strcpy(h.name, name.data());
    h.type = static_cast<char>(EType::TDIR);
    h.mode = 0775;
    /* Write header */
    return Microtar::Microtar::write_header(tar, h);
}


int Microtar::Microtar::write_data(Tar &tar, const std::string &data, const size_t size) {
    /* Write data */
    const int err = twrite(tar, data, size);
    if (err) {
        return err;
    }
    tar.remaining_data -= size;
    /* Write padding if we've written all the data for this file */
    if (tar.remaining_data == 0) {
        return write_null_bytes(tar, round_up(tar.pos, 512) - tar.pos);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Microtar::finalize(Tar &tar) {
    /* Write two NULL records */
    return write_null_bytes(tar, sizeof(raw_header_t) * 2);
}

void write(const std::string &filename) {
    Microtar::Tar tar{};
    std::ifstream fstream;
    fstream.open(filename, std::ios_base::binary);
    std::stringstream fcont;
    fcont << fstream.rdbuf();
    std::string fcontent(fcont.str());
/* Open archive for writing */
    Microtar::Microtar::open(tar, "test.tar", std::ios_base::out);

/* Write strings to files `test1.txt` and `test2.txt` */
    Microtar::Microtar::write_file_header(tar, filename, fcontent.size());
    Microtar::Microtar::write_data(tar, fcontent, fcontent.size());

/* Finalize -- this needs to be the last thing done before closing */
    Microtar::Microtar::finalize(tar);

/* Close archive */
    Microtar::Microtar::close(tar);
}

void read(const std::string &filename) {
    Microtar::Tar tar{};
    Microtar::header_t h;
/* Open archive for reading */
    Microtar::Microtar::open(tar, "test.tar", std::ios_base::in);

/* Print all file names and sizes */
    while (Microtar::Microtar::read_header(tar, h) != static_cast<int>(Microtar::EStatus::ENULLRECORD)) {
        printf("%s (%d bytes)\n", h.name, h.size);
        Microtar::Microtar::next(tar);
    }

/* Load and print contents of file "test.txt" */
    Microtar::Microtar::find(tar, filename, h);
    std::string content(h.size, '\0');
    Microtar::Microtar::read_data(tar, content, h.size);
    std::ofstream ofstream;
    ofstream.open(filename, std::ios_base::binary);
    ofstream.write(content.c_str(), content.size());
/* Close archive */
    Microtar::Microtar::close(tar);
}

int main(int argc, char *argv[]) {
//    if (argc < 2) {
//        std::cerr << "Filename?\n";
//        std::exit(-1);
//    }
//
//    write(argv[1]);
//    read(argv[1]);

    return 0;
}