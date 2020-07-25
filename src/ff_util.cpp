#include "ff_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SD_FAT_TYPE == 2
SdExFat sd;
static ExFile parent_dir[MAX_DEPTH_DIR]; 	// preserve parent directory
static ExFile dir;
static ExFile file;
static ExFile file_temp;
#elif SD_FAT_TYPE == 3
SdFs sd;
static FsFile parent_dir[MAX_DEPTH_DIR]; 	// preserve parent directory
static FsFile dir;
static FsFile file;
static FsFile file_temp;
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

static int path_depth = 0;					// preserve path depth because SdFat doesn't support relative directory
static char _str[256];	// for debug print

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

static FRESULT idx_f_stat(uint16_t idx,  FsBaseFile *file)
{
	if (idx == 0) {
		*file = parent_dir[path_depth-1];
        return FR_OK;
	}
	if (f_stat_cnt == 1 || f_stat_cnt > idx) {
        dir.rewindDirectory();
		f_stat_cnt = 1;
	}
	for (;;) {
        file->openNext(&dir);
		if (file->isHidden()) continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (file->isDir()) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(file->isDir())) continue;
		}
		if (f_stat_cnt++ >= idx) break;
	}
	if (file) {
        return FR_OK;
	} else {
        return FR_INVALID_PARAMETER;
	}
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
	const char *fno_ptr;
	const char *fno_temp_ptr;
    char name[FF_LFN_BUF+1];
    char name_temp[FF_LFN_BUF+1];
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
		sprintf(_str, "partial %d %d %d", start_next, end_1_next, end_1); Serial.println(_str);
		idx_qsort_entry_list_by_range(r_start, r_end_1, start_next, end_1_next);
		if (end_1_next < end_1) {
			idx_qsort_entry_list_by_range(r_start, r_end_1, end_1_next, end_1);
		}
		return;
	}
	//sprintf(_str, "r_start %d r_end_1 %d start %d end_1 %d", r_start, r_end_1, start, end_1); Serial.println(_str);
	/* DEBUG
	Serial.println();
	for (int k = start; k < end_1; k++) {
		idx_f_stat(entry_list[k], &file);
        file.getName(name, sizeof(name));
		sprintf(_str, "before[%d] %d %s", k, entry_list[k], name); Serial.println(_str);
	}
	*/
	if (end_1 - start <= 1) {
		set_sorted(start);
	} else if (end_1 - start <= 2) {
		// try fast_fname_list compare
		result = get_is_file(entry_list[start]) - get_is_file(entry_list[start+1]);
		if (result == 0) {
			result = my_strncmp(fast_fname_list[entry_list[start]], fast_fname_list[entry_list[start+1]], FFL_SZ);
		}
		//sprintf(_str, "fast_fname_list %s %s %d, %d", fast_fname_list[entry_list[0]], fast_fname_list[entry_list[1]], entry_list[0], entry_list[1]); Serial.println(_str);
		if (result > 0) {
			idx_entry_swap(start, start+1);
		} else if (result < 0) {
			// do nothing
		} else {
			// full name compare
			idx_f_stat(entry_list[start], &file);
            file.getName(name, sizeof(name));
			idx_f_stat(entry_list[start+1], &file_temp);
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
		idx_f_stat(key_idx, &file_temp);
        file_temp.getName(name_temp, sizeof(name_temp));
		//sprintf(_str, "key %s", name_temp); Serial.println(_str); // DEBUG
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
				idx_f_stat(entry_list[top], &file);
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
			idx_f_stat(entry_list[k], &file);
            file.getName(name, sizeof(name));
			sprintf(_str, "top[%d] %d %s", k, entry_list[k], name); Serial.println(_str);
		}
		for (int k = top; k < max_entry_cnt; k++) {
			idx_f_stat(entry_list[k], &file);
            file.getName(name, sizeof(name));
			sprintf(_str, "bottom[%d] %d %s", k, entry_list[k], name); Serial.println(_str);
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
	int16_t cnt = 1;
    dir.rewindDirectory();
	// Directory search completed with null character
	for (;;) {
        file.openNext(&dir);
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
	int i, k;
    char name[FF_LFN_BUF+1];
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
		idx_f_stat(i, &file);
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
		sprintf(_str, "fast_fname_list[%d] = %4s, is_file = %d", i, temp_str, get_is_file(i)); Serial.println(_str);
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
	const int range = 20;
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
			while (get_range_full_unsorted(r_start, r_end_1) && r_end_1 <= max_entry_cnt && r_end_1 - r_start <= range) {
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
				while (get_range_full_unsorted(r_start, r_end_1) && r_start != 0 && r_end_1 - r_start <= range) {
					r_start--;
				}				
			}
			up_down++;
		}
		break;
	}
	
	sprintf(_str, "implicit sort %d %d", r_start, r_end_1); Serial.println(_str);
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
	//sprintf(_str, "scope_start %d %d %d %d", scope_start, scope_end_1, wing_start, wing_end_1); Serial.println(_str);
	if (!get_range_full_sorted(scope_start, scope_end_1)) {
		idx_qsort_entry_list_by_range(wing_start, wing_end_1, 0, max_entry_cnt);
	}
}

