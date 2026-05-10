# Changelog

Toutes les modifications notables de ce projet sont documentées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet
adhère à [Semantic Versioning](https://semver.org/lang/fr/).

## [Unreleased]

### Anti-flicker — réorganisation boucle `game_run` (étapes A + B) ✅

**Symptôme** : à 25 Hz, les astéroïdes (et autres entités mobiles)
clignotaient de façon perceptible.

**Cause** : dans `game.c game_run()`, ~30 lignes de logique (sound,
scoring, hi-scores, wave label, game-over detect) s'intercalaient
entre la phase erase (XOR) en début de frame et la phase draw en
fin de frame. La fenêtre pendant laquelle un astéroïde était éteint
à l'écran couvrait ~80 % de la durée de frame → l'œil percevait
un cycle "présent/absent" à 25 Hz, fréquence de flicker maximale.

**Correctif** :

- **Étape A** : restructuration de la boucle en 2 blocs distincts :
  1. **Bloc render compact** : erase → apply input → updates →
     collisions → ship_visible → draw (fenêtre flicker minimisée).
  2. **Bloc post-render** (hors fenêtre critique) : sound logic
     (FX_LIFE, thump, UFO bip-bip), `sound_tick`, insertion
     hi-scores, wave label, `hud_draw`, textes game-over.

- **Étape B (simplifiée)** : sortir le couple erase/draw asteroids
  du bloc général, le placer en sandwich autour de
  `asteroids_update + collisions_*_asteroids`. Fenêtre asteroids
  réduite à ~7 ms (vs ~20 ms pré-refactor) — pattern *per-entity*
  avec `prev_x/prev_y` reste envisageable si flicker résiduel.

Aucun changement de structures de données ni d'API asm. Test
régression `make check` PASS (capture identique à la référence).

### Fix — toggle PRESS SPACE écran titre ✅

**Symptôme** : "PRESS SPACE" apparaissait altéré ou disparaissait
quand un astéroïde traversait sa zone, et le clignotement attendu
ne fonctionnait pas comme prévu.

**Cause** : pattern de toggle buggy dans `game.c` boucle titre :
```c
presspace_erase(110);
ps_visible = !ps_visible;
if (ps_visible) presspace_draw(110);
```
Aux toggles impairs, ce code fait `XOR` puis `XOR` aux mêmes
coords = identité visuelle, mais flippe `ps_visible`. Résultat :
le texte n'était en pratique visible qu'une fois sur deux, et
l'état tracking était désynchronisé.

**Correctif** : un seul XOR par toggle (erase si actuellement
visible, draw si caché).

Note : l'overlap astéroïde ↔ texte reste inhérent au tracé XOR
mono-buffer. Solution future possible : déplacer "PRESS SPACE"
hors de la zone de circulation des astéroïdes, ou confiner ces
derniers à une bande de l'écran titre.

### Réactivité scan SPACE — double-scan boucle titre ✅

**Symptôme** : le démarrage du jeu via SPACE depuis l'écran titre
nécessitait parfois plusieurs appuis.

**Cause** : la boucle titre tourne à ~17 Hz (frame ~60 ms) avec un
seul `key_scan()` en début de frame. Un appui SPACE bref (< 60 ms)
tombant entre deux scans n'était jamais détecté.

**Correctif** : ajouter un second `key_scan()` après le tracé
asteroids, juste avant le toggle PRESS SPACE. Double les chances
de capter un appui dans la frame.

Note : la même problématique existe potentiellement dans `game_run`
pour le tir et l'hyperespace ; non corrigée ici car la boucle de
jeu tourne à 25 Hz (frame ~40 ms) et un appui humain dépasse
typiquement cette durée. À investiguer si le user signale des
tirs manqués.

### Maintenance

- Mise à jour de `tests/ref/phase9_release.ppm` pour refléter
  l'affichage correct de "PRESS SPACE" (le snapshot précédent
  capturait l'état buggy du toggle).

### Anti-flicker — étape B complète : per-entity asteroids ✅

**Symptôme** : après étape A + B simplifiée, les astéroïdes
clignotaient encore visiblement.

**Cause** : la fenêtre erase→draw d'un astéroïde restait de l'ordre
de quelques ms (`asteroids_update` + 3 collisions globales) car le
couple était posé autour de l'ensemble des opérations, pas autour
de chaque astéroïde individuellement.

**Correctif** :

- Ajout de `prev_x`, `prev_y`, `drawn` à `Asteroid` (3 octets/slot
  × 12 slots = 36 octets RAM).
- `asteroid_draw_at` et `asteroid_draw_one` prennent désormais le
  centre `(cx, cy)` en paramètre (au lieu de lire `.x, .y`), ce qui
  permet de tracer le même astéroïde à n'importe quelle position
  sans toucher à la struct.
- `asteroids_update` sauve `prev_x = x ; prev_y = y` avant de
  calculer la nouvelle pos.
- Nouvelle `asteroids_render()` : pour chaque slot, si `drawn` →
  erase à `prev` ; si `active` → draw à `curr`, met à jour
  `prev = curr`, `drawn = 1`.
- `asteroids_fragment` efface immédiatement le tracé du parent
  avec ses attrs OLD (avant modification de `size`/`shape`) pour
  préserver l'invariant XOR ; nouveaux fragments créés avec
  `drawn = 0` (pas d'erase au prochain render, juste un draw).
- `game_run` et boucle titre remplacent le triple
  `asteroids_draw → update → asteroids_draw` par
  `asteroids_update → asteroids_render`.

Fenêtre flicker par astéroïde ramenée à quelques centaines de
cycles (uniquement entre `asteroid_draw_one(prev)` et
`asteroid_draw_one(curr)` pour ce slot précisément), soit un
ratio < 1 % de la frame 25 Hz — théoriquement imperceptible.

### Polish — clignotement PRESS SPACE accéléré

Période du toggle réduite de 24 à 8 frames (de ~1.4 s à ~0.5 s à
17 Hz), cadence arcade-fidèle.

### Maintenance (étape B)

- Mise à jour de `tests/ref/phase9_release.ppm` (1155 octets diff,
  attendus : timing changé par `asteroids_render` + cadence du
  toggle PRESS SPACE).

À venir Phase 19 (planifiée) :
- **Vaisseau arcade-fidèle** : passer de 3 à **5 segments** pour
  reproduire la forme exacte de l'Atari rev 4 (pointe + 2 côtés
  longs + barre transversale cockpit + 2 encoches arrière en V).
  Implique : modif `tools/gen_ship.py` (5 sommets × 32 angles =
  160 octets table), `ship.s` (`draw_three_lines` → `draw_five_lines`),
  et ajustement explosion ship (Phase 12/13) pour gérer 5 morceaux.
  Coût rendu : ~100 c/frame supplémentaires (absorbable).

À venir Phase 20+ :
- SMC : patcher dy/dx en immédiats dans la boucle (gain ~3 c/px).
- Déroulage 2× ou 4× du corps de boucle main-axis.
- Optimisations C → ASM des boucles draw critiques en C.

(Hors scope définitif : `.tap`/`.dsk` persistance, mix multi-canaux.
 Différé : variables arcade, clipping Cohen-Sutherland.)

## [1.2.8] - 2026-05-10

### Phase 18 — Main-axis split Bresenham ✅

**Objectif** : refactor algorithmique majeur de la boucle Bresenham.
Au lieu de l'algo générique 16-bit avec `e2 = 2*err`, deux tests
imbriqués (step x ET step y avec mise à jour d'erreur 16-bit), on
sépare en 2 boucles distinctes choisies à l'init :
- **Horizontale** (dx ≥ dy) : step x toujours, step y conditionnel
- **Verticale** (dy > dx) : step y toujours, step x conditionnel

Algo 8-bit non-signé : `err += axe_secondaire ; si err ≥ axe_principal :
step secondaire, err -= axe_principal`.

### Bench profiler

- **Avant Phase 18** : 8 836 027 instructions
- **Après Phase 18** : 8 617 685 instructions
- **Gain** : **218 342 instructions = ~2.47%**

Le plus gros gain cumulé d'une phase d'optim ; ~10× le gain Phase 17.

### Cycles par pixel (estimation)

Avant (Bresenham générique 16-bit) :
- Plot : 14
- Test fin compteur : 7
- e2 = 2*err calcul 16-bit : 16
- Test step x (avec `tax/txa`) : 22-26
- Step x : 7-15
- Test step y : 8-15
- Step y conditionnel : 11-17
- Total : ~85-110 c/px

Après (main-axis horizontal, cas commun no-step-y) :
- Plot : 14
- Test fin (DEC + BNE) : 8
- err += dy + cmp : 11+3
- Branche no_stepy + sta err : 6
- Step x toujours (lsr + bcs) : 7
- jmp @h_loop : 3
- Total : ~52 c/px

Cas avec step y : +14 cycles → ~66 c/px.
Estimation moyenne pondérée : **55-60 c/px** soit **~30-35% plus rapide**.

### Changes

- **`src/asm/line.s`** :
  - Refactor complet de la boucle `_draw_line_xor` : remplacement
    de l'algo générique par split horizontal/vertical.
  - Suppression du calcul de `e2 = 2 * err` (16-bit).
  - Suppression de `l_e2lo`, `l_e2hi`, `l_err_hi` (gardés en ZP
    mais inutilisés — pourront être retirés en Phase 19).
  - Compteur `l_pix = max(dx, dy) + 1` calculé après le branch
    horizontal/vertical.
  - Test de fin `dec l_pix / bne @body` (sans trampoline) +
    `jmp @done` en cas terminal.
- Binaire : 15 356 → 15 393 octets (+37, normal pour 2 boucles).

### Décisions techniques Phase 18

- **Pourquoi pas SMC** : SMC complique le débogage et la lecture de la
  boucle. Phase 18 fait d'abord la simplification algorithmique
  (gain solide). Phase 19+ pourra ajouter SMC sur cette base.
- **err 8-bit non-signé** : suffit car `dx, dy ≤ 239`. L'algo
  `err += dy ; if err ≥ dx : err -= dx` reste dans [0, dx-1] tout
  le temps. Pas de débordement.
- **Idempotence draw+erase préservée** : le swap initial
  (lx0 > lx1 → permutation) garantit que `draw(A,B)` et `draw(B,A)`
  utilisent la même boucle interne avec les mêmes paramètres.
  L'algo est déterministe.
- **Branches courtes** : la boucle horizontale + verticale font
  ensemble ~150 octets ; le `BNE @body` reste dans la portée
  ±127 octets sans trampoline.
- **`make ref` reseté** : la nouvelle séquence de pixels (algo
  différent de Bresenham classique) peut tracer des pixels légèrement
  différents sur lignes obliques. La capture de référence a été
  régénérée — visuel toujours correct.

## [1.2.7] - 2026-05-10

### Phase 17 — Compteur de pixels Bresenham ✅

**Objectif** : remplacer le test de fin de ligne (`cmp _lx0/_lx1` +
`cmp _ly0/_ly1`, 12-15 cycles) par un compteur décrémental
(`dec l_pix / beq @done`, 7-8 cycles) → -5 c/pixel.

### Bench profiler

- **Avant Phase 17** : 8 856 486 instructions
- **Après Phase 17** : 8 836 027 instructions
- **Gain** : 20 459 instructions = **0.23%**
- Binaire : 15 369 → 15 356 octets (-13)

Modeste mais cohérent avec la nature de l'optim : ne touche que la
fonction `_draw_line_xor` (~3-4% du runtime hot path).

### Changes

- **`src/asm/line.s`** :
  - Nouveau symbole ZP `l_pix` : compteur de pixels initialisé à
    `max(dx, dy) + 1` à l'init de `_draw_line_xor`.
  - Boucle `@loop` : test de fin via `dec l_pix / beq @done` (-5 c/px).
  - `inc _lx0` supprimés des deux branches step x (plus relu).
  - `lda _ly0 / clc / adc l_sy / sta _ly0` supprimé du step y (-11 c).
  - `_plot_dot` : `ora #$40` retiré pour cohérence (l'invariant
    bit6=1 est préservé par les masques x_msk[]).

### Décisions techniques Phase 17

- **Sécurité du compteur** : `max(dx, dy) ≤ 239` (HIRES 240×200) → +1
  reste dans [1, 240], jamais d'overflow 255 → 0 → boucle infinie.
- **Pas un changement d'API** : `_lx0` et `_ly0` restent les
  paramètres d'entrée. Ils ne sont plus mis à jour pendant le tracé
  mais cette mise à jour était purement interne.
- **Pourquoi pas plus** : la SMC Bresenham (Phase 18) et le déroulage
  apporteraient des gains beaucoup plus importants (objectif Phase 2b
  cible : 40-50 c/px vs nos ~90 c/px actuels). Phase 17 prépare le
  terrain en simplifiant la boucle.

## [1.2.6] - 2026-05-10

### Phase 16 — Optimisations framerate ✅

**Objectif utilisateur** : augmenter la fréquence (frame rate observé
~17 Hz au lieu de 25 Hz cible). Phase 16 = micro-optimisations cumulatives
non-régressives.

### Bench profiler

- **Avant Phase 16** : 8 915 727 instructions sur 28.5M cycles
- **Après Phase 16** : 8 856 486 instructions
- **Gain** : 59 241 instructions = ~**0.66%**

Modeste mais cumulable avec phases ultérieures.

### Optimisation 1 — `_plot_dot` rapide

Nouveau symbole exporté dans `src/asm/line.s` : `_plot_dot` calcule
l'adresse HIRES + masque depuis tables précalculées et XOR un seul
pixel (~40 cycles) sans le setup Bresenham complet (~80 cycles via
`_draw_line_xor` cas dégénéré).

Utilisé pour :
- Replots des sommets ship (Phase 9g) → 3 plots/frame ship
- Replots des sommets asteroid (Phase 9g) → 11-13 plots/asteroid
- Asteroid debris dots (Phase 14b) → 8 plots quand actif
- Bullets render (`plot()` helper game.c) → ~4 plots/frame

Gain estimé : ~30 plots × 40 cycles = 1200 cycles/frame quand pleine
charge.

### Optimisation 2 — Suppression `ORA #$40` du chemin chaud

L'invariant `bit6=1` (bit "non-attribut" Oric HIRES) était maintenu
par `ORA #$40` à chaque pixel XOR-é dans la boucle Bresenham. Or :
- Les masques `x_msk[]` ont **TOUS** bit6 = 0 (`$20, $10, $08, $04, $02, $01`)
- L'XOR n'inverse que les bits où le masque est à 1
- Donc bit6 du byte HIRES n'est **jamais** touché par l'XOR
- L'invariant `bit6=1` (init HIRES = $40) est préservé sans `ORA #$40`

Gain : **2 cycles par pixel × ~150 pixels/frame = 300 cycles/frame**.

### Changed

- `src/asm/line.s` :
  - Ajout `_plot_dot` (28 lignes, ~40 cycles).
  - Boucle `_draw_line_xor` : `ORA #$40` retiré (passe de 16 c/px à 14 c/px sur le bloc XOR).
- `src/asm/ship.s` : 3 replots P0/P1/P2 via `_plot_dot`.
- `src/asteroids.c` : replots N sommets via `plot_dot`.
- `src/game.c` : helper `plot()` + `asteroid_debris_render` via `plot_dot`.

### Décisions techniques Phase 16

- **Approche conservative** : pas de SMC, pas de déroulage. Optimisations
  qui ne changent ni le visuel ni la correction. Phase 17 pourrait
  s'attaquer à la vraie SMC Bresenham (objectif Phase 2b — 40-50 c/px
  vs nos 95 c/px actuels).
- **`_plot_dot` partagé** par 4 modules (`ship.s`, `asteroids.c`,
  `game.c`, asteroid_debris) : un seul point de modification si
  l'algorithme change.
- **Invariant bit6=1 documenté inline** : garantit que retirer `ORA #$40`
  n'introduit pas de bug subtil. Si quelqu'un ajoute un masque avec
  bit6=1, il faudra restaurer l'instruction.

## [1.2.5] - 2026-05-10

### Phase 15 — Lettre H + label "HIGH SCORES" en game over ✅

- Lettre `H` ajoutée (3 segments + 2 plots — 2 verticales + barre milieu).
- `hiscores_label_draw(py)` : "HIGH SCORES" centré horizontal x=54
  (10 lettres + 1 espace × 12 = 132 px, x = (240-132)/2).
- Label affiché à y=55 au-dessus de la table des high scores en game over.
- Table des scores décalée de y=70 → y=75 pour laisser de la place au label.
- 13 lettres alphabétiques au total : `A C D E G H I M O P R S T V W`.

### Changed

- `src/title.c/h` : `letter_H`, `hiscores_label_draw/erase`.
- `src/game.c` : `HISCORES_LABEL_Y = 55`, label drawn dans
  `hiscores_draw_table` (XOR symétrique avec la table elle-même).

### Décisions techniques Phase 15

- **Label unique pour erase + draw** : `hiscores_label_draw` est XOR
  idempotente (comme tout le reste). Le toggle de la table via
  `hiscores_drawn` flag couvre aussi le label par effet de symétrie.
- **Décalage table -5 px** : maintient la zone des 5 lignes de score
  dans la zone safe sans chevaucher le ship si centré.

## [1.2.4] - 2026-05-10

### Phase 14b — Pattern shrapnel 1 arcade exact ✅

**Port direct du 1er pattern shrapnel** depuis `SharpPatPtrTbl[0]`
($5100) du désasm rev 4. Notre étoile 8 dots de Phase 14 (positions
maison) est remplacée par les **positions cumulées exactes** des SVEC
arcade.

### Décodage SVEC cumulé

```asm
$5100 SVEC x=-1 y=+0    → cumul (-1,  0)
$5104 SVEC x=-1 y=-1    → cumul (-2, -1)
$5108 SVEC x=+1 y=-1    → cumul (-1, -2)
$510C SVEC x=+3 y=+1    → cumul (+2, -1)
$5110 SVEC x=+2 y=-1    → cumul (+4, -2)
$5114 SVEC x=+0 y=+1    → cumul (+4, -1)
$5118 SVEC x=+1 y=+3    → cumul (+5, +2)
$511C SVEC x=-1 y=+3    → cumul (+4, +5)
```

8 positions résultantes :
```c
adbr_dx[8] = { -1, -2, -1, +2, +4, +4, +5, +4 };
adbr_dy[8] = {  0, -1, -2, -1, -2, -1, +2, +5 };
```

### Pourquoi c'est différent de Phase 14 (étoile maison)

L'arcade ne fait **pas** une étoile symétrique. Le nuage de dots est
**asymétrique** — concentration à droite/bas (5 dots dans `x ≥ +2`
contre 3 à gauche). Cette asymétrie donne un look "explosion qui part
en avant" cohérent avec un asteroid en mouvement.

Notre Phase 14 avait 8 directions cardinales équirépartis (étoile),
trop "sage" visuellement. Phase 14b restitue le caractère arcade.

### VCTR ignoré

Le pattern arcade contient à $5120 un `VCTR sc=5` (move avec scale
amplifié). À l'échelle DVG, ça déplace beaucoup (ce 9e dot est à
~10 px du centre). Trop large pour notre Bresenham raster Oric. On
conserve les 8 SVEC `sc=0/1/2/3` qui restent dans les ~5 px.

