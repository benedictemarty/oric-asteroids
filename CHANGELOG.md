# Changelog

Toutes les modifications notables de ce projet sont documentées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
adhère à [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

À venir : Phase 3 (vaisseau rotatif + tirs + input clavier).

## [0.3.0] - 2026-05-09

### Phase 2 — Bresenham XOR + benchmark cycles ✅

**Résultats benchmark** : ~97 c/px (Phosphoric --profile, 1000×151 px,
estimation via répartition cycles code/ROM/infini). Objectif ≤18 c/px non
atteint ; 25 Hz acté comme cible nominale.

**Définition de fin validée :**
- `line.s` trace tout segment XOR en HIRES, bit6=1 maintenu.
- Bresenham idempotent (draw+erase = état initial).
- 10 scénarios géométriques versionnés (H, V, diagonales, obliques).
- Benchmark mesuré et décision Hz documentée.
- `make check` PASS.

### Added

- `src/asm/line.s` — Bresenham XOR 6502 complet :
  - Tables précalculées en RODATA (880 octets) : adresses lignes HIRES,
    colonnes et masques de pixels.
  - `_hires_init` : déclenchement mode HIRES ($1C → $BB80) + effacement
    HIRES avec 0x40 (correction bug : reload A=$40 dans boucle externe).
  - `_draw_line_xor` : normalisation sx=+1 (swap si lx0>lx1), init unique
    de l_ptr/l_mask via tables, mises à jour incrémentales dans la boucle
    (mask LSR sur pas x, ptr±40 sur pas y).
- `tests/ref/phase2_lines.ppm` — capture de référence Phase 2 (triangle
  + 10 lignes géométriques, 2273 pixels blancs).

### Changed

- `src/asm/crt0.s` — fix critique : init `c_sp` via symbole linker
  (était adresse hardcodée $80 = `_lx0`, causait stack C corrompu).
- `Makefile` — targets `bench` + `BENCH_PROF`, `BENCH_CYCLES` étendu.

### Fixed (bugs découverts et corrigés durant Phase 2)

- `crt0.s` : c_sp initialisé à $80/$81 (= _lx0/_ly0 en ZP) au lieu du
  vrai `c_sp` (à $8E avec notre layout ZP). Cause : appels de fonctions
  cc65 avec paramètres stack corrompus.
- `hires_init` : boucle externe `lda l_ptr+1` écrasait A=$40, pages
  $A1-$BE remplies avec leur numéro de page au lieu de $40.
- `_draw_line_xor` : `bpl` (bit de signe) utilisé pour comparer dx/dy
  pouvant dépasser 127. Remplacé par `bcs/bcc` (carry, comparaison
  unsigned). Symptôme : sy=-1 pour des lignes descendantes.

### Décision Hz

**25 Hz acté comme cible nominale.** À 97 c/px :
- 15 seg × 15 px × 97 c/px × 2 = 43 650 cycles ≈ budget 25 Hz (40 000)
- 30 seg × 20 px × 97 c/px × 2 = 116 400 cycles >> budget

Scènes de jeu simplifiées (≤15 segments, ≤15 px/segment) pour la cible
25 Hz. SMC + déroulage (Phase 2b) nécessaires pour ≥30 segments à 25 Hz
ou toute cible 50 Hz.

## [0.2.0] - 2026-05-09

### Phase 1 — Squelette sans OSDK + HIRES + triangle statique ✅

**Définition de fin validée** : `make check` passe, 177 pixels blancs détectés,
triangle visible autour de (120, 28) sur Phosphoric HIRES 240×200.

### Added

- `cfg/oric1.cfg` — configuration linker ld65 pour Oric-1 (RAM à $0500,
  ZP à $80, pile C à $9FFF).
- `src/asm/crt0.s` — startup bare-metal cc65 / ld65 (stack matériel + logiciel,
  effacement BSS, appel `_main`, boucle infinie au retour).
- `src/main.c` — init HIRES + triangle statique (vaisseau Asteroids).
- `Makefile` — build cc65+ld65+bin2tap, sans OSDK ; cibles `all`, `run`,
  `test`, `ref`, `check`, `clean`.
- `tests/ref/phase1_triangle.ppm` — capture de référence Phosphoric headless
  (6M cycles, CALL 1280 à 3,5M cycles).

### Changed

- Correction guide §2 et CLAUDE.md HIRES : discriminateur réel
  `(byte & 0x60) == 0` (bits 6 ET 5 à 0 = attribut), bit 7 = inverse vidéo,
  init HIRES = `0x40` (jamais `0x80` qui serait classé attribut).

### Technical notes

- Chaîne sans OSDK : `cc65 -t none` → `ca65` → `ld65 -C cfg/oric1.cfg`
  → `bin2tap` (outil Phosphoric).
- Fast load Phosphoric : injection différée à ~3M cycles (après RAM test
  BASIC 1.0) ; exécution via `--type-keys 3500000:CALL 1280\n`.
- HIRES init : `0x1C` à `$BB80` (vid_mode=4, persiste), HIRES rempli `0x40`.

## [0.1.0] - 2026-05-09

Phase de cadrage technique. Aucun code livré, uniquement la documentation
d'architecture, le plan d'attaque et les fichiers de gouvernance projet.

### Added

- `asteroids-oric1-48k-guide.md` — guide de développement complet :
  cible matérielle Oric‑1 48 Ko, chaîne d'outils OSDK + Phosphoric,
  architecture logicielle, étapes de développement, contraintes
  spécifiques, stratégie de test 3 couches, références primaires
  (6502disassembly.com rev 4 + Nick Mikstas), roadmap optimiste/réaliste.
- `CLAUDE.md` — directives pour les futures instances Claude Code,
  alignées sur le guide.
- `.gitignore` — exclusion des artefacts OSDK, captures Phosphoric,
  dumps de debug.
- `CHANGELOG.md` — ce fichier.
- `ROADMAP.md` — feuille de route projet, miroir du §10 du guide.

### Décisions techniques actées

- **Cible jeu** : clone fidèle d'*Asteroids* arcade Atari (1979),
  CPU 6502 commun avec Oric ⇒ logique de jeu réutilisable au niveau
  code depuis le désassemblage rev 4.
- **Mode HIRES Oric** : bit 7 = discriminateur pixel/attribut,
  attributs figés au boot pour rendu fil de fer monochrome PAPER 0 / INK 7.
- **Émulateur projet** : Phosphoric (`/home/bmarty/Oric1/oric1-emu`).
  Mode `--headless` natif pour CI, `--screenshot` pour diff bit‑à‑bit,
  `--type-keys` pour input scripté, `--profile` pour benchmark cycles.
  Oricutron exclu.
- **Cible nominale 25 Hz** (frame‑skip 1/2). 50 Hz objectif de polish
  conditionné par SMC + boucle déroulée + ≤ 8 segments par astéroïde.
- **Politique C vs ASM** : tout ce qui est appelé > 1×/frame en asm
  (line drawer, rotation, intégration 8.8, collisions, lecture clavier,
  player AY). C uniquement pour boucle de jeu, machine à états,
  écrans, IA soucoupe.
- **Wraparound** : par duplication d'instance, pas de clipping.
  La routine `line.s` peut supposer coords ∈ `[0,239] × [0,199]`.
- **Self‑modifying code** assumé pour la routine de ligne.
- **Synchro vidéo** : vrai signal VSync ULA en cible finale ;
  Timer 1 du VIA en repli intermédiaire.
- **AY‑3‑8912** non mappé en mémoire ; piloté via VIA 6522
  (port A données + lignes BC1/BDIR contrôle). Effets à recréer
  fonctionnellement (l'arcade utilisait des oscillateurs analogiques
  discrets, pas de tables transposables).
- **Format livré** : `.tap` prioritaire (cible Oric‑1 stock,
  pas de driver disque résident). `.dsk` cible secondaire.
- **Stratégie de test** : (1) host x86 pour routines portables,
  (2) Phosphoric headless + diff bit‑à‑bit pour non‑régression,
  (3) assertions debug compilées out en release.

### Considéré et écarté

- **Mine Storm Vectrex** comme jeu source : envisagé pour son
  identité (13 niveaux, mines magnétiques/fireball) mais écarté
  car CPU d'origine 6809 ⇒ aucune réutilisation de code 6502
  possible, et perte de l'argument central du projet.

### Gouvernance

- Dépôt git initialisé, identité locale `bmarty <bmarty@mailo.com>`.
- Pas encore de remote configurée.
