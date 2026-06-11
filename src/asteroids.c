/*
 * asteroids.c — Astéroïdes
 *
 * Tableau d'astéroïdes en BSS. Chacun = (x, y, vx, vy, shape, size, active).
 * Rendu (Phase 26 / P3) : sprites pré-rendus XOR blittés octet par octet
 * (_spr_blit, line.s) — les asteroids ne tournent jamais, donc 12
 * silhouettes × 6 phases intra-octet générées par tools/gen_shapes.py.
 * Shapes arcade rev 4 complètes (11-13 sommets, décimation supprimée).
 * Mouvement : intégration 8.8 + wraparound par duplication d'instance.
 * Fragmentation : 1 parent réduit + 2 enfants = 3 fragments par hit.
 *
 * Le tableau est exposé à game.c via les fonctions ci-dessous.
 */

#include "asteroids.h"
#include "line.h"

/* Phase 26 (P3) — sprites pré-rendus par tools/gen_shapes.py.
 * Les asteroids ne tournent jamais : chaque silhouette (size*4+shape)
 * est un bitmap HIRES pré-décalé dans ses 6 phases intra-octet,
 * XOR-blitté par _spr_blit (line.s). Le coût de rendu ne dépend plus
 * du nombre de sommets → shapes arcade 11-13 sommets restaurées. */
extern const unsigned char spr_w[12];     /* stride octets */
extern const unsigned char spr_h[12];     /* rangées */
extern const signed char  spr_oy[12];     /* coin haut vs centre */
extern const signed char  spr_cold[72];   /* delta colonne, id*6+phase */
extern const unsigned char spr_lo[72];    /* ptr bitmap (bas) */
extern const unsigned char spr_hi[72];    /* ptr bitmap (haut) */
extern const unsigned char x_phase[240];  /* cx mod 6 */
extern const unsigned char shape_radii[3];

/* État des astéroïdes (BSS, zéro à l'init via bullets_init logique) */
Asteroid asteroids[MAX_ASTEROIDS];

/* Phase 10c — wave counter arcade */
unsigned char current_wave;
static unsigned char ast_per_wave;

/* Phase 10h — ScrSpeedup arcade ($02FD) : seuil d'asteroids count en
 * dessous duquel le saucer apparaît plus souvent. Init à 5, +1 par
 * vague, max 11 (cf. InitGameVars $690E + InitWaveVars $717A-$7184). */
unsigned char scr_speedup;

/* Phase 10k — AstBreakTimer arcade ($02F9) : compteur frames après
 * destruction d'un asteroid. Pendant ce délai, le saucer ne peut pas
 * spawn. Reload dans BreakAsteroid ($75EE — 80 frames arcade 60 Hz,
 * transposé 33 frames à 25 Hz), décrément 1/frame dans DoScrTmrUpdate
 * ($6BAF). */
unsigned char ast_break_timer;

/* RNG : LFSR 8-bit Galois (polynôme x^8 + x^6 + x^5 + x^4 + 1) */
static unsigned char rng_state;

unsigned char rng8(void)
{
    unsigned char lsb = rng_state & 1;
    rng_state >>= 1;
    if (lsb) rng_state ^= 0xB8;
    return rng_state;
}

void asteroids_init(unsigned char seed)
{
    register Asteroid *p;
    unsigned char i;
    rng_state = seed ? seed : 0x42;
    current_wave = 0;
    ast_per_wave = 0;
    scr_speedup = 5;            /* arcade : init à 5 */
    ast_break_timer = 0;        /* arcade : aucun hit en attente */
    /* Phase 24 — itération par pointeur : l'indexation asteroids[i]
     * (sizeof = 13, pas une puissance de 2) force cc65 à appeler
     * mul8x16 à CHAQUE accès champ (~8-9 % du CPU au profil bench).
     * Le pointeur courant la remplace par un adc #sizeof par tour. */
    for (i = 0, p = asteroids; i < MAX_ASTEROIDS; i++, p++) {
        p->active = 0;
        p->drawn  = 0;
    }
}

