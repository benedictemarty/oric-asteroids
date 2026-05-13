# Demande à l'équipe Phosphoric — aide debug IRQ T1 user vector

**Émetteur** : bmarty <bmarty@mailo.com>
**Date** : 2026-05-13
**Projet** : Asteroids Oric‑1 48 Ko (C + ASM cc65)
**Phosphoric** : `v1.16.11-alpha`
**Repo** : `/home/bmarty/Oric asteroids` (commit `8af36cd`)

---

## 1. Contexte

Je porte un clone d'Asteroids arcade sur Oric‑1 (cible nominale, BASIC 1.0).
Le projet est à v1.2.10, jouable, et le player AY actuel tourne en mode
polling (`sound_tick()` appelé une fois par frame dans le main loop, 25 Hz
nominal).

Sprint en cours : déplacer `sound_tick()` sous **IRQ Timer 1** (50 Hz)
pour libérer du temps CPU dans la boucle de jeu et fiabiliser le timing
du player AY (jitter-free).

## 2. Ce que j'ai tenté

**Plan** :
1. Programmer T1 en free-run @ 20 ms (déjà fait, déjà testé OK en
   polling IFR).
2. Activer T1 IRQ via `VIA_IER = $C0`.
3. Hooker le user IRQ vector :
   - Oric‑1 : `$0228` ← `JMP _irq_handler` (3 octets)
   - Atmos : `$0244` ← `JMP _irq_handler` (3 octets)
   - Je patche les deux pour portabilité Oric‑1 / Atmos.
4. Handler asm minimal : check IFR bit 6 (T1), `LDA T1CL` pour clear,
   `JSR _sound_tick`, `INC frame_cnt`, puis `PLA / TAY / PLA / TAX /
   PLA / RTI` (la ROM a déjà fait PHA A/X/Y avant `JMP ($0228)`).
5. `frame_wait()` en C poll `frame_cnt` au lieu d'IFR.

**Code asm du handler** (sound.s) :
```asm
_irq_handler:
        lda  VIA_IFR             ; $030D
        and  #$40                ; T1 flag ?
        beq  @not_t1
        lda  VIA_T1CL            ; $0304 — clear T1 flag
        jsr  _sound_tick
        inc  _frame_cnt
@not_t1:
        pla
        tay
        pla
        tax
        pla
        rti

_irq_install:
        sei
        lda  #$4C                ; JMP opcode
        sta  $0228               ; Oric-1
        lda  #<_irq_handler
        sta  $0229
        lda  #>_irq_handler
        sta  $022A
        lda  #$4C                ; idem $0244 Atmos
        sta  $0244
        lda  #<_irq_handler
        sta  $0245
        lda  #>_irq_handler
        sta  $0246
        lda  #$C0                ; IER : bit 7=1 set, bit 6=1 T1
        sta  $030E
        lda  $0304               ; clear T1 flag initial
        lda  #0
        sta  _frame_cnt
        cli
        rts
```

**Modif game.c** :
```c
static void frame_wait(void) {
    unsigned char start = frame_cnt;
    while ((unsigned char)(frame_cnt - start) < VSYNCS_PER_FRAME) { }
}

/* dans main() */
hires_init();
timer_init();
sound_init();
irq_install();           /* nouveau */
title_draw();
...
```

## 3. Symptôme

**Le jeu se bloque sur l'écran titre en cours de tracé.**

Test Phosphoric headless :
```bash
oric1-emu -m oric1 -r basic10.rom -t build/asteroids.tap -f -n \
  -c 20000000 --screenshot /tmp/test.ppm
```

À 10 M cycles : écran affiche "ASTEI" (5 lettres au lieu des 9 de
"ASTEROIDS"). À 20 M cycles : **identique** (115 pixels blancs, même PC
zone). Donc le programme est **bloqué** quelque part — vraisemblablement
dans `frame_wait()` à attendre un `frame_cnt` qui ne s'incrémente jamais.