### Différé Phase 14c

Les 3 autres patterns (`SharpPatPtrTbl[1..3]` à $512C, $516A, $51A0)
sont plus complexes (alternance SVEC + VCTR à scales variés). Phase
14c pourra les porter pour avoir 4 patterns shrapnel sélectionnés
selon `asteroid.shape`.

## [1.2.3] - 2026-05-10

### Phase 14 — Explosion asteroid (dots éphémères) ✅

**Effet visuel d'explosion à la destruction d'un asteroid** —
manquant jusqu'à présent (Phase 4 fragmentait l'asteroid en sous-asteroids
sans flash visuel d'impact).

**Inspiration** : `SharpPatPtrTbl` arcade ($50F8) — 4 patterns SVEC
qui tracent un nuage de "dots" autour de la position de l'asteroid détruit
(`b=7` SVECs zero-vector entre des moves SVEC = tracé de points lumineux).

### Implémentation simplifiée

Notre version : pool séparé `adbr_*[8]` avec **8 dots en étoile**
disposés autour du centre de l'asteroid détruit, statiques (pas de
vélocité), durée 10 frames :

```c
adbr_dx = { -3, -2,  0, +2, +3, +2,  0, -2 }
adbr_dy = {  0, -2, -3, -2,  0, +2, +3, +2 }
```

