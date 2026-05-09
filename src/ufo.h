/*
 * ufo.h — Soucoupe Phase 6 (1 simultanée, grande ou petite)
 */

#ifndef UFO_H
#define UFO_H

#define UFO_LARGE     0   /* tirs aléatoires, +200 pts */
#define UFO_SMALL     1   /* tirs ship-tracking, +1000 pts */

extern unsigned char ufo_active;
extern unsigned char ufo_x, ufo_y;
extern signed char   ufo_vx, ufo_vy;
extern unsigned char ufo_type;
extern unsigned char ufo_bullet_active;
extern unsigned char ufo_bullet_x, ufo_bullet_y;
extern signed char   ufo_bullet_vx, ufo_bullet_vy;
extern unsigned char ufo_bullet_ttl;

void ufo_init(void);
void ufo_tick(unsigned char ship_x_in, unsigned char ship_y_in,
              unsigned int score_in);
void ufo_draw(void);
void ufo_kill(void);             /* désactive UFO + son tir */
unsigned char ufo_radius(void);  /* rayon collision selon type */
void ufo_bullet_update(void);
void ufo_bullet_draw(void);

#endif /* UFO_H */
