# Portage d'Asteroids sur Oric‑1 48 Ko — Guide de développement

> Document de cadrage technique pour développer un clone de l'*Asteroids* arcade Atari (1979) en fil de fer sur Oric‑1 48 Ko, en C + assembleur 6502 via l'OSDK.

---

## 1. Présentation du projet

L'objectif est un clone fidèle d'*Asteroids* arcade (Atari, 1979) :
vaisseau triangulaire, astéroïdes polygonaux, soucoupe volante (grande et
petite), hyperespace, tirs, vagues progressives, table de high scores —
tout en **graphique vectoriel monochrome** (fil de fer) sur le mode HIRES
de l'Oric.

### Atout central : CPU 6502 commun

L'arcade Atari et l'Oric‑1 partagent **le même CPU MOS 6502**. Cela donne
un avantage rare pour un portage : la logique de jeu d'origine (machine
à états, physique, IA de la soucoupe, fragmentation des astéroïdes,
tables de scores, RNG) est **directement transposable** depuis le
désassemblage de la ROM arcade — modulo l'adaptation du rendu (DVG
vectoriel arcade → Bresenham XOR raster Oric) et de l'audio (oscillateurs
analogiques discrets arcade → AY‑3‑8912 Oric).

| Aspect | Source | Réutilisable ? |
|---|---|---|
| Logique de jeu (états, physique, fragmentation, IA soucoupe, RNG) | Désassemblage 6502 arcade rev 4 | **Code largement réutilisable** |
| CPU | 6502 ↔ 6502 | Idem, à modulo accès matériels |
| Shapes (listes de segments) | ROM arcade (données DVG) | Oui après extraction et reformatage |
| Rendu vectoriel | DVG/AVG matériel arcade | Non — Oric raster, simulation par Bresenham XOR |
| Audio | Oscillateurs analogiques discrets | **Non** — à recréer sur AY‑3‑8912 à partir des specs fonctionnelles |

---

## 2. Cible matérielle — Oric‑1 48 Ko

| Caractéristique | Valeur |
|---|---|
| CPU | MOS 6502 à 1,0 MHz |
| RAM | 48 Ko (3 Ko ULA + 45 Ko utiles) |
| ROM | 16 Ko (BASIC + système, $C000–$FFFF) |
| Mode graphique | HIRES 240 × 200, monochrome avec attributs couleur |
| Mémoire écran HIRES | 8 000 octets à $A000–$BF3F |
| Son | AY‑3‑8912 (3 voies + bruit), piloté via VIA 6522 |
| Entrées | Clavier matricé via VIA 6522 (port B) |
| Synchro vidéo | VBL ~ 50 Hz (PAL), accessible via timer VIA |

### Particularité du mode HIRES Oric

Chaque ligne fait **40 octets** = 240 pixels (6 pixels par octet). Le type de chaque
octet est déterminé par le test `(byte & 0x60) == 0` (bits 6 ET 5 tous deux à zéro) :

- `(byte & 0x60) == 0` → **octet attribut** : modifie INK / PAPER pour la suite de la
  ligne ; ses 6 positions ne s'affichent pas comme pixels (couleur de fond visible).
- `(byte & 0x60) != 0` → **octet pixel** :
  - **bit 7 = 1** → inverse vidéo (fg/bg échangés)
  - **bits 5–0** → 6 pixels (bit 5 = gauche, bit 0 = droite)
  - **bit 6** = « bit de sécurité » : doit être mis à 1 dès que les bits 5–0 valent tous
    0, sinon le byte tomberait dans la catégorie attribut

