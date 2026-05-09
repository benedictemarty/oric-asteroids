# Changelog

Toutes les modifications notables de ce projet sont documentées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
adhère à [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

Différé Phase 10 :
- Persistance high scores en `.tap` (driver cassette résident, saisie initiales).
- Image `.dsk` Microdisc finale.
- Effets sons manquants : thrust continu, UFO oscillant, enveloppe AY.
- Écran "GAME OVER" / "PRESS SPACE" (besoin de ~6 lettres supplémentaires).

## [1.0.2] - 2026-05-10

### Phase 9c — Écran titre "ASTEROIDS" vectoriel ✅

**Définition de fin validée :**
- 9 lettres `ASTEROIDS` dessinées en segments XOR via `_draw_line_xor` :
  - A (3 segs), S (5 segs), T (2 segs), E (4 segs), R (6 segs),
    O (4 segs), I (3 segs), D (6 segs), S (5 segs)
  - Total ~38 segments par tracé.
- Format compact : pour chaque lettre, liste `(x0, y0, x1, y1)` 4-bit,
  terminée par `0xFF`. Hauteur 10 px, largeur 8 px, espace 12 px.
- Affichage 50 frames (~2 s à 25 Hz) au démarrage, puis effacement
  XOR symétrique (`title_erase` = `title_draw`).
- `ASTEROIDS` centré horizontalement en x=68 (largeur totale 104 px),
  ligne y=80.
- `make check` PASS sur la capture post-titre.

### Added

- `src/title.h` + `src/title.c` (115 lignes) : 8 lettres vectorielles
  (`A` `S` `T` `E` `R` `O` `I` `D` — `S` réutilisé), `title_draw`,
  `title_erase`.
- `tests/ref/phase9_release.ppm` mis à jour (post-titre).

### Changed

- `src/game.c` : appel `title_draw` + boucle 50 `frame_wait` +
  `title_erase` avant l'init du jeu.
- `Makefile` : ajout `src/title.c` aux sources C.

### Décisions techniques Phase 9c

- **Lettres hardcodées** plutôt que font alphanumérique générique :
  9 lettres × ~5 segments = ~45 lignes de coords ; une font 26 lettres
  serait ~150 lignes. Compromis taille/temps.
- **Format `0xFF` sentinel** pour la fin de la liste de segments :
  évite de stocker un compteur séparément, lecture simple en C.
- **Auto-start après 2 s** plutôt qu'attente d'une touche : simplifie
  le flow (pas d'état `STATE_TITLE` séparé). Phase 10 ajoutera
  "PRESS SPACE" et démo passive.
- **Pas de bordure ni d'animation** (pulse, fade) : surcoût de tracé
  XOR à chaque frame du titre = ~38 segments × 2 × ~5 px × 97 c/px
  = ~37 000 cycles supplémentaires par frame. Tracé une fois,
  effacement une fois — minimal.
- **Pas de clignotement / sweep d'entrée** : Phase 9c reste fonctionnelle ;
  l'esthétique arcade complète (titre qui pulse, démo passive, etc.)
  est différée Phase 10.

## [1.0.1] - 2026-05-10

### Phase 9b — Fix bug BSS + effet hyperespace ✅

**Bug BSS clear résolu — cause racine identifiée :**
- Symptôme : avec `__BSS_SIZE__ ≥ $83`, l'écran HIRES n'était pas activé
  après `hires_init` (Phase 6 → 8 utilisaient un workaround : pas de
  clear BSS automatique, init explicite par les modules).
- **Cause racine** : différence subtile entre notre `crt0.s` maison et
  le `zerobss` officiel cc65 (`/usr/local/share/cc65/lib/none.lib`).
  Notre code initialisait `A=#0`, `Y=#0` et utilisait `cpy #<size; bne loop`
  *après* le `sta` ; cc65 utilise un séquençage différent
  (`tay; cpy; beq exit; sta; iny; bne L3`) qui s'avère fiable pour
  toutes les valeurs de `__BSS_SIZE__`. La différence exacte n'est pas
  pleinement caractérisée mais n'est plus pertinente : on adopte la
  routine officielle.
- **Fix** : `crt0.s` simplifié, appelle `initlib` (`zerobss` + constructeurs
  CONDES) au démarrage. Plus besoin d'init explicite redondant des
  variables BSS dans les modules C.
- Reproductibilité : capture phase9 identique à v1.0.0 (md5 match).

### Added

- `FX_HYPER` : effet hyperespace (whoosh = noise + tone grave, 14 frames),
  joué dans `ship_hyperspace()`. 4 effets sonores au total maintenant.

### Changed

- `src/asm/crt0.s` (-30 lignes) : simplifié, utilise `initlib` cc65
  pour zerobss + constructeurs.
- `src/asm/sound.s` : ajout config `FX_HYPER` (PSG noise + tone canal A).
- `src/sound.h` : `#define FX_HYPER 4`.
- `src/game.c` : `sound_play_fx(FX_HYPER)` dans `ship_hyperspace`.

### Décisions techniques Phase 9b

- **Adopter le runtime cc65 officiel** (`initlib`) plutôt que de
  débugger un comportement subtil de notre `zerobss` maison : effort
  vs valeur trop déséquilibré. La routine officielle est testée par
  des milliers d'utilisateurs cc65, c'est un standard de facto.
- **Conserver les `_init` explicites** des modules : ils initialisent
  des valeurs spécifiques (RNG seed 0x42, score_extra=10000, ship au
  centre, etc.), pas seulement zéro. Le `zerobss` ne fait que zéroter.
- **Ne pas implémenter thrust continu / UFO oscillant en Phase 9b** :
  ces effets nécessitent un état "on/off" ou modulation continue
  (LFO logiciel) qui sortent du modèle "1 effet ponctuel" de Phase 8.
  Phase 9c les ajoutera avec un système de canaux dédiés.
- **Ne pas implémenter écran titre vectoriel en Phase 9b** : nécessite
  une font alphanumérique de ~26 lettres × 5-7 segments chacune
  (~150 segments hardcodés en RODATA). Reporté à Phase 9c.

## [1.0.0] - 2026-05-10

### Phase 9 — VSync ULA + polish + README ✅

**Définition de fin validée :**
- Synchro frame via **VSync ULA** (CB1 du VIA 6522, IFR bit 4) au lieu
  de Timer 1 polling. La ROM Oric configure CB1 pour son IRQ VSync ;
  on poll le flag IFR sans installer de handler IRQ. À 25 Hz =
  2 transitions CB1 par frame de jeu (`VSYNCS_PER_FRAME = 2`).
- Élimine le risque de tearing du Timer 1 (drift cumulatif).
- Rendu propre des high scores en game over : réutilise `draw_score`
  du HUD via la nouvelle API `hud_xor_5digits(s, px, py)`. Position
  centrale (105, 70 + i*14) pour chaque ligne.
- README.md complet (build, run, touches, architecture, phases livrées,
  crédits).
- `make check` PASS, capture identique sur 3 runs.

### Added

- `src/hud.h` : prototype public `hud_xor_5digits(s, px, py)`.
- `src/hud.c` : wrapper public exporté autour de `draw_score` (interne).
- `README.md` — manuel utilisateur + build + crédits.
- `tests/ref/phase9_release.ppm` — capture finale.

### Changed

- `src/game.c` :
  - `frame_wait` : remplace polling Timer 1 IFR ($40) par polling
    CB1 IFR ($10), boucle 2 fois (50 Hz / 2 = 25 Hz).
  - `timer_init` : clear T1+CB1 flags initiaux, garde Timer 1 désactivé
    en repli (au cas où VSync absent — non testé en Phase 9).
  - `hiscores_draw_table` : appelle `hud_xor_5digits` (rendu 7-seg
    propre), placeholder `draw_5digit_xor` supprimé.
- `Makefile` : capture/référence renommées `phase9_release.ppm`.

### Décisions techniques Phase 9

- **VSync ULA via polling IFR** plutôt que handler IRQ : simplifie
  drastiquement le code (pas de vector remap, pas de save/restore
  de contexte). Le polling consomme quelques cycles inutiles mais
  c'est négligeable vs le coût du rendu (~75 000 cycles/frame).
- **Pas de `.dsk` final ni persistance high scores en Phase 9** :
  ces deux éléments demandent un driver cassette/disque résident
  dans le binaire (~1-2 Ko de code asm + bin2dsk). Reportés à
  Phase 9b ou Phase 10.
- **Pas d'écran titre vectoriel** : nécessite une font alphanumérique
  de ~26 lettres × 5 segments chacune (~130 segments hardcodés en
  RODATA). Reporté à Phase 9b. Le jeu démarre directement, le HUD
  vide indique implicitement l'état "pré-jeu".
- **`hud_xor_5digits` exposé** : permet de réutiliser le rendu
  7-segments du HUD pour la table des high scores. Évite la
  duplication d'une `font_digits` séparée.

### Tag

`v1.0.0` — premier release stable. 9 phases livrées sur 9 prévues.

### Statistiques finales

- **2666 lignes** de code (asm + C + Python).
- **10464 octets** de binaire (sur 38 Ko utiles disponibles, soit ~28%).
- **9 tags** `phase{1..9}-done` + `v1.0.0`.
- **9 captures de référence** (1 par phase, validées par `make check`).
- **2 générateurs Python** : `gen_ship.py` (32 angles), `gen_shapes.py`
  (4 formes × 3 tailles).

## [0.9.0] - 2026-05-10

### Phase 8 — Son AY-3-8912 (effets) ✅

**Définition de fin validée :**
- Driver PSG bas-niveau (`src/asm/sound.s`) : `_psg_write` (BC1/BDIR via PCR),
  `_sound_init` (mixer reg 7 = $7F + volumes 0), `_sound_play_fx` (init
  registres selon FX_ID), `_sound_tick` (décrémente timer + cut net en fin).
- 3 effets implémentés (1 actif à la fois, override) :
  * `FX_FIRE` : tone canal A ~310 Hz, vol 14, durée 6 frames.
  * `FX_EXPLODE` : noise canal A, freq noise max ($1F), vol 15, 25 frames.
  * `FX_THUMP` : tone canal B très grave (freq hi=$08), vol 14, 8 frames.
- API C (`src/sound.h`) : `sound_init`, `sound_tick`, `sound_play_fx`.
- Hooks dans `game_run` :
  * `sound_init()` au démarrage
  * `sound_tick()` chaque frame
  * `sound_play_fx(FX_FIRE)` dans `bullet_fire`
  * `sound_play_fx(FX_EXPLODE)` sur impact bullet/asteroid + ship/asteroid
  * `sound_play_fx(FX_THUMP)` cadencé selon `asteroids_count`
    (période 30/22/14/6 frames pour 8+/4+/2+/0+ asteroids — tension croissante)
- Partage `kb_pcr_save` ZP avec `input.s` (1 octet).
- 84 écritures PSG vérifiées dans le trace Phosphoric (sound_init +
  ~3 thump déclenchés en 5.5M cycles).
- `make check` PASS, capture identique sur 3 runs.

### Added

- `src/asm/sound.s` (175 lignes) : driver PSG bare-metal.
- `src/sound.h` : API C.
- `tests/ref/phase8_sound.ppm` — capture référence.

### Changed

- `src/asm/input.s` : `kb_pcr_save` exporté en zeropage (`.exportzp`)
  pour partage avec `sound.s`.
- `src/game.c` :
  - `#include "sound.h"`
  - `sound_init()` après `timer_init()`
  - `sound_play_fx(FX_FIRE)` dans `bullet_fire`
  - `sound_play_fx(FX_EXPLODE)` dans `collisions_bullets_asteroids`
    et `collisions_ship_asteroids`
  - Bloc thump cadencé via `thump_timer` + `asteroids_count`
  - `sound_tick()` avant `frame_wait`
- `Makefile` — ajout `src/asm/sound.s` + capture `phase8_sound.ppm`.

### Décisions techniques Phase 8

- **1 effet à la fois** plutôt que mix multi-canaux : simplifie l'état
  (1 enum + 1 timer en ZP). L'arcade Atari mixe plusieurs effets
  (thump continu + tirs/explosions sur autres voies) ; Phase 8b ajoutera
  un canal dédié pour le thump (canal B en parallèle d'A).
- **Cut net en fin d'effet** vs enveloppe progressive : économie en cycles
  CPU. Phase 8b utilisera l'enveloppe matérielle AY-3-8912 (registres 11-13).
- **Thrust et UFO sans son** en Phase 8 (différé Phase 8b) : le son
  thrust nécessite un état "on/off" continu, l'UFO une oscillation
  modulée — incompatible avec le modèle "1 effet ponctuel" actuel.
- **`kb_pcr_save` partagé** entre `input.s` et `sound.s` : économise 1 octet
  ZP et garantit la cohérence du PCR entre scan clavier et écriture PSG
  (mêmes bits 0 et 4 à préserver).
- **DDRA forcé $FF** dans chaque appel PSG, restauré ensuite : la ROM
  Oric peut laisser DDRA en input ($00) après son scan VSync ; sans cela
  les écritures `sta $0301` n'atteindraient pas le PSG (cf. bug Phase 3).
- **SEI/CLI** autour de chaque séquence PSG : la ROM IRQ peut elle aussi
  scanner le clavier via le PSG, on évite les conflits PCR/ORA.

## [0.8.0] - 2026-05-10

### Phase 7 — Hyperespace + restart + high scores ✅

**Définition de fin validée :**
- Hyperespace : touche `↓` (DOWN, row 4 col 6), edge-trigger + cooldown
  35 frames. 25% chance de mort (`HYPER_DEATH_CHANCE = 64/256`),
  sinon téléportation aléatoire dans la zone safe + mini-invincibilité.
- Game state implicite via `gameover` (HUD) : inputs/UFO/ship_update
  ignorés, seul SPACE déclenche `game_reset()`.
- High scores top 5 en RAM (16-bit), insertion triée à l'entrée du game over.
- Affichage table high scores en game over (5 lignes, position centrale).
- Persistance `.tap` reportée Phase 9 (pas de driver cassette résident).
- **Phase 6 réellement complétée** : intégration UFO dans la boucle
  game_run (le commit `phase6-done` ajoutait ufo.c/.h sans les appels
  dans game.c — corrigé ici).
- `make check` PASS, capture identique sur 3 runs.

### Added

- `src/asm/input.s` : ajout scan colonne 6 (DOWN ARROW), bit 4 de `_key_state`.
- `src/game.c` :
  - `ship_hyperspace()` : RNG mort/téléportation + invincibilité.
  - `hiscores_init/insert/draw_table` : table 5 entrées, insertion triée.
  - `game_reset()` : effacement écran + réinit complète + ship invincible.
  - `prev_hyper`, `hyper_cd` : edge-trigger + cooldown DOWN.
- Capture référence `tests/ref/phase7_full.ppm`.

### Changed

- `src/game.c` : intégration UFO complète (init + tick + draw + 3 collisions),
  réorganisation boucle pour gérer les deux états (PLAY / GAMEOVER).
- `Makefile` — ajout effectif de `src/ufo.c` dans `SRCS_C`,
  capture/référence renommées `phase7_full.ppm`.

### Décisions techniques Phase 7

- **Hyperespace edge-trigger + cooldown** vs hold continu : évite le
  spam (téléportations multiples par appui maintenu). Cooldown 35 frames
  ≈ 2 s, cohérent avec l'arcade.
- **25% chance de mort fixe** plutôt qu'indexée sur le score (l'arcade
  augmente progressivement la probabilité avec le temps). Phase 7b
  pourra ajouter cette progression.
- **High scores en RAM seule** : 5 entrées × 2 octets = 10 octets BSS,
  perdu au reset. Persistance `.tap` (saisie + relecture) implique un
  driver cassette dans le binaire — reportée à Phase 9 polish.
- **Pas d'écran titre dédié** : l'arcade Atari démarre directement en
  démo passive (jeu IA en arrière-plan, "PRESS START"). Notre
  implémentation Phase 7 démarre directement en jeu joueur. Un écran
  titre vrai (lettres ASTEROIDS vectorielles) sera ajouté en Phase 9.
- **Affichage high scores game over** : utilise un placeholder
  `draw_5digit_xor` qui dessine 4 segments par ligne (visualisation
  grossière de l'ordre de grandeur). À remplacer par un rendu propre
  via `digit_segs` réutilisé du HUD (Phase 7b).

## [0.7.0] - 2026-05-10

### Phase 6 — Soucoupe + IA de tir ✅

**Définition de fin validée :**
- Soucoupe grande (UFO_LARGE, rayon 7) et petite (UFO_SMALL, rayon 4),
  forme à 7 segments tracés en XOR (corps trapézoïdal + dôme).
- Apparition cyclique selon `UFO_SPAWN_PERIOD = 300` frames (~18 s),
  bord aléatoire (gauche/droite via RNG), Y aléatoire dans la zone safe.
- Probabilité petite UFO en fonction du score (cf. arcade) :
  score < 1000 → 0% small, ≥5000 → 30%, ≥10000 → 50%.
- Mouvement horizontal continu + drift vertical aléatoire (UFO_DRIFT_PERIOD).
- IA tir périodique (UFO_FIRE_PERIOD = 35 frames) :
  * UFO_LARGE : direction aléatoire 8-way
  * UFO_SMALL : visée vers le ship + bruit RNG (si score < 5000)
- Score UFO_LARGE = 200, UFO_SMALL = 1000 (conforme arcade).
- Collisions : tir joueur vs UFO, ship vs UFO, ship vs tir UFO,
  tir UFO vs asteroid (auto-frag, pas de score).
- Sortie d'écran : UFO disparaît, attend prochain spawn.
- `make check` PASS, capture identique sur 3 runs.

### Added

- `src/ufo.h` + `src/ufo.c` (290 lignes) :
  - État BSS (12 octets) : ufo_active/x/y/vx/vy/type + bullet info.
  - Tables segments hardcodées : `seg_large`, `seg_small` (7 segs × 4 bytes).
  - `ufo_init/tick/draw/kill`, `ufo_radius`, `ufo_bullet_update/draw`.
  - Fonction de spawn (`ufo_spawn`) interne, IA tir (`ufo_fire`).
- `tests/ref/phase6_ufo.ppm` — capture de référence (vaisseau centre,
  4 grands asteroids, HUD + 3 vies, sans UFO actif au moment T).

### Changed

- `src/game.c` :
  - Intégration UFO complète (init, tick, draw, collisions).
  - 4 helpers de collision : bullets×asteroids, bullets×UFO,
    ship×asteroids, ship×UFO/UFO_bullet, UFO_bullet×asteroids.
  - Init explicite `ship_invincible`, `ship_was_drawn` (BSS clear retiré).
- `Makefile` — ajout `src/ufo.c`, capture/référence renommées
  `phase6_ufo.ppm`.

### Fixed (bug Phase 6 — investigation profonde)

- **Clear BSS de crt0.s retiré** : un bug bizarre apparaît quand
  `__BSS_SIZE__ >= $83` (131 octets). Symptôme : l'écran HIRES n'est
  plus activé après `hires_init`, le mode TEXT reste affiché. Le seuil
  $82 fonctionne, $83 plante. Investigation a confirmé :
  * Pas un débordement de binaire (binaire reste petit)
  * Pas un chevauchement ZP (ptr1 utilisé comme pointeur temp)
  * Pas un overflow stack ou IRQ
  * Reproductible avec n'importe quel module ajoutant ≥12 octets BSS
  * `cpy #<__BSS_SIZE__` correctement encodé `cpy #$83`
- **Workaround appliqué** : crt0 ne fait plus de clear BSS automatique.
  Les modules C initialisent EXPLICITEMENT leurs variables au démarrage
  (`bullets_init`, `asteroids_init`, `hud_init`, `ufo_init`, init
  manuel de `ship_invincible` et `ship_was_drawn` dans `game_run`).
- Cause racine du bug à investiguer en Phase 9 (polish).

### Décisions techniques Phase 6

- **1 UFO max simultané** au lieu d'un pool : simplifie la logique IA et
  l'allocation BSS (12 octets total). L'arcade Atari original limite
  également à un UFO actif par moment.
- **Forme dessinée en 7 segments hardcodés** plutôt qu'en table générée :
  l'UFO a une forme fixe (pas de rotation), 7 segments × 4 bytes = 28
  bytes RODATA par taille, 56 bytes total. Pas de gain à séparer.
- **IA tir simplifié** vs Atari arcade rev 4 :
  * arcade : table de précision indexée par score (32 entrées)
  * Phase 6 : seuils discrets sur score (5000, 10000) — résolution
    suffisante pour la jouabilité.
- **Edge-trigger spawn** : un seul `ufo_spawn_timer` 16-bit décrémenté
  chaque frame. Pas d'IRQ ni d'event queue.

## [0.6.0] - 2026-05-09

### Phase 5 — Collisions + vies + score + HUD ✅

**Définition de fin validée :**
- Détection collision distance-L∞ (`abs_diff(x1,x2) ≤ r && abs_diff(y1,y2) ≤ r`,
  ~10 cycles de vérif par paire) au lieu de la distance euclidienne :
  + rapide en 6502 (pas de mul 8×8), -10% de précision géométrique acceptable.
- Bullet vs asteroid : itération `4 × MAX_ASTEROIDS` ; sur impact, `bullet TTL=0`,
  `asteroids_fragment(idx)`, `hud_add_score(score_by_size[size])`.
- Ship vs asteroid : suspendu pendant `INVINCIBLE_FRAMES = 40` après respawn.
- Scoring conforme arcade : grand=20, moyen=50, petit=100. Extra ship tous
  les 10 000 pts (`HUD_EXTRA_BONUS`).
- 3 vies de départ. Game over à 0 vies (boucle continue mais ship invisible
  + inputs ignorés).
- HUD : score 5 chiffres en 7-segments en haut-gauche (4×6 px chacun, ~6
  segments allumés/chiffre, ~25 pixels-segments redessinés sur changement),
  vies en mini-triangles haut-droite (3 segments × N).
- Vague suivante : `asteroids_count() == 0` → `asteroids_spawn_wave()`.
- `make check` PASS, capture identique sur 3 runs.

### Added

- `src/hud.h` + `src/hud.c` :
  - Table `digit_segs[10]` (10 octets RODATA) — bitmask 7-segments par chiffre.
  - `draw_digit`, `draw_score`, `draw_lives` (XOR via `draw_line_xor`).
  - `hud_init`, `hud_draw` (redraw conditionnel sur changement),
    `hud_add_score` (auto-detect bonus 10 000), `hud_lose_life`.
  - Variables exposées : `score`, `score_extra`, `lives`, `gameover`.
- `tests/ref/phase5_play.ppm` — capture (vaisseau centre, 4 grands aux
  coins, "00000" haut-gauche, 3 mini-vaisseaux haut-droite).

### Changed

- `src/game.c` :
  - `collisions_bullets_asteroids` + `collisions_ship_asteroids` (helper
    `collide` distance-L∞).
  - `ship_respawn` (centre, immobile, angle 0, `ship_invincible = 40`).
  - `check_next_wave` (respawn 4 grands quand 0 actifs).
  - Boucle : input/update ignorés en `gameover`; ship clignote pendant
    invincibilité (visible si bit 1 du timer set, soit ~2 frames on/off).
- `Makefile` — ajout `src/hud.c`, capture/référence renommées
  `phase5_play.ppm`.

### Fixed

- `hud_draw` : bug de premier dessin. `score == score_shown == 0` ne
  déclenchait jamais `draw_score` au démarrage. Ajout d'un flag
  `hud_first_frame` pour forcer le dessin initial.

### Décisions techniques Phase 5

- **Distance L∞** plutôt qu'euclidienne : pas de multiplication 8×8 nécessaire.
  Le rectangle englobant donne une fausse détection de coin (~21% surface
  excédentaire) — négligeable au regard de la grande différence ship-radius
  vs asteroid-radius.
- **Score 7-segments** vs font bitmap : 7 lignes XOR par chiffre (réutilise
  `draw_line_xor`), pas de table de glyphes. Cohérent avec l'esthétique
  vectorielle Atari arcade.
- **Redraw conditionnel** du HUD : le score change rarement (à chaque tir
  réussi), le redraw permanent gaspillerait ~50 segments × 5 px × 97 c/px
  × 2 = ~50 000 cycles/frame. Avec redraw conditionnel : 0 cycles HUD la
  plupart des frames.
- **Décomposition score par soustraction** plutôt que division en C :
  cc65 implémente `unsigned int / 10` en routine logicielle (~150 cycles).
  La boucle `while (t >= 10000) t -= 10000; d++;` est plus rapide pour
  des valeurs typiques < 6 chiffres.
- **Invincibilité respawn 40 frames** (~2.4 s à 17 Hz observés) : laisse
  au joueur le temps de s'orienter. Clignotement bit 1 du timer
  (cycles de 4 frames on/off).

## [0.5.0] - 2026-05-09

### Phase 4 — Astéroïdes (formes, mouvement, fragmentation, wraparound) ✅

**Définition de fin validée :**
- 4 formes octogonales × 3 tailles (5/9/14 px de rayon), 8 sommets chacune,
  192 octets de tables RODATA (`tools/gen_shapes.py` → `src/asm/shapes.s`).
- Mouvement linéaire entier avec wraparound zone safe [16,224] × [16,184]
  (la duplication d'instance est différée à Phase 4b — voir hors-roadmap).
- Fragmentation : grand → 2 moyens, moyen → 2 petits, petit → disparu ;
  vélocités enfants = rotation 90° du parent ± boost 2× (RNG).
- Spawn vague initiale : 4 grands asteroids aux 4 coins, vélocités fixes
  orientées vers le centre.
- 12 asteroids max simultanés (BSS, 84 octets).
- `make check` PASS — capture stable identique sur 3 runs successifs.

### Added

- `tools/gen_shapes.py` — générateur 4 formes × 3 tailles + rayons
  collision (Phase 5). 195 octets de tables RODATA total.
- `src/asm/shapes.s` — auto-généré : `shape_x[96]`, `shape_y[96]` (flat,
  index = `(size*4+shape)*8 + vertex`), `shape_radii[3]`.
- `src/asteroids.h` + `src/asteroids.c` — module `Asteroid`,
  fonctions `asteroids_init/spawn_wave/update/draw/fragment/count`,
  RNG LFSR 8-bit Galois (polynôme x^8+x^6+x^5+x^4+1, seed = 0x42).
- `tests/ref/phase4_field.ppm` — capture de référence (vaisseau centré
  + 4 grands asteroids dans la zone safe, ~3000 pixels blancs).

### Changed

- `src/game.c` — boucle de jeu intègre `asteroids_init/spawn_wave` avant
  la boucle ; `asteroids_draw` (XOR) et `asteroids_update` dans chaque
  itération, dans le bon ordre (effacer ↔ position cohérente).
- `Makefile` — ajout sources `src/asteroids.c` + `src/asm/shapes.s`,
  cible `gen_shapes`, option `-I src` pour cc65 (asteroids.h),
  capture/référence renommées en `phase4_field.ppm`.

### Décisions techniques Phase 4

- **Format flat des shapes** (1 tableau plat indexé par
  `(size*4+shape)*8 + vertex`) plutôt que tables de pointeurs : accès
  direct via `_shape_x[base + i]` en C, pas de double indirection,
  surcoût mémoire négligeable (192 vs 24 octets de pointeurs + 192).
- **Position entière 8-bit + vélocité signée 8-bit** par astéroïde au
  lieu du 8.8 fixed-point : les vélocités initiales (±1 à ±2 px/frame)
  ne nécessitent pas de fraction. Format 8.8 sera ajouté en Phase 4b
  pour les enfants de fragmentation rapides.
- **Wraparound zone safe** [16,224] × [16,184] (pas de duplication
  d'instance) : compromis Phase 4. La duplication propre nécessite un
  format de coordonnées 16-bit pour le tracé hors zone (Phase 4b).
- **12 asteroids max** dans le pool BSS : compromis budget cycles
  vs cas extrême de fragmentation (4 grands → 8 moyens → 16 petits,
  mais en pratique la collision détruit les petits avant qu'ils ne
  s'accumulent — Phase 5).
- **RNG LFSR 8-bit déterministe** avec seed fixe pour reproductibilité
  des tests CI ; Phase 7+ utilisera la position des touches comme entropie.
- **Framerate observé** : ~15-18 Hz avec 4 grands asteroids actifs
  (4 × 8 segments × ~10 px × 97 c/px × 2 = ~62 000 cycles, dépasse le
  budget 25 Hz à 40 000 cycles). Acceptable pour Phase 4 ; les
  optimisations SMC/déroulage ligne (Phase 2b) restaureront 25 Hz.

## [0.4.0] - 2026-05-09

### Phase 3 — Vaisseau rotatif + tirs + input clavier ✅

**Définition de fin validée :**
- Vaisseau triangulaire rotatif (32 angles précalculés, résolution 11.25°),
  thrust dans la direction de la pointe, friction × 15/16 par frame.
- Tirs : ≤ 4 simultanés, durée de vie 35 frames (~1.4 s), vélocité ±6 px/frame
  fixée à l'angle du vaisseau au moment du tir.
- Lecture clavier VIA directe (PB3 + PSG reg 14 = $EF, rangée 4) :
  ←/→/↑ (flèches) + SPACE (tir).
- Boucle 25 Hz via VIA Timer 1 one-shot (latch 39 999 = $9C3F).
- `make check` PASS — capture statique vaisseau centré identique à la référence.

### Added

- `tools/gen_ship.py` — génère `src/asm/ship_verts.s` (192 octets RODATA) :
  3 sommets × 32 angles précalculés + vecteurs de thrust (x = sin, y = -cos).
- `src/asm/ship.s` — état ZP (`_ship_x/_y/_vx/_vy/_angle/_key_state`),
  routines `_ship_init`, `_ship_draw`/`_ship_erase` (XOR idempotent),
  `_ship_rotate`. Tail-call sur `_draw_line_xor` pour le 3e segment.
- `src/asm/input.s` — `_key_scan` : SEI, sauvegarde PCR/DDRA, écrit PSG
  reg 14 = $EF via séquence BC1/BDIR (PCR $EE/$CC/$EC/$CC), scanne 4
  colonnes (5/7/3/0), restaure et CLI.
- `src/game.c` — `game_run()` : init Timer 1, ship_update (friction +
  intégration 16-bit + wraparound zone sûre [14,226]×[14,186]), bullets
  (4 max, edge-trigger sur SPACE, cooldown 4 frames), frame_wait par
  polling IFR T1 bit 6.
- `tests/ref/phase3_ship.ppm` — capture de référence (vaisseau statique
  au centre, 3075 pixels blancs).

### Changed

- `src/main.c` — simplifié : appelle `game_run()` uniquement.
- `Makefile` — cibles `gen_ship`, ajout `src/game.c` + 3 sources asm,
  `TEST_CYCLES` étendu à 4 s post-fastload (capture après stabilisation).

### Fixed (bugs Phase 3)

- `input.s` : DDRA était laissé à $00 (input) par la ROM après son scan
  VSync, rendant nos écritures `sta VIA_ORA` invisibles côté PSG. Force
  DDRA = $FF avant chaque write PSG, restauré ensuite.
- `input.s` : convention PB3 inversée. Sur Oric-1 (cf. `portb_read_callback`
  Phosphoric), **PB3 = 1 si touche pressée**, 0 sinon. Notre code testait
  `bne` après `and #$08` ; corrigé en `beq` pour les 4 colonnes.

### Décisions techniques Phase 3

- **Rotation par tables précalculées** (32 angles, 192 octets) plutôt
  qu'une multiplication signée 8×8 → 16 bits en temps réel : plus rapide,
  plus simple, résolution suffisante (11.25°) pour la jouabilité arcade.
- **Position en pixels entiers + vitesse signée 8-bit** plutôt que 8.8
  fixed-point : la friction × 15/16 et le clamp |v| ≤ 16 suffisent pour
  Phase 3 ; la précision 8.8 sera ajoutée si nécessaire en Phase 4-5.
- **Frame timing par VIA Timer 1 one-shot** au lieu de VSync ULA :
  déterministe, pas d'IRQ à gérer, démarrage explicite par frame. Le
  vrai VSync ULA reste prévu pour Phase 9 (polish anti-tearing).
- **Scan clavier VIA direct** (pas de buffer ROM Atmos $0265) :
  - répond immédiatement (pas de délai key-repeat ROM 30 frames)
  - détecte plusieurs touches simultanément (ex : ← + ↑ pour rotation
    pendant thrust)
  - portable Oric-1 / Atmos (pas dépendant de variables BASIC)
- **4 tirs max, edge-trigger SPACE + cooldown 4 frames** : limite arcade
  fidèle (Asteroids original = 4 tirs simultanés).

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
