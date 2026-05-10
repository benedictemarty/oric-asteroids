# Roadmap

Feuille de route du portage *Asteroids* (arcade Atari 1979) sur Oric‑1 48 Ko.
Document vivant, mis à jour à chaque jalon. Référence de cadrage technique
détaillée : [`asteroids-oric1-48k-guide.md`](./asteroids-oric1-48k-guide.md) (§10).

---

## Vue d'ensemble

| Statut | Phase | Livrable | Optimiste | Réaliste |
|---|---|---|---|---|
| ✅ | 1 | Squelette sans OSDK + HIRES + triangle statique | 1 sem. | 2 sem. |
| ✅ | 2 | Bresenham XOR + benchmark (97 c/px, 25 Hz acté) | 2 sem. | 4–6 sem. |
| ✅ | 3 | Vaisseau rotatif (32 angles) + tirs + scan clavier VIA direct | 2 sem. | 3 sem. |
| ✅ | 4 | Astéroïdes (4 formes × 3 tailles, mouvement, fragmentation, wraparound safe) | 2 sem. | 3 sem. |
| ✅ | 5 | Collisions L∞ + score 7-seg + vies + HUD + respawn invincible | 1 sem. | 2 sem. |
| ✅ | 6 | Soucoupe grande/petite + IA tir 8-way / ship-tracking + spawn cyclique | 1 sem. | 2 sem. |
| ✅ | 7 | Hyperespace + restart + high scores top 5 (RAM) | 1 sem. | 2 sem. |
| ✅ | 8 | Son AY-3-8912 : tir + explosion + thump cadencé sur asteroids_count | 1 sem. | 3 sem. |
| ✅ | 9 | VSync ULA via CB1 + polish hiscores + README (v1.0.0) | 1 sem. | 2 sem. |
| ✅ | 9b | Fix bug BSS clear (initlib cc65) + FX_HYPER (v1.0.1) | — | 1 j |
| ✅ | 9c | Écran titre "ASTEROIDS" vectoriel 9 lettres XOR (v1.0.2) | — | 1 j |
| ✅ | 9d | Lettres GMV + écran "GAME OVER" XOR (v1.0.3) | — | 1 j |
| ✅ | 9e | Lettres PC + "PRESS SPACE" + attente input (v1.0.4) | — | 1 j |
| ✅ | 9f | FX_THRUST + FX_LIFE (6 effets sons total) (v1.0.5) | — | 1 j |
| ✅ | 9g | Fix bug XOR sommets partagés (ship/lettres/asteroids/UFO) (v1.0.6) | — | 1 j |
| ✅ | 10a | Shapes asteroids ATARI ARCADE rev 4 authentiques (v1.1.0) | — | 1 j |
| ✅ | 10b | Shapes Atari N sommets variables (11-13) sans décimation (v1.1.1) | — | 1 j |
| ✅ | 10c | Spawn arcade-fidèle (vagues progressives, RNG positions/vél.) (v1.1.2) | — | 1 j |
| ✅ | 10d | Lettre W + affichage "WAVE n" dans HUD (v1.1.3) | — | 1 j |
| ✅ | 10e | Démo passive arcade-style en écran titre (v1.1.4) | — | 1 j |
| ✅ | 10f | Fragmentation arcade authentique (3 fragments, RNG vél.) (v1.1.5) | — | 1 j |
| ✅ | 10g | IA UFO arcade authentique (seuil 35000, précision indexée) (v1.1.6) | — | 1 j |
| ✅ | 10h | ScrSpeedup arcade (saucer pressure fin-de-vague) (v1.1.7) | — | 1 j |
| ✅ | 10i | Ship explosion debris (5 fragments éphémères) (v1.1.8) | — | 1 j |
| ✅ | 10j | Affichage WAVE 2 chiffres (10, 11) (v1.1.9) | — | 1 j |
| ✅ | 10k | AstBreakTimer (anti-saucer 80 frames post-hit) (v1.1.10) | — | 1 j |
| ✅ | 10l | Wraparound par duplication d'instance (vrai cylindre) (v1.1.11) | — | 1 j |
| ✅ | 10m | Clignotement PRESS SPACE écran titre (effet pulse arcade) (v1.1.12) | — | 1 j |
| ✅ | 10n | FX_UFO bip-bip continu (8 effets sons total) (v1.1.13) | — | 1 j |
| ✅ | 11 | Sons Mine Storm Vectrex authentiques (AY-3-8912) (v1.2.0) | — | 1 j |
| ✅ | 12 | Destruction ship + UFO arcade-fidèle (ShipExpVelTbl rev 4) (v1.2.1) | — | 1 j |
| ✅ | 13 | Fragments morceaux du ship (ShipExpPtrTbl rev 4) (v1.2.2) | — | 1 j |
| ✅ | 14 | Explosion asteroid (dots éphémères) (v1.2.3) | — | 1 j |
| ✅ | 14b | Pattern shrapnel 1 arcade exact (v1.2.4) | — | 1 j |
| ✅ | 15 | Lettre H + label "HIGH SCORES" en game over (v1.2.5) | — | 1 j |
| ✅ | 16 | Optimisations framerate (`_plot_dot` + `ORA #$40` retiré) (v1.2.6) | — | 1 j |
| ✅ | 17 | Compteur pixels Bresenham + suppression updates _lx0/_ly0 (v1.2.7) | — | 1 j |
| ✅ | 18 | Main-axis split Bresenham (refactor algo, gain 2.47%) (v1.2.8) | — | 1 j |
| ✅ | 18b | Anti-flicker `game_run` (resserrage fenêtre erase→draw) | — | 1 j |
| ✅ | 18c | Anti-flicker per-entity asteroids (prev_x/prev_y, asteroids_render) | — | 1 j |
| ⏳ | 19 | Vaisseau arcade-fidèle (5 segments avec barre cockpit + encoches) | — | 1 j |
| | **Total** | | **~3 mois** | **~6 mois** |