Cas du jeu — **fil de fer monochrome PAPER 0 / INK 7** :
Phosphoric (et le vrai Oric) réinitialisent ink = BLANC / paper = NOIR au début de
chaque scanline, donc **aucun octet attribut INK/PAPER n'est nécessaire**.
La routine de tracé écrit uniquement des octets pixels avec **bit 6 = 1 toujours**
(`ORA #$40` pour garantir la non‑attribution quand aucun bit pixel n'est allumé).

Pour déclencher le mode HIRES depuis le mode texte : écrire `0x1C`
(= `0b00011100`, attribut de contrôle vidéo : vid_mode = 4) dans la zone texte
(`$BB80`). `vid_mode` persiste ensuite de frame en frame sans réécriture.

**Init HIRES** : remplir `$A000`–`$BF3F` (8 000 octets) avec **`0x40`** (octet pixel
vierge, bit 6 = 1, aucun pixel allumé). **Ne pas utiliser `0x80`** : `(0x80 & 0x60) = 0`
le classe comme attribut.

**Conséquence sur la résolution utile** : si l'on utilise des colonnes d'attributs, 1 à
2 colonnes sont consommées ⇒ **~228 à 234 px horizontaux exploitables**. Sans attributs
(monochrome PAPER 0 / INK 7 par défaut), les **240 px sont disponibles**.

---

## 3. Chaîne d'outils

### 3.1 OSDK — Oric Software Development Kit

Cross‑compilateur C + assembleur 6502 + outils de génération de cassettes/disquettes.
Maintenu par Dbug et la communauté Defence‑Force.

- Site : <http://osdk.defence-force.org/>
- Forum : <https://forum.defence-force.org/>

L'OSDK fournit `cc65` (compilateur C 6502), `xa` (assembleur), `header` (génère
l'en-tête .tap), `tap2dsk`, `pictconv` (conversion d'images), etc.

### 3.2 Phosphoric — émulateur

Émulateur Oric‑1 / Atmos cycle‑accurate écrit en C11, taillé pour
l'**automatisation** (mode headless natif, captures écran, traces CPU,
profilage). C'est l'émulateur de référence pour ce projet.

- Binaire local : `/home/bmarty/Oric1/oric1-emu` (v1.16.x).
- ROM Oric‑1 : `/home/bmarty/Oric1/roms/basic10.rom`.

Lancer en mode Oric‑1 avec un `.tap` (chargement rapide) :

```bash
/home/bmarty/Oric1/oric1-emu \
  -m oric1 \
  -r /home/bmarty/Oric1/roms/basic10.rom \
  -t build/asteroids.tap \
  -f
```

Options clés pour ce projet (voir `oric1-emu --help` pour la liste complète) :

