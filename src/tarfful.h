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
        std::cout << "Removing leading / from path" << ' ' << parent_path
                  << '\n';
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
    if (header.checksum[0] == '\0') {
      return static_cast<int>(Tarfful::EStatus::ENULLRECORD);
    }
    const unsigned chksum1 = checksum(header);
    const unsigned chksum2 = strtoul(header.checksum.data(), nullptr, 8);

    if (chksum1 != chksum2) {
      return static_cast<int>(Tarfful::EStatus::EBADCHKSUM);
    }

    // auto mode = strtoul(header.mode.data(), nullptr, 8);
    // header.owner = strtoul(header.owner.data(), nullptr, 8);
    // header.size = strtoul(header.size.data(), nullptr, 8);
    // header.mtime = strtoul(header.mtime.data(), nullptr, 8);
    // strncpy(header.name.data(), header.name.data(), header.name.size());
    // strncpy(header.linkname.data(), header.linkname.data(),
    //         header.linkname.size());

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

  int file_read(std::ofstream &outputFile, const int size) {
    std::string fileContent(size, '\0');
    fstream.read(fileContent.data(), size);
    outputFile.write(fileContent.data(), size);

    if (fstream.bad() || outputFile.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  int file_read(header_t &rh, const size_t &size) {
    std::array<Byte, sizeof(header_t)> dest;
    fstream.read(dest.data(), size);
    std::memcpy(&rh, dest.data(), sizeof(header_t));

    if (fstream.bad()) {
      return static_cast<int>(EStatus::EREADFAIL);
    }
    return static_cast<int>(EStatus::ESUCCESS);
  }

  void file_seek(const int offset) {
    fstream.seekg(offset, std::ios_base::cur);
  }

  int read_header(header_t &header) {
    int err = tread(header);
    err = raw_to_header(header);

    return err;
  }

  void next() {
    const auto currentPos = fstream.tellg();
    const int n = round_up(currentPos) - currentPos;
    file_seek(n);
  }

  int find(const std::string &filepath, header_t &header) {
    while (read_header(header) == static_cast<int>(EStatus::ESUCCESS)) {
      std::string currentFile(header.filename_prefix.data());
      currentFile += '/';
      currentFile += header.name.data();

      if (filepath.compare(currentFile) == 0) {
        return static_cast<int>(EStatus::ESUCCESS);
      }
      file_seek(octalToDecimal(std::stoi(header.size.data())));
      next();
    }

    return static_cast<int>(EStatus::ENOTFOUND);
  }

  int octalToDecimal(int n) {
    int decimal = 0;
    int base = 1;
    int temp = n;

    while (temp) {
      int last_digit = temp % 10;
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

public:
  explicit Tar(const std::string &archive) : archive_name(archive) {
    fstream.open(archive_name, std::fstream::in | std::fstream::out |
                                   std::fstream::binary | std::fstream::app);
  }

  void Archive(const std::string &path) {
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

  void ExtractFile(const header_t &header) {
    const std::string parentDirectory = header.filename_prefix.data();
    const std::string filename = header.name.data();
    const std::string filepath = parentDirectory + "/" + filename;

    if (!fs::exists(parentDirectory)) {
      fs::create_directories(parentDirectory);
    }

    {
      std::ofstream fileStream(filepath,
                               std::fstream::out | std::fstream::binary);
      read_data(fileStream, header);
    }
  }

  void ExtractAll() {
    header_t header;
    while (read_header(header) !=
           static_cast<int>(Tarfful::EStatus::ENULLRECORD)) {
      const std::string filename(header.name.data());

      ExtractFile(header);
      next();
    }
  }
};
} // namespace Tarfful

#endif
