#include "ff_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SD_FAT_TYPE == 2
static SdExFat sd;
static ExFile dir;
//static ExFile file;
#elif SD_FAT_TYPE == 3
static SdFs sd;
static FsFile dir;
//static FsFile file;
//static FsFile file_temp;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16))
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16))
#endif  // HAS_SDIO_CLASS

static char _name[256];
static char _str[256];
static char path_info[8][256];
static int path_depth = 0;
static int target = TGT_DIRS | TGT_FILES; // TGT_DIRS, TGT_FILES
static int16_t f_stat_cnt;
static uint16_t max_entry_cnt;
static uint16_t *entry_list;
static uint32_t *sorted_flg;
static char (*fast_fname_list)[FFL_SZ];
static uint32_t *is_file_flg; // 0: Dir, 1: File
static uint16_t last_order; // order number memo for last file_menu_get_fname() request

//==============================
// idx Internal Funcions
//   provided by readdir 'index'
//==============================

static void idx_entry_swap(uint32_t entry_number1, uint32_t entry_number2)
{
	uint16_t tmp_entry;
	tmp_entry = entry_list[entry_number1];
	entry_list[entry_number1] = entry_list[entry_number2];
	entry_list[entry_number2] = tmp_entry;
}

static int32_t my_strcmp(const char *str1 , const char *str2)
{
	int32_t result = 0;

	// Compare until strings do not match
	while (result == 0) {
		result = *str1 - *str2;
		if (*str1 == '\0' || *str2 == '\0') break;	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return result;
}

static int32_t my_strncmp(char *str1 , char *str2, int size)
{
	int32_t result = 0;
	int32_t len = 0;

	// Compare until strings do not match
	while (result == 0) {
		result = *str1 - *str2;
		len++;
		if (*str1 == '\0' || *str2 == '\0' || len >= size) break;	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return result;
}

static FsFile idx_f_stat(uint16_t idx)
{
    FsFile file;
	int error_count = 0;
    /*
	if (idx == 0) {
        sd.chdir("..");
        dir.rewind();
        return dir.openNextFile();
	}
    */
	//if (f_stat_cnt == 1 || f_stat_cnt > idx) {
	if (f_stat_cnt == 0 || f_stat_cnt == 1 || f_stat_cnt > idx) {
		// Rewind directory index
        dir.rewind();
		f_stat_cnt = 1;
	}
	for (;;) {
        //file = dir.openNextFile();
        file.openNext(&dir);
		if (file.isHidden()) continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (file.isDir()) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(file.isDir())) continue;
		}
		if (f_stat_cnt++ >= idx) break;
	}
	return file;
}

static void set_sorted(uint16_t pos) /* pos is entry_list's position, not index number */
{
	*(sorted_flg + (pos/32)) |= 1<<(pos%32);
}

static int get_sorted(uint16_t pos) /* pos is entry_list's position, not index number */
{
	return ((*(sorted_flg + (pos/32)) & 1<<(pos%32)) != 0);
}

static int get_range_full_sorted(uint16_t start, uint16_t end_1)
{
	int res = 1;
	for (int i = start; i < end_1; i++) {
		res &= get_sorted(i);
	}
	return res;
}

static int get_range_full_unsorted(uint16_t start, uint16_t end_1)
{
	int res = 0;
	for (int i = start; i < end_1; i++) {
		res |= get_sorted(i);
	}
	return !res;
}

static void set_is_file(uint16_t idx)
{
	*(is_file_flg + (idx/32)) |= 1<<(idx%32);
}

static int get_is_file(uint16_t idx)
{
	return ((*(is_file_flg + (idx/32)) & 1<<(idx%32)) != 0);
}