| Option | Usage |
|---|---|
| `-n / --headless` | Pas d'affichage SDL — pour CI |
| `-c N / --cycles N` | Stoppe après N cycles 6502 (run déterministe) |
| `--screenshot FILE` | Capture HIRES `.ppm` ou `.bmp` à la sortie |
| `--screenshot-at C:FILE` | Capture après C cycles (snapshot d'état) |
| `--type-keys C:TEXT` | Auto‑frappe clavier après C cycles (`\n` = Return) |
| `-b ADDR / --break ADDR` | Breakpoint PC en hex (ex. `ED8A`) |
| `-D / --debug` | Lance en mode debugger interactif |
| `--trace FILE` | Log d'instructions CPU |
| `--profile FILE` | Profil de performance CPU à la sortie |

### 3.3 Workflow recommandé

1. Édition du code sur PC (VS Code, Sublime, …)
2. `make` → OSDK produit `asteroids.tap`
3. Phosphoric charge le `.tap` et démarre le jeu (avec `-f` pour skipper le `CLOAD`)
4. Débogage : `--debug` pour entrer en debugger, `-b ADDR` pour breakpoint, `--trace`
   pour log CPU.
5. CI : Phosphoric en `--headless --cycles N --screenshot ...` pour comparer
   bit‑à‑bit aux snapshots de référence.

---

## 4. Architecture logicielle proposée

### Politique C vs ASM

`cc65` produit du code 6502 **5 à 10× plus lent** que de l'assembleur soigné
(call frames volumineux, allocation registres médiocre, pas de pseudo‑registres
en zero page par défaut). À 1 MHz et 20 000 cycles/frame, on ne peut pas
mettre toute la logique en C.

| Couche | Langage | Justification |
|---|---|---|
| Boucle de jeu, machine à états, écrans titre/score | C | Pas dans le chemin chaud, lisibilité gagnante |
| Spawning, gestion des vies, score | C | Une fois par frame, pas critique |
| Tracé de ligne XOR | **ASM** | Cœur de performance |
| Init/effacement HIRES, table d'adresses lignes | **ASM** | Boucle serrée, accès direct mémoire |
| Rotation des sommets (cos/sin × coord) | **ASM** | Appelé N×M fois par frame (N entités, M segments) |
| Intégration physique (pos += vel en 8.8) | **ASM** | Boucle sur toutes les entités |
| Détection de collision (cercle‑cercle) | **ASM** | Idem, et opère sur 8 bits signés |
| Lecture clavier VIA | ASM | Accès registres matériels |
| Player AY (IRQ) | **ASM** | Sous IRQ, contrainte temps |
| IA soucoupe, comportement de tir | C | Décisions rares, pas critique |

**Règle pratique** : tout ce qui est appelé `> 1×/frame` finit en asm.

```
asteroids-oric1/
├── Makefile                  # cible OSDK
├── src/
│   ├── main.c                # boucle principale, état du jeu
│   ├── game.c / game.h       # logique de gameplay (transposée du disasm Atari)
│   ├── entities.c            # vaisseau, astéroïdes, tirs, soucoupe
│   ├── input.c               # lecture clavier
│   ├── sound.c               # AY-3-8912
│   └── asm/
│       ├── line.s            # tracé de ligne XOR (Bresenham 6502)
│       ├── hires.s           # init/effacement HIRES, table d'adresses lignes
│       └── trig.s            # tables sin/cos, rotation
├── data/
│   ├── shapes.h              # listes de segments (vaisseau, astéroïdes, soucoupe)
│   └── trig_tables.bin       # 256 × sin précalculé en 8 bits signé
└── build/                    # produits OSDK
```

---

## 5. Étapes de développement

### Étape 1 — Squelette OSDK

Hello world HIRES : passer en mode HIRES (`HIRES` BASIC ou poke direct), tracer
un triangle (le vaisseau) au centre, attendre une touche.

### Étape 2 — Routine de tracé de ligne en XOR

Cœur de performance. Algorithme de Bresenham, écriture en **EOR** (XOR) sur
l'octet HIRES. Cette technique permet :
- Tracer une frame complète
- Re‑tracer la même liste de segments la frame suivante → efface proprement
- Tracer la nouvelle frame

Optimisations 6502 indispensables :
- Table de **400 octets** (200 × 2) avec l'adresse de début de chaque ligne HIRES
- Table de **8 masques de bits** pour le pixel intra‑octet
- Pas d'opérations 16 bits inutiles : décomposer en deux registres 8 bits
- Boucle interne déroulée si le compromis taille/vitesse l'autorise
- Tester `BVC/BVS` plutôt que `BCC/BCS` quand pratique
- **Self‑modifying code** assumé : modifier les opérandes immédiats
  (`LDA #imm`, `ADC #imm`) plutôt que passer par la zero‑page indirecte.
  Sur 6502 c'est idiomatique ; à 1 MHz c'est souvent le seul moyen de descendre
  sous 15 c/px. Précautions : code en RAM (pas en ROM évidemment), pas de cache
  à invalider, mais attention au debug — le désassemblage statique ne reflète
  plus le code exécuté.

### Étape 3 — Modèles vectoriels

Chaque entité = liste de segments en coordonnées objet (x, y) sur 8 bits signés.
À chaque frame :
1. Effacer (re‑tracer la liste précédente en XOR)
2. Mettre à jour position/angle
3. Calculer cos θ et sin θ depuis la table
4. Pour chaque segment : rotation + translation → coords écran
5. Tracer

#### Wraparound aux bords d'écran

*Asteroids* a un torus topologique (objet sortant à droite réapparaît à gauche).
Deux stratégies, choix à faire **avant l'étape 2** car il impacte le contrat
de la routine de ligne :

- **Wraparound par duplication d'instance** (recommandé) : quand le centre
  d'une entité est à moins de R pixels d'un bord, on dessine l'entité **deux
  fois** (ou quatre dans les coins) avec un offset ±240 / ±200. La routine
  de ligne reste sans clipping, donc rapide. Coût : segments potentiellement
  doublés en bordure, +RAM pour la display list.
- **Clipping vrai** (Cohen–Sutherland ou similaire) : la ligne est coupée
  proprement aux bords. Plus économe en pixels dessinés mais ajoute du code
  dans le chemin chaud (≈ +30–50 % de coût pour les segments concernés).
  Déconseillé compte tenu du budget cycles.

**Décision retenue** : duplication d'instance. La routine `line.s` peut
supposer que toutes les coordonnées en entrée sont dans `[0, 239] × [0, 199]`.

### Étape 4 — Logique de jeu

**Source** : désassemblage de la ROM Asteroids arcade rev 4 (CPU 6502).
Comme l'Oric est aussi 6502, on peut **réutiliser le code**, pas seulement
la logique. La transposition consiste à :
1. importer le code 6502 d'origine (machine à états, physique, IA soucoupe,
   RNG, fragmentation, scoring, tables de niveaux),
