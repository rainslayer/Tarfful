#pragma once

#include <array>
#include <cstring>
#include <fstream>
#include <grp.h>
#include <linux/kdev_t.h>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <utime.h>

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

constexpr size_t BUFF_SIZE = 8192 * 8192;
constexpr size_t headerSize = sizeof(header_t);

size_t RoundUp(const size_t pos) {
  return pos + (headerSize - pos % headerSize) % headerSize;
}

size_t GenerateChecksum(const header_t& header) {
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

size_t OctalToDecimal(const size_t n) {
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

int VerifyChecksum(const header_t& header) {
  if (header.checksum[0] == '\0') {
    return static_cast<int>(EStatus::ENULLRECORD);
  }

  const auto chksum1 = GenerateChecksum(header);
  const auto chksum2 = OctalToDecimal(std::stoi(header.checksum.data()));

  if (chksum1 != chksum2) {
    return static_cast<int>(EStatus::EBADCHKSUM);
  }

  return static_cast<int>(EStatus::ESUCCESS);
}

class Tar {
private:
  std::fstream fstream;
  const std::string archive_name;
  std::unordered_map<uid_t, char*> users;
  std::unordered_map<uid_t, char*> groups;

private:
  char* GetFileOwnerName(const uid_t st_uid);

  char* GetFileOwnerGroup(const uid_t st_gid);

  void WriteFileHeader(const std::string& name);

  void WriteFileContent(const std::string& filename);

  void WriteNullBytes(const size_t n);

  int WriteFile(const header_t& header);

  int ReadFile(std::ofstream& outputFile, const size_t size);

  int ReadFile(header_t& rh, const size_t size);

  void SeekFile(const size_t offset);

  int ReadHeader(header_t& header);

  void Next();

  int Find(const std::string& filepath, header_t& header);

  int ReadData(std::ofstream& outputFile, const header_t& header);

  void ArchiveFile(const std::string& filename);

  void ExtractFile(const header_t& header);

public:
  explicit Tar(const std::string& archive);

  void Archive(const fs::path& path);

  void Extract(const std::string& filepath);

  void ExtractAll();
};
} // namespace Tarfful