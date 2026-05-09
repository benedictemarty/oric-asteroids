/*
 * hud.h — Score + vies, rendu 7-segments + mini-triangles (Phase 5)
 */

#ifndef HUD_H
#define HUD_H

#define HUD_LIVES_INIT  3
#define HUD_EXTRA_BONUS 10000U      /* extra ship tous les 10 000 pts */

extern unsigned int  score;
extern unsigned int  score_extra;
extern unsigned char lives;
extern unsigned char gameover;

void hud_init(void);
/* Effacer + redessiner le HUD à chaque frame uniquement si nécessaire. */
void hud_draw(void);
/* Ajoute delta au score, déclenche la vie bonus si seuil atteint. */
void hud_add_score(unsigned int delta);
/* Décrémente une vie ; passe à gameover=1 si lives atteint 0. */
void hud_lose_life(void);

#endif /* HUD_H */