/* Spawn d'une vague — Phase 10c arcade-fidèle.
 * Cf. InitWaveVars à $7168 (`AstPerWave += 2`, max 11).
 *
 * Logique transposée de `InitWaveAsteroids` ($719B-$71D5) :
 *   - Pour chaque asteroid : choisir shape aléatoirement
 *   - Position aléatoire alternant : top/bottom (Y=0/max, X aléa)
 *     ou left/right (X=0/max, Y aléa) selon RNG bit
 *   - Vélocité aléatoire (RNG sur signe + magnitude) */
void asteroids_spawn_wave(void)
{
    unsigned char i, n, r;

    if (current_wave == 0) {
        current_wave = 1;
        ast_per_wave = 3;          /* réduit pour cap MAX_ASTEROIDS=6 (fragmentation × 3) */
    } else {
        ast_per_wave++;
        if (ast_per_wave > 5) ast_per_wave = 5;
        current_wave++;
        /* Phase 10h : ScrSpeedup += 1 (max 11) — saucer plus pressant */
        if (scr_speedup < 11) scr_speedup++;
    }
    n = ast_per_wave;
    if (n > MAX_ASTEROIDS) n = MAX_ASTEROIDS;

    {
    register Asteroid *p = asteroids;      /* Phase 24 — anti-mul8x16 */
    for (i = 0; i < n; i++, p++) {
        p->active = 1;
        p->drawn  = 0;            /* pas encore tracé : skip erase au 1er render */
        p->size   = SIZE_LARGE;
        /* Shape aléatoire 0-3 (cf. arcade : AND #$18 puis ORA #$04 → status) */
        p->shape  = (rng8() >> 3) & 3;

        /* Position : RNG bit 0 décide top/bottom ou left/right */
        r = rng8();
        if (r & 1) {
            /* Apparaît sur bord gauche/droit, Y aléatoire */
            p->x = (r & 2) ? 12 : 220;
            p->y = 24 + (rng8() % 144);     /* zone safe Y */
        } else {
            /* Apparaît sur bord haut/bas, X aléatoire */
            p->x = 20 + (rng8() % 192);     /* zone safe X */
            p->y = (r & 2) ? 20 : 180;
        }
        p->prev_x = p->x;
        p->prev_y = p->y;
        p->x_frac = 0;
        p->y_frac = 0;

        /* Vélocité 8.8 : 96 ou 192 = 0.375 ou 0.75 px/frame initial.
         * Phase 37 (tuning) : -25 % vs 128/256 — les gros astéroïdes
         * traversaient l'écran trop vite après validation manuelle
         * des torpilles 2×2 (retour joueur 2026-06-12). */
        r = rng8();
        p->vx = ((r & 1) ? 1 : -1) * ((r & 2) ? 192 : 96);
        p->vy = ((r & 4) ? 1 : -1) * ((r & 8) ? 192 : 96);
    }
    /* Marquer les autres comme libres */
    for (; i < MAX_ASTEROIDS; i++, p++) {
        p->active = 0;
        p->drawn  = 0;
    }
    }
}

/* Mise à jour : intégration + wraparound écran complet (Phase 10l).
 * Le wraparound utilise désormais l'écran complet [0, 239] × [0, 199] ;
 * les asteroids près des bords dessinent leur instance fantôme à l'autre
 * extrémité (duplication d'instance arcade-fidèle).  Le clipping segment
 * par segment dans asteroid_draw_one_at évite l'overflow HIRES si des
 * vertices dépassent. */
#define WRAP_X_MIN  0
#define WRAP_X_MAX  239
#define WRAP_Y_MIN  0
#define WRAP_Y_MAX  199
#define WRAP_X_SPAN 240
#define WRAP_Y_SPAN 200

