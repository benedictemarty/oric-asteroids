/*
 * asteroids.h — Interface du module astéroïdes (Phase 4)
 */

#ifndef ASTEROIDS_H
#define ASTEROIDS_H

#define MAX_ASTEROIDS 12

#define SIZE_SMALL    0
#define SIZE_MEDIUM   1
#define SIZE_LARGE    2

typedef struct {
    unsigned char x, y;
    signed char   vx, vy;
    unsigned char shape;     /* 0-3 */
    unsigned char size;      /* SIZE_SMALL / MEDIUM / LARGE */
    unsigned char active;
} Asteroid;

extern Asteroid asteroids[MAX_ASTEROIDS];

void asteroids_init(unsigned char seed);
void asteroids_spawn_wave(void);
void asteroids_update(void);
void asteroids_draw(void);
void asteroids_fragment(unsigned char idx);
unsigned char asteroids_count(void);
unsigned char rng8(void);

#endif /* ASTEROIDS_H */
