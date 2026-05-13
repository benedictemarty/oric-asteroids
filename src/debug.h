/*
 * debug.h — assertions de debug pour la logique de jeu.
 *
 * Spec CLAUDE.md §7-3 : "macro ASSERT_DBG(cond) qui émet BRK ou code
 * d'erreur HIRES en build debug, compile en zéro octet en release."
 *
 * Mode debug   : ASSERT_DBG(cond) trappe à BRK si cond == 0
 *                (l'émulateur Phosphoric reporte alors PC + état CPU).
 * Mode release : compile en aucune instruction.
 *
 * Activation : passer `-DDEBUG=1` aux CFLAGS Makefile (par défaut DEBUG=0).
 *
 * Usage type :
 *   ASSERT_DBG(idx < MAX_ASTEROIDS);
 *   ASSERT_DBG(ship_was_drawn == 0 || ship_was_drawn == 1);
 *
 * Coût : 0 cycles en release.
 *        En debug : 3 octets (lda + 2 bytes condition) + 2 octets (bne + brk).
 *        À utiliser pour invariants critiques, pas en hot path.
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG

/* Émet un BRK ($00) si la condition est fausse. L'émulateur Phosphoric
 * intercepte le BRK et affiche le PC + état CPU dans la console. */
#define ASSERT_DBG(cond)                                  \
    do { if (!(cond)) __asm__ ("brk"); } while (0)

/* Variante : panique avec un code d'erreur arbitraire (1..255) écrit à
 * $0000 avant le BRK, lisible dans le crash dump. */
#define PANIC_DBG(code)                                   \
    do { __asm__ ("lda #%b", (code));                     \
         __asm__ ("sta $00");                             \
         __asm__ ("brk"); } while (0)

#else  /* !DEBUG : release build, zéro octet */

#define ASSERT_DBG(cond) ((void)0)
#define PANIC_DBG(code)  ((void)0)

#endif

#endif /* DEBUG_H */
