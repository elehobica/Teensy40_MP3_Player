/*-----------------------------------------------------------/
/ MutexFsBaseFile: FsBaseFile Wrapper with Mutex
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#ifndef MutexFsBaseFile_h
#define MutexFsBaseFile_h

#include "FsFile.h"
#include <TeensyThreads.h>

extern Threads::Mutex mylock;

/**
 * \class MutexFsBaseFile
 * \brief MutexFsBaseFile class.
 */
class MutexFsBaseFile : public FsBaseFile {
 public:
  MutexFsBaseFile();

  ~MutexFsBaseFile();
  /** Copy constructor.
   *
   * \param[in] from Object used to initialize this instance.
   */
  MutexFsBaseFile(const MutexFsBaseFile& from);
  /** Copy assignment operator
   * \param[in] from Object used to initialize this instance.
   * \return assigned object.
   */
  MutexFsBaseFile& operator=(const MutexFsBaseFile& from);
  /** The parenthesis operator.
    *
    * \return true if a file is open.
    */
  operator bool();
  /** \return number of bytes available from the current position to EOF
   *   or INT_MAX if more than INT_MAX bytes are available.
   */
  int available();

  /** Set writeError to zero */
  void clearWriteError();
  /** Close a file and force cached data and directory information
   *  to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool close();
  /** \return The current position for a file or directory. */
  uint64_t curPosition();
  /** \return Directory entry index. */
  uint32_t dirIndex();
  /** Test for the existence of a file in a directory
   *
   * \param[in] path Path of the file to be tested for.
   *
   * The calling instance must be an open directory file.
   *
   * dirFile.exists("TOFIND.TXT") searches for "TOFIND.TXT" in  the directory
   * dirFile.
   *
   * \return true if the file exists else false.
   */
  bool exists(const char* path);
  /** get position for streams
   * \param[out] pos struct to receive position
   */
  void fgetpos(fspos_t* pos);
 /**
   * Get a string from a file.
   *
   * fgets() reads bytes from a file into the array pointed to by \a str, until
   * \a num - 1 bytes are read, or a delimiter is read and transferred to \a str,
   * or end-of-file is encountered. The string is then terminated
   * with a null byte.
   *
   * fgets() deletes CR, '\\r', from the string.  This insures only a '\\n'
   * terminates the string for Windows text files which use CRLF for newline.
   *
   * \param[out] str Pointer to the array where the string is stored.
   * \param[in] num Maximum number of characters to be read
   * (including the final null byte). Usually the length
   * of the array \a str is used.
   * \param[in] delim Optional set of delimiters. The default is "\n".
   *
   * \return For success fgets() returns the length of the string in \a str.
   * If no data is read, fgets() returns zero for EOF or -1 if an error occurred.
   */
  int fgets(char* str, int num, char* delim = nullptr);
  /** \return The total number of bytes in a file. */
  uint64_t fileSize();
  /** Ensure that any bytes written to the file are saved to the SD card. */
  void flush();
  /** set position for streams
   * \param[in] pos struct with value for new position
   */
  void fsetpos(const fspos_t* pos);
  /** \return All error bits. */
  uint8_t getError();
  /**
   * Get a file's name followed by a zero byte.
   *
   * \param[out] name An array of characters for the file's name.
   * \param[in] len The size of the array in bytes. The array
   *             must be at least 13 bytes long.  The file's name will be
   *             truncated if the file's name is too long.
   * \return The length of the returned string.
   */
  size_t getName(char* name, size_t len);
  size_t getUTF16Name(char16_t* name, size_t len);

