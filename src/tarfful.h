#ifndef Tarfful
#define Tarfful

#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <linux/kdev_t.h>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>

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

typedef struct raw_header_t {
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
} raw_header_t;

typedef struct header_t {
  size_t mode = 0;
  size_t owner = 0;
  size_t group = 0;
  size_t size = 0;
  size_t mtime = 0;
  unsigned char type = 0;
  std::array<Byte, 100> name = {};
  std::array<Byte, 100> linkname = {};
  std::array<Byte, 155> filename_prefix = {};
  std::array<Byte, 32> owner_name = {""};
  std::array<Byte, 32> group_name = {""};
  std::array<Byte, 8> device_major = {};
  std::array<Byte, 8> device_minor = {};
} header_t;

size_t round_up(const size_t &n, const size_t &incr) {
  return n + (incr - n % incr) % incr;
}

size_t checksum(const Tarfful::raw_header_t &rh) {
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

int header_to_raw(Tarfful::raw_header_t &rh, const Tarfful::header_t &h) {
  sprintf(&rh.mode[0], "%zo", h.mode);
  sprintf(&rh.owner[0], "%zo", h.owner);
  sprintf(&rh.group[0], "%zo", h.group);
  sprintf(&rh.size[0], "%zo", h.size);
  sprintf(&rh.mtime[0], "%zo", h.mtime);
  strncpy(&rh.name[0], &h.name[0], rh.name.size());
  strncpy(&rh.linkname[0], &h.linkname[0], h.linkname.size());
  strncpy(&rh.filename_prefix[0], &h.filename_prefix[0],
          rh.filename_prefix.size());
  strncpy(rh.owner_name.data(), h.owner_name.data(), rh.owner_name.size());
  strncpy(rh.group_name.data(), h.group_name.data(), rh.group_name.size());
  strncpy(rh.device_major.data(), h.device_major.data(),
          rh.device_major.size());
  strncpy(rh.device_minor.data(), h.device_minor.data(),
          rh.device_minor.size());

  rh.type = h.type;

  const unsigned chksum = checksum(rh);
  sprintf(&rh.checksum[0], "%06o", chksum);
  rh.checksum[7] = ' ';

  return static_cast<int>(Tarfful::EStatus::ESUCCESS);
}

class Tar {
private:
  header_t header;
  std::fstream fstream;
  const std::string archive_name = "";
  size_t pos = 0;
  size_t remaining_data = 0;
  size_t last_header = 0;

private:
  int write_header() {
    raw_header_t rh;
    /* Build raw header and write */
    header_to_raw(rh, header);
    remaining_data = header.size;
    const int err = twrite(rh, sizeof(rh));
    return err;
  }

  int write_file_header(const std::string &name, const size_t &size) {
    strncpy(header.name.data(), fs::path(name).filename().c_str(),
            header.name.size());

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
    header.size = size;

    struct stat info = {};
    stat(name.data(), &info);
    const struct passwd *pw = getpwuid(info.st_uid);
    const struct group *gr = getgrgid(info.st_gid);
    header.mode = info.st_mode;
    header.owner = pw->pw_uid;
    header.group = gr->gr_gid;
    header.mtime = info.st_mtim.tv_sec;
    strncpy(header.owner_name.data(), pw->pw_name, strlen(pw->pw_name));
    strncpy(header.group_name.data(), gr->gr_name, strlen(gr->gr_name));
    sprintf(header.device_major.data(), "%zo", MAJOR(info.st_dev));
    sprintf(header.device_minor.data(), "%zo", MINOR(info.st_dev));

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
    };

    return write_header();
  }

  int twrite(const raw_header_t &rh, const size_t &size) {
    const int err = file_write(rh, size);
    pos += size;
    return err;
  }

  int twrite(const std::string &data, const size_t &size) {
    const int err = file_write(data, size);
    pos += size;
    return err;
  }

  int write_data(const std::string &data, const size_t &size) {
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

  int write_null_bytes(const size_t &n) {
    for (size_t i = 0; i < n; ++i) {
      const int err = twrite("\0", 1);
      if (err) {
        return err;
      }
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int raw_to_header(const raw_header_t &rh) {
    if (rh.checksum[0] == '\0') {
      return static_cast<int>(Tarfful::EStatus::ENULLRECORD);
    }

    const unsigned chksum1 = checksum(rh);
    const unsigned chksum2 = strtoul(&rh.checksum[0], nullptr, 8);
    if (chksum1 != chksum2) {
      return static_cast<int>(Tarfful::EStatus::EBADCHKSUM);
    }

    header.mode = strtoul(&rh.mode[0], nullptr, 8);
    header.owner = strtoul(&rh.owner[0], nullptr, 8);
    header.size = strtoul(&rh.size[0], nullptr, 8);
    header.mtime = strtoul(&rh.mtime[0], nullptr, 8);
    strncpy(header.name.data(), &rh.name[0], rh.name.size());
    strncpy(header.linkname.data(), &rh.linkname[0], rh.linkname.size());

    return static_cast<int>(Tarfful::EStatus::ESUCCESS);
  }

  int file_write(const raw_header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(raw_header_t)> dest;
    std::memcpy(dest.data(), &rh, sizeof(raw_header_t));
    fstream.write(dest.data(), size);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EWRITEFAIL);
    } else {
      return static_cast<int>(EStatus::ESUCCESS);
    }
  }

  int file_write(const std::string &data, const size_t &size) {
    fstream.write(data.data(), size);

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

  int file_read(raw_header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(raw_header_t)> dest;
    std::memcpy(dest.data(), &rh, sizeof(raw_header_t));
    fstream.read(dest.data(), size);
    std::memcpy(&rh, dest.data(), sizeof(raw_header_t));

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
    raw_header_t rh;
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

  int next() {
    /* Seek to next record */
    const int n = round_up(header.size, 512) + sizeof(raw_header_t);
    return seek(pos + n);
  }

  int rewind() {
    remaining_data = 0;
    last_header = 0;
    return seek(0);
  }

  int find(const std::string &name) {
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
      next();
    }
    /* Return error */
    return static_cast<int>(EStatus::ENOTFOUND);
  }

  int seek(const size_t &newPos) {
    const int err = file_seek(newPos);
    pos = newPos;
    return err;
  }

  int read_data(std::ofstream &outputFile, const size_t &size) {
    /* If we have no remaining data then this is the first read, we get the
     * size, set the remaining data and seek to the beginning of the data */
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

  int tread(std::ofstream &outputFile, const size_t &size) {
    const int err = file_read(outputFile, size);
    pos += size;
    return err;
  }

  int tread(raw_header_t &rh) {
    const int err = file_read(rh, 512);
    pos += 512;
    return err;
  }

public:
  explicit Tar(const std::string &archive) : archive_name(archive){};

  int ArchiveFile(const std::string &filename) {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::out | std::fstream::binary |
                                     std::fstream::app);
    }

    if (fstream.good()) {
      header = header_t();
      std::ifstream ifstream;
      ifstream.open(filename, std::fstream::in);

      if (ifstream.bad()) {
        return static_cast<int>(EStatus::EWRITEFAIL);
      }

      std::stringstream fileContent;
      fileContent << ifstream.rdbuf();
      int err = write_file_header(filename, fileContent.str().size());

      if (err) {
        return err;
      }

      err = write_data(fileContent.str(), fileContent.str().size());
      if (err) {
        return err;
      }

      return static_cast<int>(EStatus::ESUCCESS);

    } else {
      return static_cast<int>(EStatus::EWRITEFAIL);
    }
  }

  int ArchiveDirectoryContent(const std::string &path) {
    if (!fstream.is_open()) {
      fstream.open(archive_name, std::fstream::out | std::fstream::binary |
                                     std::fstream::app);
    }

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

      int err = find(filename);

      if (err) {
        return err;
      }

      std::ofstream fileContent(filename,
                                std::fstream::out | std::fstream::binary);
      err = read_data(fileContent, header.size);
      if (err) {
        fs::remove(filename);
        return err;
      }

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
        const std::string filename = header.name.data();
        const std::string parentPath = fs::path(filename).parent_path();

        if (!parentPath.empty() && !fs::exists(parentPath)) {
          fs::create_directories(parentPath);
        }

        std::ofstream fileContent(filename,
                                  std::fstream::out | std::fstream::binary);
        const int err = read_data(fileContent, header.size);
        if (err) {
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