2. remplacer les accès matériels arcade (DVG, POKEY‑like, watchdog) par
   les équivalents Oric (Bresenham XOR, AY‑3‑8912, VIA),
3. réécrire la couche d'I/O (clavier au lieu des switches arcade,
   `.tap`/`.dsk` pour persister les high scores au lieu de la NVRAM).

Sources primaires recommandées :
- **6502disassembly.com / va‑asteroids** (rev 4, McDougall & Howell) —
  désassemblage de référence le plus utilisé.
- **Nick Mikstas — Asteroids Disassembly** — sources reconstruites avec
  outils de décodage des données vectorielles (utile pour extraire les
  shapes vers `data/shapes.h`).

Variables clés à porter (noms d'origine du disasm Atari, conservés pour
faciliter la traçabilité) :

| Variable Atari | Rôle |
|---|---|
| `statusShip` | État du vaisseau (vivant / explosion / hyperespace) |
| `statusAsteroids[X]` | État + taille + forme de chaque astéroïde |
| `statusSaucer` | Soucoupe (grande / petite / inactive) |
| `horzVelShip` / `vertVelShip` | Vélocité du vaisseau (8.8 fixed point) |
| `angleShip` | Angle (256 entrées sin/cos) |
| `astWaveTimerReload` | Cadence du « thump‑thump » sonore |
| `numAstThisWave` | Nombre d'astéroïdes au début de la vague |
| `randomSeed` | RNG pour soucoupe et tirs |

### Étape 5 — Son AY‑3‑8912

**Note importante** : l'arcade Atari de 1979 ne possède pas de puce
audio standard. Les sons sont produits par des **oscillateurs analogiques
discrets** hand‑tunés (transistors, condensateurs, bruit blanc analogique).
Ces circuits ne sont **pas transposables** vers l'AY‑3‑8912 de l'Oric —
on doit **recréer** chaque effet à partir de sa spécification fonctionnelle
en utilisant les 3 voies + bruit + envelope hardware de l'AY.

Effets à reproduire :
- **Tirs (`fire`)** : note brève voie A, attack instantanée, decay rapide
- **Explosions petites/moyennes/grandes** : générateur de bruit + envelope
  descendant ; durée + filtrage de fréquence selon taille
- **Thrust** : bruit voie A modulé en amplitude (pulses courts répétés)
- **Thump‑thump** : alternance deux fréquences basses (≈ 50 Hz / 100 Hz),
  s'accélère à mesure que le nombre d'astéroïdes baisse — cadence
  contrôlée par `astWaveTimerReload`, exactement comme l'arcade
- **Soucoupe** : modulation de fréquence sur voie B (whoop‑whoop)
- **Hyperespace** : balayage de fréquence rapide voie A
- **Score extra ship** : fanfare 3 notes voie A

**L'AY‑3‑8912 n'est pas mappé en mémoire**. On lui parle à travers le VIA 6522
(zone `$0300–$030F`) :
- registre AY sélectionné via le **port A** du VIA (`$0300` = ORA),
- **lignes de contrôle BC1 / BDIR** pilotées via le port B / CA2 selon revision,
- séquence type : poser l'index registre sur le port A → BDIR=1 → BDIR=0 →
  poser la donnée → BDIR=1 → BDIR=0.

Référence d'implémentation : routines AY de l'OSDK (recherche `ay_*` dans
`samples/`), et code source d'Oricium pour la version IRQ.

### Étape 6 — Synchronisation vidéo

Pour éviter le déchirement, synchroniser le re‑tracé avec le VBL. Deux options :

1. **Vrai signal VSync** issu de l'ULA, lisible via un bit du port matériel
   (cf. notes de Fabrice Frances sur `oric.free.fr`). C'est la méthode propre,
   sans dérive, à privilégier dès que l'on a confirmé l'adresse exacte selon
   la révision Oric‑1 ciblée.
2. **Timer 1 du VIA 6522** programmé sur la période trame (20 ms PAL). Plus
   simple à mettre en place mais **dérive lentement** par rapport au balayage
   réel et finira par produire du tearing sporadique. Acceptable comme
   solution intermédiaire, à remplacer par (1) avant le polish final.

Référence : code source d'Oricium pour la version VSync « propre ».

---

## 6. Contraintes Oric‑1 spécifiques

