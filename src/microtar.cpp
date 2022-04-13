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
int Microtar::Tar::twrite(const T &data, const size_t size) {
    const int err = file_write(data, size);
    pos += size;
    return err;
}


int Microtar::Tar::write_null_bytes(const int n) {
    for (size_t i = 0; i < n; ++i) {
        const int err = twrite('\0', 1);
        if (err) {
            return err;
        }
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


static int raw_to_header(Microtar::header_t &h, Microtar::raw_header_t &rh) {
    /* If the checksum starts with a null byte we assume the record is NULL */
    if (rh.checksum[0] == '\0') {
        return static_cast<int>(Microtar::EStatus::ENULLRECORD);
    }

    /* Build and compare checksum */
    const unsigned chksum1 = checksum(rh);
    const unsigned chksum2 = strtoul(&rh.checksum[0], nullptr, 8);
    if (chksum1 != chksum2) {
        return static_cast<int>(Microtar::EStatus::EBADCHKSUM);
    }

    h.mode = strtoul(&rh.mode[0], nullptr, 8);
    h.owner = strtoul(&rh.owner[0], nullptr, 8);
    h.size = strtoul(&rh.size[0], nullptr, 8);
    h.mtime = strtoul(&rh.mtime[0], nullptr, 8);

    h.type = rh.type;
    strncpy(&h.name[0], &rh.name[0], h.name.size());
    strncpy(&h.linkname[0], &rh.linkname[0], h.linkname.size());

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}

static int header_to_raw(Microtar::raw_header_t &rh, const Microtar::header_t &h) {
    sprintf(&rh.mode[0], "%o", h.mode);
    sprintf(&rh.owner[0], "%o", h.owner);
    sprintf(&rh.size[0], "%o", h.size);
    sprintf(&rh.mtime[0], "%o", h.mtime);
    rh.type = h.type ? h.type : static_cast<char>(Microtar::EType::TREG);
    strncpy(&rh.name[0], &h.name[0], rh.name.size());
    strncpy(&rh.linkname[0], &h.linkname[0], rh.linkname.size());

    /* Calculate and write checksum */
    const unsigned chksum = checksum(rh);
    sprintf(&rh.checksum[0], "%06o", chksum);
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
    fstream.write(reinterpret_cast<const char *>(&data), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_write(const std::string &data, const size_t size) {
    fstream.write(data.c_str(), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

template<typename T>
int Microtar::Tar::file_read(T &data, const size_t size) {
    fstream.read(reinterpret_cast<char *>(&data), size);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_read(std::string &data, const size_t size) {
    fstream.read(&data[0], size);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_seek(const long offset) {
    fstream.seekg(offset, std::ios_base::beg);

    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_close() {
    fstream.close();

    return static_cast<int>(EStatus::ESUCCESS);
}

void Microtar::Tar::Write(const std::vector<std::string> &filenames) {
    fstream.open(archiveName, std::fstream::out | std::fstream::binary | std::fstream::app);

    if (fstream.good()) {
        for (auto &filename: filenames) {
            std::ifstream ifstream;
            ifstream.open(filename, std::fstream::in);
            std::string fileContent((std::istreambuf_iterator<char>(ifstream)), std::istreambuf_iterator<char>());
            write_file_header(filename, fileContent.size());
            write_data(fileContent, fileContent.size());
        }
    } else {
        std::cerr << fstream.rdstate() << '\n';
    }
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


int Microtar::Tar::next() {
    /* Load header */
    const int err = read_header();
    if (err) {
        return err;
    }
    /* Seek to next record */
    const int n = round_up(header.size, 512) + sizeof(raw_header_t);
    return Microtar::Microtar::seek(*this, this->pos + n);
}


int Microtar::Tar::find(const std::string &name) {
    /* Start at beginning */
    int err = Microtar::Microtar::rewind(*this);
    if (err) {
        return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while ((err = read_header()) == static_cast<int>(EStatus::ESUCCESS)) {
        if (!strcmp(&header.name[0], name.c_str())) {
            return static_cast<int>(EStatus::ESUCCESS);
        }
        next();
    }
    /* Return error */
    return err;
}


int Microtar::Tar::read_header() {
    raw_header_t rh;
    /* Save header position */
    last_header = pos;
    /* Read raw header */
    int err = tread(*this, rh, sizeof(rh));
    if (err) {
        return static_cast<int>(err);
    }
    /* Seek back to start of header */
    err = Microtar::Microtar::seek(*this, this->last_header);
    return raw_to_header(header, rh);
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


int Microtar::Tar::write_header() {
    raw_header_t rh;
    /* Build raw header and write */
    header_to_raw(rh, header);
    remaining_data = header.size;
    return twrite(rh, sizeof(rh));
}

int Microtar::Tar::write_file_header(const std::string &name, const size_t size) {
    strcpy(&header.name[0], name.c_str());
    header.size = size;
    header.type = static_cast<int>(EType::TREG);
    header.mode = 0664;
    /* Write header */
    return write_header();
}

int Microtar::Tar::write_dir_header(const std::string &name) {
    header_t h;
    /* Build header */
//    h.name = name;
    strcpy(&h.name[0], name.c_str());
    h.type = static_cast<char>(EType::TDIR);
    h.mode = 0664;
    /* Write header */
    return write_header();
}


int Microtar::Tar::write_data(const std::string &data, const size_t size) {
    /* Write data */
    const int err = twrite(data, size);
    if (err) {
        return err;
    }
    remaining_data -= size;
    /* Write padding if we've written all the data for this file */
    if (remaining_data == 0) {
        return write_null_bytes(round_up(pos, 512) - pos);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}

void archiveFiles(std::string archive, const std::vector<std::string> &filenames) {
    Microtar::Tar tar(archive);
    tar.Write(filenames);
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        std::vector<std::string> files = {argv + 2, argv + argc};
        archiveFiles(argv[1], files);
    } else {
        std::cerr << "Filename?\n";
        std::exit(-1);
    }
    return 0;
}