#include "tarfful.h"

Tarfful::Tar::Tar(const std::string& archive) : archive_name(archive) {
  fstream.open(archive_name, std::fstream::in | std::fstream::out |
                                 std::fstream::binary | std::fstream::app);
}

char* Tarfful::Tar::GetFileOwnerName(const uid_t st_uid) {
  if (!users[st_uid]) {
    const auto ownername = getpwuid(st_uid)->pw_name;
    users[st_uid] = ownername;
    return ownername;
  } else {
    return users[st_uid];
  }
}

char* Tarfful::Tar::GetFileOwnerGroup(const uid_t st_gid) {
  if (!groups[st_gid]) {
    const auto ownergroup = getgrgid(st_gid)->gr_name;
    groups[st_gid] = ownergroup;
    return ownergroup;
  } else {
    return groups[st_gid];
  }
}

void Tarfful::Tar::WriteFileHeader(const std::string& name) {
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
    strncpy(header.owner_name.data(), GetFileOwnerName(info.st_uid),
            header.owner_name.size());
    strncpy(header.group_name.data(), GetFileOwnerGroup(info.st_gid),
            header.owner_name.size());

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
  sprintf(&header.checksum[0], "%06zo", GenerateChecksum(header));
  header.checksum[7] = ' ';

  WriteFile(header);
}

void Tarfful::Tar::WriteFileContent(const std::string& filename) {
  std::ifstream ifstream;
  ifstream.open(filename, std::fstream::in | std::fstream::binary);

  while (!ifstream.eof()) {
    std::string chunk(BUFF_SIZE, 0);
    ifstream.read(&chunk[0], BUFF_SIZE);

    auto contentSize = ifstream.gcount();
    fstream.write(chunk.data(), contentSize);
  }

  const auto currentPos = fstream.tellg();
  WriteNullBytes(RoundUp(currentPos) - currentPos);
}

void Tarfful::Tar::WriteNullBytes(const size_t n) {
  const std::string nullstring(n, '\0');
  fstream.write(nullstring.data(), n);
}

int Tarfful::Tar::WriteFile(const header_t& header) {
  std::array<Byte, headerSize> dest = {};
  std::memcpy(dest.data(), &header, dest.size());
  fstream.write(dest.data(), dest.size());

  if (fstream.bad()) {
    return static_cast<int>(EStatus::EWRITEFAIL);
  } else {
    return static_cast<int>(EStatus::ESUCCESS);
  }
}

int Tarfful::Tar::ReadFile(std::ofstream& outputFile, const size_t size) {
  std::string fileContent(size, '\0');
  fstream.read(&fileContent[0], size);
  outputFile.write(fileContent.data(), size);

  if (fstream.bad() || outputFile.bad()) {
    return static_cast<int>(EStatus::EREADFAIL);
  }
  return static_cast<int>(EStatus::ESUCCESS);
}

int Tarfful::Tar::ReadFile(header_t& rh, const size_t size) {
  std::array<Byte, headerSize> dest = {};
  fstream.read(dest.data(), size);
  std::memcpy(&rh, dest.data(), headerSize);

  if (fstream.bad()) {
    return static_cast<int>(EStatus::EREADFAIL);
  }
  return static_cast<int>(EStatus::ESUCCESS);
}

void Tarfful::Tar::SeekFile(const size_t offset) {
  fstream.seekg(offset, std::ios_base::cur);
}

int Tarfful::Tar::ReadHeader(header_t& header) {
  ReadFile(header, 512);
  return VerifyChecksum(header);
}

void Tarfful::Tar::Next() {
  const auto currentPos = fstream.tellg();
  SeekFile(RoundUp(currentPos) - currentPos);
}

int Tarfful::Tar::Find(const std::string& filepath, header_t& header) {
  fstream.clear();
  fstream.seekg(0);

  while (ReadHeader(header) == static_cast<int>(EStatus::ESUCCESS)) {
    std::string currentFile(header.filename_prefix.data());
    currentFile += '/';
    currentFile += header.name.data();

    if (filepath == currentFile) {
      return static_cast<int>(EStatus::ESUCCESS);
    }

    SeekFile(OctalToDecimal(std::stoi(header.size.data())));
    Next();
  }

  return static_cast<int>(EStatus::ENOTFOUND);
}

int Tarfful::Tar::ReadData(std::ofstream& outputFile, const header_t& header) {
  const int size = OctalToDecimal(std::stoi(header.size.data()));
  return ReadFile(outputFile, size);
}

void Tarfful::Tar::ArchiveFile(const std::string& filename) {
  WriteFileHeader(filename);
  WriteFileContent(filename);
}

void Tarfful::Tar::ExtractFile(const header_t& header) {
  const std::string parentDirectory = header.filename_prefix.data();
  const std::string filename = header.name.data();
  const std::string filepath = parentDirectory + "/" + filename;

  if (!fs::exists(parentDirectory)) {
    fs::create_directories(parentDirectory);
    chown(parentDirectory.data(),
          OctalToDecimal(std::stoi(header.owner.data())),
          OctalToDecimal(std::stoi(header.group.data())));
  }

  {
    std::ofstream fileStream(filepath,
                             std::fstream::out | std::fstream::binary);
    ReadData(fileStream, header);
  }

  chmod(filepath.data(), OctalToDecimal(std::stoi(header.mode.data())));
  time_t mtime = OctalToDecimal(std::stoull(header.mtime.data()));
  struct utimbuf timebuf = {};
  timebuf.actime = mtime;
  timebuf.modtime = mtime;
  utime(filepath.data(), &timebuf);
  chown(filepath.data(), OctalToDecimal(std::stoi(header.owner.data())),
        OctalToDecimal(std::stoi(header.group.data())));
}

void Tarfful::Tar::Archive(const fs::path& path) {
  if (fs::is_directory(path)) {
    for (const auto& dirEntry : fs::recursive_directory_iterator(path)) {
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

void Tarfful::Tar::Extract(const std::string& filepath) {
  header_t header;

  if (Find(filepath, header) == 0) {
    ExtractFile(header);
  }
}

void Tarfful::Tar::ExtractAll() {
  fstream.clear();
  fstream.seekg(0);
  header_t header;

  while (ReadHeader(header) == static_cast<int>(EStatus::ESUCCESS)) {
    const std::string filename(header.name.data());

    ExtractFile(header);
    Next();
  }
}