#include "MutexFsBaseFile.h"

Threads::Mutex mylock;

//------------------------------------------------------------------------------
MutexFsBaseFile::MutexFsBaseFile() : FsBaseFile() {}
//------------------------------------------------------------------------------
MutexFsBaseFile::~MutexFsBaseFile() {
  close();
}
//------------------------------------------------------------------------------
MutexFsBaseFile::MutexFsBaseFile(const MutexFsBaseFile& from) : FsBaseFile(from) {}
//------------------------------------------------------------------------------
MutexFsBaseFile& MutexFsBaseFile::operator=(const MutexFsBaseFile& from) {
  if (this == &from) return *this;
  close();
  Threads::Scope scope(mylock);
  if (from.m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    *m_fFile = *from.m_fFile;
  } else if (from.m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    *m_xFile = *from.m_xFile;
  }
  return *this;
}
//------------------------------------------------------------------------------
MutexFsBaseFile::operator bool() {
  return isOpen();
}
//------------------------------------------------------------------------------
int MutexFsBaseFile::available() {
  Threads::Scope scope(mylock);
  return FsBaseFile::available();
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::clearWriteError() {
  Threads::Scope scope(mylock);
  FsBaseFile::clearWriteError();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::close() {
  Threads::Scope scope(mylock);
  return FsBaseFile::close();
}
//------------------------------------------------------------------------------
uint64_t MutexFsBaseFile::curPosition() {
  Threads::Scope scope(mylock);
  return FsBaseFile::curPosition();
}
//------------------------------------------------------------------------------
uint32_t MutexFsBaseFile::dirIndex() {
  Threads::Scope scope(mylock);
  return FsBaseFile::dirIndex();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::exists(const char* path) {
  Threads::Scope scope(mylock);
  return FsBaseFile::exists(path);
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::fgetpos(fspos_t* pos) {
  Threads::Scope scope(mylock);
  FsBaseFile::fgetpos(pos);
}
//------------------------------------------------------------------------------
int MutexFsBaseFile::fgets(char* str, int num, char* delim) {
  Threads::Scope scope(mylock);
  return FsBaseFile::fgets(str, num, delim);
}
//------------------------------------------------------------------------------
uint64_t MutexFsBaseFile::fileSize() {
  Threads::Scope scope(mylock);
  return FsBaseFile::fileSize();
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::flush() {
  Threads::Scope scope(mylock);
  FsBaseFile::flush();
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::fsetpos(const fspos_t* pos) {
  Threads::Scope scope(mylock);
  FsBaseFile::fsetpos(pos);
}
//------------------------------------------------------------------------------
uint8_t MutexFsBaseFile::getError() {
  Threads::Scope scope(mylock);
  return FsBaseFile::getError();
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::getName(char* name, size_t len) {
  Threads::Scope scope(mylock);
  return FsBaseFile::getName(name, len);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::getUTF16Name(char16_t* name, size_t len) {
  Threads::Scope scope(mylock);
  return FsBaseFile::getUTF16Name(name, len);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::getWriteError() {
  Threads::Scope scope(mylock);
  return FsBaseFile::getWriteError();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::isDir() {
  Threads::Scope scope(mylock);
  return FsBaseFile::isDir();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::isDirectory() {
  Threads::Scope scope(mylock);
  return FsBaseFile::isDirectory();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::isHidden() {
  Threads::Scope scope(mylock);
  return FsBaseFile::isHidden();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::isOpen() {
  Threads::Scope scope(mylock);
  return FsBaseFile::isOpen();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::isSubDir() {
  Threads::Scope scope(mylock);
  return FsBaseFile::isSubDir();
}
//------------------------------------------------------------------------------
#if ENABLE_ARDUINO_SERIAL
bool MutexFsBaseFile::ls(uint8_t flags) {
  Threads::Scope scope(mylock);
  return FsBaseFile::ls(flags);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::ls() {
  Threads::Scope scope(mylock);
  return FsBaseFile::ls();
}
#endif  // ENABLE_ARDUINO_SERIAL
//------------------------------------------------------------------------------
bool MutexFsBaseFile::ls(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::ls(pr);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::ls(print_t* pr, uint8_t flags) {
  Threads::Scope scope(mylock);
  return FsBaseFile::ls(pr, flags);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::mkdir(MutexFsBaseFile* dir, const char* path, bool pFlag) {
  close();
  Threads::Scope scope(mylock);
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->mkdir(dir->m_fFile, path, pFlag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->mkdir(dir->m_xFile, path, pFlag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
const char* MutexFsBaseFile::name() const {
  Threads::Scope scope(mylock);
  return FsBaseFile::name();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::open(MutexFsBaseFile* dir, const char* path, oflag_t oflag) {
  close();
  Threads::Scope scope(mylock);
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->open(dir->m_fFile, path, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->open(dir->m_xFile, path, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
/*
bool MutexFsBaseFile::open(FsVolume* vol, const char* path, oflag_t oflag) {
  if (!vol) {
    return false;
  }
  close();
  Threads::Scope scope(mylock);
  if (vol->m_fVol) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile && m_fFile->open(vol->m_fVol, path, oflag)) {
      return true;
    }
    m_fFile = nullptr;
    return false;
  } else if (vol->m_xVol) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile && m_xFile->open(vol->m_xVol, path, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
*/
//------------------------------------------------------------------------------
bool MutexFsBaseFile::open(const char* path, oflag_t oflag) {
  Threads::Scope scope(mylock);
  return FsBaseFile::open(path, oflag);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::open(MutexFsBaseFile* dir, uint32_t index, oflag_t oflag) {
  close();
  Threads::Scope scope(mylock);
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->open(dir->m_fFile, index, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->open(dir->m_xFile, index, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::openNext(MutexFsBaseFile* dir, oflag_t oflag) {
  close();
  Threads::Scope scope(mylock);
  if (dir->m_fFile) {
    m_fFile = new (m_fileMem) FatFile;
    if (m_fFile->openNext(dir->m_fFile, oflag)) {
      return true;
    }
    m_fFile = nullptr;
  } else if (dir->m_xFile) {
    m_xFile = new (m_fileMem) ExFatFile;
    if (m_xFile->openNext(dir->m_xFile, oflag)) {
      return true;
    }
    m_xFile = nullptr;
  }
  return false;
}
//------------------------------------------------------------------------------
int MutexFsBaseFile::peek() {
  Threads::Scope scope(mylock);
  return FsBaseFile::peek();
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printAccessDateTime(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printAccessDateTime(pr);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printCreateDateTime(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printCreateDateTime(pr);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printModifyDateTime(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printModifyDateTime(pr);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printName(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printName(pr);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printFileSize(print_t* pr) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printFileSize(pr);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::preAllocate(uint64_t length) {
  Threads::Scope scope(mylock);
  return FsBaseFile::preAllocate(length);
}
//------------------------------------------------------------------------------
uint64_t MutexFsBaseFile::position() {
  return curPosition();
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printField(double value, char term, uint8_t prec) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printField(value, term, prec);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::printField(float value, char term, uint8_t prec) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printField(value, term, prec);
}
//------------------------------------------------------------------------------
template<typename Type>
size_t MutexFsBaseFile::printField(Type value, char term) {
  Threads::Scope scope(mylock);
  return FsBaseFile::printField(value, term);
}
//------------------------------------------------------------------------------
int MutexFsBaseFile::read() {
  Threads::Scope scope(mylock);
  return FsBaseFile::read();
}
//------------------------------------------------------------------------------
int MutexFsBaseFile::read(void* buf, size_t count) {
  Threads::Scope scope(mylock);
  return FsBaseFile::read(buf, count);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::remove() {
  Threads::Scope scope(mylock);
  return FsBaseFile::remove();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::remove(const char* path) {
  Threads::Scope scope(mylock);
  return FsBaseFile::remove(path);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::rename(const char* newPath) {
  Threads::Scope scope(mylock);
  return FsBaseFile::rename(newPath);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::rename(MutexFsBaseFile* dirFile, const char* newPath) {
  Threads::Scope scope(mylock);
  return FsBaseFile::rename(dirFile, newPath);
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::rewind() {
  Threads::Scope scope(mylock);
  FsBaseFile::rewind();
}
//------------------------------------------------------------------------------
void MutexFsBaseFile::rewindDirectory() {
  if (isDir()) rewind();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::rmdir() {
  Threads::Scope scope(mylock);
  return FsBaseFile::rmdir();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::seek(uint64_t pos) {
  return seekSet(pos);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::seekCur(int64_t offset) {
  return seekSet(curPosition() + offset);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::seekEnd(int64_t offset) {
  return seekSet(fileSize() + offset);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::seekSet(uint64_t pos) {
  Threads::Scope scope(mylock);
  return FsBaseFile::seekSet(pos);
}
//------------------------------------------------------------------------------
uint64_t MutexFsBaseFile::size() {
  return fileSize();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::sync() {
  Threads::Scope scope(mylock);
  return FsBaseFile::sync();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::timestamp(uint8_t flags, uint16_t year, uint8_t month, uint8_t day,
                uint8_t hour, uint8_t minute, uint8_t second) {
  Threads::Scope scope(mylock);
  return FsBaseFile::timestamp(flags, year, month, day, hour, minute, second);
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::truncate() {
  Threads::Scope scope(mylock);
  return FsBaseFile::truncate();
}
//------------------------------------------------------------------------------
bool MutexFsBaseFile::truncate(uint64_t length) {
  Threads::Scope scope(mylock);
  return FsBaseFile::truncate(length);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::write(uint8_t b) {
  Threads::Scope scope(mylock);
  return FsBaseFile::write(b);
}
//------------------------------------------------------------------------------
size_t MutexFsBaseFile::write(const void* buf, size_t count) {
  Threads::Scope scope(mylock);
  return FsBaseFile::write(buf, count);
}

