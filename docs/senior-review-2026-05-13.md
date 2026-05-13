# Revue senior — Asteroids Oric-1 (2026-05-13)

**Reviewer** : développeur senior spécialiste Oric-1 / 6502 / cc65 (sous-agent dédié)
**Base** : commit `9b68540` (post Phase 2b SMC), 96 commits, phases 1-19 ✅

---

## ★★★ Problèmes architecturaux critiques

1. **BSS clear désactivé en crt0 — workaround non investigué** (`src/asm/crt0.s:24` + `src/game.c:786-800`)
   Le commentaire roadmap Phase 6 indique que `initlib` plante quand `__BSS_SIZE__ ≥ $83` ; le « fix » est de réinitialiser explicitement chaque static dans `game_run`. Cause racine ignorée → bombe à retardement dès qu'un nouveau module ajoute du BSS non initialisé (déjà visible dans `ufo.c` : `ufo_active`, `ufo_x/y`, `ufo_bullet_*` ne sont JAMAIS clear avant `ufo_init`, donc valeur RAM hasardeuse lue dans `game_reset()` ligne 749 si on entre par un chemin imprévu). *Fix :* dumper la map BSS, ré-isoler la régression réelle d'`initlib` (probable conflit ZP `c_sp` vs symbole `zerobss` ou taille de `__BSS_LOAD__/__BSS_RUN__` non définis dans `cfg/oric1.cfg`).

2. **Pas de segment BSS distinct dans `cfg/oric1.cfg`** (`cfg/oric1.cfg:21`)
   `BSS: load = RAM, type = bss, define = yes, optional = yes` mais BSS partage la zone RAM avec CODE/RODATA/DATA dans l'ordre du linker → l'image binaire émise inclut potentiellement BSS, ce qui (a) gonfle le `.tap`, (b) explique très probablement le bug Phase 6 (la routine `zerobss` cc65 utilise `__BSS_LOAD__`/`__BSS_SIZE__`, et ici le `LOAD` est défini sur RAM donc `_LOAD_ == _RUN_`). *Fix :* séparer `BSS: type = bss, define = yes, optional = yes;` sans `load`, ou utiliser un MEMORY `BSS` distinct ; vérifier `__BSS_LOAD__ == __BSS_RUN__`.

3. **`game.c` monolithique de 1161 lignes** (`src/game.c:1`)
   Mêle boucle principale, hyperespace, debris ship, debris asteroids, hiscores, séquence game-over, audio scheduling, wave label, écran titre. Forte dette de couplage : 22 statics fichier, comments-as-fix, code mort potentiel. *Fix :* extraire `debris.c`, `hiscores.c`, `gameover.c` (séquence machine à états explicite).

4. **`bullets_render`, `asteroids_render` etc. mélangent erase et draw via XOR sans contrat formel** (`src/asteroids.c:288-328`, `src/game.c:934-1047`)
   Le pattern « `*_was_drawn` flag + `drawn` per-entity + retoggles silencieux » a déjà produit 3 bugs (Phase 9g, 18b, 18c, 18h). La sémantique XOR exige un invariant écrit, pas inférable : aucune assertion ne vérifie que `drawn` reste cohérent avec l'écran. *Fix :* macro `ASSERT_DBG` (déjà prévue dans CLAUDE.md §7-3 mais absente) + invariant doc en haut de chaque `*_render`.

5. **Aucun test host (x86) pour la logique portable** (`tests/` ne contient que `ref/` et `scenarios/` vides)
   La stratégie §7 du CLAUDE.md prévoyait 3 couches (host, Phosphoric headless, assertions debug). Seul `make check` = capture PPM bit-à-bit existe. Une régression de `clamp_vel`, `rng8`, `hiscores_insert` ne se révèle que par playtest. *Fix :* harness Python ou C90 portable autour de `rng8`, `clamp_vel`, `hiscores_insert`, `rand_offset` (signed 8.8 cast — `r |= 0xF0` est sémantiquement faux sur unsigned : porte-bonheur sur cc65).

## ★★ Optimisations CPU 6502 prioritaires