### Mémoire

| Zone | Adresse | Usage |
|---|---|---|
| Page zéro | $00–$FF | Variables 6502 critiques (pointeurs, indices) |
| Pile | $100–$1FF | RTS / interruptions |
| Système | $200–$3FF | Vecteurs, ULA, VIA — **ne pas écraser** |
| RAM libre | $500–$9FFF | Code + données du jeu (~38 Ko utiles) |
| HIRES | $A000–$BF3F | Mémoire écran (8 Ko) |
| BASIC ROM | $C000–$FFFF | À garder mappée pour les routines système |

### Différences Oric‑1 ↔ Atmos

L'OSDK cible les deux. Pour rester compatible Oric‑1 :
- **Ne pas utiliser** les routines ROM Atmos spécifiques (vecteurs $0245–$024F).
- Les routines clavier diffèrent légèrement : utiliser une lecture VIA directe
  plutôt que la ROM si on veut binaire identique.
- Le `.tap` doit être généré sans flag « Atmos uniquement ».

### Format livré : `.tap` ou `.dsk` ?

| Critère | `.tap` (cassette) | `.dsk` (disquette via Microdisc/Jasmin) |
|---|---|---|
| Compatibilité Oric‑1 stock | ✅ tout le monde | Nécessite un contrôleur disque |
| Temps de chargement | ~3 min pour ~30 Ko | < 5 s |
| Empreinte RAM résidente | ~0 (loader ROM) | **~1–2 Ko driver disque résident** |
| Sauvegarde high‑scores | Save sur cassette (lent et fragile) | Trivial |
| Utilisateur Phosphoric | OK | OK avec image disque |

**Décision retenue** : livrer **`.tap` en priorité** (cible stock + simplicité +
budget RAM préservé). Le `.dsk` sera une cible secondaire, livrée après le
polish, principalement pour confort d'émulation et sauvegarde des scores.

### CPU 1 MHz

À ~1 000 000 cycles/s et 50 trames/s, on a **20 000 cycles par frame**.

