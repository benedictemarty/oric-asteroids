/*
 * title.h — Écran titre Phase 9c
 */

#ifndef TITLE_H
#define TITLE_H

void title_draw(void);
void title_erase(void);
void gameover_draw(void);
void gameover_erase(void);
void presspace_draw(unsigned char py);
void presspace_erase(unsigned char py);
/* Phase 10d — affichage "WAVE n" centré horizontal */
void wave_label_draw(unsigned char py, unsigned char digit);
void wave_label_erase(unsigned char py, unsigned char digit);
/* Phase 15 — affichage "HIGH SCORES" centré horizontal */
void hiscores_label_draw(unsigned char py);
void hiscores_label_erase(unsigned char py);

#endif /* TITLE_H */