Légende : 🔜 prochaine — 🚧 en cours — ✅ terminée — ⏳ planifiée — ❌ abandonnée.

Pour un développeur **débutant en 6502**, doubler l'estimation réaliste
(~12 mois) reste plus prudent.

---

## Critères de fin de phase

### Phase 1 — Squelette OSDK + triangle statique 🔜

**Définition de fin** :
- `make` produit un `build/asteroids.tap` valide.
- Phosphoric charge le `.tap` et affiche un triangle (vaisseau) au centre
  de l'écran HIRES en `PAPER 0 / INK 7`.
- Sortie propre sur appui touche.
- Test de non‑régression visuel : capture HIRES `tests/ref/phase1_triangle.ppm`
  versionnée et comparée bit‑à‑bit en CI.

### Phase 2 — Routine de ligne XOR ✅

**Définition de fin validée (2026-05-09)** :
- `line.s` trace tout segment XOR sur HIRES, bit6=1 toujours.
- Benchmark mesuré : **~97 cycles/pixel** (Phosphoric --profile,
  1 000 × 151 px). Objectif ≤18 c/px non atteint ; SMC = Phase 2b.
- Tracé idempotent (draw + erase = état initial). ✓
- **Décision arrêtée : 25 Hz nominal.**
  À 97 c/px, 15 seg × 15 px × 2 ≈ 43 650 cycles ~ budget 40 000. ✓
- 10 scénarios géométriques versionnés (H, V, diagonales, obliques). ✓
- `make check` PASS.

**Bugs Phase 2 documentés** :
- `crt0` : c_sp initialisé à adresse hardcodée ($80) au lieu du symbole
  linker — stack C corrompu, appels de fonctions incorrects.
- `hires_init` : boucle d'effacement écrasait A=$40 avec le numéro de page.
- `_draw_line_xor` : `bpl` pour comparer y-coords > 127 (bcs/bcc requis).

