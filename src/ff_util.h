#ifndef _FF_UTIL_H_
#define _FF_UTIL_H_

#include "SdFat.h"

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
FRESULT file_menu_ch_dir(uint16_t order);
void file_menu_close_dir(void);
uint16_t file_menu_get_size(void);
void file_menu_full_sort(void);
void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1);
FRESULT file_menu_get_fname(uint16_t order, char *str, uint16_t size);
FRESULT file_menu_get_obj(uint16_t order, FsBaseFile *file);
int file_menu_is_dir(uint16_t order);
void file_menu_idle(void);

#endif