/* Wrap 8.8 fixed-point — le pixel pos évolue dans [0, SPAN-1] mais la
 * version 16-bit (pixel<<8 | frac) dans [0, SPAN*256-1]. L'addition de vy
 * (signed) en arithmétique modulo 65536 produit un résultat correct
 * MODULO 65536 — pas modulo SPAN*256. Il faut donc tester selon le signe
 * de la vélocité :
 *   - vy ≥ 0 : si pos16 a dépassé SPAN*256, soustraire SPAN*256.
 *   - vy < 0 : si pos16 a "sous-flowé" (= maintenant ≥ SPAN*256 vu
 *              modulo 65536), ajouter SPAN*256 (modulo 65536). */
#define X_SPAN_FIX  ((unsigned int)WRAP_X_SPAN << 8)   /* 240 * 256 = 61440 */
#define Y_SPAN_FIX  ((unsigned int)WRAP_Y_SPAN << 8)   /* 200 * 256 = 51200 */

void asteroids_update(void)
{
    register Asteroid *p;                  /* Phase 24 — anti-mul8x16 */
    unsigned char i;
    unsigned int pos16;
    for (i = 0, p = asteroids; i < MAX_ASTEROIDS; i++, p++) {
        if (!p->active) continue;
        /* Sauver pos courante comme prev (= pos où l'astéroïde est tracé en
         * sortie de frame précédente) avant de calculer la nouvelle pos. */
        p->prev_x = p->x;
        p->prev_y = p->y;

        /* X : 8.8 add + wrap modulo SPAN*256 (≠ modulo 65536). */
        pos16 = ((unsigned int)p->x << 8) | p->x_frac;
        pos16 += (unsigned int)p->vx;
        if (p->vx >= 0) {
            if (pos16 >= X_SPAN_FIX) pos16 -= X_SPAN_FIX;
        } else {
            if (pos16 >= X_SPAN_FIX) pos16 += X_SPAN_FIX;
        }
        p->x      = (unsigned char)(pos16 >> 8);
        p->x_frac = (unsigned char)(pos16 & 0xFF);

        /* Y : idem. */
        pos16 = ((unsigned int)p->y << 8) | p->y_frac;
        pos16 += (unsigned int)p->vy;
        if (p->vy >= 0) {
            if (pos16 >= Y_SPAN_FIX) pos16 -= Y_SPAN_FIX;
        } else {
            if (pos16 >= Y_SPAN_FIX) pos16 += Y_SPAN_FIX;
        }
        p->y      = (unsigned char)(pos16 >> 8);
        p->y_frac = (unsigned char)(pos16 & 0xFF);
    }
}

/* id*6 précalculé — évite l'appel mulax6 cc65 dans le chemin chaud */
static const unsigned char id_x6[12] = { 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66 };

/* Phase 26 (P3) — blit d'un sprite asteroid à un centre (cx, cy).
 *
 * colofs/rowofs : 0 pour l'instance principale, ±40 colonnes / ±200
 * rangées pour l'instance fantôme du wraparound (±240 px = ±40 octets ;
 * 240 étant divisible par 6, la phase intra-octet est la MÊME pour
 * l'instance fantôme — un seul jeu de bitmaps suffit).
 *
 * Clipping rectangle : rangées hors [0,199] sautées (rskip/nr),
 * colonnes hors [0,39] tronquées (lead/cnt) — le bitmap étant
 * contigu par rangée, le clip horizontal se réduit à avancer le
 * pointeur de `lead` octets et blitter `cnt` octets par rangée. */
