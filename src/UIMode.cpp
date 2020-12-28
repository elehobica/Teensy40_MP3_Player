/*------------------------------------------------------/
/ UIMode
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "UIMode.h"
#include "ui_control.h"
#include <file_menu_SdFat.h>
#include <utf_conv.h>
#include "stack.h"

TagRead tag;

// UIMode class instances
LcdCanvas *UIMode::lcd = nullptr;
Threads::Event *UIMode::btn_evt = nullptr;
volatile button_action_t *UIMode::btn_act = nullptr;

//================================
// Implementation of UIMode class
//================================
void UIMode::linkLcdCanvas(LcdCanvas *lcd_canvas)
{
    lcd = lcd_canvas;
}

void UIMode::linkButtonAction(Threads::Event *button_event, volatile button_action_t *button_action)
{
    btn_evt = button_event;
    btn_act = button_action;
}

UIMode::UIMode(const char *name, ui_mode_enm_t ui_mode_enm, UIVars *vars) : name(name), ui_mode_enm(ui_mode_enm), vars(vars), idle_count(0)
{
}

void UIMode::entry(UIMode *prevMode)
{
    idle_count = 0;
    if (ui_mode_enm == vars->init_dest_ui_mode) { // Reached desitination of initial UI mode
        vars->init_dest_ui_mode = InitialMode;
    }
}

const char *UIMode::getName()
{
    return name;
}

ui_mode_enm_t UIMode::getUIModeEnm()
{
    return ui_mode_enm;
}

uint16_t UIMode::getIdleCount()
{
    return idle_count;
}

//=======================================
// Implementation of UIInitialMode class
//=======================================
UIInitialMode::UIInitialMode(UIVars *vars) : UIMode("UIInitialMode", InitialMode, vars)
{
}

UIMode* UIInitialMode::update()
{
    switch (vars->init_dest_ui_mode) {
        case FileViewMode:
        case PlayMode:
            return getUIMode(FileViewMode);
            break;
        default:
            return getUIMode(FileViewMode);
            break;
    }
    return this;
}

void UIInitialMode::entry(UIMode *prevMode)
{
    UIMode::entry(prevMode);
}

void UIInitialMode::draw()
{
}

//=========================================
// Implementation of UIFileViewMode class
//=========================================
UIFileViewMode::UIFileViewMode(UIVars *vars, stack_t *dir_stack) : UIMode("UIFileViewMode", FileViewMode, vars), dir_stack(dir_stack)
{
}

void UIFileViewMode::listIdxItems()
{
    char str[256];
    for (int i = 0; i < vars->num_list_lines; i++) {
        if (vars->idx_head+i >= file_menu_get_num()) {
            lcd->setFileItem(i, ""); // delete
            continue;
        }
        if (vars->idx_head+i == 0) {
            lcd->setFileItem(i, "..", file_menu_is_dir(vars->idx_head+i), (i == vars->idx_column));
        } else {
            file_menu_get_fname_UTF16(vars->idx_head+i, (char16_t *) str, sizeof(str)/2);
            lcd->setFileItem(i, utf16_to_utf8((const char16_t *) str).c_str(), file_menu_is_dir(vars->idx_head+i), (i == vars->idx_column), utf8);
        }
    }
}

bool UIFileViewMode::isAudioFile()
{
    uint16_t idx = vars->idx_head + vars->idx_column;
    return (
        file_menu_match_ext(idx, "mp3", 3) ||  file_menu_match_ext(idx, "MP3", 3) ||  
        file_menu_match_ext(idx, "wav", 3) ||  file_menu_match_ext(idx, "WAV", 3) ||  
        file_menu_match_ext(idx, "m4a", 3) ||  file_menu_match_ext(idx, "M4A", 3) ||  
        file_menu_match_ext(idx, "flac", 4) ||  file_menu_match_ext(idx, "FLAC", 4)
    );
}

uint16_t UIFileViewMode::getNumAudioFiles()
{
    uint16_t num_tracks = 0;
    num_tracks += file_menu_get_ext_num("mp3", 3) + file_menu_get_ext_num("MP3", 3);
    num_tracks += file_menu_get_ext_num("wav", 3) + file_menu_get_ext_num("WAV", 3);
    num_tracks += file_menu_get_ext_num("m4a", 3) + file_menu_get_ext_num("M4A", 3);
    num_tracks += file_menu_get_ext_num("flac", 4) + file_menu_get_ext_num("FLAC", 4);
    return num_tracks;
}

void UIFileViewMode::chdir()
{
    stack_data_t item;
    if (vars->idx_head+vars->idx_column == 0) { // upper ("..") dirctory
        if (stack_get_count(dir_stack) > 0) {
            file_menu_ch_dir(vars->idx_head+vars->idx_column);
        } else { // Already in top directory
            file_menu_close_dir();
            file_menu_open_root_dir(); // Root directory
            item.head = 0;
            item.column = 0;
            stack_push(dir_stack, &item);
        }
        stack_pop(dir_stack, &item);
        vars->idx_head = item.head;
        vars->idx_column = item.column;
    } else { // normal directory
        item.head = vars->idx_head;
        item.column = vars->idx_column;
        stack_push(dir_stack, &item);
        file_menu_ch_dir(vars->idx_head+vars->idx_column);
        vars->idx_head = 0;
        vars->idx_column = 0;
    }
}

UIMode *UIFileViewMode::nextPlay()
{
    vars->next_play_action = None;
    return randomSearch(2);
}

UIMode *UIFileViewMode::randomSearch(uint16_t depth)
{
    int i;
    int stack_count;

    Serial.println("Random Search");
    vars->idx_play = 0;
    stack_count = stack_get_count(dir_stack);
    if (stack_count < depth) { return this; }
    for (i = 0; i < depth; i++) {
        vars->idx_head = 0;
        vars->idx_column = 0;
        chdir(); // cd ..;
    }
    while (1) {
        for (i = 0; i < depth; i++) {
            if (file_menu_get_dir_num() == 0) { break; }
            while (1) {
                vars->idx_head = random(1, file_menu_get_num());
                file_menu_sort_entry(vars->idx_head, vars->idx_head+1);
                if (file_menu_is_dir(vars->idx_head) > 0) { break; }
            }
            vars->idx_column = 0;
            chdir();
        }
        // Check if Next Target Dir has Audio track files
        if (stack_count == stack_get_count(dir_stack) && getNumAudioFiles() > 0) { break; }
        // Otherwise, chdir to stack_count-depth and retry again
        Serial.println("Retry Random Search");
        while (stack_count - depth != stack_get_count(dir_stack)) {
            vars->idx_head = 0;
            vars->idx_column = 0;
            chdir(); // cd ..;
        }
    }
    return getUIPlayMode();
}

void UIFileViewMode::idxInc(void)
{
    if (vars->idx_head >= file_menu_get_num() - vars->num_list_lines && vars->idx_column == vars->num_list_lines-1) { return; }
    if (vars->idx_head + vars->idx_column + 1 >= file_menu_get_num()) { return; }
    vars->idx_column++;
    if (vars->idx_column >= vars->num_list_lines) {
        if (vars->idx_head + vars->num_list_lines >= file_menu_get_num() - vars->num_list_lines) {
            vars->idx_column = vars->num_list_lines-1;
            vars->idx_head++;
        } else {
            vars->idx_column = 0;
            vars->idx_head += vars->num_list_lines;
        }
    }
}

void UIFileViewMode::idxDec(void)
{
    if (vars->idx_head == 0 && vars->idx_column == 0) { return; }
    if (vars->idx_column == 0) {
        if (vars->idx_head - vars->num_list_lines < 0) {
            vars->idx_column = 0;
            vars->idx_head--;
        } else {
            vars->idx_column = vars->num_list_lines-1;
            vars->idx_head -= vars->num_list_lines;
        }
    } else {
        vars->idx_column--;
    }
}

void UIFileViewMode::idxFastInc(void)
{
    if (vars->idx_head >= file_menu_get_num() - vars->num_list_lines && vars->idx_column == vars->num_list_lines-1) { return; }
    if (vars->idx_head + vars->idx_column + 1 >= file_menu_get_num()) { return; }
    if (file_menu_get_num() < vars->num_list_lines) {
        idxInc();
    } else if (vars->idx_head + vars->num_list_lines >= file_menu_get_num() - vars->num_list_lines) {
        vars->idx_head = file_menu_get_num() - vars->num_list_lines;
        idxInc();
    } else {
        vars->idx_head += vars->num_list_lines;
    }
}

void UIFileViewMode::idxFastDec(void)
{
    if (vars->idx_head == 0 && vars->idx_column == 0) { return; }
    if (vars->idx_head - vars->num_list_lines < 0) {
        vars->idx_head = 0;
        idxDec();
    } else {
        vars->idx_head -= vars->num_list_lines;
    }
}

UIMode *UIFileViewMode::getUIPlayMode()
{
    if (vars->idx_play == 0) {
        vars->idx_play = vars->idx_head + vars->idx_column;
    }
    vars->num_tracks = getNumAudioFiles();
    return getUIMode(PlayMode);
}

UIMode* UIFileViewMode::update()
{
    switch (vars->init_dest_ui_mode) {
        case PlayMode:
            return getUIPlayMode();
            break;
        default:
            break;
    }
    if (btn_evt->getState()) {
        btn_evt->clear();
        idle_count = 0;
        vars->next_play_action = None;
        switch (*btn_act) {
            case ButtonCenterSingle:
                if (file_menu_is_dir(vars->idx_head+vars->idx_column) > 0) { // Target is Directory
                    chdir();
                    listIdxItems();
                } else { // Target is File
                    file_menu_full_sort();
                    if (isAudioFile()) {
                        return getUIPlayMode();
                    }
                }
                break;
            case ButtonCenterDouble:
                // upper ("..") dirctory
                vars->idx_head = 0;
                vars->idx_column = 0;
                chdir();
                listIdxItems();
                break;
            case ButtonCenterTriple:
                return nextPlay();
                break;
            case ButtonCenterLongLong:
                return getUIMode(PowerOffMode);
                break;
            case ButtonPlusSingle:
                idxDec();
                listIdxItems();
                break;
            case ButtonPlusLong:
                idxFastDec();
                listIdxItems();
                break;
            case ButtonMinusSingle:
                idxInc();
                listIdxItems();
                break;
            case ButtonMinusLong:
                idxFastInc();
                listIdxItems();
                break;
            default:
                break;
        }
    }
    switch (vars->next_play_action) {
        case ImmediatePlay:
            return nextPlay();
            break;
        case TimeoutPlay:
            if (idle_count > WaitCyclesForRandomPlay) {
                return nextPlay();
            }
            break;
        default:
            break;
    }
    if (idle_count > WaitCyclesForPowerOffWhenNoOp) {
        return getUIMode(PowerOffMode);
    } else if (idle_count > 100) {
        file_menu_idle(); // for background sort
    }
    lcd->setBatteryVoltage(vars->bat_mv);
    idle_count++;
    return this;
}

void UIFileViewMode::entry(UIMode *prevMode)
{
    UIMode::entry(prevMode);
    listIdxItems();
    lcd->switchToFileView();
}

void UIFileViewMode::draw()
{
    lcd->drawFileView();
}

//====================================
// Implementation of UIPlayMode class
//====================================
UIPlayMode::UIPlayMode(UIVars *vars) : UIMode("UIPlayMode", PlayMode, vars)
{
}

UIMode* UIPlayMode::update()
{
    AudioCodec *codec = audio_get_codec();
    if (btn_evt->getState()) {
        btn_evt->clear();
        idle_count = 0;
        switch (*btn_act) {
            case ButtonCenterSingle:
                codec->pause(!codec->isPaused());
                break;
            case ButtonCenterDouble:
                vars->idx_play = 0;
                codec->stop();
                vars->next_play_action = None;
                return getUIMode(FileViewMode);
                break;
            case ButtonCenterTriple:
                vars->next_play_action = ImmediatePlay;
                return getUIMode(FileViewMode);
                break;
            case ButtonCenterLongLong:
                return getUIMode(PowerOffMode);
                break;
            case ButtonPlusSingle:
            case ButtonPlusLong:
                audio_volume_up();
                break;
            case ButtonMinusSingle:
            case ButtonMinusLong:
                audio_volume_down();
                break;
            default:
                break;
        }
    }
    if (codec->isPaused() && idle_count > WaitCyclesForPowerOffWhenPaused) {
        return getUIMode(PowerOffMode);
    } else if (!codec->isPlaying() || (codec->positionMillis() + 500 > codec->lengthMillis())) {
        MutexFsBaseFile file;
        vars->idx_play++;
        idle_count = 0;
        audio_codec_enm_t next_audio_codec_enm = getAudioCodec(&file);
        if (next_audio_codec_enm != CodecNone) {
            readTag();
            while (codec->isPlaying()) { /*delay(1);*/ } // minimize gap between tracks
            audio_set_codec(next_audio_codec_enm);
            audio_play(&file);
            lcd->switchToPlay();
        } else {
            while (codec->isPlaying()) { delay(1); }
            codec->stop();
            vars->next_play_action = TimeoutPlay;
            return getUIMode(FileViewMode);
        }
    }
    lcd->setVolume(audio_get_volume());
    lcd->setBitRate(codec->bitRate());
    lcd->setPlayTime(codec->positionMillis()/1000, codec->lengthMillis()/1000, codec->isPaused());
    lcd->setBatteryVoltage(vars->bat_mv);
    idle_count++;
    return this;
}