**Hypothèse pessimiste réaliste** pour une routine Bresenham 6502 optimisée
mais non auto‑modifiante : **15–22 cycles/pixel** (changements de masque,
wraps d'octet, gestion du pas X/Y). Avec 30 segments × 20 px de moyenne :

| Hypothèse | c/px | Tracé seul | + effacement (×2) | % budget 50 Hz |
|---|---|---|---|---|
| Optimiste (asymptote théorique) | 10 | 6 000 | 12 000 | 60 % |
| Réaliste sans SMC | 18 | 10 800 | 21 600 | **108 %** |
| Réaliste avec SMC + déroulage | 13 | 7 800 | 15 600 | 78 % |

**Lecture honnête** : sans self‑modifying code et boucle déroulée, on **dépasse
le budget 50 Hz**. Trois leviers à combiner :

1. **Plan B = 25 Hz acté**. Frame‑skip sur 1 trame sur 2 dès l'étape 2.
   Cohérent avec beaucoup de jeux Oric. À documenter comme cible nominale,
   pas comme dégradation.
2. **Réduire la charge** : ~20 segments × 15 px = budget jouable à 50 Hz
   ⇒ contraindre la complexité des shapes (astéroïdes 6–8 segments max,
   pas 12 comme sur l'arcade).
3. **Investir lourdement dans `line.s`** (étape 2) : SMC, table déroulée,
   spécialisation des cas (ligne horizontale / verticale / 45° pures).
   C'est le **risque n°1** du projet ; benchmark obligatoire avant
   l'étape 3.

---

## 7. Stratégie de test

À 1 MHz, le coût d'un bug découvert tard est élevé : pas de print debug
décent, pas de stack trace, et chaque test sur cible coûte ~5 s de chargement
(ou un cycle de redémarrage Phosphoric). On organise donc la testabilité en
trois couches complémentaires.

### 7.1 Tests host (x86) — boucle rapide

Les routines arithmétiques portables (rotation 8 bits, intégration 8.8 fixed,
fragmentation d'astéroïdes, détection de collision) sont écrites de telle
sorte qu'elles compilent **aussi** sur PC avec un compilateur standard. Un
harness en C ou Python pilote :

- vecteurs de test entrée → sortie attendue,
- comparaisons exactes (8 bits, donc déterministe),
- fuzzing simple sur les bornes 8 bits signé.

Pour les routines purement asm 6502 critiques (line drawer), on peut utiliser
un **émulateur 6502 minimaliste** intégrable au harness (lib6502, py65) qui
exécute le code asm cible et compare l'état mémoire après tracé.

Cible : feedback **< 1 s** par run, exécutable en CI.

### 7.2 Tests cible (Phosphoric headless) — non‑régression visuelle

Phosphoric a un mode **`--headless`** natif et capture HIRES en `.ppm`/`.bmp`,
ce qui permet d'automatiser proprement :

```bash
/home/bmarty/Oric1/oric1-emu \
  --headless \
  -r /home/bmarty/Oric1/roms/basic10.rom \
  -t tests/scenarios/spawn_3ast.tap -f \
  --cycles 1000000 \
  --screenshot tests/out/spawn_3ast.ppm
```

Pipeline :
- chargement d'un `.tap` de scénario figé (vaisseau au centre, 3 astéroïdes
  positions/vélocités fixées, **RNG seed connu**),
- exécution N cycles 6502 (déterministe — pas de N frames flottant),
- capture HIRES,
- comparaison **bit à bit** au snapshot de référence stocké dans le repo
  (ex. `cmp tests/ref/spawn_3ast.ppm tests/out/spawn_3ast.ppm`).

Toute divergence ⇒ build CI rouge. Cibles : ~10 scénarios de référence
couvrant gameplay nominal, hyperespace, fragmentation, soucoupe, mort.

**Inputs scriptés** : `--type-keys C:TEXT` permet de jouer un scénario avec
appuis clavier déterministes, ex. `--type-keys 500000:LLLF` pour rotation
gauche × 3 + tir au cycle 500 000.

**Profilage** : `--profile out.prof` produit un fichier de profilage à la
sortie ; à utiliser pour valider le budget cycles de la routine de ligne
au moment du benchmark de l'étape 2.

### 7.3 Assertions debug compilées

Macros `ASSERT_DBG(cond)` qui :
- en build **debug** : émettent un breakpoint logiciel (`BRK`) ou écrivent
  un code d'erreur en HIRES coin haut‑droit ;
- en build **release** : compilent en zéro octet (`#define ASSERT_DBG(c)`).

Couverture cible : invariants page zéro, bornes des tableaux d'entités,
cohérence display list (avant XOR ↔ après XOR).

### 7.4 Manuel — playtest

Aucun automate ne remplace 30 min de jeu réel sur un Oric‑1 physique
(ou Phosphoric en mode interactif SDL2). Cadence : à chaque jalon de roadmap.

---

## 8. Références

### Désassemblage Asteroids arcade (sources primaires)

CPU 6502 partagé avec l'Oric ⇒ ces sources fournissent du **code
réutilisable**, pas seulement de la logique.

- **6502disassembly.com — va‑asteroids (rev 4)** :
  <https://www.6502disassembly.com/va-asteroids/>
  Désassemblage commenté de référence par Mark McDougall et Lonnie Howell.
  **Source primaire pour la logique de jeu** — c'est cette base qu'on
  transpose vers `src/game.c` et les modules asm.
- **Nick Mikstas — Asteroids Disassembly** :
  <https://nmikstas.github.io/portfolio/asteroidsDisassembly/asteroidsDisassembly.html>
  Sources reconstruites + **outils de décodage des données vectorielles**.
  **Source primaire pour l'extraction des shapes** vers `data/shapes.h`
  (vaisseau, astéroïdes 4 formes × 3 tailles, soucoupe).

### Désassemblage Asteroids — sources secondaires

- **Computer Archeology — Asteroids Code** :
  <https://www.computerarcheology.com/Arcade/Asteroids/Code.html>
  Code commenté ligne par ligne, format html. Pratique en lecture
  comparative ou pour clarifier un point ambigu de la rev 4.
- **Jed Margolin — The Secret Life of Vector Generators** :
  <https://www.jmargolin.com/vgens/vgens.htm>
  Référence sur le DVG/AVG ; utile pour comprendre le format des
  shapes avant reformatage vers le moteur Bresenham Oric.

### Outils Oric

- **OSDK — Oric Software Development Kit** : <http://osdk.defence-force.org/>
- **Phosphoric (émulateur, local)** : `/home/bmarty/Oric1/` — émulateur cycle‑accurate utilisé pour ce projet
- **Oricutron (émulateur, alternative externe)** : <https://github.com/pete-gordon/oricutron>
- **Defence‑Force (forum + ressources)** : <https://www.defence-force.org/>
- **Forum Defence‑Force** : <https://forum.defence-force.org/>

### Exemples open‑source de jeux Oric

- **Orickong_C** (DJChloe) — port en C+asm via OSDK :
  <https://github.com/DJChloe/Orickong_C>
- **Edge for ORIC** (drpsy77) — modules sprites/masks réutilisables :
  <https://github.com/drpsy77/ORIC>
- **Oricium** (Chema, Defence‑Force) — shoot 'em up, optimisations VBL :
  <https://www.defence-force.org/index.php?page=games&game=oricium>
- **Game of Thrones theme player** (DJChloe) — exemple d'usage AY‑3‑8912 :
  <https://github.com/DJChloe/got>

### Programmation 6502 et Oric

- **Easy 6502** (Nick Morgan) : <https://skilldrick.github.io/easy6502/>
- **6502.org tutorials** : <http://www.6502.org/tutorials/>
- **Oric Hardware Manual** (Paul Kaufman) — disponible sur defence‑force.org
- **Fabrice Frances' Oric pages** : <http://oric.free.fr/>
  (notes sur la synchro VSync et les hacks matériel)

### Inspiration — autres clones libres

- **Vectoroids** (Bill Kendrick, SDL) : <https://github.com/midzer/vectoroids>
- **ASTEROID‑ARCADE** (kuiperzone, Qt/C++ GPLv3) :
  <https://github.com/kuiperzone/ASTEROID-ARCADE>
- **oscilloscope-asteroids** (joemck) : <https://github.com/joemck/oscilloscope-asteroids>

---

## 9. Premiers pas concrets

```bash
# 1. Installer l'OSDK (suivre instructions defence-force.org)

# 2. Cloner ou créer le squelette
mkdir asteroids-oric1 && cd asteroids-oric1

# 3. Premier Makefile minimal
cat > Makefile <<'EOF'
PROJECT = asteroids
SOURCES = src/main.c src/asm/line.s

include $(OSDK)/lib/osdk.mk
EOF

# 4. main.c : passer en HIRES et boucler
# 5. make
# 6. /home/bmarty/Oric1/oric1-emu -m oric1 \
#       -r /home/bmarty/Oric1/roms/basic10.rom \
#       -t build/asteroids.tap -f
```

---

## 10. Roadmap

Deux estimations sont données ci‑dessous : **optimiste** (dev expérimenté
6502 + temps plein + tout va bien) et **réaliste** (solo, temps partiel,
inclut benchmarks, debug bas‑niveau, allers‑retours design).

| Phase | Contenu | Optimiste | Réaliste |
|---|---|---|---|
| 1 | Squelette OSDK + HIRES + triangle statique | 1 sem. | 2 sem. |
| 2 | Bresenham XOR optimisé + **benchmark cycles** + SMC | 2 sem. | 4–6 sem. |
| 3 | Vaisseau rotatif + tirs + input clavier | 2 sem. | 3 sem. |
| 4 | Astéroïdes (formes, mouvement, fragmentation, wraparound) | 2 sem. | 3 sem. |
| 5 | Collisions + gestion vies/score | 1 sem. | 2 sem. |
| 6 | Soucoupe (grande/petite) + IA de tir (transpo disasm 6502 Atari) | 1 sem. | 2 sem. |
| 7 | Hyperespace + écran titre + high scores | 1 sem. | 2 sem. |
| 8 | Son AY‑3‑8912 (effets + thump) | 1 sem. | 3 sem. |
| 9 | Synchro VSync « propre » + polish + `.dsk` final | 1 sem. | 2 sem. |
| **Total** | | **~3 mois** | **~6 mois** |

**Risque calendrier principal** : la phase 2. Si la routine de ligne ne tient
pas le budget cycles après benchmark, **stop** et arbitrage avant la phase 3 :
soit on baisse la complexité visuelle (moins de segments par astéroïde,
résolution réduite), soit on acte définitivement le 25 Hz, soit on revoit
en profondeur l'algo (ex. tracé de polygones plutôt que segments
indépendants, pour amortir les coûts d'amorce).

Pour un développeur **débutant en 6502**, doubler encore l'estimation
réaliste (~12 mois) est plus prudent.

---

*Bon code, et que les bits du 6502 soient avec vous.*
