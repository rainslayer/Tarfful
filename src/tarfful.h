#ifndef Tarfful
#define Tarfful

#include <array>
#include <cstring>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <linux/kdev_t.h>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <unordered_map>

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

constexpr size_t BUFF_SIZE = 8192;
constexpr size_t headerSize = sizeof(header_t);

size_t round_up(const size_t pos) {
  constexpr auto incr = headerSize;
  return pos + (incr - pos % incr) % incr;
}

size_t checksum(const header_t &header) {
  std::array<Byte, headerSize> headerPtr = {};
  std::memcpy(headerPtr.data(), &header, headerSize);

  auto res = 256;
  for (size_t i = 0, offset = offsetof(header_t, checksum); i < offset; ++i) {
    res += headerPtr[i];
  }

  for (size_t i = offsetof(header_t, type), size = 512; i < size; ++i) {
    res += headerPtr[i];
  }

  return res;
}

class Tar {
private:
  std::fstream fstream;
  const std::string archive_name;
  std::unordered_map<uid_t, char*> users;
  std::unordered_map<uid_t, char*> groups;

private:
    char* getFileOwnerName(const uid_t st_uid) {

      if (!users[st_uid]) {
        const auto ownername = getpwuid(st_uid)->pw_name;
        users[st_uid] = ownername;
        return ownername;
      } else {
        return users[st_uid];
      }
    }

    char* getFileOwnerGroup(const uid_t st_gid) {
      if (!groups[st_gid]) {
        const auto ownergroup = getgrgid(st_gid)->gr_name;
        groups[st_gid] = ownergroup;
        return ownergroup;
      } else {
        return groups[st_gid];
      }
    }