static void idx_qsort_entry_list_by_range(uint16_t r_start, uint16_t r_end_1, uint16_t start, uint16_t end_1)
{
	int result;
	int start_next;
	int end_1_next;
    FsFile file, file_temp;
	const char *fno_ptr;
	const char *fno_temp_ptr;
    char name[256];
    char name_temp[256];
	if (r_start < start) r_start = start;
	if (r_end_1 > end_1) r_end_1 = end_1;
	if (get_range_full_sorted(r_start, r_end_1)) return;
	if (get_range_full_sorted(start, end_1)) return;
	if (!get_range_full_unsorted(start, end_1)) { // Sorting including previous sorted items broke correct result
		start_next = start;
		while (get_sorted(start_next)) {
			start_next++;
		}
		end_1_next = start_next+1;
		while (!get_sorted(end_1_next) && end_1_next < end_1) {
			end_1_next++;
		}
		sprintf(_str, "partial %d %d %d", start_next, end_1_next, end_1);
        Serial.println(_str);
		idx_qsort_entry_list_by_range(r_start, r_end_1, start_next, end_1_next);
		if (end_1_next < end_1) {
			idx_qsort_entry_list_by_range(r_start, r_end_1, end_1_next, end_1);
		}
		return;
	}
	//sprintf(_str, "r_start %d r_end_1 %d start %d end_1 %d", r_start, r_end_1, start, end_1);
    //Serial.println(_str);
	/* DEBUG
	Serial.println();
	for (int k = start; k < end_1; k++) {
		file = idx_f_stat(entry_list[k]);
        file.getName(name, sizeof(name));
		sprintf(_str, "before[%d] %d %s", k, entry_list[k], name);
        Serial.println(_str);
	}
	*/
	if (end_1 - start <= 2) {
		// try fast_fname_list compare
		result = get_is_file(entry_list[start]) - get_is_file(entry_list[start+1]);
		if (result == 0) {
			result = my_strncmp(fast_fname_list[entry_list[start]], fast_fname_list[entry_list[start+1]], FFL_SZ);
		}
		//sprintf(_str, "fast_fname_list %s %s %d, %d\n\r", fast_fname_list[entry_list[0]], fast_fname_list[entry_list[1]], entry_list[0], entry_list[1]);
        //Serial.println(_str);
		if (result > 0) {
			idx_entry_swap(start, start+1);
		} else if (result < 0) {
			// do nothing
		} else {
			// full name compare
			file = idx_f_stat(entry_list[start]);
            file.getName(name, sizeof(name));
			file_temp = idx_f_stat(entry_list[start+1]);
            file_temp.getName(name_temp, sizeof(name_temp));
			fno_ptr = (strncmp(name, "The ", 4) == 0) ? &name[4] : name;
			fno_temp_ptr = (strncmp(name_temp, "The ", 4) == 0) ? &name_temp[4] : name_temp;
			result = my_strcmp(fno_ptr, fno_temp_ptr);
			if (result >= 0) {
				idx_entry_swap(start, start+1);
			}
		}
		set_sorted(start);
		set_sorted(start+1);
	} else {
		int top = start;
		int bottom = end_1 - 1;
		uint16_t key_idx = entry_list[start+(end_1-start)/2];
		file_temp = idx_f_stat(key_idx);
        file_temp.getName(name_temp, sizeof(name_temp));
		//sprintf(_str, "key %s", name_temp); // DEBUG
        //Serial.println(_str);
		while (1) {
			// try fast_fname_list compare
			result = get_is_file(entry_list[top]) - get_is_file(key_idx);
			if (result == 0) {
				result = my_strncmp(fast_fname_list[entry_list[top]], fast_fname_list[key_idx], FFL_SZ);
			}
			if (result < 0) {
				top++;
			} else if (result > 0) {
				idx_entry_swap(top, bottom);
				bottom--;				
			} else {
				// full name compare
				file = idx_f_stat(entry_list[top]);
                file.getName(name, sizeof(name));
				fno_ptr = (strncmp(name, "The ", 4) == 0) ? &name[4] : name;
				fno_temp_ptr = (strncmp(name_temp, "The ", 4) == 0) ? &name_temp[4] : name_temp;
				result = my_strcmp(fno_ptr, fno_temp_ptr);
				if (result < 0) {
					top++;
				} else {
					idx_entry_swap(top, bottom);
					bottom--;
				}
			}
			if (top > bottom) break;
		}
		/* DEBUG
		for (int k = 0; k < top; k++) {
			file = idx_f_stat(entry_list[k]);
            file.getName(name, sizeof(name));
			sprintf(_str, "top[%d] %d %s", k, entry_list[k], name);
            Serial.println(_str);
		}
		for (int k = top; k < max_entry_cnt; k++) {
			file = idx_f_stat(entry_list[k]);
            file.getName(name, sizeof(name));
			sprintf(_str, "bottom[%d] %d %s", k, entry_list[k], name);
            Serial.println(_str);
		}
		*/
		if ((r_start < top && r_end_1 > start) && !get_range_full_sorted(start, top)) {
			if (top - start > 1) {
				idx_qsort_entry_list_by_range(r_start, r_end_1, start, top);
			} else {
				set_sorted(start);
			}
		}
		if ((r_start < end_1 && r_end_1 > top) && !get_range_full_sorted(top, end_1)) {
			if (end_1 - top > 1) {
				idx_qsort_entry_list_by_range(r_start, r_end_1, top, end_1);
			} else {
				set_sorted(top);
			}
		}
	}
}