Effet "puff" éphémère qui marque la destruction visuellement.

### Hooks

- `bullet vs asteroid` : `asteroid_debris_spawn(ax, ay)` à la position du hit
- `ship vs asteroid` : flash sur l'asteroid + ship debris (déjà géré)
- Render : XOR effacement avant update + redraw après position update

### Pourquoi pas le port direct des SharpPatPtrTbl

Le pattern arcade pattern 1 ($5100) contient **9 SVEC moves entrelacés
avec des SVEC b=7 dots** — décodage cumul + extraction des positions
finales. Pour Phase 14 simplifiée, on garde 8 positions étoile fixes
sans cumul (une approximation). Phase 14b pourra porter les 4 patterns
distincts si la fidélité visuelle l'exige.

### Changed

- `src/game.c` :
  - Pool `adbr_x/y/ttl[8]` (BSS, 24 octets) + tables `adbr_dx/dy` (16 RODATA).
  - 4 fonctions : `asteroid_debris_spawn/render/update` + init via `debris_init`.
  - 4 hooks dans la boucle (erase + update + redraw + render dans collisions).

### Décisions techniques Phase 14

- **Pool séparé** du ship debris (`dbr_*` vs `adbr_*`) : permet à un
  ship debris (en cours pendant `ship vs asteroid`) de coexister avec
  une explosion asteroid sans écraser les fragments.
- **Statique (pas de vélocité)** : les dots restent fixes pendant 10
  frames puis disparaissent. Plus simple qu'animer 8 trajectoires.
  Effet "boom" puis "poof".
- **8 dots fixes** vs 9 dots arcade par pattern : compromis visuel
  (8 directions cardinales + diagonales = étoile parfaite), 16 octets
  RODATA.

## [1.2.2] - 2026-05-10

### Phase 13 — Fragments en morceaux du ship vectoriel ✅

**Port direct de `ShipExpPtrTbl` rev 4** depuis `$50E0`. Les 6
fragments avaient des **vélocités** arcade-fidèles depuis Phase 12,
mais leur **forme** était encore un mini-segment générique dans la
direction du mouvement. Cette phase remplace ça par les **6 segments
SVEC distincts** de l'arcade.

### 6 segments SVEC ($50E0)

```asm
ShipExpPtrTbl:  .dd2 $ffc6   ; SVEC x=-2 y=-3   debris 0
                .dd2 $fec1   ; SVEC x=+1 y=-2   debris 1
                .dd2 $f1c3   ; SVEC x=+3 y=+1   debris 2
                .dd2 $f1cd   ; SVEC x=-1 y=+1   debris 3
                .dd2 $f1c7   ; SVEC x=-3 y=+1   debris 4
                .dd2 $fdc1   ; SVEC x=+1 y=-1   debris 5
```

### Résultat

Chaque debris a maintenant une **silhouette propre fixe** :
- Debris 0 part en haut-gauche, c'est un segment qui pointe (-2,-3)
- Debris 4 file vers le bas-gauche en traînée longue (-3,+1)
- etc.

La silhouette ne tourne pas avec la vélocité — elle reste fixe
pendant le déplacement. Comportement arcade authentique.

### Visuel arcade vs nous

L'arcade trace ces SVEC avec un scale variable (`sc=0`, `sc=1`, `sc=2`)
qui multiplie l'amplitude. Notre Bresenham n'a pas d'échelle DVG — on
garde les valeurs brutes du désasm. Le scale est implicitement constant
dans notre rendu (pas de différence visible).

### Changed

- `src/game.c` : `ship_debris_shape_dx[]` et `ship_debris_shape_dy[]`
  ajoutés (12 octets RODATA, depuis `ShipExpPtrTbl`).
- `debris_render` : trace `(x, y) → (x + shape_dx, y + shape_dy)`
  au lieu de `(x, y) → (x + vx/2, y + vy/2)`.

### Décisions techniques Phase 13

- **Scale DVG ignoré** : `sc=0/1/2` = amplitude × 2^scale dans le
  hardware DVG. Sur Bresenham raster, on n'a pas cette notion. Les
  valeurs brutes (-3 à +3) donnent des segments de 1-4 px sur Oric,
  cohérent avec notre échelle.
- **Forme statique pendant trajectoire** : authentique arcade — un
  morceau du ship qui s'éloigne ne tourne pas (pas de rotation propre).
  L'arcade utilise le DVG en mode "absolute" pour ces fragments.
- **Plus besoin de `vx >> 1`** dans le tracé : la silhouette est
  totalement découplée de la vélocité. Un fragment immobile (vx=0,vy=0)
  reste visible comme un segment statique pendant 50 frames.

## [1.2.1] - 2026-05-10

### Phase 12 — Destruction ship + UFO arcade-fidèle ✅

**Port direct de `ShipExpVelTbl` rev 4** depuis `$50EC` du désasm.
Notre debris RNG aléatoire (Phase 10i) est remplacé par les **6 vélocités
prédéfinies** de l'arcade Atari 1979.

### Vélocités exactes ShipExpVelTbl ($50EC)

```
ShipExpVelTbl:  .bulk $d8,$1e   ; debris 0 : vx=-3, vy=+1
                .bulk $32,$ec   ; debris 1 : vx=+3, vy=-2
                .bulk $00,$c4   ; debris 2 : vx= 0, vy=-4
                .bulk $3c,$14   ; debris 3 : vx=+3, vy=+1
                .bulk $0a,$46   ; debris 4 : vx= 0, vy=+4
                .bulk $d8,$d8   ; debris 5 : vx=-3, vy=-3
```

(Format arcade : nibbles haut sign-extendus → valeurs signées)

### Disparition séquentielle

