/*------------------------------------------------------/
/ UIMode
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#ifndef _UIMODE_H_
#define _UIMODE_H_

#include <TeensyThreads.h>
#include <LcdCanvas.h>
#include "stack.h"
#include "TagRead.h"
#include "audio_playback.h"

typedef enum _ui_mode_enm_t {
    InitialMode = 0,
    FileViewMode,
    PlayMode,
    PowerOffMode
} ui_mode_enm_t;

typedef enum _button_action_t {
    ButtonCenterSingle = 0,
    ButtonCenterDouble,
    ButtonCenterTriple,
    ButtonCenterLongLong,
    ButtonPlusSingle,
    ButtonPlusLong,
    ButtonMinusSingle,
    ButtonMinusLong,
    ButtonOthers
} button_action_t;

typedef enum _next_play_action_t {
    None = 0,
    ImmediatePlay,
    TimeoutPlay
} next_play_action_t;

typedef enum _next_play_type_t {
    SequentialPlay = 0,
    RandomPlay,
} next_play_type_t;

class UIVars
{
public:
    uint16_t num_list_lines = 1;
    ui_mode_enm_t init_dest_ui_mode = InitialMode;
    uint16_t bat_mv = 4200;
    uint16_t idx_head = 0;
    uint16_t idx_column = 0;
    uint16_t idx_play = 0;
    uint16_t num_tracks = 0;
    next_play_action_t next_play_action = None;
    next_play_type_t next_play_type = RandomPlay;
    const char *power_off_msg = NULL;
};

//===========================
// Interface of UIMode class
//===========================
class UIMode
{
public:
    UIMode(const char *name, ui_mode_enm_t ui_mode_enm, UIVars *vars);
    virtual UIMode* update() = 0;
    virtual void entry(UIMode *prevMode);
    virtual void draw() = 0;
    const char *getName();
    ui_mode_enm_t getUIModeEnm();
    uint16_t getIdleCount();
    static Threads::Event *btn_evt;
    static volatile button_action_t *btn_act;
    static LcdCanvas *lcd;
    static const int UpdateCycleMs = 50; // loop cycle (ms)
    static const int OneSec = 1000 / UpdateCycleMs; // 1 Sec
    static const int OneMin = 60 * OneSec; // 1 Min
    static void linkLcdCanvas(LcdCanvas *lcd_canvas);
    static void linkButtonAction(Threads::Event *button_event, volatile button_action_t *button_action);
protected:
    const char *name;
    ui_mode_enm_t ui_mode_enm;
    UIVars *vars;
    uint16_t idle_count;
};

//===================================
// Interface of UIInitialMode class
//===================================
class UIInitialMode : public UIMode
{
public:
    UIInitialMode(UIVars *vars);
    UIMode* update();
    void entry(UIMode *prevMode);
    void draw();
};

//===================================
// Interface of UIFileViewMode class
//===================================
class UIFileViewMode : public UIMode
{
public:
    static const int WaitCyclesForRandomPlay = 1 * OneMin;
    static const int WaitCyclesForPowerOffWhenNoOp = 3 * OneMin;
    UIFileViewMode(UIVars *vars, stack_t *dir_stack);
    UIMode* update();
    void entry(UIMode *prevMode);
    void draw();
protected:
    stack_t *dir_stack;
    void listIdxItems();
    bool isAudioFile();
    uint16_t getNumAudioFiles();
    void chdir();
    UIMode *nextPlay();
    UIMode *randomSearch(uint16_t depth);
    void idxInc();
    void idxDec();
    void idxFastInc();
    void idxFastDec();
    UIMode* getUIPlayMode();
};

//===============================
// Interface of UIPlayMode class
//===============================
class UIPlayMode : public UIMode
{
public:
    static const int WaitCyclesForPowerOffWhenPaused = 3 * OneMin;
    UIPlayMode(UIVars *vars);
    UIMode* update();
    void entry(UIMode *prevMode);
    void draw();
protected:
    audio_codec_enm_t getAudioCodec(MutexFsBaseFile *f);
    void readTag();
};

//===================================
// Interface of UIPowerOffMode class
//===================================
class UIPowerOffMode : public UIMode
{
public:
    UIPowerOffMode(UIVars *vars);
    UIMode* update();
    void entry(UIMode *prevMode);
    void draw();
    void setMsg(const char *msg);
protected:
    const char *msg = NULL;
};

#endif //_UIMODE_H_
