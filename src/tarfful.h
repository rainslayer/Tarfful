#ifndef Tarfful
#define Tarfful

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#if (__cplusplus < 201703L)
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

#ifdef __linux__
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#endif

namespace Tarfful {
#if (__cplusplus < 201703L)
namespace fs = std::experimental::filesystem;
#else
namespace fs = std::filesystem;
#endif

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

inline size_t round_up(const size_t &n, const size_t &incr) {
  return n + (incr - n % incr) % incr;
}

inline size_t checksum(const Tarfful::raw_header_t &rh) {
  std::array<Tarfful::Byte, sizeof(Tarfful::raw_header_t)> headerPtr;
  std::memcpy(headerPtr.data(), &rh, sizeof(Tarfful::raw_header_t));

  unsigned res = 256;
  for (size_t i = 0, offset = offsetof(Tarfful::raw_header_t, checksum);
       i < offset; ++i) {
    res += headerPtr[i];
  }
  for (size_t i = offsetof(Tarfful::raw_header_t, type), size = 512; i < size;
       ++i) {
    res += headerPtr[i];
  }
  return res;
}

inline int header_to_raw(Tarfful::raw_header_t &rh,
                         const Tarfful::header_t &h) {
  sprintf(&rh.mode[0], "%zo", h.mode);
  sprintf(&rh.owner[0], "%zo", h.owner);
  sprintf(&rh.group[0], "%zo", h.group);
  sprintf(&rh.size[0], "%zo", h.size);
  sprintf(&rh.mtime[0], "%zo", h.mtime);
  strncpy(&rh.name[0], &h.name[0], h.name.size());
  strncpy(&rh.linkname[0], &h.linkname[0], h.linkname.size());
  rh.type = h.type;
  /* Calculate and write checksum */
  const unsigned chksum = checksum(rh);
  sprintf(&rh.checksum[0], "%06o", chksum);
  rh.checksum[7] = ' ';

  return static_cast<int>(Tarfful::EStatus::ESUCCESS);
}

class Tar {
private:
  std::unique_ptr<header_t> header;
  std::fstream fstream;
  const std::string archive_name = "";
  size_t pos = 0;
  size_t remaining_data = 0;
  size_t last_header = 0;

private:
  inline int write_header() {
    const std::unique_ptr<raw_header_t> rh(new raw_header_t);
    /* Build raw header and write */
    header_to_raw(*rh, *header);
    remaining_data = header->size;
    const int err = twrite(*rh, sizeof(*rh));
    return err;
  }

  inline int write_file_header(const std::string &name, const size_t &size) {
    strncpy(&header->name[0], name.data(), name.size());
    header->size = size;

#ifdef __linux__
    struct stat info = {};
    stat(name.data(), &info);
    const struct passwd *pw = getpwuid(info.st_uid);
    const struct group *gr = getgrgid(info.st_gid);
    header->mode = info.st_mode;
    header->owner = pw->pw_uid;
    header->group = gr->gr_gid;

    if (S_ISLNK(info.st_mode)) {
      header->type = 2;
    } else if (S_ISCHR(info.st_mode)) {
      header->type = 3;
    } else if (S_ISBLK(info.st_mode)) {
      header->type = 4;
    } else if (S_ISDIR(info.st_mode)) {
      header->type = 5;
    } else if (S_ISFIFO(info.st_mode)) {
      header->type = 6;
    };
#endif
    /* Write header */
    return write_header();
  }

  inline int twrite(const raw_header_t &rh, const size_t &size) {
    const int err = file_write(rh, size);
    pos += size;
    return err;
  }

  inline int twrite(const std::string &data, const size_t &size) {
    const int err = file_write(data, size);
    pos += size;
    return err;
  }

