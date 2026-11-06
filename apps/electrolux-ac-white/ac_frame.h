#pragma once
/* Geracao dos frames IR do ar-condicionado Electrolux (controle com MUDO/DISPLAY/OSCILAR/VELOCID./TIMER/VENTILAR/ECO).
 * Codigo C puro (sem dependencias do Flipper) para poder ser testado no computador.
 * Protocolo decifrado de gravacoes reais: bloco A (56 bits, 4,4 ms) + frame Gree (B 32 bits + footer 010 + C 32 bits);
 * com timer ligado, um segundo frame Gree e enviado em seguida. Bytes sao enviados LSB primeiro. */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    bool power;
    uint8_t temp;  /* 16..30 */
    uint8_t fan;   /* 1 minima, 2 media, 3 alta, 4 turbo */
    bool swing, eco, vent, display, mudo;
    uint8_t timer; /* 0 = desligado, 1..24 horas */
} AcState;

enum {
    AcKeyPower = 1, AcKeyMudo = 2, AcKeyDisplay = 3, AcKeyFan = 4, AcKeyTempUp = 5,
    AcKeyTempDown = 6, AcKeyTimer = 7, AcKeySwing = 8, AcKeyVent = 9, AcKeyEco = 10,
};

#define AC_MAX_TIMINGS 400

/* checksum Gree: soma dos nibbles baixos de B[0..3] + nibbles altos de C[0..2] + 10 */
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

/* Monta os tempos (us, comecando por mark) do sinal para o estado e tecla dados. Retorna a quantidade. */
static inline size_t ac_build_timings(const AcState* s, uint8_t key, uint32_t* t) {
    const uint32_t hA = 4330, hAs = 4427, mA = 675, s0A = 510, s1A = 1534, gap1 = 40171;
    const uint32_t hB = 9010, hBs = 4533, mB = 648, s0B = 561, s1B = 1666, gap2 = 20078;
    const uint32_t gap3 = 40118, gap4 = 20053;
    const uint8_t foot[1] = {0x02}; /* bits 0,1,0 (LSB primeiro) */

    uint8_t h = s->timer > 24 ? 24 : s->timer;
    uint8_t fan = s->fan < 1 ? 1 : (s->fan > 4 ? 4 : s->fan);
    uint8_t temp = s->temp < 16 ? 16 : (s->temp > 30 ? 30 : s->temp);
    uint8_t A[7], B[4], C[4];

    A[0] = 0x24;
    A[1] = (s->power << 7) | (s->mudo << 6) | (s->display << 5) | (s->swing << 4) | (temp - 16);
    A[2] = fan << 5;
    A[3] = 0;
    A[4] = (s->eco << 5) | (s->vent << 7);
    A[5] = key;
    B[0] = (s->vent ? 3 : 0) | (s->power << 3) | ((fan > 3 ? 3 : fan) << 4) | (s->swing << 6) | (s->eco << 7);
    B[1] = temp - 16;
    B[2] = (s->power ? 0x40 : 0) | (fan == 4 ? 0x10 : 0) | (s->display ? 0x20 : 0);
    B[3] = 0x50;
    if(h) {
        A[2] |= (h - 1) & 0x1F;
        A[3] = (uint8_t)(((16 - (h & 15)) & 15) << 4);
        B[1] |= 0x80 | ((h / 10) << 5);
        B[2] = (B[2] & 0xF0) | (h % 10);
    }
    A[6] = A[0] ^ A[1] ^ A[2] ^ A[3] ^ A[4] ^ A[5] ^ 0xAA;
    C[0] = s->swing ? 1 : 0;
    C[1] = 0x01;
    C[2] = 0x00;
    C[3] = ac_gree_chk(B, C) << 4;

    size_t n = 0;
    t[n++] = hA; t[n++] = hAs;
    n = ac_emit_bits(t, n, A, 7, mA, s0A, s1A);
    t[n++] = mA; t[n++] = gap1;
    t[n++] = hB; t[n++] = hBs;
    n = ac_emit_bits(t, n, B, 4, mB, s0B, s1B);
    for(int j = 0; j < 3; j++) { t[n++] = mB; t[n++] = ((foot[0] >> j) & 1) ? s1B : s0B; }
    t[n++] = mB; t[n++] = gap2;
    n = ac_emit_bits(t, n, C, 4, mB, s0B, s1B);
    t[n++] = mB;

    if(h) { /* segundo frame (so com timer ligado): B com B3=0x60 e minutos em C */
        uint8_t B2[4] = {B[0], B[1], B[2], 0x60}, C2[4];
        uint16_t v = (uint16_t)(((60u * h) << 4) | 8u);
        C2[0] = 0; C2[1] = v & 0xFF; C2[2] = v >> 8;
        C2[3] = (uint8_t)((ac_gree_chk(B2, C2) << 4) | 1);
        t[n++] = gap3; t[n++] = hB; t[n++] = hBs;
        n = ac_emit_bits(t, n, B2, 4, mB, s0B, s1B);
        for(int j = 0; j < 3; j++) { t[n++] = mB; t[n++] = ((foot[0] >> j) & 1) ? s1B : s0B; }
        t[n++] = mB; t[n++] = gap4;
        n = ac_emit_bits(t, n, C2, 4, mB, s0B, s1B);
        t[n++] = mB;
    }
    return n;
}
