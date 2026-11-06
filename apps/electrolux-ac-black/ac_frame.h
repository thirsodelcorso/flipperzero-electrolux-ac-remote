#pragma once
/* Geracao dos frames IR do ar-condicionado Electrolux "Black" (controle premium OLED:
 * POWER/Oscilar/AR INDIRETO/AUTO SENSE/DORMIR BEM/VENTILAR/Turbo/Mudo/Led/Timer).
 * Codigo C puro (sem dependencias do Flipper) para poder ser testado no computador.
 * Protocolo decifrado de gravacoes reais: bloco A (56 bits, 4,6 ms) + frame Gree (B 32 bits + footer 010 + C 32 bits).
 * Bytes enviados LSB primeiro. Checksum Gree validado nos 27 sinais; modelo reproduz as gravacoes bit a bit. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    bool power;
    uint8_t temp; /* 16..30 */
    uint8_t fan;  /* 1 minima, 2 media, 3 alta, 4 turbo */
    bool swing, sleep, vent, display, mudo; /* sleep = Dormir Bem; vent = modo Ventilar; display = Led */
    bool ar_ind, autosense; /* Ar Indireto / Auto Sense: so estado p/ UI; enviados por replay raw (ac_raw.h) */
    uint8_t timer; /* 0 = desligado, 1..24 horas */
} AcState;

enum {
    AcKeyPower = 0x01, AcKeyMudo = 0x02, AcKeyDisplay = 0x03, AcKeyFan = 0x17, AcKeyTempUp = 0x05,
    AcKeyTempDown = 0x06, AcKeyTimer = 0x07, AcKeySwing = 0x08, AcKeyVent = 0x09, AcKeySleep = 0x1B,
};

#define AC_MAX_TIMINGS 600

/* checksum Gree: soma dos nibbles baixos de B[0..3] + nibbles altos de C[0..2] + 10 (nibble alto de C[3]) */
static inline uint8_t ac_gree_chk(const uint8_t* b, const uint8_t* c) {
    unsigned s = (b[0] & 15) + (b[1] & 15) + (b[2] & 15) + (b[3] & 15) + (c[0] >> 4) + (c[1] >> 4) +
                 (c[2] >> 4) + 10;
    return s & 15;
}

static inline size_t ac_emit_bits(uint32_t* t, size_t n, const uint8_t* bytes, size_t len,
                                  uint32_t mark, uint32_t s0, uint32_t s1) {
    for(size_t i = 0; i < len; i++)
        for(int j = 0; j < 8; j++) {
            t[n++] = mark;
            t[n++] = ((bytes[i] >> j) & 1) ? s1 : s0;
        }
    return n;
}

/* Preenche A (7 bytes) e o frame Gree B/C (4+4) para o estado e tecla dados.
 * Com timer (s->timer 1..24), aplica os bits de hora no bloco A e no B (validado nas gravacoes). */
static inline void ac_build_bytes(const AcState* s, uint8_t key, uint8_t* A, uint8_t* B, uint8_t* C) {
    uint8_t fan = s->fan < 1 ? 1 : (s->fan > 4 ? 4 : s->fan);
    uint8_t temp = s->temp < 16 ? 16 : (s->temp > 30 ? 30 : s->temp);
    uint8_t h = s->timer > 24 ? 24 : s->timer;

    A[0] = 0x24;
    A[1] = (s->power << 7) | (s->mudo << 6) | (s->display << 5) | (s->swing << 4) | (temp - 16);
    A[2] = (fan << 5) | (h ? (uint8_t)(((unsigned)h * 15) >> 4) : 0);
    A[3] = h ? (uint8_t)(((16 - h) & 0x0F) << 4) : 0;
    A[4] = (s->sleep << 5) | (s->vent << 7);
    A[5] = key;
    A[6] = A[0] ^ A[1] ^ A[2] ^ A[3] ^ A[4] ^ A[5] ^ 0xAA;

    B[0] = (s->vent ? 3 : 6) | (s->power << 3) | ((fan > 3 ? 3 : fan) << 4) | (s->sleep << 7);
    B[1] = (temp - 16) | (h ? (0x80 | ((h / 10) << 5)) : 0);
    B[2] = 0x80 | (s->display ? 0x20 : 0) | (fan == 4 ? 0x10 : 0);
    if(h) B[2] = (B[2] & 0xF0) | (h % 10);
    B[3] = 0x50;
    C[0] = s->swing ? 1 : 0;
    C[1] = 0xC0;
    C[2] = 0x00;
    C[3] = ac_gree_chk(B, C) << 4;
}

/* Emite um quadro Gree (header + B[4] + footer 010 + gap + C[4] + mark). */
static inline size_t ac_emit_gree(uint32_t* t, size_t n, const uint8_t* B, const uint8_t* C,
                                  uint32_t hB, uint32_t hBs, uint32_t mB, uint32_t s0B, uint32_t s1B, uint32_t gap2) {
    const uint8_t foot = 0x02; /* bits 0,1,0 (LSB primeiro) */
    t[n++] = hB; t[n++] = hBs;
    n = ac_emit_bits(t, n, B, 4, mB, s0B, s1B);
    for(int j = 0; j < 3; j++) { t[n++] = mB; t[n++] = ((foot >> j) & 1) ? s1B : s0B; }
    t[n++] = mB; t[n++] = gap2;
    n = ac_emit_bits(t, n, C, 4, mB, s0B, s1B);
    t[n++] = mB;
    return n;
}

/* Monta os tempos (us, comecando por mark) do sinal. Retorna a quantidade.
 * Sem timer: bloco A + 1 quadro Gree (0x50). Com timer: + quadro 0x60 (minutos) + quadro 0x70. */
static inline size_t ac_build_timings(const AcState* s, uint8_t key, uint32_t* t) {
    const uint32_t hA = 4594, hAs = 4427, mA = 695, s0A = 513, s1A = 1514, gap1 = 40137;
    const uint32_t hB = 9036, hBs = 4532, mB = 669, s0B = 541, s1B = 1645, gap2 = 20058;
    uint8_t A[7], B[4], C[4];
    ac_build_bytes(s, key, A, B, C);
    uint8_t h = s->timer > 24 ? 24 : s->timer;

    size_t n = 0;
    t[n++] = hA; t[n++] = hAs;
    n = ac_emit_bits(t, n, A, 7, mA, s0A, s1A);
    t[n++] = mA; t[n++] = gap1;
    n = ac_emit_gree(t, n, B, C, hB, hBs, mB, s0B, s1B, gap2); /* quadro 0x50 */

    if(h) {
        unsigned V = (unsigned)h * 15; /* minutos/4; C2=V>>2, C1=(V&3)<<6 */
        uint8_t B2[4] = {B[0], B[1], B[2], 0x60};
        uint8_t C2[4] = {0x00, (uint8_t)((V & 3) << 6), (uint8_t)(V >> 2), 0};
        C2[3] = ac_gree_chk(B2, C2) << 4;
        uint8_t B3[4] = {B[0], B[1], B[2], 0x70};
        uint8_t C3[4] = {0x00, 0x00, 0x00, 0};
        C3[3] = ac_gree_chk(B3, C3) << 4;
        t[n++] = gap1;
        n = ac_emit_gree(t, n, B2, C2, hB, hBs, mB, s0B, s1B, gap2); /* quadro 0x60 (minutos) */
        t[n++] = gap1;
        n = ac_emit_gree(t, n, B3, C3, hB, hBs, mB, s0B, s1B, gap2); /* quadro 0x70 */
    }
    return n;
}
