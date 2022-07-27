#ifndef Tarfful
#define Tarfful

#include <array>
#include <chrono>
#include <vector>
#include <cstring>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <linux/kdev_t.h>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <exception>
#include <algorithm>

#if (__cplusplus < 201703L)
#include <experimental/filesystem>
#else
#include <filesystem>
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

typedef struct header_t {
  std::array<Byte, 100> name = {};
  std::array<Byte, 8> mode = {};
  std::array<Byte, 8> owner = {};
  std::array<Byte, 8> group = {};
  std::array<Byte, 12> size = {};
  std::array<Byte, 12> mtime = {};
  std::array<Byte, 8> checksum = {};
  unsigned char type = 0;
  std::array<Byte, 100> linkname = {};
  std::array<Byte, 6> ustar = {"ustar"};
  std::array<Byte, 2> ustar_version = {};
  std::array<Byte, 32> owner_name = {""};
  std::array<Byte, 32> group_name = {""};
  std::array<Byte, 8> device_major = {};
  std::array<Byte, 8> device_minor = {};
  std::array<Byte, 155> filename_prefix = {};
  std::array<Byte, 12> _padding = {};
} header_t;

size_t round_up(const size_t pos) {
  constexpr auto incr = sizeof(header_t);
  return pos + (incr - pos % incr) % incr;
}

size_t checksum(const header_t &header) {
    std::array<Byte, sizeof(header_t)> headerPtr;
    std::memcpy(headerPtr.data(), &header, sizeof(header_t));

    auto res = 256;

    for (size_t i = 0, offset = offsetof(header_t, checksum);
       i < offset; ++i) {
    res += headerPtr[i];
  }

  for (size_t i = offsetof(header_t, type), size = 512; i < size;
       ++i) {
      res += headerPtr[i];
  }

  return res;
}

class Tar {
private:
  std::fstream fstream;
  const std::string archive_name = "";
  size_t pos = 0;
  size_t remaining_data = 0;
  size_t last_header = 0;
  static constexpr int BUFF_SIZE = 8192;

private:
  void write_file_header(const std::string &name) {
    auto header = header_t();

    {
      const std::string filename = fs::path(name).filename();
      strncpy(header.name.data(), filename.data(), header.name.size());
    }

    {
      const std::string parent_path = fs::path(name).parent_path();
      if (parent_path[0] == '/') {
        #ifdef verbose
              std::cout << "Removing leading / from path" << ' ' << parent_path << '\n';
        #endif

        strncpy(header.filename_prefix.data(), parent_path.data() + 1,
                parent_path.size());
      } else {
        strncpy(header.filename_prefix.data(), parent_path.data(),
                parent_path.size());
      }
    }

    {
      struct stat info = {};
      stat(name.data(), &info);
      const struct passwd *pw = getpwuid(info.st_uid);
      const struct group *gr = getgrgid(info.st_gid);

      sprintf(&header.mode[0], "%o", info.st_mode);
      sprintf(&header.owner[0], "%o", pw->pw_uid);
      sprintf(&header.group[0], "%o", gr->gr_gid);
      sprintf(&header.mtime[0], "%zo", info.st_mtim.tv_sec);
      strncpy(header.owner_name.data(), pw->pw_name, header.owner_name.size());
      strncpy(header.group_name.data(), gr->gr_name, header.group_name.size());
      sprintf(header.device_major.data(), "%lo", MAJOR(info.st_dev));
      sprintf(header.device_minor.data(), "%lo", MINOR(info.st_dev));

      if (S_ISLNK(info.st_mode)) {
        header.type = 2;
      } else if (S_ISCHR(info.st_mode)) {
        header.type = 3;
      } else if (S_ISBLK(info.st_mode)) {
        header.type = 4;
      } else if (S_ISDIR(info.st_mode)) {
        header.type = 5;
      } else if (S_ISFIFO(info.st_mode)) {
        header.type = 6;
      }
    }

      sprintf(&header.size[0], "%zo", fs::file_size(name));
      sprintf(&header.checksum[0], "%06zo", checksum(header));
      header.checksum[7] = ' ';

      file_write(header);
  }

  void write_data(const std::string &filename) {
    std::ifstream ifstream;
    ifstream.open(filename, std::fstream::in | std::fstream::binary);

    while (!ifstream.eof()) {
      std::string chunk(BUFF_SIZE, 0);
      ifstream.read(chunk.data(), BUFF_SIZE);

      auto contentSize = ifstream.gcount();
      fstream.write(chunk.data(), contentSize);
      pos += contentSize;
    }
    
    write_null_bytes(round_up(pos) - pos);
  }

  void write_null_bytes(const size_t &n) {
    const std::string nullstring(n, '\0');
    pos += n;
    fstream.write(nullstring.data(), n);
  }

  int raw_to_header(const header_t &header) {
    // if (rh.checksum[0] == '\0') {
    //   return static_cast<int>(Tarfful::EStatus::ENULLRECORD);
    // }

    const unsigned chksum1 = checksum(header);
    const unsigned chksum2 = strtoul(header.checksum.data(), nullptr, 8);

    if (chksum1 != chksum2) {
      return static_cast<int>(Tarfful::EStatus::EBADCHKSUM);
    }

    // header.mode = strtoul(&rh.mode[0], nullptr, 8);
    // header.owner = strtoul(&rh.owner[0], nullptr, 8);
    // header.size = strtoul(&rh.size[0], nullptr, 8);
    // header.mtime = strtoul(&rh.mtime[0], nullptr, 8);
    // strncpy(header.name.data(), &rh.name[0], rh.name.size());
    // strncpy(header.linkname.data(), &rh.linkname[0], rh.linkname.size());

    return static_cast<int>(Tarfful::EStatus::ESUCCESS);
  }

