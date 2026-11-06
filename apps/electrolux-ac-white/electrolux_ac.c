/* Electrolux AC Remote - app para Flipper Zero (interface no estilo dos apps AC Remote de Midea/Hitachi).
 * Mantem o estado do ar (como o controle original) e, a cada tecla, envia o sinal IR completo. */
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <infrared_transmit.h>
#include <stdlib.h>
#include <stdio.h>

#include <electrolux_ac_icons.h>
#include "ac_ui.h"

#define TAG "ElectroluxAC"
#define STATE_FILE APP_DATA_PATH("state.bin")
#define STATE_MAGIC 0xE1

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    NotificationApp* notif;
    AcState st;
    Page page;
    uint8_t focus;
    uint8_t cursor;
    bool sending;
    uint32_t timings[AC_MAX_TIMINGS];
} App;

static void state_defaults(AcState* s) {
    s->power = false; s->temp = 22; s->fan = 2;
    s->swing = s->eco = s->vent = s->display = false;
    s->mudo = true; /* mesmo estado base do Liga gravado */
    s->timer = 0;
}

static void state_load(AcState* s) {
    state_defaults(s);
    Storage* st = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(st);
    uint8_t buf[12];
    if(storage_file_open(f, STATE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(f, buf, sizeof(buf)) == sizeof(buf) && buf[0] == STATE_MAGIC &&
           buf[2] >= 16 && buf[2] <= 30 && buf[3] >= 1 && buf[3] <= 4 && buf[11] <= 24) {
            s->power = buf[1]; s->temp = buf[2]; s->fan = buf[3];
            s->swing = buf[4]; s->eco = buf[5]; s->vent = buf[6];
            s->display = buf[7]; s->mudo = buf[8]; s->timer = buf[11];
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void state_save(const AcState* s) {
    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(st, APP_DATA_PATH(""));
    File* f = storage_file_alloc(st);
    uint8_t buf[12] = {STATE_MAGIC, s->power, s->temp, s->fan, s->swing, s->eco, s->vent,
                       s->display, s->mudo, 0, 0, s->timer};
    if(storage_file_open(f, STATE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) storage_file_write(f, buf, sizeof(buf));
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static void send_ir(App* app, uint8_t key) {
    AcState s = app->st;
    size_t n = ac_build_timings(&s, key, app->timings);
    app->sending = true;
    view_port_update(app->vp);
    notification_message(app->notif, &sequence_blink_start_magenta);
    furi_hal_power_insomnia_enter();
    infrared_send_raw_ext(app->timings, n, true, 38000, 0.33f);
    furi_hal_power_insomnia_exit();
    notification_message(app->notif, &sequence_blink_stop);
    app->sending = false;
    state_save(&s);
    FURI_LOG_I(TAG, "enviado key=%u (%u pulsos)", key, (unsigned)n);
}

static void draw_cb(Canvas* c, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    AcState s = app->st;
    Page page = app->page;
    uint8_t focus = app->focus, cursor = app->cursor;
    bool sending = app->sending;
    furi_mutex_release(app->mutex);

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    if(page == PageMain) ac_ui_draw_main(c, &s, focus, sending);
    else ac_ui_draw_functions(c, &s, cursor);
}

static void input_cb(InputEvent* ev, void* ctx) {
    App* app = ctx;
    furi_message_queue_put(app->queue, ev, 0);
}

/* OK sobre o botao em foco: altera o estado e devolve a tecla a enviar (0 = nada) */
static int activate(App* app) {
    AcState* s = &app->st;
    switch(app->focus) {
    case BtnPower: s->power = !s->power; return AcKeyPower;
    case BtnMode: s->vent = !s->vent; return AcKeyVent;
    case BtnTempUp: if(s->temp < 30) { s->temp++; return AcKeyTempUp; } return 0;
    case BtnTempDown: if(s->temp > 16) { s->temp--; return AcKeyTempDown; } return 0;
    case BtnFan: s->fan = s->fan >= 4 ? 1 : s->fan + 1; return AcKeyFan; /* aumenta (turbo volta p/ minima) */
    case BtnSwing: s->swing = !s->swing; return AcKeySwing;
    case BtnEco: s->eco = !s->eco; return AcKeyEco;
    case BtnLed: s->display = !s->display; return AcKeyDisplay;
    case BtnMore: app->page = PageFunctions; return 0;
    default: return 0;
    }
}

int32_t electrolux_ac_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    state_load(&app->st);
    app->focus = BtnPower;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    app->vp = view_port_alloc();
    view_port_set_orientation(app->vp, ViewPortOrientationVertical);
    view_port_draw_callback_set(app->vp, draw_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    InputEvent ev;
    bool running = true;
    while(running && furi_message_queue_get(app->queue, &ev, FuriWaitForever) == FuriStatusOk) {
        bool is_short = ev.type == InputTypeShort, is_rep = ev.type == InputTypeRepeat,
             is_long = ev.type == InputTypeLong;
        if(!is_short && !is_rep && !is_long) continue;
        if(is_long && ev.key != InputKeyOk) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        AcState* s = &app->st;
        int send_key = 0;

        if(ev.key == InputKeyBack) {
            if(is_short) {
                if(app->page == PageFunctions) app->page = PageMain;
                else running = false;
            }
        } else if(app->page == PageMain) {
            int dir = ev.key == InputKeyUp ? NavUp : ev.key == InputKeyDown ? NavDown :
                      ev.key == InputKeyLeft ? NavLeft : ev.key == InputKeyRight ? NavRight : -1;
            if(dir >= 0) {
                if(is_short || is_rep) app->focus = AC_NAV[app->focus][dir];
            } else if(ev.key == InputKeyOk) {
                bool temp_btn = app->focus == BtnTempUp || app->focus == BtnTempDown;
                if(is_short || (is_rep && temp_btn)) {
                    send_key = activate(app);
                } else if(is_long && app->focus == BtnFan) { /* segurar OK no FAN: diminui */
                    if(s->fan > 1) { s->fan--; send_key = AcKeyFan; }
                }
            }
        } else {
            if(ev.key == InputKeyUp && (is_short || is_rep)) app->cursor = app->cursor ? app->cursor - 1 : FnCount - 1;
            else if(ev.key == InputKeyDown && (is_short || is_rep)) app->cursor = (app->cursor + 1) % FnCount;
            else if(app->cursor == FnTimer && (ev.key == InputKeyLeft || ev.key == InputKeyRight) && (is_short || is_rep)) {
                if(ev.key == InputKeyRight) s->timer = s->timer >= 24 ? 0 : s->timer + 1;
                else s->timer = s->timer == 0 ? 24 : s->timer - 1;
            } else if(ev.key == InputKeyOk && is_short) {
                switch(app->cursor) {
                case FnSwing: s->swing = !s->swing; send_key = AcKeySwing; break;
                case FnEco: s->eco = !s->eco; send_key = AcKeyEco; break;
                case FnVent: s->vent = !s->vent; send_key = AcKeyVent; break;
                case FnDisplay: s->display = !s->display; send_key = AcKeyDisplay; break;
                case FnMudo: s->mudo = !s->mudo; send_key = AcKeyMudo; break;
                default: send_key = AcKeyTimer; break;
                }
            }
        }
        furi_mutex_release(app->mutex);
        view_port_update(app->vp);
        if(send_key) {
            send_ir(app, send_key);
            view_port_update(app->vp);
        }
    }

    gui_remove_view_port(app->gui, app->vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(app->vp);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