audio_codec_enm_t UIPlayMode::getAudioCodec(MutexFsBaseFile *f)
{
    audio_codec_enm_t audio_codec_enm = CodecNone;
    bool flg = false;
    int ofs = 0;
    char str[256];

    while (vars->idx_play + ofs < file_menu_get_num()) {
        file_menu_get_obj(vars->idx_play + ofs, f);
        f->getName(str, sizeof(str));
        char* ext_pos = strrchr(str, '.');
        if (ext_pos) {
            if (strncmp(ext_pos, ".mp3", 4) == 0 || strncmp(ext_pos, ".MP3", 4) == 0) {
                audio_codec_enm = CodecMp3;
                flg = true;
                break;
            } else if (strncmp(ext_pos, ".wav", 4) == 0 || strncmp(ext_pos, ".WAV", 4) == 0) {
                audio_codec_enm = CodecWav;
                flg = true;
                break;
            } else if (strncmp(ext_pos, ".m4a", 4) == 0 || strncmp(ext_pos, ".M4A", 4) == 0) {
                audio_codec_enm = CodecAac;
                flg = true;
                break;
            } else if (strncmp(ext_pos, ".flac", 5) == 0 || strncmp(ext_pos, ".FLAC", 5) == 0) {
                audio_codec_enm = CodecFlac;
                flg = true;
                break;
            }
        }
        ofs++;
    }
    if (flg) {
        file_menu_get_fname_UTF16(vars->idx_play + ofs, (char16_t *) str, sizeof(str)/2);
        Serial.println(utf16_to_utf8((const char16_t *) str).c_str());
        vars->idx_play += ofs;
    } else {
        vars->idx_play = 0;
    }
    return audio_codec_enm;
}