  int file_write(const header_t &header) {
    std::array<Byte, sizeof(header_t)> dest = {};
    std::memcpy(dest.data(), &header, dest.size());
    fstream.write(dest.data(), dest.size());

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
      return static_cast<int>(EStatus::ESUCCESS);
    }
  }

  int file_write(const std::string &data) {
    fstream.write(data.data(), data.size());

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
      return static_cast<int>(EStatus::ESUCCESS);
    }
  }

  int file_read(std::ofstream &outputFile, const size_t &size) {
    std::string fileContent(size, '\0');
    fstream.read(&fileContent[0], size);
    outputFile.write(&fileContent[0], size);

    if (fstream.bad() || outputFile.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int file_read(header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(header_t)> dest;
    std::memcpy(dest.data(), &rh, sizeof(header_t));
    fstream.read(dest.data(), size);
    std::memcpy(&rh, dest.data(), sizeof(header_t));

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int file_seek(const size_t &offset) {
    fstream.seekg(offset, std::ios_base::beg);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::ESEEKFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int read_header() {
    header_t rh;
    /* Save header position */
    last_header = pos;
    /* Read raw header */
    int err = tread(rh);
    if (err) {
      return static_cast<int>(err);
    }
    /* Seek back to start of header */
    err = seek(last_header);

    if (err) {
      return err;
    }
    err = raw_to_header(rh);
    return err;
  }

  int next(header_t &header) {
    /* Seek to next record */
    const int n = round_up(std::stoi(header.size.data())) + sizeof(header_t);
    return seek(pos + n);
  }

  int rewind() {
    remaining_data = 0;
    last_header = 0;
    return seek(0);
  }

  int find(const std::string &name, header_t &header) {
    /* Start at beginning */
    const int err = rewind();
    if (err) {
      return err;
    }
    /* Iterate all files until we hit an error or find the file */
    while (read_header() == static_cast<int>(EStatus::ESUCCESS)) {
      if (!strcmp(header.name.data(), name.data())) {
        return static_cast<int>(EStatus::ESUCCESS);
      }
      next(header);
    }
    /* Return error */
    return static_cast<int>(EStatus::ENOTFOUND);
  }

  int seek(const size_t &newPos) {
    const int err = file_seek(newPos);
    pos = newPos;
    return err;
  }

  int read_data(std::ofstream &outputFile, const header_t &header) {
      const size_t size = std::stoi(header.size.data());

    if (remaining_data == 0) {
      /* Read header */
      int err = read_header();
      if (err) {
        return err;
      }
      seek(pos + sizeof(header_t));
      remaining_data = size;
    }

    tread(outputFile, size);
    remaining_data -= size;

    if (remaining_data == 0) {
      return seek(last_header);
    }

    return static_cast<int>(EStatus::ESUCCESS);
  }

  int tread(std::ofstream &outputFile, const size_t &size) {
    const int err = file_read(outputFile, size);
    pos += size;
    return err;
  }

  int tread(header_t &rh) {
    const int err = file_read(rh, 512);
    pos += 512;
    return err;
  }

public:
  explicit Tar(const std::string &archive) : archive_name(archive) {
      fstream.open(archive_name, std::fstream::out |
                    std::fstream::binary | std::fstream::app);
  }


  void ArchiveFile(const std::string &filename) {
      write_file_header(filename);
      write_data(filename);
  }

  int ArchiveDirectoryContent(const std::string &path) {
    if (fstream.good()) {
      if (fs::is_directory(path)) {
        for (const auto &dirEntry : fs::recursive_directory_iterator(path)) {
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

  int Extract(const std::string &filename) {
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
      header_t header;
      find(filename, header);

      std::ofstream fileContent(filename,
                                std::fstream::out | std::fstream::binary);

      read_data(fileContent, header);
      return static_cast<int>(EStatus::ESUCCESS);
    } else {
      return static_cast<int>(EStatus::EOPENFAIL);
    }
  }

  int ExtractAll() {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::in | std::fstream::binary);
    }
    if (fstream.good()) {

      while (read_header() != static_cast<int>(Tarfful::EStatus::ENULLRECORD)) {
        header_t header;
        const std::string filename = header.name.data();
        const std::string parentPath = fs::path(filename).parent_path();

        if (!parentPath.empty() && !fs::exists(parentPath)) {
          fs::create_directories(parentPath);
        }

        std::ofstream fileContent(filename,
                                  std::fstream::out | std::fstream::binary);

        const int err = read_data(fileContent, header);

        if (err) {
          fs::remove(filename);
          return err;
        }

        pos = last_header;
        remaining_data = 0;
        next(header);
      }
      return static_cast<int>(EStatus::ESUCCESS);
    } else {
      return static_cast<int>(EStatus::EOPENFAIL);
    }
  }
};
} // namespace Tarfful

#endif