void file_menu_full_sort(void)
{
	file_menu_sort_entry(0, max_entry_cnt);
}

FRESULT file_menu_get_fname(uint16_t order, char *str, size_t size)
{
	file_menu_sort_entry(order, order+5);
	if (order >= max_entry_cnt) { return FR_INVALID_PARAMETER; }
	if (order == 0) {
		strncpy(str, "..", size);
	} else {
		idx_f_stat(entry_list[order], &file);
        file.getName(str, size);
		last_order = order;
	}
	return FR_OK;
}

FRESULT file_menu_get_obj(uint16_t order, FsBaseFile *file)
{
	file_menu_sort_entry(order, order+5);
	if (order >= max_entry_cnt) { return FR_INVALID_PARAMETER; }
	if (order == 0) {
		return FR_INVALID_PARAMETER;
	} else {
		idx_f_stat(entry_list[order], file);
		last_order = order;
	}
	return FR_OK;
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

FRESULT file_menu_open_root_dir()
{
	f_stat_cnt = 1;
	last_order = 0;
	path_depth = 0;
	// Initialize the SD.
	if (!sd.begin(SD_CONFIG)) {
		// stop here, but print a message repetitively
		while (1) {
			Serial.println("Unable to access the SD card");
			delay(500);
		}
        return FR_INVALID_PARAMETER;
	}
	Serial.println("SD card OK");
    if (dir.open("/")) {
		parent_dir[path_depth++] = dir;
		idx_sort_new();
        return FR_OK;
	} else {
        return FR_INVALID_PARAMETER;
    }
}

FRESULT file_menu_ch_dir(uint16_t order)
{
	f_stat_cnt = 1;
	last_order = 0;
	if (order == 0) {
		if (path_depth > 0) {
			idx_sort_delete();
			dir = parent_dir[--path_depth];
			idx_sort_new();
		}
	} else if (order < max_entry_cnt && path_depth < MAX_DEPTH_DIR - 1) {
		FsFile new_dir;
		idx_f_stat(entry_list[order], &file);
		new_dir.open(&dir, file.dirIndex(), O_RDONLY);
		dir.rewindDirectory();
		parent_dir[path_depth++] = dir;
		idx_sort_delete();
		dir = new_dir;
		idx_sort_new();
	} else {
        return FR_INVALID_PARAMETER;
	}
	return FR_OK;
}

void file_menu_close_dir(void)
{
	/*
	for (int i = 0; i < max_entry_cnt; i++) {
		char temp_str[5] = "    ";
		strncpy(temp_str, fast_fname_list[i], 4);
		sprintf(_str, "fast_fname_list[%d] = %4s", i, temp_str); Serial.println(_str);
	}
	*/
	idx_sort_delete();
    dir.close();
}