  /** \return value of writeError */
  bool getWriteError();
  /** \return True if this is a directory else false. */
  bool isDir();
  /** This function reports if the current file is a directory or not.
   * \return true if the file is a directory.
   */
  bool isDirectory();
  /** \return True if this is a hidden file else false. */
  bool isHidden();
  /** \return True if this is an open file/directory else false. */
  bool isOpen();
  /** \return True if this is a subdirectory file else false. */
  bool isSubDir();
#if ENABLE_ARDUINO_SERIAL
  /** List directory contents.
   *
   * \param[in] flags The inclusive OR of
   *
   * LS_DATE - %Print file modification date
   *
   * LS_SIZE - %Print file size.
   *
   * LS_R - Recursive list of subdirectories.
   */
  bool ls(uint8_t flags);
  /** List directory contents. */
  bool ls();
#endif  // ENABLE_ARDUINO_SERIAL
  /** List directory contents.
   *
   * \param[in] pr Print object.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr);
  /** List directory contents.
   *
   * \param[in] pr Print object.
   * \param[in] flags The inclusive OR of
   *
   * LS_DATE - %Print file modification date
   *
   * LS_SIZE - %Print file size.
   *
   * LS_R - Recursive list of subdirectories.
   *
   * \return true for success or false for failure.
   */
  bool ls(print_t* pr, uint8_t flags);
  /** Make a new directory.
   *
   * \param[in] dir An open FatFile instance for the directory that will
   *                   contain the new directory.
   *
   * \param[in] path A path with a valid 8.3 DOS name for the new directory.
   *
   * \param[in] pFlag Create missing parent directories if true.
   *
   * \return true for success or false for failure.
   */
  bool mkdir(MutexFsBaseFile* dir, const char* path, bool pFlag = true);
  /** No longer implemented due to Long File Names.
   *
   * Use getName(char* name, size_t size).
   * \return a pointer to replacement suggestion.
   */
  const char* name() const;
  /** Open a file or directory by name.
   *
   * \param[in] dir An open file instance for the directory containing
   *                    the file to be opened.
   *
   * \param[in] path A path with a valid 8.3 DOS name for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of flags from the following list
   *
   * O_RDONLY - Open for reading only..
   *
   * O_READ - Same as O_RDONLY.
   *
   * O_WRONLY - Open for writing only.
   *
   * O_WRITE - Same as O_WRONLY.
   *
   * O_RDWR - Open for reading and writing.
   *
   * O_APPEND - If set, the file offset shall be set to the end of the
   * file prior to each write.
   *
   * O_AT_END - Set the initial position at the end of the file.
   *
   * O_CREAT - If the file exists, this flag has no effect except as noted
   * under O_EXCL below. Otherwise, the file shall be created
   *
   * O_EXCL - If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
   *
   * O_TRUNC - If the file exists and is a regular file, and the file is
   * successfully opened and is not read only, its length shall be truncated to 0.
   *
   * WARNING: A given file must not be opened by more than one file object
   * or file corruption may occur.
   *
   * \note Directory files must be opened read only.  Write and truncation is
   * not allowed for directory files.
   *
   * \return true for success or false for failure.
   */
  bool open(MutexFsBaseFile* dir, const char* path, oflag_t oflag = O_RDONLY);
  /** Open a file by index.
   *
   * \param[in] dir An open FsFile instance for the directory.
   *
   * \param[in] index The \a index of the directory entry for the file to be
   * opened.  The value for \a index is (directory file position)/32.
   *
   * \param[in] oflag bitwise-inclusive OR of open flags.
   *            See see FsFile::open(FsFile*, const char*, uint8_t).
   *
   * See open() by path for definition of flags.
   * \return true for success or false for failure.
   */
  bool open(MutexFsBaseFile* dir, uint32_t index, oflag_t oflag);
  /** Open a file or directory by name.
   *
   * \param[in] vol Volume where the file is located.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of open flags.
   *
   * \return true for success or false for failure.
   */
  bool open(FsVolume* vol, const char* path, oflag_t oflag);
  /** Open a file or directory by name.
   *
   * \param[in] path A path for a file to be opened.
   *
   * \param[in] oflag Values for \a oflag are constructed by a
   *                  bitwise-inclusive OR of open flags.
   *
   * \return true for success or false for failure.
   */
  bool open(const char* path, oflag_t oflag = O_RDONLY);
  /** Opens the next file or folder in a directory.
   * \param[in] dir directory containing files.
   * \param[in] oflag open flags.
   * \return a file object.
   */
  bool openNext(MutexFsBaseFile* dir, oflag_t oflag = O_RDONLY);
  /** Return the next available byte without consuming it.
   *
   * \return The byte if no error and not at eof else -1;
   */
  int peek();
  /** Print a file's access date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printAccessDateTime(print_t* pr);
  /** Print a file's creation date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printCreateDateTime(print_t* pr);
  /** Print a file's modify date and time
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printModifyDateTime(print_t* pr);
  /** Print a file's name
   *
   * \param[in] pr Print stream for output.
   *
   * \return true for success or false for failure.
   */
  size_t printName(print_t* pr);
  /** Print a file's size.
   *
   * \param[in] pr Print stream for output.
   *
   * \return The number of characters printed is returned
   *         for success and zero is returned for failure.
   */
  size_t printFileSize(print_t* pr);
  /** Allocate contiguous clusters to an empty file.
   *
   * The file must be empty with no clusters allocated.
   *
   * The file will contain uninitialized data for FAT16/FAT32 files.
   * exFAT files will have zero validLength and dataLength will equal
   * the requested length.
   *
   * \param[in] length size of the file in bytes.
   * \return true for success or false for failure.
   */
  bool preAllocate(uint64_t length);
  /** \return the current file position. */
  uint64_t position();
   /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
   */
  size_t printField(double value, char term, uint8_t prec = 2);
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \param[in] prec Number of digits after decimal point.
   * \return The number of bytes written or -1 if an error occurs.
   */
  size_t printField(float value, char term, uint8_t prec = 2);
  /** Print a number followed by a field terminator.
   * \param[in] value The number to be printed.
   * \param[in] term The field terminator.  Use '\\n' for CR LF.
   * \return The number of bytes written or -1 if an error occurs.
   */
  template<typename Type>
  size_t printField(Type value, char term);
  /** Read the next byte from a file.
   *
   * \return For success return the next byte in the file as an int.
   * If an error occurs or end of file is reached return -1.
   */
  int read();
  /** Read data from a file starting at the current position.
   *
   * \param[out] buf Pointer to the location that will receive the data.
   *
   * \param[in] count Maximum number of bytes to read.
   *
   * \return For success read() returns the number of bytes read.
   * A value less than \a count, including zero, will be returned
   * if end of file is reached.
   * If an error occurs, read() returns -1.  Possible errors include
   * read() called before a file has been opened, corrupt file system
   * or an I/O error occurred.
   */
  int read(void* buf, size_t count);
  /** Remove a file.
   *
   * The directory entry and all data for the file are deleted.
   *
   * \note This function should not be used to delete the 8.3 version of a
   * file that has a long name. For example if a file has the long name
   * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
   *
   * \return true for success or false for failure.
   */
  bool remove();
   /** Remove a file.
   *
   * The directory entry and all data for the file are deleted.
   *
   * \param[in] path Path for the file to be removed.
   *
   * Example use: dirFile.remove(filenameToRemove);
   *
   * \note This function should not be used to delete the 8.3 version of a
   * file that has a long name. For example if a file has the long name
   * "New Text Document.txt" you should not delete the 8.3 name "NEWTEX~1.TXT".
   *
   * \return true for success or false for failure.
   */
  bool remove(const char* path);
  /** Rename a file or subdirectory.
   *
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(const char* newPath);
  /** Rename a file or subdirectory.
   *
   * \param[in] dirFile Directory for the new path.
   * \param[in] newPath New path name for the file/directory.
   *
   * \return true for success or false for failure.
   */
  bool rename(MutexFsBaseFile* dirFile, const char* newPath);
  /** Set the file's current position to zero. */
  void rewind();
  /** Rewind a file if it is a directory */
  void rewindDirectory();
  /** Remove a directory file.
   *
   * The directory file will be removed only if it is empty and is not the
   * root directory.  rmdir() follows DOS and Windows and ignores the
   * read-only attribute for the directory.
   *
   * \note This function should not be used to delete the 8.3 version of a
   * directory that has a long name. For example if a directory has the
   * long name "New folder" you should not delete the 8.3 name "NEWFOL~1".
   *
   * \return true for success or false for failure.
   */
  bool rmdir();
  /** Seek to a new position in the file, which must be between
   * 0 and the size of the file (inclusive).
   *
   * \param[in] pos the new file position.
   * \return true for success or false for failure.
   */
  bool seek(uint64_t pos);
  /** Set the files position to current position + \a pos. See seekSet().
   * \param[in] offset The new position in bytes from the current position.
   * \return true for success or false for failure.
   */
  bool seekCur(int64_t offset);
  /** Set the files position to end-of-file + \a offset. See seekSet().
   * Can't be used for directory files since file size is not defined.
   * \param[in] offset The new position in bytes from end-of-file.
   * \return true for success or false for failure.
   */
  bool seekEnd(int64_t offset = 0);
  /** Sets a file's position.
   *
   * \param[in] pos The new position in bytes from the beginning of the file.
   *
   * \return true for success or false for failure.
   */
  bool seekSet(uint64_t pos);
  /** \return the file's size. */
  uint64_t size();
  /** The sync() call causes all modified data and directory fields
   * to be written to the storage device.
   *
   * \return true for success or false for failure.
   */
  bool sync();
  /** Set a file's timestamps in its directory entry.
   *
   * \param[in] flags Values for \a flags are constructed by a bitwise-inclusive
   * OR of flags from the following list
   *
   * T_ACCESS - Set the file's last access date.
   *
   * T_CREATE - Set the file's creation date and time.
   *
   * T_WRITE - Set the file's last write/modification date and time.
   *
   * \param[in] year Valid range 1980 - 2107 inclusive.
   *
   * \param[in] month Valid range 1 - 12 inclusive.
   *
   * \param[in] day Valid range 1 - 31 inclusive.
   *
   * \param[in] hour Valid range 0 - 23 inclusive.
   *
   * \param[in] minute Valid range 0 - 59 inclusive.
   *
   * \param[in] second Valid range 0 - 59 inclusive
   *
   * \note It is possible to set an invalid date since there is no check for
   * the number of days in a month.
   *
   * \note
   * Modify and access timestamps may be overwritten if a date time callback
   * function has been set by dateTimeCallback().
   *
   * \return true for success or false for failure.
   */
  bool timestamp(uint8_t flags, uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour, uint8_t minute, uint8_t second);