**Phase 2b (future)** : SMC + déroulage boucle pour atteindre 40-50 c/px
et permettre 30 segments × 20 px à 25 Hz (scènes arcade complètes).

### Phase 3 — Vaisseau rotatif + tirs + input clavier ✅

**Définition de fin validée (2026-05-09)** :
- Vaisseau triangulaire 32 angles (résolution 11.25°), tables précalculées
  192 octets en RODATA. Rotation horaire visuelle par incrément ±1.
- Thrust dans la direction de la pointe, friction × 15/16 par frame
  (cc65 `>>4` en signed = ASR), clamp |v| ≤ 16 px/frame.
- Wraparound limité à zone sûre [14,226] × [14,186] (anti-overflow vertices).
- Tirs : 4 simultanés en BSS, vélocité ±6 px/frame fixe, TTL 35 frames,
  edge-trigger SPACE + cooldown 4 frames.
- Lecture clavier VIA directe (PB3 + PSG reg 14 = $EF), 4 colonnes :
  ← (col 5) / → (col 7) / ↑ (col 3, thrust) / SPACE (col 0, tir).
- Synchro 25 Hz via VIA Timer 1 one-shot (39 999 cycles = $9C3F),
  IFR bit 6 polling. Pas de gestion IRQ.
- `make check` PASS (capture statique vaisseau centré).

**Bugs Phase 3 documentés** :
- DDRA = $00 (laissé en input par la ROM) rendait les écritures PSG
  invisibles. Forcé $FF dans `_key_scan` avec save/restore.
- Convention PB3 inversée vs intuition : PB3 = 1 = touche pressée
  (cf. `portb_read_callback` Phosphoric). `bne`/`beq` corrigés.

**Phase 3b (future, optionnelle)** : ajout flame de thrust visible,
support touches HOLD via debouncing logiciel, détection touche multiple
si la rangée 4 ne suffit pas (ex: ESC pour pause).

### Phase 4 — Astéroïdes ✅

**Définition de fin validée (2026-05-09)** :
- 4 formes octogonales × 3 tailles (rayons 5/9/14), 8 sommets/forme.
  Tables précalculées 195 octets RODATA (gen_shapes.py — perturbations
  radiales propres au projet, en remplacement du désasm Atari pour Phase 4).
- Mouvement linéaire entier (vx, vy ∈ {-2..+2}) + wraparound zone safe
  [16,224] × [16,184]. La duplication d'instance vraie est reportée en
  Phase 4b (nécessite tracé en coordonnées 16-bit).
- Fragmentation implémentée (`asteroids_fragment(idx)`) : casse en 2
  enfants size-1, vélocités = ±90° du parent (1 boost ×2 aléatoire).
- Spawn vague initiale = 4 grands aux 4 coins (positions/vélocités
  déterministes, RNG seed fixe = 0x42 pour reproductibilité tests).
- Framerate observé ~15-18 Hz avec 4 grands actifs (rendu = ~62k cycles
  > budget 25 Hz). Accepté en Phase 4 ; SMC + déroulage Bresenham
  (Phase 2b) restaureront 25 Hz.

**Bugs Phase 4 documentés** :
- cc65 préfixe les symboles C avec `_` ; `extern const signed char _shape_x[]`
  cherchait `__shape_x` (deux underscores). Corrigé en utilisant `shape_x`
  côté C (cc65 → `_shape_x`, matche le label asm).

**Phase 4b (future, optionnelle)** :
- Wraparound par duplication d'instance (tracé entité 2× ou 4× près des
  bords avec coords 16-bit signées).
- Format 8.8 fixed-point pour les enfants de fragmentation (vitesses
  fractionnaires plus douces).
- Extraction des shapes Atari authentiques (Nick Mikstas) en remplacement
  des octogones génériques actuels.

### Phase 5 — Collisions + vies + score ✅