static uint16_t idx_get_size(int target)
{
    FsFile file;
	int16_t cnt = 1;
	// Rewind directory index
    dir.rewind();
	// Directory search completed with null character
	for (;;) {
        file = dir.openNextFile();
        if (!file) break;
		if (file.isHidden()) continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (file.isDir()) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(file.isDir())) continue;
		}
		cnt++;
	}
	// Returns the number of entries read
	return cnt;
}

static void idx_sort_new(void)
{
    FsFile file;
	int i, k;
    char name[256];
	max_entry_cnt = idx_get_size(target);
	entry_list = (uint16_t *) malloc(sizeof(uint16_t) * max_entry_cnt);
	if (entry_list == NULL) Serial.println("malloc entry_list failed");
	for (i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
	sorted_flg = (uint32_t *) malloc(sizeof(uint32_t) * (max_entry_cnt+31)/32);
	if (sorted_flg == NULL) Serial.println("malloc sorted_flg failed");
	memset(sorted_flg, 0, sizeof(uint32_t) * (max_entry_cnt+31)/32);
	is_file_flg = (uint32_t *) malloc(sizeof(uint32_t) * (max_entry_cnt+31)/32);
	if (is_file_flg == NULL) Serial.println("malloc is_file_flg failed");
	memset(is_file_flg, 0, sizeof(uint32_t) * (max_entry_cnt+31)/32);
	fast_fname_list = (char (*)[FFL_SZ]) malloc(sizeof(char[FFL_SZ]) * max_entry_cnt);
	if (fast_fname_list == NULL) Serial.println("malloc fast_fname_list failed");
	for (i = 0; i < max_entry_cnt; i++) {
		file = idx_f_stat(i);
		if (!(file.isDir())) set_is_file(i);
        file.getName(name, sizeof(name));
		if (strncmp(name, "The ", 4) == 0) {
			for (k = 0; k < FFL_SZ; k++) {
				fast_fname_list[i][k] = name[k+4];
			}
		} else {
			for (k = 0; k < FFL_SZ; k++) {
				fast_fname_list[i][k] = name[k];
			}
		}
		/* DEBUG
		char temp_str[5] = "    ";
		strncpy(temp_str, fast_fname_list[i], 4);
		sprintf(_str, "fast_fname_list[%d] = %4s, is_file = %d", i, temp_str, get_is_file(i));
        Serial.println(_str);
		*/
	}
}

static void idx_sort_delete(void)
{
    free(entry_list);
    free(sorted_flg);
    free(fast_fname_list);
	free(is_file_flg);
}

//==============================
// File Menu Public Funcions
//   provided by sorted 'order'
//==============================
// For implicit sort all entries
void file_menu_idle(void)
{
	static int up_down = 0;
	uint16_t r_start = 0;
	uint16_t r_end_1 = 0;
	if (get_range_full_sorted(0, max_entry_cnt)) return;
	for (;;) {
		if (up_down & 0x1) {
			r_start = last_order + 1;
			while (get_range_full_sorted(last_order, r_start) && r_start < max_entry_cnt) {
				r_start++;
			}
			r_start--;
			r_end_1 = r_start + 1;
			while (get_range_full_unsorted(r_start, r_end_1) && r_end_1 <= max_entry_cnt && r_end_1 - r_start <= 5) {
				r_end_1++;
			}
			r_end_1--;
			up_down++;
		} else {
			if (last_order > 0) {
				r_end_1 = last_order - 1;
				while (get_range_full_sorted(r_end_1, last_order+1) && r_end_1 != 0) {
					r_end_1--;
				}
				r_end_1++;
				r_start = r_end_1 - 1;
				while (get_range_full_unsorted(r_start, r_end_1) && r_start != 0 && r_end_1 - r_start <= 5) {
					r_start--;
				}				
			}
			up_down++;
		}
		break;
	}
	
	sprintf(_str, "implicit sort %d %d\n\r", r_start, r_end_1);
    Serial.println(_str);
	Serial.println("implicit sort");
	idx_qsort_entry_list_by_range(r_start, r_end_1, 0, max_entry_cnt);
}

void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1)
{
	uint16_t wing;
	uint16_t wing_start, wing_end_1;
	if (scope_start >= scope_end_1) return;
	if (scope_start > max_entry_cnt - 1) scope_start = max_entry_cnt - 1;
	if (scope_end_1 > max_entry_cnt) scope_end_1 = max_entry_cnt;
	wing = (scope_end_1 - scope_start)*2;
	wing_start = (scope_start > wing) ? scope_start - wing : 0;
	wing = (scope_end_1 - scope_start)*4 - (scope_start - wing_start);
	wing_end_1 = (scope_end_1 + wing < max_entry_cnt) ? scope_end_1 + wing : max_entry_cnt;
	//sprintf(_str, "scope_start %d %d %d %d", scope_start, scope_end_1, wing_start, wing_end_1);
    //Serial.println(_str);
	if (!get_range_full_sorted(scope_start, scope_end_1)) {
		idx_qsort_entry_list_by_range(wing_start, wing_end_1, 0, max_entry_cnt);
	}
}

