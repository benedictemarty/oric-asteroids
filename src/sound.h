/*
 * sound.h — API effets sonores Phase 8 (driver AY-3-8912)
 *
 * Politique : 1 effet à la fois (override), pas de mix multi-canaux.
 * Phase 8b raffinera : enveloppe progressive, thrust continu, UFO oscillant.
 */

#ifndef SOUND_H
#define SOUND_H

#define FX_NONE         0
#define FX_FIRE         1   /* tir vaisseau         (~6 frames, tone aigu)   */
#define FX_EXPLODE      2   /* explosion LARGE      (~25 fr, noise R6=12 ≈167 Hz arcade) */
#define FX_THUMP        3   /* thump cadencé        (~8 frames, tone grave)  */
#define FX_HYPER        4   /* hyperespace whoosh   (~14 frames, tone+noise) */
#define FX_THRUST       5   /* thrust noise         (~3 frames, re-déclenché) */
#define FX_LIFE         6   /* chime extra ship     (~20 frames, tone aigu)  */
#define FX_UFO          7   /* UFO bip-bip          (~4 frames, re-déclenché) */
#define FX_BANG_MEDIUM  8   /* explosion MEDIUM     (~25 fr, noise R6=8  ≈246 Hz arcade) */
#define FX_BANG_SMALL   9   /* explosion SMALL      (~22 fr, noise R6=6  ≈306 Hz arcade) */
#define FX_THUMP_2     10   /* thump cadencé Beat2  (sym. Beat1, sweep 129→77 Hz) */
#define FX_UFO_SMALL   11   /* UFO small bip       (sweep MONTANT 983→1354 Hz)   */
/* FX_THUMP (3) = Beat1 (sweep 134→81 Hz) ; FX_UFO (7) = large UFO (sweep 1259→879 Hz) */

extern unsigned char sfx_id;
extern unsigned char sfx_timer;
extern unsigned char frame_cnt;       /* incrémenté par IRQ T1 (50 Hz) */
#pragma zpsym ("sfx_id")
#pragma zpsym ("sfx_timer")
#pragma zpsym ("frame_cnt")

void sound_init(void);
void sound_tick(void);                /* appelé par _irq_handler à 50 Hz */
void sound_play_fx(unsigned char fx_id);
void irq_install(void);               /* installe handler IRQ T1 — appeler après timer_init */

#endif /* SOUND_H */