  inline int write_data(const std::string &data, const size_t &size) {
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

  inline int write_null_bytes(const size_t &n) {
    for (size_t i = 0; i < n; ++i) {
      const int err = twrite("\0", 1);
      if (err) {
        return err;
      }
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  inline int raw_to_header(const raw_header_t &rh) {
    /* If the checksum starts with a null byte we assume the record is NULL */
    if (rh.checksum[0] == '\0') {
      return static_cast<int>(Tarfful::EStatus::ENULLRECORD);
    }

    /* Build and compare checksum */
    const unsigned chksum1 = checksum(rh);
    const unsigned chksum2 = strtoul(&rh.checksum[0], nullptr, 8);
    if (chksum1 != chksum2) {
      return static_cast<int>(Tarfful::EStatus::EBADCHKSUM);
    }

    header->mode = strtoul(&rh.mode[0], nullptr, 8);
    header->owner = strtoul(&rh.owner[0], nullptr, 8);
    header->size = strtoul(&rh.size[0], nullptr, 8);
    header->mtime = strtoul(&rh.mtime[0], nullptr, 8);
    strncpy(&header->name[0], &rh.name[0], rh.name.size());
    strncpy(&header->linkname[0], &rh.linkname[0], rh.linkname.size());

    return static_cast<int>(Tarfful::EStatus::ESUCCESS);
  }

  inline int file_write(const raw_header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(raw_header_t)> dest;
    std::memcpy(dest.data(), &rh, sizeof(raw_header_t));
    fstream.write(dest.data(), size);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
      return static_cast<int>(EStatus::ESUCCESS);
    }
  }

  inline int file_write(const std::string &data, const size_t &size) {
    fstream.write(data.data(), size);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
      return static_cast<int>(EStatus::ESUCCESS);
    }
  }

  inline int file_read(std::ofstream &outputFile, const size_t &size) {
    std::string fileContent(size, '\0');
    fstream.read(&fileContent[0], size);
    outputFile.write(&fileContent[0], size);

    if (fstream.bad() || outputFile.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  inline int file_read(raw_header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(raw_header_t)> dest;
    std::memcpy(dest.data(), &rh, sizeof(raw_header_t));
    fstream.read(dest.data(), size);
    std::memcpy(&rh, dest.data(), sizeof(raw_header_t));

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  inline int file_seek(const size_t &offset) {
    fstream.seekg(offset, std::ios_base::beg);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::ESEEKFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  inline int read_header() {
    const std::unique_ptr<raw_header_t> rh(new raw_header_t);
    /* Save header position */
    last_header = pos;
    /* Read raw header */
    int err = tread(*rh);
    if (err) {
      return static_cast<int>(err);
    }
    /* Seek back to start of header */
    err = seek(last_header);

    if (err) {
      return err;
    }
    err = raw_to_header(*rh);
    return err;
  }

  inline int next() {
    /* Seek to next record */
    const int n = round_up(header->size, 512) + sizeof(raw_header_t);
    return seek(pos + n);
  }

  inline int rewind() {
    remaining_data = 0;
    last_header = 0;
    return seek(0);
  }

  inline int find(const std::string &name) {
    /* Start at beginning */
    const int err = rewind();
    if (err) {
      return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while (read_header() == static_cast<int>(EStatus::ESUCCESS)) {
      if (!strcmp(header->name.data(), name.data())) {
        return static_cast<int>(EStatus::ESUCCESS);
      }
      next();
    }
    /* Return error */
    return static_cast<int>(EStatus::ENOTFOUND);
  }

  inline int seek(const size_t &newPos) {
    const int err = file_seek(newPos);
    pos = newPos;
    return err;
  }

  inline int read_data(std::ofstream &outputFile, const size_t &size) {
    /* If we have no remaining data then this is the first read, we get the
     * size, set the remaining data and seek to the beginning of the data */
    if (remaining_data == 0) {
      const std::unique_ptr<header_t> h(new header_t);
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
      remaining_data = h->size;
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

  inline int tread(std::ofstream &outputFile, const size_t &size) {
    const int err = file_read(outputFile, size);
    pos += size;
    return err;
  }

  inline int tread(raw_header_t &rh) {
    const int err = file_read(rh, 512);
    pos += 512;
    return err;
  }

public:
  inline explicit Tar(const std::string &archive)
      : header(new header_t), archive_name(std::move(archive)){};

  inline int ArchiveFile(const std::string &filename) {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::out | std::fstream::binary |
                                     std::fstream::app);
    }

    if (fstream.good()) {
      header = std::unique_ptr<header_t>(new header_t);
      std::ifstream ifstream;
      ifstream.open(filename, std::fstream::in);

      if (ifstream.bad()) {
        return static_cast<int>(EStatus::EWRITEFAIL);
      }

#ifdef verbose
      std::cout << filename << '\n';
#endif
      std::stringstream fileContent;
      fileContent << ifstream.rdbuf();
      int err = write_file_header(filename, fileContent.str().size());

      if (err) {
#ifdef verbose
        std::cerr << "Error writing file header for: " << filename << '\n';
#endif
        return err;
      }

      err = write_data(fileContent.str(), fileContent.str().size());
      if (err) {
#ifdef verbose
        std::cerr << "Error writing file content for: " << filename << '\n';
#endif
        return err;
      }

      return static_cast<int>(EStatus::ESUCCESS);

    } else {
      return static_cast<int>(EStatus::EWRITEFAIL);
    }
  }

  inline int ArchiveDirectoryContent(const std::string &path) {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::out | std::fstream::binary |
                                     std::fstream::app);
    }

    if (fstream.good()) {
      if (fs::is_directory(path)) {
        for (const auto &dirEntry : fs::recursive_directory_iterator(path)) {
          header = std::unique_ptr<header_t>(new header_t);
          if (fs::is_directory(dirEntry)) {
            continue;
          } else {
            ArchiveFile(dirEntry.path());
          }
        }
        return static_cast<int>(EStatus::ESUCCESS);
      }
      throw std::invalid_argument("Not a directory");
    }
    return static_cast<int>(EStatus::EWRITEFAIL);
  }

  inline int Extract(const std::string &filename) {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::in | std::fstream::binary);
    }

    if (fstream.good()) {
      if (!fs::exists(filename)) {
        const std::string parentPath = fs::path(filename).parent_path();
        if (!parentPath.empty() && !fs::exists(parentPath)) {
          fs::create_directories(parentPath);
        }
      }

      int err = find(filename);

      if (err) {
#ifdef verbose
        std::cerr << "File not found: " << filename << '\n';
#endif

        return err;
      }

      std::ofstream fileContent(filename,
                                std::fstream::out | std::fstream::binary);
      err = read_data(fileContent, header->size);
      if (err) {
#ifdef verbose
        std::cerr << "Error extracting file: " << filename << '\n';
#endif
        fs::remove(filename);
        return err;
      }

      return static_cast<int>(EStatus::ESUCCESS);
    } else {
      return static_cast<int>(EStatus::EOPENFAIL);
    }
  }

  inline int ExtractAll() {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::in | std::fstream::binary);
    }
    if (fstream.good()) {

      while (read_header() != static_cast<int>(Tarfful::EStatus::ENULLRECORD)) {
        const std::string filename = header->name.data();

#ifdef verbose
        std::cout << filename << '\n';
#endif

        const std::string parentPath = fs::path(filename).parent_path();
        if (!parentPath.empty() && !fs::exists(parentPath)) {
          fs::create_directories(parentPath);
        }

        std::ofstream fileContent(filename,
                                  std::fstream::out | std::fstream::binary);
        const int err = read_data(fileContent, header->size);
        if (err) {
#ifdef verbose
          std::cerr << "Error extracting file: " << filename << '\n';
#endif

          fs::remove(filename);
          return err;
        }

        pos = last_header;
        remaining_data = 0;
        next();
      }
      return static_cast<int>(EStatus::ESUCCESS);
    } else {
      return static_cast<int>(EStatus::EOPENFAIL);
    }
  }
};
} // namespace Tarfful

#endif