static void asteroid_blit_at(unsigned char id, unsigned char cx, unsigned char cy,
                             signed char colofs, int rowofs)
{
    unsigned char sidx;
    signed char col0;
    int row0;
    unsigned char w, lead, cnt, rskip, nr;

    sidx = id_x6[id] + x_phase[cx];
    col0 = (signed char)((signed char)x_col[cx] + spr_cold[sidx] + colofs);
    w    = spr_w[id];

    /* Clip horizontal en colonnes-octets [0..39] */
    lead = 0;
    cnt  = w;
    if (col0 < 0) {
        lead = (unsigned char)-col0;
        if (lead >= cnt) return;
        cnt -= lead;
        col0 = 0;
    }
    if ((unsigned char)col0 >= 40) return;
    if ((unsigned char)col0 + cnt > 40) cnt = 40 - (unsigned char)col0;

    /* Clip vertical en rangées [0..199] */
    row0 = (int)cy + spr_oy[id] + rowofs;
    if (row0 >= 200) return;
    rskip = 0;
    nr = spr_h[id];
    if (row0 < 0) {
        if (row0 + nr <= 0) return;
        rskip = (unsigned char)-row0;
        nr -= rskip;
        row0 = 0;
    }
    if (row0 + nr > 200) nr = (unsigned char)(200 - row0);

    blt_sptr = (unsigned char *)(spr_lo[sidx] | ((unsigned int)spr_hi[sidx] << 8));
    if (rskip) blt_sptr += (unsigned int)rskip * w;
    blt_sptr  += lead;
    blt_row    = (unsigned char)row0;
    blt_col    = (unsigned char)col0;
    blt_stride = w;
    blt_cnt    = cnt;
    blt_nrows  = nr;
    spr_blit();
}

/* Phase 10l — duplication d'instance : asteroid près d'un bord est
 * dessiné aussi à son emplacement "fantôme" de l'autre côté de l'écran
 * (coins : jusqu'à 4 instances). Le centre (cx, cy) est passé en
 * paramètre pour permettre le pattern erase à prev_pos puis draw à
 * curr_pos sans modifier la struct. Une instance fantôme entièrement
 * hors écran est rejetée à coût quasi nul par le clip du blit. */
static void asteroid_draw_one(const Asteroid *p, unsigned char cx, unsigned char cy)
{
    /* Rayon max conservateur : 14 (= rayon grand asteroid) */
    unsigned char id = (unsigned char)((p->size << 2) + p->shape);
    signed char dup_c = 0;
    int dup_r = 0;

    asteroid_blit_at(id, cx, cy, 0, 0);

    if (cx <= 14)        dup_c = +40;
    else if (cx >= 226)  dup_c = -40;
    if (cy <= 14)        dup_r = +200;
    else if (cy >= 186)  dup_r = -200;

    if (dup_c) asteroid_blit_at(id, cx, cy, dup_c, 0);
    if (dup_r) asteroid_blit_at(id, cx, cy, 0, dup_r);
    if (dup_c && dup_r) asteroid_blit_at(id, cx, cy, dup_c, dup_r);
}

/* Trace XOR à pos courante (x, y) pour chaque actif.
 * Met à jour prev_x/prev_y/drawn pour amorcer le pattern erase+draw
 * de la frame suivante (asteroids_render). Utilisé pour init / game_reset
 * / nettoyage : appel en paire (draw puis re-draw) = annule l'XOR. */
void asteroids_draw(void)
{
    register Asteroid *p;                  /* Phase 24 — anti-mul8x16 */
    unsigned char i;
    for (i = 0, p = asteroids; i < MAX_ASTEROIDS; i++, p++) {
        if (p->active) {
            asteroid_draw_one(p, p->x, p->y);
            p->prev_x = p->x;
            p->prev_y = p->y;
            p->drawn  = !p->drawn;  /* toggle XOR */
        }
    }
}

/* Pattern par-frame anti-flicker : pour chaque asteroid, erase à prev
 * (= pos en sortie de frame précédente) puis draw à curr consécutivement.
 * La fenêtre pendant laquelle l'astéroïde est éteint à l'écran tombe à
 * quelques centaines de cycles (les opérations entre les deux appels à
 * asteroid_draw_one), au lieu de plusieurs ms si le couple était dispersé.
 * Pré-conditions :
 *   - asteroids_update doit avoir tourné cette frame (sauve prev_x/y).
 *   - drawn = 1 ssi l'asteroid est actuellement tracé à (prev_x, prev_y).
 * Post-conditions :
 *   - chaque actif est tracé à (x, y), prev = (x, y), drawn = 1.
 *   - chaque inactif (juste désactivé par collision) est effacé,
 *     drawn = 0. */