**Définition de fin validée (2026-05-09)** :
- Détection collision distance-L∞ (`max(|∆x|,|∆y|) ≤ rsum`) en C : pas de
  multiplication 8×8 requise, ~20 cycles par paire. Précision suffisante
  pour gameplay arcade (faux-positifs aux coins ≤ 21%).
- Bullet vs asteroid (itération `4 × 12 = 48` paires/frame max) : impact
  → `asteroids_fragment(idx)` + `hud_add_score(20/50/100)`.
- Ship vs asteroid : suspendu pendant `INVINCIBLE_FRAMES = 40` post-respawn.
- Scoring conforme arcade : grand=20, moyen=50, petit=100. Extra ship
  tous les 10 000 pts.
- 3 vies de départ. Game over (lives=0) bloque inputs et ship invisible.
- HUD : score 5 chiffres en 7-segments (`draw_line_xor` × 6 segments/chiffre)
  haut-gauche ; vies en mini-triangles haut-droite. Redraw conditionnel
  sur changement uniquement.
- Vague suivante : `asteroids_count() == 0` → respawn 4 grands.

**Bugs Phase 5 documentés** :
- Premier rendu HUD : `score == score_shown == 0` n'entrait jamais dans
  la branche redraw. Flag `hud_first_frame` ajouté pour forcer le dessin
  initial.

