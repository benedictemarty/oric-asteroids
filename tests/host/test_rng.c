/*
 * test_rng.c — test host C90 portable du RNG du jeu.
 *
 * Cible le LFSR 8-bit Galois utilisé dans src/asteroids.c rng8() et
 * la fonction signée rand_offset(). Compile sur gcc/clang x86, tourne
 * en quelques µs, peut être branché en CI.
 *
 * Build : gcc -Wall -Wextra -std=c90 -O2 tests/host/test_rng.c -o test_rng
 * Run   : ./test_rng
 *
 * Vérifie :
 *   T1 — déterminisme : seed identique ⇒ séquence identique.
 *   T2 — période : LFSR Galois 8-bit non nul a période 255.
 *   T3 — bornes rand_offset : sortie ∈ [-16*128, +15*128] = [-2048, +1920].
 *        Asymétrie : (signed char)0x80 sign-extend en -16 (pas -15) car le
 *        AND #$8F sur bit 7 + bits 0-3 produit magnitude 0..15 ou -16..-1.
 *   T4 — distribution rand_offset : bit signe ~50/50 sur 1024 tirages.
 */

#include <stdio.h>

/* ── Copies portables des fonctions à tester ─────────────────────── */

static unsigned char rng_state;

/* Identique à src/asteroids.c:48-54 */
static unsigned char rng8(void)
{
    unsigned char lsb = rng_state & 1;
    rng_state >>= 1;
    if (lsb) rng_state ^= 0xB8;
    return rng_state;
}

/* Identique à src/asteroids.c:332-346 (version révisée batch 5) */
static int rand_offset(void)
{
    signed char s = (signed char)(rng8() & 0x8F);
    if (s < 0) s |= (signed char)0xF0;
    return ((int)s) << 7;
}

/* ── Tests ───────────────────────────────────────────────────────── */

static int test_determinism(void)
{
    unsigned char seed = 0x42;
    unsigned char seq1[16], seq2[16];
    int i;
    rng_state = seed;
    for (i = 0; i < 16; i++) seq1[i] = rng8();
    rng_state = seed;
    for (i = 0; i < 16; i++) seq2[i] = rng8();
    for (i = 0; i < 16; i++) {
        if (seq1[i] != seq2[i]) {
            printf("FAIL T1 : seq1[%d]=$%02X != seq2[%d]=$%02X\n",
                   i, seq1[i], i, seq2[i]);
            return 0;
        }
    }
    return 1;
}

static int test_period(void)
{
    unsigned char seed = 0x42;
    int count = 0;
    rng_state = seed;
    do {
        rng8();
        count++;
        if (count > 256) {
            printf("FAIL T2 : période > 256 (état piégé ou seed = 0 ?)\n");
            return 0;
        }
    } while (rng_state != seed);
    if (count != 255) {
        printf("FAIL T2 : période = %d, attendue = 255 (Galois LFSR 8-bit)\n", count);
        return 0;
    }
    return 1;
}

static int test_rand_offset_bounds(void)
{
    int i;
    rng_state = 0x42;
    for (i = 0; i < 4096; i++) {
        int v = rand_offset();
        if (v < -16 * 128 || v > 15 * 128) {
            printf("FAIL T3 : rand_offset()=%d hors [-2048, +1920]\n", v);
            return 0;
        }
    }
    return 1;
}

static int test_rand_offset_dist(void)
{
    int i, n_pos = 0, n_neg = 0, n_zero = 0;
    rng_state = 0x42;
    for (i = 0; i < 1024; i++) {
        int v = rand_offset();
        if (v > 0) n_pos++;
        else if (v < 0) n_neg++;
        else n_zero++;
    }
    /* Tolérance large : LFSR pas un vrai RNG, mais bit signe doit
     * être ~équilibré (±20 % sur 1024 tirages). */
    if (n_pos < 350 || n_pos > 650 || n_neg < 350 || n_neg > 650) {
        printf("FAIL T4 : distribution déséquilibrée pos=%d neg=%d zero=%d\n",
               n_pos, n_neg, n_zero);
        return 0;
    }
    return 1;
}

int main(void)
{
    int passed = 0, total = 4;
    printf("test_rng (host C90, src/asteroids.c rng8/rand_offset)\n");
    printf("  T1 déterminisme        : ");
    if (test_determinism())          { printf("PASS\n"); passed++; }
    printf("  T2 période LFSR 255    : ");
    if (test_period())               { printf("PASS\n"); passed++; }
    printf("  T3 rand_offset bornes  : ");
    if (test_rand_offset_bounds())   { printf("PASS\n"); passed++; }
    printf("  T4 rand_offset distrib : ");
    if (test_rand_offset_dist())     { printf("PASS\n"); passed++; }
    printf("Total : %d/%d\n", passed, total);
    return (passed == total) ? 0 : 1;
}