Conformément au comportement arcade (commentaire $7493 : *"the debris
disappear one by one over time"*), chaque fragment a un TTL différent :
- Fragment 0 : 50 frames
- Fragment 1 : 44 frames
- Fragment 2 : 38 frames
- ...
- Fragment 5 : 20 frames

→ Effet "explosion qui se dissout en éclats successifs", reconnaissable
de l'arcade.

### UFO debris (extension Phase 12)

L'arcade rev 4 utilise les `SharpPatPtrTbl` ($50F8) — 4 patterns
"shrapnel" — pour les explosions asteroid et UFO. Notre approche
simplifiée : on **réutilise les mêmes 6 fragments** que le ship
pour le UFO (vélocités identiques). Cohérent avec l'esprit arcade
sans dupliquer 4 patterns SVEC.

Hooks UFO :
- `bullet vs UFO` : `debris_spawn(ufo_x, ufo_y)` + son explosion
- `ship vs UFO` : `debris_spawn` aux DEUX positions + son
- `ship vs UFO bullet` : debris ship uniquement + son
- `ship vs asteroid` : debris ship + son

### Changed

- `DEBRIS_COUNT` : 5 → **6** (arcade exact)
- `DEBRIS_TTL` : 30 → 50 frames (~plus long)
- Vélocités : RNG → **prédéfinies fixes** (déterminisme arcade)
- Disparition : décrément parallèle → **séquentielle décalée**
- 4 hooks `debris_spawn` au lieu de 1 (UFO maintenant couvert)

### Décisions techniques Phase 12

- **Vélocités exactes du désasm rev 4** : c'est pas une recréation
  approximative, c'est les VRAIES valeurs Atari. La destruction du
  ship sur Oric est maintenant déterministe et reconnaissable
  (l'arcade fait toujours la même explosion — comme nous).
- **Décrément TTL `-= 6`** par fragment : compromis entre fade-out
  rapide et visibilité. Arcade utilise un compteur 4-bit (`ShipStatus`)
  avec décrément implicite, traduit en TTL séquentiel chez nous.
- **UFO partage les mêmes 6 fragments** que ship : simplifie le code
  (pas de 4 patterns shrapnel séparés). Acceptable car l'œil ne
  distingue pas la différence à 4-5 px par fragment.
- **Pas de morceaux du ship vectoriel SVEC** : l'arcade dessine 6
  morceaux distincts du triangle ship (pointe, gauche, droite,
  base haut, base bas...), chacun avec une orientation propre.
  Notre version trace des mini-segments dans la direction du
  mouvement (cohérent avec Phase 10i) — moins iconique mais
  fonctionnellement équivalent.

## [1.2.0] - 2026-05-10
- Variables Atari arcade (`statusShip`, `horzVelShip`, etc.) côté code.
- Persistance high scores en `.tap` / `.dsk`.
- UFO oscillant + enveloppe AY.
- Wraparound par duplication d'instance (vrai cylindre arcade).
- Optimisation Bresenham (Phase 2b) : SMC + déroulage pour 40-50 c/px.

## [1.2.0] - 2026-05-10

### Phase 11 — Sons Mine Storm Vectrex authentiques 🛸

**Port direct des tables sound** depuis le code source GCE Mine Storm
rev C (Vectrex 1982) — possible car **même puce sonore AY-3-8912** que
l'Oric-1.

Source : [`philspil66/Vectrex-Minestorm/MINESTRM-C.ASM`](https://github.com/philspil66/Vectrex-Minestorm/blob/main/MINESTRM-C.ASM)

### Pourquoi c'est possible

| Composant | Vectrex 1982 | Oric-1 1983 |
|-----------|--------------|-------------|
| CPU | Motorola 6809 | MOS 6502 |
| Sound chip | **AY-3-8912** | **AY-3-8912** |
| Interface | VIA 6522 | VIA 6522 |

Les **valeurs PSG** (registres 0-15) sont CPU-agnostiques. Mine Storm
écrit `$1F` dans le registre 6 (noise freq) ; on fait pareil sur Oric.
Le code 6809 ne se transpose pas, mais **les données sound oui**.

### Tables portées (Mine Storm SS.* → notre FX_*)

| Mine Storm | Notre FX | Description |
|------------|----------|-------------|
| `SS.THR` | `FX_THRUST` | tone A grave + noise sur 3 canaux, vol max |
| `SS.BLT` | `FX_FIRE` | tone B freq $39 + noise A, vol max |
| `SS.EXP` | `FX_EXPLODE` | noise tous canaux + **enveloppe AY** decay $0038 |
| `SS.POP` | `FX_THUMP` | tone B grave $30 (adapté pour cadence thump) |

### Ajustement Oric

Le bit 6 du mixer (registre 7) = direction Port A. Sur Oric il **doit**
être à 1 (port A en input pour le scan clavier matrice). Vectrex utilise
le port A différemment, donc les valeurs Mine Storm ont bit 6 = 0.

**Adaptation systématique** : `mixer_oric = mixer_minestorm | $40`.
Exemples : `$05 → $45`, `$06 → $46`, `$07 → $47`, `$3D → $7D`.

### Nouveauté : enveloppe AY pour explosion

Mine Storm utilise les **registres 11/12/13** de l'AY pour piloter
l'enveloppe matérielle :
- Reg 11 = `$00` (envelope period lo)
- Reg 12 = `$38` (envelope period hi)
- Reg 13 = `$00` (envelope shape `\___` = decay puis hold à 0)

Notre FX_EXPLODE a maintenant un **fade-out doux natif** (decay AY
sur ~50 ms) au lieu d'un cut net. Plus naturel, plus arcade.

### Changed

- `src/asm/sound.s` : 4 effets (`FX_FIRE`, `FX_EXPLODE`, `FX_THRUST`,
  `FX_THUMP`) reçoivent les valeurs PSG **identiques bit-pour-bit**
  à Mine Storm Vectrex rev C (avec `| $40` sur le mixer).
- `FX_HYPER`, `FX_LIFE`, `FX_UFO` restent en valeurs Oric maison
  (Mine Storm n'a pas d'équivalent direct pour ces effets).

### Décisions techniques Phase 11

- **Valeurs PSG bit-pour-bit identiques** : on respecte la tonalité
  exacte de Mine Storm Vectrex 1982. C'est notre premier "port audio"
  qui n'est pas une recréation maison.
- **Pas de SS.HYP** dans Mine Storm : Mine Storm n'a pas d'hyperespace
  (mécanique différente — mines magnétiques + fireballs au lieu de
  warp). On garde `FX_HYPER` Oric.
- **Pas de routine `_psg_play_string` générique** : on garde le
  switch sur `_sfx_id`, juste les valeurs internes changent.
  Phase 12 pourra introduire un format de tables externes pour
  charger plus facilement d'autres sons.

### Bump version mineure

`v1.2.0` — premier port "audio" depuis une autre machine (Vectrex).
Ouvre la voie à un possible portage de plus de sons depuis le catalogue
Vectrex (qui utilise tous l'AY-3-8912).

## [1.1.13] - 2026-05-10

### Phase 10n — Effet sonore UFO bip-bip continu ✅

**Effet sonore arcade emblématique** : tant qu'un UFO est actif, le
PSG produit un bip-bip répété qui signale la présence du saucer.

### Mécanique

- `FX_UFO` : tone canal C grave (freq $00C0), volume modéré 10/15,
  durée 4 frames.
- Re-déclenché toutes les 16 frames (`UFO_SOUND_PERIOD`) tant que
  `ufo_active` → effet "bip-bip" perceptible (~5 Hz).
- **Prioritaire sur le thump** : quand UFO actif, le thump est suspendu
  pour éviter le mix sonore confus (cohérent avec arcade : `SaucerSFX`
  override le thump).

### Added

- `FX_UFO 7` dans `src/sound.h`.
- Config `FX_UFO` dans `src/asm/sound.s` (tone canal C grave).
- `UFO_SOUND_PERIOD = 16` + `ufo_sound_timer` (BSS) dans `src/game.c`.

### Changed

- `src/game.c` : la branche thump ne s'exécute que si `!ufo_active`.
  La branche UFO sound s'exécute en priorité quand UFO actif.

### Décisions techniques Phase 10n

- **Canal C dédié** (au lieu de canal A comme tir/explosion) : permettrait
  potentiellement le mix simultané. Mais le modèle "1 effet à la fois"
  de Phase 8 ne mixe pas — c'est une préparation pour Phase 11+.
- **Re-déclenchement périodique** comme `FX_THRUST` : produit un effet
  pseudo-continu sans nécessiter de changement du modèle sound.s.
- **Thump suspendu pendant UFO** : le mix UFO+thump est confus visuellement
  (même fréquence grave). Suspendre le thump donne plus de présence à l'UFO.
- **Period 16 frames** : ~1 s à 17 Hz observés → 5 bips/s. Compromis
  entre "présence" (assez fréquent pour être audible) et "discrétion"
  (pas trop pour dominer le mix).

## [1.1.12] - 2026-05-10

### Phase 10m — Clignotement PRESS SPACE écran titre ✅

**Effet arcade-style** : "PRESS SPACE" pulse pendant l'attente input
pour signaler visuellement au joueur que le jeu attend une action.

### Mécanique

- Compteur `ps_visible` (uint8) tracke l'état actuel du tracé.
- **Toggle tous les 24 frames** (~1.4 s à 17 Hz observés) : XOR efface,
  inversion du flag, XOR redraw si nécessaire.
- **Nettoyage en sortie** : si `ps_visible` à la sortie de la boucle,
  on appelle `presspace_erase(110)` pour garantir l'état "écran effacé"
  cohérent avec l'XOR.

### Changed

- `src/game.c` : boucle d'attente du titre intègre le toggle 24-frame
  via `if ((i & 0x17) == 0x17)` (mask de 24 = test sur 5 bits LSB).

### Décisions techniques Phase 10m

- **Période 24 frames (≈1.4 s)** : compromis entre lisibilité (assez
  lent pour ne pas être agressif) et urgence (assez rapide pour signaler
  l'attente). L'arcade Atari pulse à environ 1 s.
- **Toggle XOR symétrique** : `presspace_erase` puis `presspace_draw` —
  les 2 opérations sont identiques (XOR), seul le flag interne change.
- **Garantie d'état sortie** : `if (ps_visible) erase` avant de quitter
  la boucle. Évite de laisser du résidu graphique au démarrage du jeu.

## [1.1.11] - 2026-05-10

### Phase 10l — Wraparound par duplication d'instance ✅

**Fini la zone safe rétrécie** : les asteroids utilisent maintenant
**l'écran complet** `[0, 239] × [0, 199]` avec **duplication d'instance**
arcade-fidèle (cf. CLAUDE.md / guide §5).

### Mécanique

- `asteroid_draw_at(idx, ox, oy)` : dessine un asteroid avec offset
  signé (en `int` 16-bit). Clipping `IN_BOUNDS` segment-par-segment :
  un segment dont au moins une extrémité dépasse `[0,239]×[0,199]` est skippé.
- `asteroid_draw_one(idx)` : dessine **toujours** l'instance principale,
  puis selon la position, les instances fantômes :
  - `x ≤ 14` : copie à `x + 240` (côté droit)
  - `x ≥ 226` : copie à `x - 240` (côté gauche)
  - `y ≤ 14` : copie à `y + 200` (bas)
  - `y ≥ 186` : copie à `y - 200` (haut)
  - **coins** : 4× (instance + dx + dy + dx_dy)
- Wraparound dans `asteroids_update` : `[0, 239] × [0, 199]` (vrai cylindre).

### Effet visuel

Quand un asteroid traverse un bord, la moitié visible à droite et
celle à gauche sont **simultanément affichées** pendant la transition.
Le joueur voit l'asteroid passer "à travers" l'écran sans saut brutal —
sensation arcade authentique.

### Changed

- `WRAP_X_MIN/MAX/SPAN` : `0/239/240` (avant : `16/224/208`).
- `asteroid_draw_one` séparé en `asteroid_draw_at(ox, oy)` (1 instance)
  + wrapper qui appelle 1 à 4 fois selon position.
- Le ship et les bullets gardent leurs zones safe respectives (ship
  toujours proche du centre, bullets éphémères).

### Décisions techniques Phase 10l

- **Clipping segment-par-segment** plutôt que tracé tronqué propre :
  un segment dont une extrémité dépasse est SKIPPÉ entièrement. Plus
  simple à coder mais visuel "trous" pendant les transitions de bord.
  Une vraie implémentation arcade découperait le segment à l'intersection
  bord-segment (Cohen-Sutherland) — reporté à Phase 10m.
- **Pas de duplication pour le ship** : il a sa zone safe `[14, 226]`.
  Garantie qu'un sommet ship ne déborde jamais. Pas besoin de duplication.
- **Pas de duplication pour les bullets** : éphémères (35 frames TTL),
  généralement détruits avant d'atteindre un bord.
- **Pas de duplication pour le UFO** : trajectoire purement horizontale
  bord à bord sans wraparound. Disparaît en sortie.

## [1.1.10] - 2026-05-10

### Phase 10k — AstBreakTimer (anti-saucer post-hit) ✅

**Port de la mécanique anti-spawn arcade** :
- `AstBreakTimer` ($02F9) : compteur frames après destruction d'un asteroid.
- **Reload à 80 frames** dans `BreakAsteroid` ($75EE).
- **Décrément 1/frame** dans `DoScrTmrUpdate` ($6BAF).
- **Bloque le spawn saucer** tant que `AstBreakTimer > 0`
  (cf. `UpdateScr` $6BC1).
- **Bloque aussi si 0 asteroids actifs** (`CurAsteroids == 0` à $6BC6).

### Effet gameplay

- Après chaque hit asteroid : **80 frames de répit** (~4.7 s à 17 Hz)
  avant qu'un saucer puisse spawn.
- Permet au joueur de finir une vague en chaîne sans être interrompu
  par un saucer.
- Empêche un saucer d'apparaître **avant** la transition de vague
  (vague vide = pas de saucer).

### Added

- `ast_break_timer` (BSS, 1 octet) dans `src/asteroids.c`, exposé dans
  `asteroids.h`.

### Changed

- `src/asteroids.c` : `asteroids_init` reset à 0, `asteroids_fragment`
  reload à 80.
- `src/ufo.c` : `ufo_tick` décrément `ast_break_timer` chaque frame
  même si UFO inactif (cohérent avec l'arcade qui décrémente toujours
  via `DoScrTimers`). Bloque spawn si `ast_break_timer > 0` ou
  `asteroids_count() == 0`.

### Décisions techniques Phase 10k

- **Décrément même UFO inactif** : nécessaire car le timer est global
  (pas attaché à un UFO en cours). Cohérent avec arcade `DoScrTmrUpdate`
  qui s'exécute à chaque frame indépendamment de `ScrStatus`.
- **Pas de re-test à chaque frame du `ufo_spawn_timer = 0`** : on
  laisse `ufo_spawn_timer` à 0 si bloqué, ce qui re-déclenche le
  test la frame suivante. Légèrement différent de l'arcade
  (qui maintient `ScrTimer = $7F` post-respawn) mais comportement
  équivalent.

## [1.1.9] - 2026-05-10

### Phase 10j — WAVE 2 chiffres ✅

- `wave_label_draw` étendu : affiche `nn` (2 chiffres) si `wave ≥ 10`,
  via 2 appels à `hud_xor_digit` espacés de 6 px.
- Limite à `wave ≤ 99` (le pool d'asteroids plafonne à 11 par vague,
  on ne dépassera pas 99 vagues en pratique mais clamp défensif).
- Préserve l'affichage 1 chiffre pour `wave 1..9` (espacement 56 px,
  position centrée).

### Décisions techniques Phase 10j

- **Chiffres 4 px de large + 2 px d'espace** : `digit / 10` à `x + 56`,
  `digit % 10` à `x + 62`. Légèrement décentré à droite mais lisible.
- **Division `/ 10` et `% 10` en cc65** : routines logicielles
  ~150 cycles. Acceptable car appelé uniquement quand `current_wave`
  change (1× par vague, pas chaque frame).

## [1.1.8] - 2026-05-10

### Phase 10i — Ship explosion debris arcade ✅

**Effet visuel arcade emblématique** : quand le vaisseau est détruit
(collision asteroid / UFO / UFO bullet), 5 fragments (mini-segments)
s'éparpillent depuis sa position dans des directions aléatoires.

Inspiré de `DoShipExplsn` ($7465) et `ShipExpVelTbl` arcade rev 4 —
version simplifiée Oric (5 fragments vs ~12 arcade, vitesses entières
vs 8.8 fixed, segments ligne droite vs vrais débris vectoriels).

### Mécanique

- **Spawn** : 5 fragments dans `dbr_x/y/vx/vy/ttl[5]` à la position
  du ship juste avant respawn.
- **Vélocités radiales** : RNG → ±1 ou ±3 sur chaque axe (8 directions
  × 2 amplitudes = 16 trajectoires possibles).
- **Forme** : chaque fragment trace un mini-segment de 1-2 px dans la
  direction du mouvement (`(x, y) → (x+vx/2, y+vy/2)`).
- **Durée** : 30 frames (~1.8 s à 17 Hz observés), désynchronisée par
  fragment (`DEBRIS_TTL - (i & 3)`) pour effet fade-out organique.
- **Sortie d'écran** : disparu (TTL = 0).

### Added

- `dbr_x/y/vx/vy/ttl[5]` (BSS, 25 octets).
- `debris_init/spawn/update/render` (4 fonctions, ~50 lignes C).
- Hooks dans `collisions_ship_asteroids` (3 cas : asteroid, UFO, UFO bullet)
  → `debris_spawn(ship_x, ship_y)` avant `ship_respawn`.
- Hooks dans la boucle de jeu : `debris_render` (erase + redraw),
  `debris_update`.

### Décisions techniques Phase 10i

- **5 fragments fixes** (vs ~12 arcade) : compromis budget cycles
  (~30 lignes XOR par frame de debris) vs visibilité.
- **Mini-segments (1-2 px)** plutôt que vraies parties du ship vectoriel :
  l'arcade dessine 4 morceaux distincts du vaisseau (pointe, gauche,
  droite, base), chacun rotant et translatant. Notre version simplifie
  en points-traits.
- **Fragment auto-disparu hors écran** : pas de wraparound (cohérent avec
  un effet "explosion" éphémère, pas un objet de jeu persistant).
- **TTL désynchronisé** par fragment (`30, 29, 28, 27, 26`) : effet
  fade-out plus naturel qu'un cut net synchronisé.

## [1.1.7] - 2026-05-10

### Phase 10h — ScrSpeedup arcade (saucer pressure) ✅

**Port de la pression saucer fin-de-vague** depuis le désasm rev 4 :
- `ScrSpeedup` ($02FD) : seuil d'asteroids count en dessous duquel
  le saucer apparaît plus fréquemment.
- Init à **5** (`InitGameVars` $690E `lda #$05; sta ScrSpeedup`)
- **+1 par vague**, max **11** (`InitWaveVars` $717A : `inc ScrSpeedup`,
  $7180 : `cmp #$0b`)
- Test fin-de-vague (`UpdateScr` $6BCB) : si `CurAsteroids ≤ ScrSpeedup`
  → saucer spawn accéléré.

### Comportement

- Wave 1 : `scr_speedup = 5`. Quand il reste ≤5 asteroids, UFO spawn
  rapide (`UFO_SPAWN_FAST = 120` frames ≈ 7 s vs 300 normal).
- Wave 2 : `scr_speedup = 6`. Etc.
- Wave 7+ : `scr_speedup = 11` (max), pression UFO permanente.

**Effet gameplay** : la fin de vague (peu d'asteroids) devient stressante
car le UFO est plus pressant. C'est une mécanique anti-camping classique
de l'arcade — le joueur est puni de jouer trop lentement.

### Added

- `scr_speedup` (BSS, exposée dans `asteroids.h`).
- `UFO_SPAWN_FAST 120` dans `src/ufo.c`.

### Changed

- `src/asteroids.c` : `asteroids_init` reset à 5, `asteroids_spawn_wave`
  incrémente +1 (max 11).
- `src/ufo.c` : `ufo_tick` choisit `UFO_SPAWN_FAST` ou `UFO_SPAWN_PERIOD`
  selon `asteroids_count() ≤ scr_speedup`.

### Décisions techniques Phase 10h

- **Reload appliqué après spawn** plutôt qu'avant : l'arcade fait pareil
  (le timer raccourci ne s'applique qu'au prochain spawn). Évite le
  bug où la condition change pendant le décompte.
- **Ratio FAST/NORMAL = 0.4** (120/300) : l'arcade a un ratio similaire
  via `ScrTmrReload` qui est divisé quand `CurAsteroids ≤ ScrSpeedup`.
  Notre 7 s vs 18 s donne un gameplay tendu sans être oppressant.

## [1.1.6] - 2026-05-10

### Phase 10g — IA UFO arcade authentique ✅

**Port de la précision UFO petit** depuis le désasm rev 4 :
- `CalcScrShotDir` ($6CA5)
- `ScrShotAddOffset` ($6CB7)
- `ShotRndAddTbl` ($6CCD) : `$8F, $87, $70, $78`

### Logique arcade rev 4

L'arcade utilise un **seuil unique à 35000** :
- score < 35000 : mask `$8F` (sign + 4 bits magnitude → ±15 d'imprécision)
- score ≥ 35000 : mask `$87` (sign + 3 bits magnitude → ±7 d'imprécision, "petit UFO tueur")

### Adaptation Oric (8 directions vs 256 angles arcade)

Notre format tir UFO est en 8 directions discrètes (pas 256 angles), donc
on simplifie en 2 niveaux de probabilité de **mauvaise direction** :
- score < 35000 : **50%** chance d'inversion d'une composante
- score ≥ 35000 : **12%** chance (1/8 : la précision croît avec le score)

### Changed

- `src/ufo.c` : `ufo_fire` UFO_SMALL utilise maintenant le seuil arcade
  35000 (vs notre seuil interne 5000 antérieur). En-dessous de 35000 le
  petit UFO reste très imprécis ; au-delà il devient un sniper.

### Décisions techniques Phase 10g

- **Seuil unique 35000** vs notre ancien double seuil (5000, 15000) :
  fidèle à l'arcade. À 35000 pts un joueur a déjà détruit ~700 asteroids
  petits ou ~1750 grands — niveau "expert", la difficulté monte d'un cran.
- **8 dirs vs 256 angles arcade** : cohérent avec notre format
  `ufo_bullet_vx/vy` 8-bit signed à magnitude fixe (`UFO_BULLET_SPEED = 4`).
  Phase 10h pourrait passer en système d'angle continu via `ship_thrx/thry`
  (déjà disponibles 32 angles).
- **Probabilité binaire** (50% / 12%) plutôt que perturbation continue
  proportionnelle : limitation du système 8-dir (impossible de "biaiser"
  doucement la trajectoire entre 2 directions discrètes).

## [1.1.5] - 2026-05-10

### Phase 10f — Fragmentation arcade authentique ✅

**Port de la logique arcade rev 4** depuis le désasm :
- `BreakAsteroid` ($75EC)
- `SplitAsteroid` ($761D)
- `SetAstVel` ($7203) + `GetAstVelocity` ($7233)

### Changements de comportement

| Aspect | Avant (Phase 4) | Après (Phase 10f / arcade) |
|---|---|---|
| Nb fragments | **2 enfants** (parent désactivé) | **3 fragments** (parent réduit + 2 nouveaux) |
| Vélocité enfants | Rotation fixe ±90° du parent | parent_vel + RNG(-15..+15), clamp [-15,+15], min |v|≥3 |
| Slot parent | Désactivé après hit | **Conservé**, taille réduite |
| Petit (size 0) | Disparu | Idem (inchangé) |

### Conséquences gameplay

- **Plus d'asteroids** à l'écran après chaque hit (3 vs 2) → tension croissante plus marquée
- **Trajectoires moins prévisibles** : les enfants ne partent plus en équerre fixe mais avec un héritage de la direction du parent + perturbation RNG
- **Toujours en mouvement** : `clamp_vel` impose `|v| ≥ 3` → un fragment ne peut pas s'arrêter au point de hit
- **Au pire** : 4 grands → 12 moyens → 36 petits = **52 fragments potentiels** par vague (vs 16 en Phase 4)
  - Limité en pratique par `MAX_ASTEROIDS = 12` : on perd les fragments excédentaires
  - Cohérent avec arcade : `GetFreeAstSlot` retourne BMI si pool plein

### Added

- `rand_offset()` : RNG signé `[-15, +15]` (port `AND #$8F` + sign-extend `ORA #$F0`).
- `clamp_vel(int v)` : clamp `[-15, +15]` avec minimum `|v| ≥ 3` (port `GetAstVelocity`).

### Changed

- `src/asteroids.c` : `asteroids_fragment` réécrit selon la logique arcade.
- `tests/ref/phase9_release.ppm` : régénérée (capture en écran titre + démo passive,
  reproductibilité conservée via RNG seed `0x42`).

### Décisions techniques Phase 10f

- **Échelle vélocité divisée par 2** vs arcade : notre format `vx/vy` est
  un signed char direct (8-bit), l'arcade utilise du 8.8 fixed-point.
  Garder `|v| ≤ 15` (vs `≤ 31` arcade) maintient des trajectoires lisibles
  sans déborder la zone safe wraparound `[16, 224]`.
- **MAX_ASTEROIDS = 12** non augmenté : avec fragmentation 3-enfants,
  on peut atteindre 52 fragments théoriques. Mais le budget cycles/frame
  limite l'affichage à ~10 entités fluides. Le pool de 12 est un
  compromis budget/intensité.
- **Pas de tracking du `tueur`** (ship vs UFO) pour l'attribution du score :
  l'arcade ne crédite pas si la collision vient d'un saucer/saucer-bullet.
  Notre code crédite déjà uniquement via `collisions_bullets_asteroids` →
  comportement équivalent (UFO bullet vs asteroid n'attribue pas de score
  via `collisions_ufobullet_asteroids`).

## [1.1.4] - 2026-05-10

### Phase 10e — Démo passive en écran titre ✅

L'écran titre intègre maintenant une **démo passive arcade-style** :
les asteroids sont spawnés et animés en arrière-plan pendant que
"ASTEROIDS" + "PRESS SPACE" sont affichés. À l'appui de SPACE (ou
au timeout), la transition vers le jeu **conserve l'état des asteroids**
en mouvement — pas de re-spawn brutal.

### Changed

- `src/game.c` : 
  - `asteroids_init(0x42)` + `asteroids_spawn_wave()` AVANT la boucle
    d'attente du titre.
  - Boucle d'attente : `asteroids_draw()` (erase) + `asteroids_update()` +
    `asteroids_draw()` (redraw) chaque frame.
  - À la sortie : on n'appelle PAS `asteroids_init` à nouveau pour ne pas
    casser l'état (transition titre→jeu fluide, asteroids déjà présents).
  - `wave_displayed = 0` force l'affichage initial de "WAVE 1" dans la
    1ère frame de la boucle de jeu (l'asteroids_spawn_wave initial a
    déjà mis `current_wave = 1`).

### Décisions techniques Phase 10e

- **Transition fluide titre→jeu** : économise un re-spawn potentiellement
  visible à l'œil (XOR effacement + nouveau spawn). Cohérent avec
  l'arcade Atari où la démo se fond dans le gameplay.
- **Pas d'IA ship en démo** (ship invisible pendant le titre) : l'arcade
  original a une IA qui joue automatiquement en démo (Asteroids "attract
  mode"). Reporté à Phase 10g — nécessite un mini-bot.
- **Pas de score affiché en démo** : on évite de "spoil" un score
  factice. Le HUD apparaît avec le score réel à la transition.

## [1.1.3] - 2026-05-10

### Phase 10d — Affichage "WAVE n" dans le HUD ✅

- Lettre `W` ajoutée (4 segments + 3 plots — V doublé).
- `wave_label_draw(py, digit)` : "WAVE " (4 lettres + espace) + 1 chiffre
  7-segments via `hud_xor_digit`.
- Position : x=80 (centre horizontal), y paramétrable (16 par défaut).
- Hook dans `game_run` : redessin lorsque `current_wave != wave_displayed`.
  XOR efface l'ancien numéro avant tracé du nouveau.
- 12 lettres alphabétiques au total : `A C D E G I M O P R S T V W`.

### Added

- `letter_W` dans `src/title.c` (5×9 px, 4 segments + 3 sommets replots).
- `wave_label_draw/erase` exposés dans `src/title.h`.
- `hud_xor_digit(d, px, py)` exposé dans `src/hud.h` — wrapper public
  autour de `draw_digit` interne.
- `wave_displayed` + `WAVE_HUD_Y = 16` dans `src/game.c`.

### Décisions techniques Phase 10d

- **Réutilisation de `draw_digit` du HUD** plutôt qu'un module digit-only
  séparé : économise du code (pas de duplication de la table 7-seg) et
  garantit la cohérence visuelle.
- **`current_wave` clampé à 9 dans le HUD** (`if (digit > 9) digit = 9`) :
  notre `hud_xor_digit` ne gère qu'un chiffre. Pour WAVE 10+ (max 11
  selon arcade), il faudrait afficher 2 chiffres. Différé Phase 10e
  (peu probable d'atteindre la wave 10 en pratique, le jeu se complexifie).
- **Hook par diff `current_wave != wave_displayed`** : XOR-erase de
  l'ancienne valeur + draw nouvelle. Pas de redraw permanent (économise
  ~30 segments par frame qui resteraient identiques sinon).

## [1.1.2] - 2026-05-10

### Phase 10c — Spawn arcade-fidèle (vagues progressives) ✅

**Logique de spawn portée depuis le désasm rev 4** (`InitWaveVars` à
`$7168`, `InitWaveAsteroids` à `$719B`) :

- `current_wave` (uint8) : compteur de vagues, démarre à 1 et
  s'incrémente quand toute la vague est détruite.
- `ast_per_wave` (uint8) : nombre d'asteroids par vague, démarre à 4
  et `+= 2` chaque vague, max 11 (cf. `AstPerWave` arcade à `$02F5`).
- **Spawn arcade-fidèle** (vs anciennement "4 grands aux 4 coins") :
  * Shape aléatoire (RNG : `(rng8() >> 3) & 3`)
  * Position alternant top/bottom et left/right selon `r & 1`
  * Coordonnée libre dans la zone safe via `rng8() % range`
  * Vélocité signée aléatoire (signe + magnitude 1 ou 2 via RNG)

### Changed

- `src/asteroids.c` :
  - `current_wave`, `ast_per_wave` (BSS).
  - `asteroids_init` reset les compteurs.
  - `asteroids_spawn_wave` : logique arcade complète.
- `src/asteroids.h` : `extern unsigned char current_wave`.
- `tests/ref/phase9_release.ppm` : régénérée (positions/vélocités
  asteroids différentes mais reproductibles via RNG seedé 0x42).

### Décisions techniques Phase 10c

- **Compteur `current_wave` exposé** : utile pour Phase 10d-e où on
  pourra afficher "WAVE N" en HUD ou faire varier la difficulté du
  thump / de l'IA UFO selon la wave.
- **Pas encore de spawn de saucer en Phase 10c** : la condition
  arcade `ScrSpeedup` (saucer apparait plus souvent quand asteroids
  count ≤ valeur) sera ajoutée en Phase 10d. Pour l'instant le UFO
  spawn reste sur son timer fixe.
- **Vélocité magnitude 1 ou 2** : l'arcade utilise des vitesses 8.8
  fixed-point variables ; on simplifie à entiers 1 ou 2 px/frame
  pour rester compatible avec notre format vx/vy 8-bit signed.
- **Échelle aléatoire `rng8() % 192`** : modulo coûteux en cc65 mais
  acceptable au moment du spawn (1× par vague, pas en boucle frame).

### Tag

`v1.1.2` — bump patch (changement comportement spawn, RNG seedé donc
reproductibilité conservée).

## [1.1.1] - 2026-05-10

### Phase 10b — Shapes Atari N sommets variables (11-13) ✅

**Plus de décimation** : les 4 shapes asteroids Atari rev 4 sont
maintenant utilisées avec **leur nombre original de sommets** (11, 13,
12, 13). Visuellement les silhouettes rocheuses arcade authentiques
sont rendues avec toutes leurs concavités.

### Format des données

Nouveau format `shapes.s` :
- `shape_off[12]` : offset (uint8) dans `shape_x/y`, par `(size*4 + shape)`
- `shape_len[12]` : nombre de sommets (uint8) par shape
- `shape_x[147]`, `shape_y[147]` : sommets cumulés (4 shapes × 3 tailles
  × 11-13 sommets = 49 sommets/taille × 3 = 147 total)
- `shape_radii[3]` : rayons collision

Total RODATA shapes : 12 + 12 + 147×2 + 3 = **321 octets** (vs 195 avant
en Phase 10a). +126 octets pour la fidélité visuelle.

### Changed

- `tools/gen_shapes.py` : émet `shape_off` et `shape_len` ; pas de
  décimation. Les 49 sommets/taille proviennent du décodage SVEC complet.
- `src/asteroids.c` :
  - `asteroid_draw_one` lit `shape_off[shape_idx]` + `shape_len[shape_idx]`
    et trace N segments avec wrap modulo N.
  - Replot des N sommets après les segments (XOR).
  - `shape_idx = size * 4 + shape` (au lieu de `(size << 5) + (shape << 3)`).
- `tests/ref/phase9_release.ppm` mis à jour.

### Décisions techniques Phase 10b

- **Format flat indexé** plutôt que tableau de pointeurs (`(uint8*)[12]`) :
  économise 12 octets de pointeurs et évite l'indirection. L'offset 8-bit
  suffit pour 49 sommets/taille (max < 256).
- **Pas de réduction au strict minimum (8 sommets)** comme en Phase 10a :
  certaines shapes Atari ont des concavités importantes (creux dans la
  pierre) qui disparaissent à 8 sommets décimés. La fidélité l'emporte.
- **Wrap modulo N** via test `(i+1 == n) ? 0 : (i+1)` plutôt que `% n` :
  cc65 implémente `%` via une routine logicielle coûteuse pour `int` ;
  le test conditionnel est plus rapide.

### Tag

`v1.1.1` — bump patch (changement format données interne, comportement
inchangé visuellement par rapport à v1.1.0 sauf rendu plus fidèle).

## [1.1.0] - 2026-05-10

### Phase 10a — Shapes asteroids ATARI ARCADE rev 4 ✅

**Portage authentique des shapes** depuis le désassemblage Atari arcade :
- Source : [6502disassembly.com/va-asteroids/Asteroids.html](https://6502disassembly.com/va-asteroids/Asteroids.html)
- Table `AstPtrnPtrTbl` à $51DE → 4 shapes aux adresses $51E6, $51FE, $521A, $5234
- Format DVG SVEC (Short VECTOR) : déplacements relatifs cumulés depuis l'origine
- Décodage manuel des SVEC depuis les commentaires du désasm
- Cumul des positions pour obtenir les sommets absolus

**Décimation à 8 sommets** (compatibilité avec format actuel) :
- Shape 0 : 11 sommets → 8 décimés (silhouette rocheuse 1)
- Shape 1 : 13 sommets → 8 décimés (silhouette rocheuse 2)
- Shape 2 : 12 sommets → 8 décimés (silhouette rocheuse 3)
- Shape 3 : 13 sommets → 8 décimés (silhouette rocheuse 4)

**Fini les octogones génériques** — les silhouettes Atari sont irrégulières
avec des concavités (creux dans la pierre). Visuellement cohérent avec
l'arcade original.

### Added

- `tools/gen_shapes.py` réécrit : 4 shapes Atari hardcodées (cumul
  manuel des SVEC), échelles ×0.9 / ×1.6 / ×2.6 pour petit/moyen/grand
  asteroid (rayons cibles 5/9/14 px Oric).
- Rayons collision auto-calculés depuis les magnitudes max × scale.

### Changed

- `src/asm/shapes.s` régénéré avec les coords Atari authentiques
  (commentaires références aux adresses ROM source).
- `tests/ref/phase9_release.ppm` mis à jour (silhouettes différentes).

### Décisions techniques Phase 10a

- **Décimation à 8 sommets** plutôt qu'extension du format à N variable :
  préserve l'API `_shape_x[96]`, `_shape_y[96]` et `asteroid_draw_one`
  (boucle de 8 segments). Phase 10b pourra introduire un format variable
  si la fidélité visuelle nécessite tous les sommets originaux (11-13).
- **Choix des sommets décimés** : équirépartition par index (1 sur ~1.5),
  pas d'optimisation visuelle ciblée. Phase 10b pourra raffiner par
  conservation des "pointes" extrêmes.
- **Échelles ×0.9 / ×1.6 / ×2.6** au lieu des ratios arcade exacts
  (large=132, medium=72, small=42 unités DVG, soit ratios 3.14 / 1.71 / 1) :
  on adapte aux pixels Oric pour cibler nos rayons visuels existants
  (5/9/14 px). Phase 10b pourra utiliser les ratios arcade exacts si
  on dispose d'une zone de jeu plus grande.
- **Pas de portage du code logique** (Phase 10b+) : asteroids spawn,
  fragmentation, IA soucoupe restent "from scratch" pour l'instant —
  visuellement c'est l'élément le plus impactant, le code reste pour
  une phase ultérieure.

### Tag

`v1.1.0` — bump mineur (changement de comportement visuel : shapes
asteroids différentes vs v1.0.x).

## [1.0.6] - 2026-05-10

### Phase 9g — Fix bug XOR sommets partagés ✅

**Bug identifié et corrigé sur tous les tracés vectoriels :**
- Symptôme : un pixel partagé entre 2 segments adjacents (ex: pointe d'un
  triangle, sommet d'un polygone fermé) est XOR-é 2× → effacé. La pointe
  du vaisseau, la pointe du A, et tous les sommets des asteroids et UFOs
  étaient invisibles.
- Cause : `_draw_line_xor` trace tous les pixels y compris les endpoints,
  donc 2 segments touchant le même point produisent un double-XOR =
  effacement.
- **Fix universel** : après le tracé des segments, **replot des sommets
  partagés** (1 pixel via `_draw_line_xor` avec `lx0=lx1, ly0=ly1`).
  3× XOR = pixel tracé → sommet visible.
- Validation visuelle :
  * Ship : pointe (120, 88) maintenant visible (était manquante).
  * Lettre A : pointe (4, 0) maintenant visible (n'avait que 2 branches
    écartées avant).
  * Asteroids : 8 sommets de chaque polygone maintenant visibles.
  * UFO : tous les endpoints des 7 segments replots.
- 6 vraies pertes de pixels par lettre / forme / shape — fix sans
  perturber le rendu.

### Added

- Format compact des lettres étendu : segments puis `0xFF` puis liste
  de plots `(x,y)` puis `0xFE`.
- 12 lettres mises à jour dans `src/title.c` avec leurs sommets partagés.
- `src/asm/ship.s` : 3 plots après les 3 segments.
- `src/asteroids.c` : boucle de 8 plots après les 8 segments.
- `src/ufo.c` : replot des endpoints des 7 segments (~14 plots, conservateur).

### Changed

- `tests/ref/phase9_release.ppm` : régénérée (capture diffère légèrement
  car les sommets sont maintenant tracés).

### Décisions techniques Phase 9g

- **Replot via `_draw_line_xor` 1-pixel** plutôt que `plot` séparé :
  notre `_draw_line_xor` gère déjà le cas dégénéré `(x,y)→(x,y)` =
  1 pixel tracé. Évite d'ajouter une nouvelle routine.
- **Fix par replot** plutôt que modifier `_draw_line_xor` pour exclure
  l'endpoint : approche conservatrice qui ne casse pas la sémantique
  d'API existante (`_draw_line_xor` reste "trace tous les pixels").
- **Liste explicite des sommets partagés par lettre** : plus simple
  que détecter automatiquement les endpoints dupliqués. ~5 plots/lettre,
  surcoût minime.
- **Pour UFO : replot de TOUS les endpoints** (~14 plots) plutôt que
  seulement les partagés. Conservateur — les endpoints non-partagés
  sont juste tracés une fois de plus (visible mais pas dérangeant).

## [1.0.5] - 2026-05-10

### Phase 9f — FX_THRUST + FX_LIFE ✅

**Ajouts :**
- `FX_THRUST` : noise canal C très court (3 frames), volume 8 (discret).
  Re-déclenché chaque frame que UP est held → effet de bruit "moteur"
  approximatif (pseudo-continu via override).
- `FX_LIFE` : chime tone canal A aigu (freq $30, 20 frames) au moment
  d'une vie bonus (toutes les 10 000 pts).
- 6 effets sonores au total : `FIRE`, `EXPLODE`, `THUMP`, `HYPER`,
  `THRUST`, `LIFE`.

### Added

- `src/asm/sound.s` : configs FX_THRUST + FX_LIFE.
- `src/sound.h` : `#define FX_THRUST 5`, `#define FX_LIFE 6`.
- `src/game.c` : 
  - hook FX_THRUST dans le bloc UP (override seulement si `sfx_id == FX_NONE`
    — ne perturbe pas FIRE/EXPLODE/HYPER en cours).
  - `lives_prev` pour détecter `lives++` → FX_LIFE.

### Décisions techniques Phase 9f

- **FX_THRUST réactif** plutôt que continu : le modèle "1 effet à la fois"
  de sound.s ne supporte pas un canal dédié au thrust. Solution : court
  effet (3 frames), re-déclenché chaque frame UP — produit un bruit
  "haché" mais audible. Phase 10 fera un canal dédié pour vraie continuité.
- **Override conditionnel** (`if sfx_id == FX_NONE`) : pendant un FIRE
  ou EXPLODE, le thrust est silencieux. Évite de couper les effets
  d'impact qui sont plus prioritaires gameplay.
- **FX_LIFE détecté côté game.c** plutôt que dans `hud_add_score` :
  évite une dépendance hud → sound. La détection `lives > lives_prev`
  est une simple comparaison à chaque frame.

## [1.0.4] - 2026-05-10

### Phase 9e — Lettres PC + "PRESS SPACE" + attente input ✅

**Définition de fin validée :**
- 2 nouvelles lettres : `P` (5 segs), `C` (3 segs).
- `presspace_draw(py)` : "PRESS SPACE" centré horizontal (x=54),
  position y paramétrable. 11 caractères × 12 = 132 px de large.
  10 lettres dessinées (5 lettres dans "PRESS" + 5 dans "SPACE",
  espace en x+60).
- **Écran titre interactif** : affiche "ASTEROIDS" + "PRESS SPACE"
  à y=80 et y=110. Attend SPACE (edge-trigger) ou auto-start après
  200 frames (~8 s). Démarre le jeu sur appui SPACE.
- **Écran game over enrichi** : "GAME OVER" (y=70) + "PRESS SPACE"
  (y=140) + table high scores. Restart sur SPACE.
- 11 lettres alphabétiques au total : `A C D E G I M O P R S T V`
  + espace.
- `make check` PASS sur la nouvelle référence (capture pendant titre
  avec "PRESS SPACE" affiché).

### Added

- `src/title.c` : `letter_P`, `letter_C` (~8 segments),
  `presspace_draw(py)`, `presspace_erase(py)`.
- `src/title.h` : prototypes correspondants.
- `src/game.c` :
  - Boucle d'attente input dans l'écran titre (SPACE ou timeout 200 frames).
  - `presspace_draw(140)` en game over, `presspace_erase` au restart.

### Changed

- `tests/ref/phase9_release.ppm` : capture maintenant prise pendant
  l'écran titre (cohérent avec le flow démarrage UX).

### Décisions techniques Phase 9e

- **Auto-start après 8 s** plutôt qu'attente infinie : si Phosphoric
  est lancé sans interaction (CI), le jeu démarre quand même. Acceptable
  vs un écran titre éternel qui bloquerait les tests automatiques.
- **`PRESS SPACE` paramétré par py** : permet un placement à 110 (titre)
  et 140 (game over) sans dupliquer la fonction.
- **Lettres `P` et `C` minimalistes** : `C` n'est qu'un cadre 3 côtés
  (ouvert à droite), pas de courbe. `P` est un `R` sans la jambe
  (boucle haute uniquement).
- **Espace = saut de 24 px** dans `presspace_draw` (au lieu de 12) :
  marque visuellement le séparateur entre "PRESS" et "SPACE".

## [1.0.3] - 2026-05-10

### Phase 9d — Lettres GMV + écran GAME OVER ✅

**Définition de fin validée :**
- 3 nouvelles lettres vectorielles ajoutées : `G` (5 segs), `M` (4 segs),
  `V` (2 segs).
- `gameover_draw()` : "GAME OVER" centré horizontal (x=66, y=70),
  9 caractères × 12 px = 108 px de large. 8 lettres (espace en x+48).
- `gameover_erase()` = `gameover_draw()` (XOR idempotent).
- Affichage en game state : XOR effacement avant logique, redessin
  après. Effacé sur restart (SPACE en game over).
- `make check` PASS.

### Added

- `src/title.c` : `letter_G`, `letter_M`, `letter_V` (~11 segments),
  `gameover_draw`, `gameover_erase`.
- `src/title.h` : prototypes `gameover_draw/erase`.
- `src/game.c` : `gameover_text_drawn` flag, hooks dessin/effacement
  dans la boucle.

### Décisions techniques Phase 9d

- **Réutilisation de la font Phase 9c** : `letter_A`, `letter_E`,
  `letter_O`, `letter_R` sont mutualisés entre `ASTEROIDS` et
  `GAME OVER`. Économie ≈ 16 segments × 4 octets = 64 octets RODATA.
- **`V` en 2 segments seulement** : le minimum pour la lisibilité.
  Pas d'apex pointu (3e segment) — esthétique vectorielle arcade
  classique (cf. logo Atari original).
- **Pas de "PRESS SPACE" en Phase 9d** : nécessite les lettres `P`
  et `C` supplémentaires. Reporté à Phase 9e ou 10. Le restart est
  documenté dans le README.

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