void asteroids_render(void)
{
    register Asteroid *p;                  /* Phase 24 — anti-mul8x16 */
    unsigned char i;
    for (i = 0, p = asteroids; i < MAX_ASTEROIDS; i++, p++) {
        if (p->drawn) {
            asteroid_draw_one(p, p->prev_x, p->prev_y);
            p->drawn = 0;
        }
        if (p->active) {
            asteroid_draw_one(p, p->x, p->y);
            p->prev_x = p->x;
            p->prev_y = p->y;
            p->drawn  = 1;
        }
    }
}

/* 8.8 fixed-point pour asteroid velocities — rand_offset en scale ×64
 * (= 0.25 px/unit, port arcade inchangé), bornes clampées séparément :
 *   V_MAX_AST = 576 = 2.25 px/frame max
 *   V_MIN_AST = 128 = 0.50 px/frame min (asteroid ne peut pas s'arrêter)
 *
 * Tuning gameplay : l'échelle ×128 d'origine donnait des petits fragments
 * à 7.5 px/frame (écran traversé en 1.3 s à 25 Hz) — injouable. ×64
 * (Phase 10f) divisait par 2. Phase 37 : nouveau retour joueur après
 * validation des torpilles 2×2 — les fragments au clamp 960
 * (3.75 px/frame) restaient trop durs à viser à 25 Hz ; max abaissé à
 * 576 et min à 128 (l'offset RNG ±960 sature plus souvent le clamp,
 * la dispersion des directions reste intacte). */
#define V_MAX_AST    576
#define V_MIN_AST    128

/* RNG signé en 8.8 — port de SetAstVel ($7203) :
 *   AND #$8F garde sign + 4 bits magnitude, scale ×64.
 *
 * Revue senior C #1 : ancienne version utilisait `r |= 0xF0` sur
 * unsigned puis cast en signed — fonctionnel mais sémantiquement
 * fragile (relais à comportement cc65 quasi-implementation-defined).
 * Version courante : décale le bit 7 en sign-bit via cast direct,
 * conversion arithmétique explicite. */
static int rand_offset(void)
{
    /* AND $8F : bit 7 (signe) + bits 0-3 (magnitude 0..15) */
    signed char s = (signed char)(rng8() & 0x8F);
    /* Si bit 7 set, la valeur "raw" 0x80..0x8F doit être étendue à
     * 0xF0..0xFF (sign-extend des bits 4-6 manquants). Cast en signed
     * char direct + extension à int gère la sémantique correctement
     * sur compilateurs C89/C99 standards. cc65 implémente le cast
     * via or 0xF0 si bit 7 set (idem version précédente) mais sans
     * dépendre du détail d'impl. */
    if (s < 0) s |= (signed char)0xF0;   /* sign-extend bits 4-6 */
    return ((int)s) << 6;     /* ×64 = scale 8.8 (matches V_MAX/MIN_AST) */
}

/* Clamp arcade GetAstVelocity ($7233) en 8.8 fixed-point. */
static int clamp_vel(int v)
{
    if (v >  V_MAX_AST)        v =  V_MAX_AST;
    if (v < -V_MAX_AST)        v = -V_MAX_AST;
    if (v > 0 && v <  V_MIN_AST) v =  V_MIN_AST;
    if (v < 0 && v > -V_MIN_AST) v = -V_MIN_AST;
    return v;
}