  /** Truncate a file to the current position.
   *
   * \return true for success or false for failure.
   */
  bool truncate();
  /** Truncate a file to a specified length.
   * The current file position will be set to end of file.
   *
   * \param[in] length The desired length for the file.
   *
   * \return true for success or false for failure.
   */
  bool truncate(uint64_t length);
  /** Write a byte to a file. Required by the Arduino Print class.
   * \param[in] b the byte to be written.
   * Use getWriteError to check for errors.
   * \return 1 for success and 0 for failure.
   */
  size_t write(uint8_t b);
  /** Write data to an open file.
   *
   * \note Data is moved to the cache but may not be written to the
   * storage device until sync() is called.
   *
   * \param[in] buf Pointer to the location of the data to be written.
   *
   * \param[in] count Number of bytes to write.
   *
   * \return For success write() returns the number of bytes written, always
   * \a nbyte.  If an error occurs, write() returns -1.  Possible errors
   * include write() is called before a file has been opened, write is called
   * for a read-only file, device is full, a corrupt file system or an
   * I/O error.
   */
  size_t write(const void* buf, size_t count);

 private:
};

#if 0
/**
 * \class FsFile
 * \brief FsBaseFile file with Arduino Stream.
 */
class FsFile : public StreamFile<FsBaseFile, uint64_t> {
 public:
  /** Opens the next file or folder in a directory.
   *
   * \param[in] oflag open flags.
   * \return a FatStream object.
   */
  FsFile openNextFile(oflag_t oflag = O_RDONLY) {
    FsFile tmpFile;
    tmpFile.openNext(this, oflag);
    return tmpFile;
  }
};
#endif
#endif  // MutexFsBaseFile_h
