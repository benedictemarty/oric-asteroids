/*
 * sound.h — API effets sonores Phase 8 (driver AY-3-8912)
 *
 * Politique : 1 effet à la fois (override), pas de mix multi-canaux.
 * Phase 8b raffinera : enveloppe progressive, thrust continu, UFO oscillant.
 */

#ifndef SOUND_H
#define SOUND_H

#define FX_NONE     0
#define FX_FIRE     1   /* tir vaisseau         (~6 frames, tone aigu)   */
#define FX_EXPLODE  2   /* explosion asteroid   (~25 frames, noise)      */
#define FX_THUMP    3   /* thump cadencé        (~8 frames, tone grave)  */
#define FX_HYPER    4   /* hyperespace whoosh   (~14 frames, tone+noise) */

extern unsigned char sfx_id;
extern unsigned char sfx_timer;
#pragma zpsym ("sfx_id")
#pragma zpsym ("sfx_timer")

void sound_init(void);
void sound_tick(void);
void sound_play_fx(unsigned char fx_id);

#endif /* SOUND_H */