CPU final state à 20 M : `PC=$444E`, `A=11 X=9F Y=FF SP=FC P=N.-..I.C`.
Le programme tourne (PC dans la zone code Asteroids `$0500-$9A22`) mais
n'avance pas dans le main loop.

Comportement avant ce sprint (commit `c6fa09a`) : à 10 M cycles, écran
titre "ASTEROIDS" complet + 2 astéroïdes — donc le polling IFR
fonctionne, c'est uniquement le passage en IRQ qui pose problème.

## 4. Questions techniques

### Q1 — Dispatch IRQ user vector Oric‑1 BASIC 1.0

Phosphoric `--rom-info` rapporte :
```
RESET: $F42D
IRQ:   $0228
NMI:   $022B
```

**Question** : la ROM BASIC 1.0 fait-elle bien un `JMP ($0228)` (indirect)
ou un `JMP $0228` (direct, donc on doit y placer un opcode exécutable
type `4C XX YY`) ?

Mon implémentation suppose `JMP $0228` direct (= la ROM exécute le code
à $0228) et j'y place `4C XX YY` (JMP _irq_handler). Est-ce correct
pour BASIC 1.0 ?

### Q2 — Save/restore registres par la ROM avant `$0228`

Mon handler suppose que la ROM IRQ a déjà fait `PHA / TXA PHA / TYA
PHA` avant de céder la main à `$0228`. Donc je termine par `PLA / TAY /
PLA / TAX / PLA / RTI`.

**Question** : ce contrat est-il correct pour la ROM Oric‑1 BASIC 1.0 ?
Et pour Atmos BASIC 1.1 (où le vecteur est `$0244`) ?

### Q3 — Comportement multi-source IRQ

La ROM utilise-t-elle elle-même des sources IRQ VIA (T2 ? CA1 ?) pour
son scan clavier ou son clock interne, qui transiteraient aussi par
`$0228` ? Si oui, ignorer les non-T1 dans notre handler (BEQ @not_t1)
peut-il poser problème ?

### Q4 — Outillage Phosphoric pour debug IRQ

J'ai débuggé jusqu'ici via screenshots à différents cycles (`--screenshot
-at C:FILE`). Pour ce genre de bug IRQ, ce n'est pas suffisant.

**Demandes feature** (par ordre d'utilité) :
1. **Trace IRQ** : option `--trace-irq FILE` qui logge chaque IRQ
   servie : PC d'entrée, source (IFR snapshot), PC de retour.
2. **Dump RAM à un cycle** : option `--dump-ram-at C:FILE` similaire
   au screenshot, mais pour la RAM zero page ou une plage donnée.
   Permettrait de vérifier `frame_cnt` (ZP) à différents instants.
3. **Breakpoint conditionnel cycle-based** : `-b ADDR --break-at-cycle
   C` ou similaire, pour s'arrêter au premier `PC=$xxxx` *après* C
   cycles.
4. Mode `-D` interactif : commande pour dumper la ZP ou une plage RAM
   pendant la pause debugger.

## 5. Reproductibilité

Le code complet est dans `/home/bmarty/Oric asteroids` (repo git local,
non poussé). Branche `main`, commit base `8af36cd`. Le sprint IRQ
(actuellement reverté) est disponible dans l'historique du shell ou
peut être ré-appliqué — diff disponible sur demande.

Le `.tap` qui reproduit le bug peut être généré par `make all` après
ré-application du diff sprint.

## 6. Contacts

- Phosphoric : `/home/bmarty/Oric1/oric1-emu` (v1.16.11-alpha)
- Asteroids : `/home/bmarty/Oric asteroids` (HEAD `8af36cd`)
- bmarty <bmarty@mailo.com>

---

**Merci d'avance pour la doc/aide sur le dispatch IRQ ROM Oric‑1 et,
si possible, pour l'amélioration de l'outillage de trace IRQ.**
