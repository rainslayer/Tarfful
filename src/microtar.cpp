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

int Microtar::Tar::tread(std::ofstream &outputFile, const size_t size) {
    const int err = file_read(outputFile, size);
    pos += size;
    return err;
}

int Microtar::Tar::tread(Microtar::raw_header_t &rh, const size_t size) {
    const int err = file_read(rh, size);
    pos += size;
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


int Microtar::Tar::raw_to_header(Microtar::raw_header_t &rh) {
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

    header.mode = strtoul(&rh.mode[0], nullptr, 8);
    header.owner = strtoul(&rh.owner[0], nullptr, 8);
    header.size = strtoul(&rh.size[0], nullptr, 8);
    header.mtime = strtoul(&rh.mtime[0], nullptr, 8);
    strncpy(&header.name[0], &rh.name[0], header.name.size());
    strncpy(&header.linkname[0], &rh.linkname[0], header.linkname.size());

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}

static int header_to_raw(Microtar::raw_header_t &rh, const Microtar::header_t &h) {
    sprintf(&rh.mode[0], "%o", h.mode);
    sprintf(&rh.owner[0], "%o", h.owner);
    sprintf(&rh.group[0], "%o", h.group);
    sprintf(&rh.size[0], "%o", h.size);
    sprintf(&rh.mtime[0], "%o", h.mtime);
    strncpy(&rh.name[0], &h.name[0], sizeof(rh.name));
    strncpy(&rh.linkname[0], &h.linkname[0], sizeof(rh.linkname));

    /* Calculate and write checksum */
    const unsigned chksum = checksum(rh);
    sprintf(&rh.checksum[0], "%06o", chksum);
    rh.checksum[7] = ' ';

    return static_cast<int>(Microtar::EStatus::ESUCCESS);
}

template<typename T>
int Microtar::Tar::file_write(const T &data, const size_t size) {
    fstream.write(reinterpret_cast<const char *>(&data), size);

    if (fstream.bad()) {
        return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
        return static_cast<int>(EStatus::ESUCCESS);
    }
}

int Microtar::Tar::file_write(const std::string &data, const size_t size) {
    fstream.write(data.c_str(), size);

    if (fstream.bad()) {
        return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
        return static_cast<int>(EStatus::ESUCCESS);
    }
}

int Microtar::Tar:: file_read(std::ofstream &outputFile, const size_t size) {
    std::string fileContent(size, '\0');
    fstream.read(&fileContent[0], size);
    outputFile.write(&fileContent[0], size);

    if (fstream.bad() || outputFile.bad()) {
        return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_read(Microtar::raw_header_t &rh, const size_t size) {
    fstream.read(reinterpret_cast<char *>(&rh), size);

    if (fstream.bad()) {
        return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}

int Microtar::Tar::file_seek(const long offset) {
    fstream.seekg(offset, std::ios_base::beg);

    if (fstream.bad()) {
        return static_cast<int>(EStatus::ESEEKFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
}


int Microtar::Tar::Archive(const std::string &path) {
    if (!fstream.is_open()) {
        fstream.open(archive_name, std::fstream::out | std::fstream::binary | std::fstream::app);
    }

    if (fstream.good()) {
        if (fs::is_directory(path)) {
            for (const auto& dirEntry :  fs::recursive_directory_iterator(path)) {
                if (fs::is_directory(dirEntry)) {
                    continue;
                }

                Archive(dirEntry.path());
            }
        } else {
            std::ifstream ifstream;
            ifstream.open(path, std::fstream::in);

            if (ifstream.bad()) {
                return static_cast<int>(EStatus::EWRITEFAIL);
            }

            #ifdef verbose
                std::cout << filename << '\n';
            #endif

            std::string fileContent((std::istreambuf_iterator<char>(ifstream)), std::istreambuf_iterator<char>());
            write_file_header(path, fileContent.size());
            write_data(fileContent, fileContent.size());
        }
        return static_cast<int>(EStatus::ESUCCESS);
    } else {
        return static_cast<int>(EStatus::EWRITEFAIL);
    }
}

int Microtar::Tar::Extract(const std::string &filename) {
    if (!fstream.is_open()) {
        fstream.open(archive_name, std::fstream::in | std::fstream::binary);
    }

    if (fstream.good()) {
        if (!fs::exists(filename)) {
            const fs::path parentPath = fs::path(filename).parent_path();
            if (!fs::exists(parentPath)) {
                fs::create_directories(parentPath);
            }
        }

        std::ofstream fileContent(filename, std::fstream::out | std::fstream::binary);
        find(filename);
        read_data(fileContent, header.size);

        return static_cast<int>(EStatus::ESUCCESS);
    } else {
        return static_cast<int>(EStatus::EREADFAIL);
    }
}

int Microtar::Tar::ExtractAll() {
    if (!fstream.is_open()) {
        fstream.open(archive_name, std::fstream::in | std::fstream::binary);
    }

    if (fstream.good()) {
        while (read_header() != static_cast<int>(Microtar::EStatus::ENULLRECORD)) {
            const std::string filename = header.name.data();

            #ifdef verbose
                std::cout << filename << '\n';
            #endif

            const std::string parentPath = fs::path(filename).parent_path();
            if (!parentPath.empty() && !fs::exists(parentPath)) {
                fs::create_directories(parentPath);
            }

            std::ofstream fileContent(filename, std::fstream::out | std::fstream::binary);
            read_data(fileContent, header.size);
            pos = last_header;
            remaining_data = 0;

            next();
        }
        return static_cast<int>(EStatus::ESUCCESS);
    } else {
        return static_cast<int>(EStatus::EREADFAIL);
    }
}

int Microtar::Tar::seek(const long newPos) {
    int err = file_seek(newPos);
    pos = newPos;
    return err;
}

int Microtar::Tar::rewind() {
    remaining_data = 0;
    last_header = 0;
    return seek(0);
}


int Microtar::Tar::next() {
    /* Seek to next record */
    const int n = round_up(header.size, 512) + sizeof(raw_header_t);
    return seek(pos + n);
}


int Microtar::Tar::find(const std::string &name) {
    /* Start at beginning */
    int err = rewind();
    if (err) {
        return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while ((err = read_header()) == static_cast<int>(EStatus::ESUCCESS)) {
        if (!strcmp(&header.name[0], name.data())) {
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
    int err = tread(rh, sizeof(rh));
    if (err) {
        return static_cast<int>(err);
    }
    /* Seek back to start of header */
    err = seek(last_header);

    if (err) {
        return err;
    }
    return raw_to_header(rh);
}

int Microtar::Tar::read_data(std::ofstream &outputFile, const size_t size) {
    /* If we have no remaining data then this is the first read, we get the size,
     * set the remaining data and seek to the beginning of the data */
    if (remaining_data == 0) {
        header_t h;
        /* Read header */
        int err = read_header();
        if (err) {
            return err;
        }
        /* Seek past header and init remaining data */
        err = seek(pos + sizeof(raw_header_t));
        if (err) {
            return err;
        }
        remaining_data = h.size;
    }
    /* Read data */
    int err = tread(outputFile, size);
    if (err) {
        return err;
    }
    remaining_data -= size;
    /* If there is no remaining data we've finished reading and seek back to the
     * header */
    if (remaining_data == 0) {
        return seek(last_header);
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
    auto fileStatus = fs::status(name);
    strcpy(&header.name[0], name.data());
    header.size = size;
    header.mode = static_cast<size_t>(fileStatus.permissions());

    #ifdef __linux__
        struct stat info;
        stat(name.data(), &info);
        struct passwd *pw = getpwuid(info.st_uid);
        struct group  *gr = getgrgid(info.st_gid);

        header.owner = pw->pw_uid;
        header.group = gr->gr_gid;
    #endif

    const auto mtime = std::chrono::time_point_cast<std::chrono::seconds>(fs::last_write_time(name));
    header.mtime = mtime.time_since_epoch().count();
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

void archiveFiles(const std::string &archive, const std::string &path) {
    Microtar::Tar tar(archive);
    tar.Archive(path);
}

void extractFiles(std::string archive, const std::vector<std::string> &filenames) {
    Microtar::Tar tar(archive);

    for (const auto &i: filenames) {
        tar.Extract(i);
    }
}

int main(int argc, char *argv[]) {
    if (std::string(argv[1]) == "c") {
        archiveFiles(argv[2], std::string(argv[3]));
    } else if (std::string(argv[1]) == "x") {
        const std::vector<std::string> files = {argv + 3, argv + argc};
        extractFiles(argv[2], files);
    } else if (std::string(argv[1]) == "xx") {
        Microtar::Tar tar(argv[2]);
        tar.ExtractAll();
    } else {
        std::cerr << "Filename?\n";
        std::exit(-1);
    }
    return 0;
}
