/*-----------------------------------------------------------/
/ file_menu_SdFat: File Menu sorting utility for SdFat v0.90
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#ifndef _FILE_MENU_H_
#define _FILE_MENU_H_

#include <SdFat.h>

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

#define FF_LFN_BUF		255 // Length of Long File Name of exFAT
#define MAX_DEPTH_DIR	8	// Directory's Maximum Depth supported

#define FFL_SZ 8			// number of charactors for fast index of file name
#define TGT_DIRS    (1<<0)
#define TGT_FILES   (1<<1)

/* File function return code (FRESULT) */

typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;

/* Type of path name strings on FatFs API */

FRESULT file_menu_open_root_dir();
uint8_t file_menu_get_fatType();
FRESULT file_menu_ch_dir(uint16_t order);
void file_menu_close_dir(void);
uint16_t file_menu_get_num(void);
uint16_t file_menu_get_dir_num(void);
int file_menu_match_ext(uint16_t order, const char *ext, size_t ext_size); // case-insensitive, ext: "mp3", "wav" (ext does not include ".")
uint16_t file_menu_get_ext_num(const char *ext, size_t ext_size); // case-insensitive, ext: "mp3", "wav" (ext does not include ".")

// Template overloads for string literals (auto-derive ext_size)
template <size_t N>
inline int file_menu_match_ext(uint16_t order, const char (&ext)[N]) {
    return file_menu_match_ext(order, ext, N - 1);
}
template <size_t N>
inline uint16_t file_menu_get_ext_num(const char (&ext)[N]) {
    return file_menu_get_ext_num(ext, N - 1);
}
// Supported audio file extensions (lowercase, uppercase covered)
static constexpr const char* audio_ext[] = {"mp3", "wav", "m4a", "flac"};
static constexpr size_t audio_ext_count = sizeof(audio_ext) / sizeof(audio_ext[0]);

int file_menu_is_audio(uint16_t order);
uint16_t file_menu_get_audio_num(void);
void file_menu_full_sort(void);
void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1);
FRESULT file_menu_get_fname(uint16_t order, char *str, size_t size);
FRESULT file_menu_get_fname_UTF16(uint16_t order, char16_t *str, size_t size);
FRESULT file_menu_get_obj(uint16_t order, MutexFsBaseFile *fp);
int file_menu_is_dir(uint16_t order);
void file_menu_idle(void);

#endif
