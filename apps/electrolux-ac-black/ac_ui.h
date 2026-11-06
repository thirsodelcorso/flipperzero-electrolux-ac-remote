#pragma once
/* Interface 64x128 (vertical) do Electrolux Black AC Remote - design v1.5 (icones em bitmap, assets/).
 *
 * DESIGN E ICONES: copiados/adaptados do "Midea AC Remote" de Vladimir Stoliarov (MIT, (c) 2025)
 *   https://github.com/xakep666/flipperzero-midea-ac-remote
 * baseado no "Mitsubishi AC Remote" de Anton Chistyakov (MIT, (c) 2023) e mesmo design do
 * "Hitachi AC Remote" de dogtopus (MIT, (c) 2025). Helice do FAN do "Samsung AC Remote" (Lia Yoffe, MIT, (c) 2026).
 * Icones derivados (fanbtn, swingnt, lednt, sono, more) por este projeto (tools_make_assets.py). A logica IR e propria (ac_frame.h).
 *
 * Adaptacoes do controle Black: botao inferior esquerdo = DORMIR BEM (icone de lua) no lugar do ECO;
 * pagina Funcoes inclui Ar Indireto e Auto Sense. MODE alterna Normal (floco) <-> Ventilar (helice).
 * O arquivo que inclui este deve incluir antes <electrolux_black_icons.h> (ou o equivalente de teste). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ac_frame.h"

typedef enum { PageMain, PageFunctions } Page;
typedef enum { FnSwing, FnSleep, FnVent, FnDisplay, FnMudo, FnArInd, FnAuto, FnTimer, FnCount } Fn;
typedef enum { BtnPower, BtnMode, BtnTempUp, BtnFan, BtnTempDown, BtnSwing, BtnSleep, BtnLed, BtnMore, BtnCount } Btn;
typedef enum { NavUp, NavDown, NavLeft, NavRight } Nav;

/* navegacao do foco: cima, baixo, esquerda, direita */
static const uint8_t AC_NAV[BtnCount][4] = {
    [BtnPower] = {BtnSleep, BtnTempUp, BtnMode, BtnMode},
    [BtnMode] = {BtnMore, BtnFan, BtnPower, BtnPower},
    [BtnTempUp] = {BtnPower, BtnTempDown, BtnFan, BtnFan},
    [BtnFan] = {BtnMode, BtnSwing, BtnTempUp, BtnTempUp},
    [BtnTempDown] = {BtnTempUp, BtnSleep, BtnSwing, BtnSwing},
    [BtnSwing] = {BtnFan, BtnLed, BtnTempDown, BtnTempDown},
    [BtnSleep] = {BtnTempDown, BtnPower, BtnMore, BtnLed},
    [BtnLed] = {BtnSwing, BtnMode, BtnSleep, BtnMore},
    [BtnMore] = {BtnSwing, BtnMode, BtnLed, BtnSleep},
};

static inline Color ac_ink(bool focus) { return focus ? ColorWhite : ColorBlack; }

/* quadradinho no canto superior direito = funcao ligada */
static void ac_mark(Canvas* c, int x, int y, int size, bool focus) {
    canvas_set_color(c, ac_ink(focus));
    canvas_draw_box(c, x, y, size, size);
    canvas_set_color(c, ColorBlack);
}