  void write_file_header(const std::string &name) {
    header_t header;

    {
      const std::string filename = fs::path(name).filename();
      strncpy(header.name.data(), filename.data(), header.name.size());
    }

    {
      const std::string parent_path = fs::path(name).parent_path();
      if (parent_path[0] == '/') {
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

      sprintf(&header.mode[0], "%o", info.st_mode);
      sprintf(&header.owner[0], "%o", info.st_uid);
      sprintf(&header.group[0], "%o", info.st_gid);
      sprintf(&header.mtime[0], "%zo", info.st_mtim.tv_sec);
      sprintf(header.device_major.data(), "%lo", MAJOR(info.st_dev));
      sprintf(header.device_minor.data(), "%lo", MINOR(info.st_dev));
      strncpy(header.owner_name.data(), getFileOwnerName(info.st_uid), header.owner_name.size());
      strncpy(header.group_name.data(), getFileOwnerGroup(info.st_gid), header.owner_name.size());

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
      ifstream.read(&chunk[0], BUFF_SIZE);

      auto contentSize = ifstream.gcount();
      fstream.write(chunk.data(), contentSize);
    }

    const auto currentPos = fstream.tellg();
    write_null_bytes(round_up(currentPos) - currentPos);
  }

  void write_null_bytes(const size_t n) {
    const std::string nullstring(n, '\0');
    fstream.write(nullstring.data(), n);
  }

  int raw_to_header(const header_t &header) {
    if (header.checksum[0] == '\0') {
      return static_cast<int>(Tarfful::EStatus::ENULLRECORD);
    }
    const unsigned chksum1 = checksum(header);
    const unsigned chksum2 = strtoul(header.checksum.data(), nullptr, 8);

    if (chksum1 != chksum2) {
      return static_cast<int>(Tarfful::EStatus::EBADCHKSUM);
    }

    return static_cast<int>(Tarfful::EStatus::ESUCCESS);
  }

  int file_write(const header_t &header) {
    std::array<Byte, headerSize> dest = {};
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

  int file_read(std::ofstream &outputFile, const size_t size) {
    std::string fileContent(size, '\0');
    fstream.read(&fileContent[0], size);
    outputFile.write(fileContent.data(), size);

    if (fstream.bad() || outputFile.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int file_read(header_t &rh, const size_t &size) {
    std::array<Byte, headerSize> dest = {};
    fstream.read(dest.data(), size);
    std::memcpy(&rh, dest.data(), headerSize);

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  void file_seek(const size_t offset) {
    fstream.seekg(offset, std::ios_base::cur);
  }

  int read_header(header_t &header) {
    int err = tread(header);
    err = raw_to_header(header);
    return err;
  }

  void next() {
    const auto currentPos = fstream.tellg();
    const auto n = round_up(currentPos) - currentPos;
    file_seek(n);
  }

  int find(const std::string &filepath, header_t &header) {
    fstream.clear();
    fstream.seekg(0);

    while (read_header(header) == static_cast<int>(EStatus::ESUCCESS)) {
      std::string currentFile(header.filename_prefix.data());
      currentFile += '/';
      currentFile += header.name.data();

      if (filepath == currentFile) {
        return static_cast<int>(EStatus::ESUCCESS);
      }

      file_seek(octalToDecimal(std::stoi(header.size.data())));
      next();
    }

    return static_cast<int>(EStatus::ENOTFOUND);
  }

  size_t octalToDecimal(const size_t n) {
    size_t decimal = 0;
    size_t base = 1;
    size_t temp = n;

    while (temp) {
      size_t last_digit = temp % 10;
      temp = temp / 10;
      decimal += last_digit * base;
      base = base * 8;
    }

    return decimal;
  }

  int read_data(std::ofstream &outputFile, const header_t &header) {
    const int size = octalToDecimal(std::stoi(header.size.data()));
    tread(outputFile, size);
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int tread(std::ofstream &outputFile, const size_t &size) {
    const int err = file_read(outputFile, size);
    return err;
  }

  int tread(header_t &rh) {
    const int err = file_read(rh, 512);
    return err;
  }

  void ArchiveFile(const std::string &filename) {
    write_file_header(filename);
    write_data(filename);
  }

  void ExtractFile(const header_t &header) {
      const std::string parentDirectory = header.filename_prefix.data();
      const std::string filename = header.name.data();
      const std::string filepath = parentDirectory + "/" + filename;

      if (!fs::exists(parentDirectory)) {
          fs::create_directories(parentDirectory);
      chown(parentDirectory.data(), octalToDecimal(std::stoi(header.owner.data())), octalToDecimal(std::stoi(header.group.data())));

      }

      {
        std::ofstream fileStream(filepath,
                               std::fstream::out | std::fstream::binary);
        read_data(fileStream, header);
      }

      chmod(filepath.data(), octalToDecimal(std::stoi(header.mode.data())));
      time_t mtime = octalToDecimal(std::stoull(header.mtime.data()));
      struct utimbuf timebuf = {};
      timebuf.actime = mtime;
      timebuf.modtime = mtime;
      utime(filepath.data(), &timebuf);
      chown(filepath.data(), octalToDecimal(std::stoi(header.owner.data())), octalToDecimal(std::stoi(header.group.data())));
  }

public:
  explicit Tar(const std::string &archive) : archive_name(archive) {
    fstream.open(archive_name, std::fstream::in | std::fstream::out |
                                   std::fstream::binary | std::fstream::app);
  }

  void Archive(const fs::path &path) {
    if (fs::is_directory(path)) {
      for (const auto &dirEntry : fs::recursive_directory_iterator(path)) {
        if (fs::is_directory(dirEntry)) {
          continue;
        } else {
          ArchiveFile(dirEntry.path());
        }
      }
    } else {
      ArchiveFile(path);
    }
  }

  void Extract(const std::string &filepath) {
    header_t header;

    if (find(filepath, header) == 0) {
      ExtractFile(header);
    }
  }

  void ExtractAll() {
    header_t header;

    while (read_header(header) != static_cast<int>(Tarfful::EStatus::ENULLRECORD)) {
      const std::string filename(header.name.data());

      ExtractFile(header);
      next();
    }
  }
};
} // namespace Tarfful

#endif