void file_menu_full_sort(void)
{
	file_menu_sort_entry(0, max_entry_cnt);
}

const TCHAR *file_menu_get_fname_ptr(uint16_t order)
{
    FsFile file;
	file_menu_sort_entry(order, order+5);
	if (order < max_entry_cnt) {
		file = idx_f_stat(entry_list[order]);
        file.getName(_name, sizeof(_name));
		last_order = order;

	}
    if (order == 0) {
		return "..";
    } else if (file) {
		return _name;
	} else {
		return "";
	}
}

FRESULT file_menu_get_fname(uint16_t order, char *str, uint16_t size)
{
    FsFile file;
	file_menu_sort_entry(order, order+5);
	if (order < max_entry_cnt) {
		file = idx_f_stat(entry_list[order]);
        file.getName(str, size);
		last_order = order;
	}
	if (file) {
        return FR_OK;
    } else {
        return FR_INVALID_PARAMETER;
    }
}

int file_menu_is_dir(uint16_t order)
{
	if (order < max_entry_cnt) {
		return !get_is_file(entry_list[order]);
	} else {
		return -1;
	}
}

uint16_t file_menu_get_size(void)
{
	return max_entry_cnt;
}

FRESULT file_menu_open_dir(const TCHAR *path)
{
	f_stat_cnt = 1;
	last_order = 0;
    strncpy(path_info[path_depth++], path, 256);
    char full_path[258*8] = {};
    for (int i = 0; i < path_depth; i++) {
        strncat(full_path, path_info[i], 256);
    }
    if (dir.open(full_path)) {
		idx_sort_new();
        return FR_OK;
	} else {
        return FR_INVALID_PARAMETER;
    }
}

FRESULT file_menu_ch_dir(uint16_t order)
{
    FsFile file;
    char name[256];
    bool res = false;
	if (order < max_entry_cnt) {
		file = idx_f_stat(entry_list[order]);
        file.getName(name, sizeof(name));
        dir.close();
		sprintf(_str, "chdir %s", name);
        Serial.println(_str);
		sd.chdir(name);
        res = dir.open(".");
		idx_sort_delete();
	}
	f_stat_cnt = 1;
	last_order = 0;
    //fr = dir.open(path);
	//sd.chdir(path);
    //fr = dir.open(".");
	if (res) {
		idx_sort_new();
        return FR_OK;
	} else {
        return FR_INVALID_PARAMETER;
    }
}

void file_menu_close_dir(void)
{
	/*
	for (int i = 0; i < max_entry_cnt; i++) {
		char temp_str[5] = "    ";
		strncpy(temp_str, fast_fname_list[i], 4);
		sprintf(_str, "fast_fname_list[%d] = %4s", i, temp_str);
        Serial.println(_str);
	}
	*/
	idx_sort_delete();
    dir.close();
}
