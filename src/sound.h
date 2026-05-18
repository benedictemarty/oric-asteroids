/*
 * sound.h — API effets sonores (driver AY-3-8912)
 *
 * Phase 22 — architecture 3 canaux indépendants :
 *   Canal A = effets primaires (fire, explode S/M/L, hyper, thrust, life)
 *   Canal B = thump dédié (Beat1/Beat2) — jamais interrompu par A
 *   Canal C = UFO dédié (large/small) — auto-restart via ufo_snd_act
 *
 * R7 est recalculé dynamiquement par update_mixer à chaque changement.
 */

#ifndef SOUND_H
#define SOUND_H

#define FX_NONE         0
#define FX_FIRE         1   /* tir vaisseau    noise 740 Hz + env decay  [A] */
#define FX_EXPLODE      2   /* explosion LARGE noise R6=12 ≈167 Hz       [A] */
#define FX_THUMP        3   /* beat1 sweep 134→81 Hz                     [B] */
#define FX_HYPER        4   /* hyperespace tone+noise ~558 ms             [A] */
#define FX_THRUST       5   /* rumble grave 82 Hz (re-déclenché)          [A] */
#define FX_LIFE         6   /* chime extra ship 2956 Hz + env decay       [A] */
#define FX_UFO          7   /* large UFO oscillation 1000/800 Hz          [C] */
#define FX_BANG_MEDIUM  8   /* explosion MEDIUM noise R6=8  ≈246 Hz      [A] */
#define FX_BANG_SMALL   9   /* explosion SMALL  noise R6=6  ≈306 Hz      [A] */
#define FX_THUMP_2     10   /* beat2 sweep 129→77 Hz (sym. Beat1)        [B] */
#define FX_UFO_SMALL   11   /* small UFO sweep montant 700→1300 Hz       [C] */

extern unsigned char sfx_id;    /* canal A id (FX_NONE = libre)         */
extern unsigned char sfx_timer; /* canal A timer                         */
extern unsigned char frame_cnt; /* incrémenté à 50 Hz par IRQ T1        */
#pragma zpsym ("sfx_id")
#pragma zpsym ("sfx_timer")
#pragma zpsym ("frame_cnt")

void sound_init(void);
void sound_tick(void);               /* appelé par _irq_handler à 50 Hz */
void sound_play_fx(unsigned char fx_id);
void sound_stop_ufo(void);           /* arrête canal C + désactive auto-restart */
void irq_install(void);

#endif /* SOUND_H */