void UIPlayMode::readTag()
{
    char str[256];
    mime_t mime;
    ptype_t ptype;
    uint64_t img_pos;
    size_t size;
    bool is_unsync;
    int img_cnt = 0;
    
    // Read TAG
    tag.loadFile(vars->idx_play);

    // copy TAG text
    if (tag.getUTF8Track(str, sizeof(str))) {
        uint16_t track = atoi(str);
        sprintf(str, "%d / %d", track, vars->num_tracks);
    } else {
        sprintf(str, "%d / %d", vars->idx_play, vars->num_tracks);
    }
    lcd->setTrack(str);
    if (tag.getUTF8Title(str, sizeof(str))) {
        lcd->setTitle(str, utf8);
    } else { // display filename if no TAG
        file_menu_get_fname_UTF16(vars->idx_play, (char16_t *) str, sizeof(str)/2);
        lcd->setTitle(utf16_to_utf8((const char16_t *) str).c_str(), utf8);
    }
    if (tag.getUTF8Album(str, sizeof(str))) lcd->setAlbum(str, utf8); else lcd->setAlbum("");
    if (tag.getUTF8Artist(str, sizeof(str))) lcd->setArtist(str, utf8); else lcd->setArtist("");
    if (tag.getUTF8Year(str, sizeof(str))) lcd->setYear(str, utf8); else lcd->setYear("");

    // copy TAG image
    lcd->deleteAlbumArt();
    for (int i = 0; i < tag.getPictureCount(); i++) {
        if (tag.getPicturePos(i, &mime, &ptype, &img_pos, &size, &is_unsync)) {
            if (mime == jpeg) { lcd->addAlbumArtJpeg(vars->idx_play, img_pos, size, is_unsync); img_cnt++; }
            else if (mime == png) { lcd->addAlbumArtPng(vars->idx_play, img_pos, size, is_unsync); img_cnt++; }
        }
    }
    // if no AlbumArt in TAG, use JPEG or PNG in current folder
    if (img_cnt == 0) {
        uint16_t idx = 0;
        while (idx < file_menu_get_num()) {
            MutexFsBaseFile f;
            file_menu_get_obj(idx, &f);
            if (file_menu_match_ext(idx, "jpg", 3) || file_menu_match_ext(idx, "JPG", 3) || 
                file_menu_match_ext(idx, "jpeg", 4) || file_menu_match_ext(idx, "JPEG", 4)) {
                lcd->addAlbumArtJpeg(idx, 0, f.fileSize());
                img_cnt++;
            } else if (file_menu_match_ext(idx, "png", 3) || file_menu_match_ext(idx, "PNG", 3)) {
                lcd->addAlbumArtPng(idx, 0, f.fileSize());
                img_cnt++;
            }
            idx++;
        }
    }
}

void UIPlayMode::entry(UIMode *prevMode)
{
    MutexFsBaseFile file;
    UIMode::entry(prevMode);
    audio_set_codec(getAudioCodec(&file));
    readTag();
    audio_play(&file);
    lcd->switchToPlay();
}

void UIPlayMode::draw()
{
    lcd->drawPlay();
}

//=======================================
// Implementation of UIPowerOffMode class
//=======================================
UIPowerOffMode::UIPowerOffMode(UIVars *vars) : UIMode("UIPowerOffMode", PowerOffMode, vars)
{
}

UIMode* UIPowerOffMode::update()
{
    return this;
}

void UIPowerOffMode::entry(UIMode *prevMode)
{
    UIMode::entry(prevMode);
    lcd->switchToPowerOff(vars->power_off_msg);
    lcd->drawPowerOff();
    audio_terminate();
    ui_terminate(prevMode->getUIModeEnm());
}

void UIPowerOffMode::draw()
{
}