static void ac_ui_draw_main(Canvas* c, const AcState* s, uint8_t focus, bool sending) {
    char buf[16];
    canvas_set_color(c, ColorBlack);

    /* titulo (barra invertida enquanto envia) */
    canvas_set_font(c, FontPrimary);
    if(sending) {
        canvas_draw_box(c, 0, 0, 64, 13);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, 6, 11, "ENVIANDO");
        canvas_set_color(c, ColorBlack);
    } else {
        canvas_draw_str(c, 6, 11, "Electrolux");
    }

    /* POWER: ar desligado mostra o simbolo de ligar; ar ligado mostra o anel */
    bool f = focus == BtnPower;
    canvas_draw_icon(c, 6, 17, s->power ? (f ? &I_off_hover_19x20 : &I_off_19x20) : (f ? &I_on_hover_19x20 : &I_on_19x20));
    canvas_draw_icon(c, 5, 39, &I_power_text_21x5);

    /* MODE: Normal (floco) / Ventilar (helice) */
    f = focus == BtnMode;
    if(s->vent) canvas_draw_icon(c, 39, 17, f ? &I_fan_hover_19x20 : &I_fan_19x20);
    else canvas_draw_icon(c, 39, 17, f ? &I_cold_hover_19x20 : &I_cold_19x20);
    canvas_draw_icon(c, 40, 39, &I_mode_text_17x5);

    /* temperatura: moldura com colchetes e "C", setas + e -, valor */
    canvas_draw_icon(c, 0, 59, &I_frame_30x39);
    canvas_draw_icon(c, 3, 47, focus == BtnTempUp ? &I_tempup_hover_24x21 : &I_tempup_24x21);
    canvas_draw_icon(c, 3, 89, focus == BtnTempDown ? &I_tempdown_hover_24x21 : &I_tempdown_24x21);
    canvas_set_font(c, FontKeyboard);
    snprintf(buf, sizeof(buf), "%u", s->temp);
    canvas_draw_str(c, 4, 82, buf);

    /* FAN: moldura com triangulo (segurar OK diminui) + helice e 4 barras de velocidade embaixo */
    f = focus == BtnFan;
    canvas_draw_icon(c, 39, 50, f ? &I_fanbtn_hover_19x20 : &I_fanbtn_19x20);
    canvas_set_color(c, ac_ink(f));
    for(int i = 0; i < 4; i++) {
        int bx = 41 + i * 4, bh = 1 + i, yb = 66;
        if(s->fan > i) canvas_draw_box(c, bx, yb - bh + 1, 3, bh);
        else canvas_draw_line(c, bx, yb, bx + 2, yb);
    }
    canvas_set_color(c, ColorBlack);
    canvas_draw_icon(c, 43, 72, &I_fan_text_12x5);

    /* SWING */
    f = focus == BtnSwing;
    canvas_draw_icon(c, 39, 83, f ? &I_swingnt_hover_19x20 : &I_swingnt_19x20);
    if(s->swing) ac_mark(c, 39 + 14, 83 + 2, 3, f);
    canvas_draw_icon(c, 38, 105, &I_swing_text_20x5);

    /* linha de baixo: SONO (Dormir Bem, icone de lua), LED e "..." (funcoes) */
    f = focus == BtnSleep;
    canvas_draw_icon(c, 1, 115, f ? &I_sono_hover_19x11 : &I_sono_19x11);
    if(s->sleep) ac_mark(c, 1 + 15, 115 + 2, 2, f);
    f = focus == BtnLed;
    canvas_draw_icon(c, 22, 115, f ? &I_lednt_hover_19x11 : &I_lednt_19x11);
    if(s->display) ac_mark(c, 22 + 15, 115 + 2, 2, f);
    f = focus == BtnMore;
    canvas_draw_icon(c, 43, 115, f ? &I_more_hover_19x11 : &I_more_19x11);
    if(s->ar_ind || s->autosense || s->timer) ac_mark(c, 43 + 15, 115 + 2, 2, f);
}

static void ac_ui_draw_functions(Canvas* c, const AcState* s, uint8_t cursor) {
    static const char* const names[FnCount] = {"Oscilar", "Dormir Bem", "Ventilar", "Led", "Mudo", "Ar Indireto", "Auto Sense", "Timer"};
    bool vals[FnCount] = {s->swing, s->sleep, s->vent, s->display, s->mudo, s->ar_ind, s->autosense, s->timer != 0};
    char buf[8];
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 9, "Funcoes");
    canvas_draw_line(c, 0, 12, 63, 12);
    canvas_set_font(c, FontSecondary);
    for(int i = 0; i < FnCount; i++) {
        int y = 22 + i * 11;
        bool f = i == cursor;
        if(f) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, 0, y - 8, 64, 10);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
        }
        canvas_draw_str(c, 3, y, names[i]);
        if(i == FnTimer) {
            if(s->timer) snprintf(buf, sizeof(buf), "%uh", s->timer); else snprintf(buf, sizeof(buf), "OFF");
        } else {
            snprintf(buf, sizeof(buf), "%s", vals[i] ? "ON" : "OFF");
        }
        canvas_draw_str_aligned(c, 61, y - 3, AlignRight, AlignCenter, buf);
    }
    canvas_set_color(c, ColorBlack);
    if(cursor == FnTimer) canvas_draw_str(c, 2, 121, "< > horas  OK envia");
    else canvas_draw_str(c, 2, 121, "OK envia   Back volta");
}