/* Fragmentation arcade-fidèle — Phase 10f.
 * Port de BreakAsteroid ($75EC) + SplitAsteroid ($761D) + SetAstVel ($7203).
 *
 * 1. Petit détruit (size==0) : désactivation, fini.
 * 2. Sinon : taille -- en place, parent reste actif (slot conservé).
 * 3. SplitAsteroid : crée 2 nouveaux asteroids même taille à la même position.
 * 4. SetAstVel pour chaque enfant + parent : nouvelle_vel = parent_vel + RNG(-15..+15),
 *    clampé [-15,+15] avec |v|≥3.
 * Total : 1 parent réduit + 2 nouveaux = **3 fragments** par hit (vs 2 avant). */
void asteroids_fragment(unsigned char idx)
{
    register Asteroid *p = &asteroids[idx];   /* Phase 24 — 1 seul mul à l'entrée */
    register Asteroid *q;
    unsigned char child_size;
    unsigned char i, found, sh;
    int vx_p, vy_p;                  /* 8.8 fixed-point : int 16-bit */
    unsigned char ax, ay;

    if (!p->active) return;

    /* Phase 10k : reload AstBreakTimer (arcade $75EE = 80 frames à
     * 60 Hz ≈ 1,33 s).  Tout hit d'asteroid retarde le prochain spawn
     * saucer.  Fix « UFO ne spawn jamais » : la valeur 80 transposée
     * telle quelle à notre boucle 25 Hz donnait 3,2 s — un joueur actif
     * touche un asteroid plus souvent que ça, repoussant le saucer
     * indéfiniment.  33 frames à 25 Hz ≈ 1,32 s = durée arcade. */
    ast_break_timer = 33;

    /* Petit détruit complètement (cf. arcade : size devient 0, asteroid disparu).
     * Le tracé existant à prev_x/prev_y sera effacé par asteroids_render
     * (drawn=1, active=0 → erase puis drawn=0). */
    if (p->size == SIZE_SMALL) {
        p->active = 0;
        return;
    }

    /* L'astéroïde parent était tracé à prev_x/prev_y avec sa size/shape
     * actuelles (= attrs OLD). On va modifier size/shape ci-dessous, ce
     * qui changerait le shape utilisé par asteroids_render lors de l'erase.
     * Pour préserver l'invariant, on efface immédiatement le tracé OLD
     * avant de modifier les attrs. drawn=0 → render skip erase, draw à
     * x/y avec NEW attrs. */
    if (p->drawn) {
        asteroid_draw_one(p, p->prev_x, p->prev_y);
        p->drawn = 0;
    }

    /* Conserver les attributs du parent avant modification */
    ax   = p->x;
    ay   = p->y;
    vx_p = p->vx;
    vy_p = p->vy;
    sh   = p->shape;
    child_size = p->size - 1;

    /* 1. Parent : taille réduite, slot conservé, vélocité perturbée */
    p->size  = child_size;
    p->shape = (sh + 1) & 3;
    p->vx    = clamp_vel(vx_p + rand_offset());
    p->vy    = clamp_vel(vy_p + rand_offset());

    /* 2. Créer 2 NOUVEAUX asteroids dans des slots libres */
    found = 0;
    for (i = 0, q = asteroids; i < MAX_ASTEROIDS && found < 2; i++, q++) {
        if (q == p || q->active) continue;
        q->active = 1;
        q->drawn  = 0;            /* jamais tracés : skip erase au render */
        q->size   = child_size;
        q->shape  = (sh + 2 + found) & 3;
        q->x      = ax;
        q->y      = ay;
        q->prev_x = ax;
        q->prev_y = ay;
        q->x_frac = 0;
        q->y_frac = 0;
        q->vx     = clamp_vel(vx_p + rand_offset());
        q->vy     = clamp_vel(vy_p + rand_offset());
        found++;
    }
    /* Si pas assez de slots libres : pool plein, on accepte la perte
     * (arcade fait pareil via GetFreeAstSlot qui retourne BMI). */
}

unsigned char asteroids_count(void)
{
    register const Asteroid *p;            /* Phase 24 — anti-mul8x16 */
    unsigned char i, n = 0;
    for (i = 0, p = asteroids; i < MAX_ASTEROIDS; i++, p++) if (p->active) n++;
    return n;
}