**Notes Phase 5** :
- Tir vs soucoupe et vaisseau vs soucoupe = Phase 6 (la soucoupe
  n'apparaît pas encore).
- Score arcade soucoupe (200 grande / 1000 petite) sera ajouté en Phase 6.

### Phase 6 — Soucoupe + IA ✅

**Définition de fin validée (2026-05-10)** :
- Soucoupe en 2 tailles (grande rayon 7, petite rayon 4), forme 7 segments
  XOR (trapèze + dôme + ligne médiane).
- Apparition cyclique tous les 300 frames (~18 s à 17 Hz observés), bord
  aléatoire RNG, Y aléatoire dans zone safe.
- Probabilité petite UFO indexée sur score (0% < 1000, 30% ≥ 5000, 50% ≥ 10000).
- IA tir simplifiée : grand = aléatoire 8-way, petit = visée ship + bruit
  RNG décroissant avec le score.
- Mouvement horizontal continu + drift vertical aléatoire.
- Score 200/1000 (grand/petit) conforme arcade.
- Collisions : 5 paires (bullet×UFO, bullet×asteroid, ship×asteroid,
  ship×UFO/bullet, UFO_bullet×asteroid).

**Bug Phase 6 documenté** :
- Le clear BSS automatique de crt0.s plante l'activation HIRES quand
  `__BSS_SIZE__ ≥ $83` (131 octets). Cause racine non identifiée (pas
  un débordement, pas un chevauchement ZP, encodage `cpy #$83` correct).
  Workaround : crt0 ne clear plus BSS, les modules C initialisent
  explicitement leur état au démarrage.

**Phase 6b (future, optionnelle)** :
- IA arcade fidèle (table de précision 32 entrées indexée par score
  vs seuils discrets actuels).
- Multiple UFO simultanés (Atari original = 1 max, mais variantes possibles).
- Investigation racine du bug BSS clear (Phase 9 polish).

### Phase 7 — Hyperespace + restart + high scores ✅

**Définition de fin validée (2026-05-10)** :
- Hyperespace via touche `↓` (DOWN, row 4 col 6), edge-trigger + cooldown
  35 frames. 25% chance de mort, sinon téléportation aléatoire +
  mini-invincibilité.
- Game state implicite (`gameover` flag du HUD) : inputs/UFO/physique
  ship gelés, seul SPACE déclenche `game_reset()` complet.
- High scores top 5 en RAM (16-bit, 10 octets BSS), insertion triée
  au passage en game over, affichage table en game over.
- **Phase 6 réellement complétée** ici (intégration UFO dans game_run).

**Hors scope Phase 7 (différé Phase 9)** :
- Persistance high scores en `.tap` (besoin driver cassette résident).
- Écran titre vectoriel (lettres "ASTEROIDS" en segments).
- Table 10 entrées (limitée à 5 par budget BSS Phase 6/7).
- Saisie initiales 3 lettres (sans alphabet en font 7-seg, complexe).

**Phase 7b (future, optionnelle)** :
- Probabilité hyperespace progressive avec score (table arcade).
- Rendu propre de la table high scores (digits 7-seg réutilisés).
- Démo passive en écran d'attente (IA simple jouant la frame 0).

### Phase 8 — Son AY-3-8912 ✅

**Définition de fin validée (2026-05-10)** :
- Driver PSG bare-metal en asm (`sound.s`, 175 lignes) : `_psg_write`,
  `_sound_init` (mixer + volumes init), `_sound_play_fx` (déclencher
  un effet par ID), `_sound_tick` (décrémente timer + cut).
- 3 effets implémentés : `FX_FIRE` (tone aigu), `FX_EXPLODE` (noise),
  `FX_THUMP` (tone très grave).
- API C (`sound.h`) appelée depuis `game.c` : sons sur tir, sur impact
  bullet/ship vs asteroid, thump cadencé par `asteroids_count` (30 →
  6 frames, tension croissante conforme arcade).
- 84 écritures PSG vérifiées dans le trace Phosphoric en 5.5M cycles.
- Mode polling (pas d'IRQ Timer 1) : `sound_tick` appelé chaque frame
  de `game_run`, suffit pour les effets ponctuels.

**Hors scope Phase 8 (différé Phase 8b)** :
- Player AY sous IRQ Timer 1 (l'IRQ libérerait CPU mais complique
  la cohérence PSG/clavier).
- Effet thrust continu (besoin état on/off, modèle "1 effet" ne suffit pas).
- Effet UFO oscillant (besoin LFO).
- Effets explosions multi-tailles (différenciation petite/moyenne/grande).
- Effet hyperespace (whoosh).
- Effet extra ship.
- Enveloppe AY (registres 11-13) au lieu de cut net.
- Mix multi-canaux (thump continu sur B + tirs/explosions sur A).

### Phase 9 — VSync ULA + polish + README ✅

**Définition de fin validée (2026-05-10)** :
- Bascule sur signal VSync ULA via CB1 (IFR bit 4), polling 2 fois
  par frame de jeu (50 Hz / 2 = 25 Hz). Élimine le drift Timer 1.
- Rendu propre de la table des high scores en game over (réutilise
  `draw_score` via API publique `hud_xor_5digits`).
- README.md complet : build, run, touches, architecture, phases.
- `v1.0.0` taggué.

**Hors scope Phase 9 (reporté Phase 9b/10)** :
- Persistance high scores en `.tap` ou `.dsk` (driver cassette résident).
- Image `.dsk` Microdisc finale (besoin tap2dsk + driver).
- Écran titre vectoriel (font alphanumérique ~26 lettres).
- Saisie initiales 3 lettres au clavier (sans alphabet en font 7-seg).

---

## Garde‑fou inter‑phases

Aucune phase N+1 n'est entamée tant que la définition de fin de la phase N
n'est pas validée par :
1. Tests host (x86) verts en local.
2. Tests cible Phosphoric headless verts (diff bit‑à‑bit).
3. Capture vidéo manuelle d'au moins 2 minutes de gameplay.
4. Entrée CHANGELOG datée.
5. Tag git `phaseN-done`.

---

## Hors‑roadmap (idées différées)

- Port Atmos avec optimisations spécifiques (RAM utile à $0400 différente).
- Mode 2 joueurs alterné.
- Variations cosmétiques (bonus shapes type Battlezone tank, easter egg).

Ces points ne sont pas planifiés et ne doivent pas dévier la roadmap principale.
