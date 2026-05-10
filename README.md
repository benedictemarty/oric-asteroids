# Asteroids — portage Oric-1 48 Ko

Clone fidèle d'**Asteroids arcade** (Atari, 1979) pour **Oric-1 48 Ko**,
développé en C + assembleur 6502.

## Build

Prérequis :
- `cc65` v2.19+ (cc65 + ca65 + ld65)
- Python 3 (pour les générateurs de tables)
- [Phosphoric](https://github.com/benedictemarty/phosphoric) (émulateur Oric-1)
  installé dans `/home/bmarty/Oric1` (ou ajuster `Makefile`)

```
make            # produit build/asteroids.tap
make run        # charge le .tap dans Phosphoric et lance le jeu
make test       # capture headless (tests/out/phase9_release.ppm)
make ref        # met à jour la capture de référence
make check      # vérifie que la capture courante == la référence
make clean      # nettoie build/ et tests/out/
```

Cibles auxiliaires :
- `make gen_ship`   → régénère `src/asm/ship_verts.s` (32 angles)
- `make gen_shapes` → régénère `src/asm/shapes.s` (4 formes × 3 tailles)

## Touches

| Touche  | Action                              |
|---------|-------------------------------------|
| `←`     | Rotation gauche                     |
| `→`     | Rotation droite                     |
| `↑`     | Thrust (poussée avant)              |
| `↓`     | Hyperespace (téléportation, 25% mort) |
| `SPACE` | Tir / Restart en game over          |

Toutes les touches utilisent un **scan VIA direct** (`PSG reg 14 = $EF`,
PB3) — pas de dépendance au buffer ROM Atmos `$0265`.

## Architecture

- **Mode HIRES** monochrome (240 × 200, `PAPER 0 / INK 7`).
- **Bresenham XOR** pour le tracé vectoriel (`src/asm/line.s`,
  ~97 cycles/pixel sur Phosphoric).
- **Boucle 25 Hz** synchronisée sur le VSync ULA (CB1, IFR bit 4).
- **Logique de jeu** transposée du désassemblage Atari arcade rev 4
  (CPU 6502 commun avec l'Oric → réutilisation directe possible).

Voir [`asteroids-oric1-48k-guide.md`](./asteroids-oric1-48k-guide.md) pour
le cadrage détaillé et [`ROADMAP.md`](./ROADMAP.md) pour l'historique
des 9 phases.

## Phases livrées

| Phase | Livrable | Tag |
|------:|----------|-----|
| 1 | Squelette HIRES + triangle statique | `phase1-done` |
| 2 | Bresenham XOR (97 c/px) | `phase2-done` |
| 3 | Vaisseau rotatif 32 angles + tirs + scan clavier | `phase3-done` |
| 4 | Astéroïdes (4 formes × 3 tailles, fragmentation) | `phase4-done` |
| 5 | Collisions + score 7-segments + vies + HUD | `phase5-done` |
| 6 | Soucoupe grande/petite + IA tir | `phase6-done` |
| 7 | Hyperespace + restart + high scores top 5 | `phase7-done` |
| 8 | Son AY-3-8912 (tir + explosion + thump cadencé) | `phase8-done` |
| 9 | VSync ULA + polish + README | `phase9-done` / `v1.0.0` |
| 9b | Fix bug BSS (initlib cc65) + FX_HYPER | `phase9b-done` / `v1.0.1` |
| 9c | Écran titre "ASTEROIDS" vectoriel 9 lettres | `phase9c-done` / `v1.0.2` |
| 9d | Lettres GMV + écran "GAME OVER" | `phase9d-done` / `v1.0.3` |
| 9e | Lettres PC + "PRESS SPACE" + attente input | `phase9e-done` / `v1.0.4` |
| 9f | FX_THRUST + FX_LIFE (6 effets sons total) | `phase9f-done` / `v1.0.5` |
| 9g | Fix bug XOR sommets partagés (ship/lettres/asteroids/UFO) | `phase9g-done` / `v1.0.6` |
| 10a | Shapes asteroids ATARI ARCADE rev 4 authentiques | `phase10a-done` / `v1.1.0` |
| 10b | Shapes Atari N sommets variables (11-13) sans décimation | `phase10b-done` / `v1.1.1` |
| 10c | Spawn arcade-fidèle (vagues progressives, RNG positions/vél.) | `phase10c-done` / `v1.1.2` |
| 10d | Lettre W + affichage "WAVE n" dans HUD | `phase10d-done` / `v1.1.3` |
| 10e | Démo passive arcade-style en écran titre | `phase10e-done` / `v1.1.4` |
| 10f | Fragmentation arcade (3 fragments, RNG vél., parent réduit) | `phase10f-done` / `v1.1.5` |
| 10g | IA UFO arcade (seuil 35000, précision indexée score) | `phase10g-done` / `v1.1.6` |
| 10h | ScrSpeedup arcade (saucer pressure fin-de-vague) | `phase10h-done` / `v1.1.7` |
| 10i | Ship explosion debris (5 fragments éphémères) | `phase10i-done` / `v1.1.8` |
| 10j | Affichage WAVE 2 chiffres (10, 11) | `phase10j-done` / `v1.1.9` |
| 10k | AstBreakTimer (anti-saucer 80 frames post-hit) | `phase10k-done` / `v1.1.10` |
| 10l | Wraparound asteroids par duplication d'instance | `phase10l-done` / `v1.1.11` |
| 10m | Clignotement PRESS SPACE écran titre (pulse arcade) | `phase10m-done` / `v1.1.12` |
| 10n | FX_UFO bip-bip continu (8 effets sons total) | `phase10n-done` / `v1.1.13` |
| 11  | **Sons Mine Storm Vectrex** (port AY-3-8912) | `phase11-done` / `v1.2.0` |
| 12  | Destruction ship + UFO arcade rev 4 (ShipExpVelTbl) | `phase12-done` / `v1.2.1` |
| 13  | Fragments morceaux du ship (ShipExpPtrTbl) | `phase13-done` / `v1.2.2` |
| 14  | Explosion asteroid (dots éphémères) | `phase14-done` / `v1.2.3` |
| 14b | Pattern shrapnel 1 arcade exact | `phase14b-done` / `v1.2.4` |
| 15  | Lettre H + label "HIGH SCORES" en game over | `phase15-done` / `v1.2.5` |
| 16  | Optimisations framerate (`_plot_dot` + `ORA #$40` retiré) | `phase16-done` / `v1.2.6` |

## Différé Phase 17+

- Persistance high scores en `.tap` ou `.dsk` (driver cassette résident).
- Image `.dsk` Microdisc avec sauvegarde native.
- Écran titre vectoriel (lettres "ASTEROIDS" en segments).
- Effets sonores manquants : thrust continu, UFO oscillant, hyperespace,
  enveloppe AY (registres 11-13).
- Player AY sous IRQ Timer 1 (libération CPU pendant les frames).

## Sources

- 6502disassembly.com — désassemblage Asteroids arcade rev 4 (logique).
- Nick Mikstas — extraction des shapes vectoriels Atari.
- Fabrice Frances — notes Oric VSync ULA, AY-3-8912 via VIA 6522.
- Manuel utilisateur Oric-1 (matrice clavier, mode HIRES).

## Crédits

Développé en autonomie agile sur 9 phases.
Identité git : `bmarty <bmarty@mailo.com>`.

🤖 Co-développé avec [Claude Code](https://claude.com/claude-code).