1. **`_psg_write` est un `jsr` à 12 instructions appelé en cascade** (`src/asm/sound.s:144-165`, ~7 jsr lors d'un `_sound_play_fx`)
   À ~80 cycles par écriture PSG × 7-9 registres × ~3 FX/s + sweeps, ça monte vite. *Fix :* inliner via macro `.macro PSG_WRITE val,reg` dans les chemins de chaque FX. Gain estimé : ~400-600 c/frame quand un FX se déclenche.

2. **`_draw_line_xor` : `jmp @h_loop` après step x quand pas de newcol** (`src/asm/line.s:391-399`)
   Saut absolu 3c là où une chute serait gratuite ; structure pareille en V (ligne 430). Refactor en plaçant `@h_loop:` directement après le step-x pour `bcs @newcol; (fallthrough)`. Gain estimé : 3 c/px × ~80 % des pixels = ~2-3 % perf.

3. **`compute_verts` (`src/asm/ship.s:86-138`) recalcule 5 sommets via 5 paires `lda ptN,x ; clc ; adc _ship_x`**
   Le `clc` n'est nécessaire qu'une fois par axe (l'addition unsigned ne déborde pas sur des centres > 5px du bord, ce qui est exigé sinon → comportement non défini). *Fix :* hoister `clc` ou utiliser `adc` sans `clc` quand on a la garantie. Gain marginal mais 5×2 = 10c, et `compute_verts` tourne 2×/frame (erase+draw).

4. **`draw_five_lines` charge `sh_tx0..ty4` huit fois redondantes** (`src/asm/ship.s:148-202`)
   Cinq segments partagent des endpoints (P0, P3, P4 chacun pris 3×). Stocker `_lx0/_ly0/_lx1/_ly1` puis dérouler avec « le `lx1/ly1` du segment N devient `lx0/ly0` du segment N+1 sans recharge » sauve 4 segments × 4 sta = 16 cycles × 2 (erase+draw). *Fix :* pipeline de segments avec endpoints chaînés.

5. **`asteroid_draw_at` (`src/asteroids.c:210-256`) appelle deux boucles : N segments + N plot_dot**
   Pour 6 asteroids × ~6 sommets × 2 (erase+draw) = ~72 `plot_dot` extra par frame. Gain : remplacer le replot par une stratégie « le premier segment du polygone re-XOR son endpoint de départ » (le polygone est déjà fermé). Gain estimé : ~72 × 40c = ~2900 c/frame (~15 % du budget 25 Hz).

## ★ Qualité C / cc65

1. **`rand_offset` (`src/asteroids.c:339-346`) : extension de signe via `r |= 0xF0` sur unsigned puis cast — fonctionne mais sémantiquement fragile.** *Fix :* `s = (signed char)(r | (r & 0x80 ? 0xF0 : 0x00))` ou `(signed char)(r << 1) >> 1`. Probable warning si optim cc65 changée.

2. **`#pragma zpsym` dupliqué dans chaque .c** (`asteroids.c:15-19`, `ufo.c:30-34`, `hud.c:12-16`, `game.c:39-43`, `title.c:11-15`)
   `lx0/ly0/lx1/ly1` redéclarés 5×. *Fix :* exposer via un `line.h` partagé. Risque actuel : si on déplace un symbole hors ZP, on doit toucher 5 endroits.

3. **Pas de `#pragma static-locals(on)` global** : chaque fonction C alloue ses locaux sur le c-stack cc65 (très lent). `ship_update`, `bullets_update`, `asteroids_update` sont appelées chaque frame avec plusieurs `int` locaux → ~100c gaspillés par appel. *Fix :* `--static-locals` dans cflags Makefile (ligne 97).

4. **`hud_xor_5digits` boucle de soustractions décimales** (`hud.c:91-104`)
   Pas optimal vs table de puissances de 10 ou DAA, mais surtout : score 16-bit, donc `t -= 10000U` peut tourner jusqu'à 6 fois → en wrap-around 65535 ≈ 6-7 sous. Acceptable, mais le compilateur cc65 émet une boucle non triviale sur `unsigned int`. *Fix :* table `{10000, 1000, 100, 10}` + helper.

5. **`Makefile` n'a pas `-Cl` / `--static-locals` / `-Or -W` / `--codesize 200` dans `$(CC65)`** (Makefile:97, 102, etc.)
   Seul `-O` est passé. *Fix :* `CFLAGS = -O -Or -Cl --register-vars -t none -I src` typique en cc65 pour gagner ~5-15 % de taille code + vitesse.

## Cohérence Oric / CLAUDE.md

1. **`ROADMAP.md:304-311` mentionne `oricutron` comme cible de validation finale** alors que `CLAUDE.md:22` dit explicitement « **Ne jamais utiliser `oricutron`** — Phosphoric est l'émulateur du projet ». *Fix :* retirer l'item oricutron de la checklist finale ou requalifier en « test optionnel tierce machine ».

2. **`crt0.s:34` fait `jmp $F800` pour sortir** — c'est bien le vecteur RESET ROM **Oric-1** (Atmos = `$F88F`). OK mais non commenté comme « Oric-1 only » ; doit être renommé ou conditionnel si futur port Atmos.

3. **Wraparound bullet asymétrique** (`game.c:543-546` : x ∈ [0, 238], `BLT_X_SPAN = 239`) et asteroid (`asteroids.c:147` : x ∈ [0, 239], span 240) — deux conventions co-existent. CLAUDE.md exige `[0, 239] × [0, 199]` pour `line.s`. Pas un bug actuel (clip ailleurs) mais source de confusion.

4. **`ship_x_frac`/`ship_y_frac` en ZP mais accédés depuis C** (`ship.s:38`, `game.c:30-33`)
   Acceptable, mais ZP a 0x50 octets (`oric1.cfg:10`) et beaucoup déjà consommée par `_lx0`, `_ly0`, `_lx1`, `_ly1`, `_ship_*`, `kb_*`, `_sfx_*`, `sound_tmp`, `cs_src`, `l_*` (Bresenham seul ≈ 14 octets). Pas de carte ZP versionnée. *Fix :* commenter `bsslist.s` ou table récap dans `cfg/oric1.cfg`.

5. **`hires_init` clear la zone TEXT `$BF68-$BFDF` avec espaces `$20`** (`line.s:156-162`)
   `$20` a bits 5-0 partiellement allumés → si l'ULA passe en mode TEXT (sortie BASIC, ESC), aucun souci. OK techniquement mais à documenter : si on garde la ROM ULA, écrire `$20` (espace TEXT) là est cohérent avec `CLAUDE.md` §2.

## Risques portabilité

1. **Identifié des dépendances ROM BASIC 1.0** : `cs_src` copie `$B400 → $9800` (`line.s:92-110`) suppose le charset ROM Oric-1 à `$B400`. **Atmos a un charset différent** (formes lettres différentes, même offset mais glyphs distincts) → si binaire chargé sur Atmos, le bas d'écran TEXT affichera des glyphs Atmos style, pas Oric-1 (cosmétique).

2. **`key_scan` (`input.s:124-280`) écrit DDRA = `$FF` et restaure** — bon réflexe, mais ne sauvegarde pas `VIA_ORB[0:2]` avant de forcer col=4 puis col=1 pour ESC. La ROM IRQ Atmos/Oric-1 1.1 pourrait s'attendre à retrouver son état après. SEI/CLI protègent partiellement mais si CLI réactive avant que la ROM ait son tour, la prochaine IRQ trouve ORB modifié. Pas observé, mais fragile.

3. **`jmp $F800` au retour** (`crt0.s:34`) → reset complet ROM. Oric-1 BASIC 1.0 et 1.1 OK ; **Atmos = `$F88F` ou `$FAA5`**. À conditionner si jamais on packe un binaire Atmos.

4. **`.tap` produit par `bin2tap`** (`Makefile:136-137`) — déjà documenté comme « non standard » dans `ROADMAP.md:308-311` (oricutron plante au CLOAD). Risque réel pour distribution : tap2dsk, Euphoric ou un Oric-1 physique peuvent rejeter. *Fix :* préfixer le `.tap` avec 4 octets `$00` + en-tête conforme `Manuel Oric-1` ou utiliser `bin2tap` OSDK officiel.

5. **Hardware variant 16K vs 48K** : la config linker (`oric1.cfg:11`) place RAM à `$0500-$9F00` (38 Ko utilisés) ; le binaire dépasse 16 Ko probablement (1161 lignes C + sound.s 798 lignes + line.s 455 = ~12-18 Ko à vue de nez). Donc 48K strict. À documenter dans README.

## Verdict global

Projet **mature pour son scope** (96 commits, 19 phases, gameplay complet, son arcade fidèle, performances optimisées Phase 16-18). Architecture clavier/PSG/HIRES solide et documentée. Dette principale **structurelle plus que technique** : `game.c` ingérable, BSS workaround non résolu (faille latente), absence de tests host (régression silencieuse possible sur `rand_offset`/`hiscores_insert`/`clamp_vel`), incohérences ZP/wraparound mineures. Priorités pour clôturer le projet proprement :

1. Diagnostiquer le bug BSS (probablement résolu en séparant le `MEMORY` BSS du `MEMORY` RAM dans `oric1.cfg`).
2. Refactor split `game.c` en 4 modules cohérents.
3. Mettre en place le harness host x86 prévu (CLAUDE.md §7-1) — non négociable pour les routines arithmétiques signées 8.8.
4. Aligner `ROADMAP.md` checklist finale sur `CLAUDE.md` (oricutron à retirer ou requalifier).
5. Optims line.s/ship.s/psg_write valent un dernier gain ~5-8 %, suffisant pour repasser à 50 Hz sur des frames calmes (≤ 3 asteroids).

Le code dans `src/asm/` est de qualité production. Le code C, fonctionnel, gagnerait à passer en `--static-locals` et à externaliser ses statics.
