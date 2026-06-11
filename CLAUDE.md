# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## État du projet

Phase de cadrage. Le dépôt ne contient pour l'instant qu'un seul document : `asteroids-oric1-48k-guide.md`, qui spécifie le portage d'un clone fidèle d'**Asteroids arcade** (Atari, 1979) sur **Oric‑1 48 Ko** en C + assembleur 6502 via l'**OSDK**. Aucun code source, Makefile, ni dépôt git n'existe encore. Toute commande de build/test reste à mettre en place — ne pas inventer de cibles `make` qui n'existent pas. Lire le guide avant toute proposition d'implémentation : il contient les choix d'architecture (§4), le plan d'attaque par étapes (§5), la stratégie de test (§7) et la roadmap (§10).

**Atout central** : l'arcade Atari et l'Oric partagent **le même CPU 6502**. La logique de jeu (machine à états, physique, IA soucoupe, fragmentation, RNG, scoring) est **réutilisable au niveau code**, pas seulement transposée. Sources primaires : 6502disassembly.com (rev 4) pour la logique, Nick Mikstas pour l'extraction des shapes.

**Mine Storm Vectrex avait été envisagé** comme cible avant d'être écarté : CPU 6809 ⇒ pas de réutilisation de code, et perte de l'argument central du projet. Si quelqu'un re‑propose Mine Storm, rappeler ce trade‑off.

## Chaîne d'outils cible

- **OSDK** (Oric Software Development Kit) : `cc65` (C 6502), `xa` (assembleur), `header`, `tap2dsk`, `pictconv`. Source : http://osdk.defence-force.org/
- **Phosphoric** : émulateur Oric‑1/Atmos cycle‑accurate utilisé pour le dev et la CI. Binaire local `/home/bmarty/Oric1/oric1-emu`, ROM Oric‑1 `/home/bmarty/Oric1/roms/basic10.rom`. Lancement type :
  ```bash
  /home/bmarty/Oric1/oric1-emu -m oric1 \
    -r /home/bmarty/Oric1/roms/basic10.rom \
    -t build/asteroids.tap -f
  ```
  Options clés CI : `--headless`, `-c N` (cycles déterministes), `--screenshot FILE`, `--type-keys C:TEXT` (input scripté), `-b ADDR` (breakpoint), `-D` (debugger), `--trace FILE`, `--profile FILE`. **Ne jamais utiliser `oricutron`** — Phosphoric est l'émulateur du projet.
- Le `Makefile` prévu inclura `$(OSDK)/lib/osdk.mk`.

## Contraintes architecturales fortes (à internaliser avant de coder)

### Mode HIRES Oric (cf. §2 du guide)
- 240 × 200 monochrome, **40 octets/ligne, 6 pixels/octet**, mémoire écran à `$A000–$BF3F`.
- **Discriminateur pixel/attribut** : `(byte & 0x60) == 0` (bits 6 ET 5 tous deux à 0) → attribut ; sinon → pixel.
  - Attribut : modifie INK/PAPER, ne s'affiche pas en pixels.
  - Pixel : bit 7 = 1 → inverse vidéo ; bits 5‑0 → 6 pixels (bit 5 = gauche).
  - **bit 6 = bit de sécurité** : toujours mettre à 1 dans les octets pixel (`ORA #$40`) pour éviter la détection attribut quand aucun bit pixel n'est allumé.
- Cas du jeu (monochrome PAPER 0 / INK 7) : Phosphoric réinitialise ink=BLANC/paper=NOIR à chaque scanline → **aucun attribut INK/PAPER nécessaire**. Écriture de pixels uniquement avec bit 6 = 1 (`ORA #$40`).
- **Init HIRES** : écrire **`0x1E`** à `$BB80` (attributs 24–31 : bit 2 = HIRES, **bit 1 = 50 Hz** ; `0x1C` = HIRES 60 Hz → signal hors standard PAL, invisible sous Phosphoric mais pas de lock RGB2HDMI sur Oric réel — bug corrigé Phase 35), puis remplir `$A000`–`$BF3F` avec **`0x40`** (JAMAIS `0x80` — `(0x80 & 0x60) = 0` serait classé attribut).
- Résolution utile : **240 px** en monochrome sans attributs par ligne.

### Carte mémoire (cf. §6)
- Page zéro `$00–$FF` : pointeurs et indices critiques.
- `$200–$3FF` : système/ULA/VIA — **ne jamais écraser**.
- `$500–$9FFF` : code + données (~38 Ko utiles).
- `$A000–$BF3F` : HIRES.
- `$C000–$FFFF` : ROM BASIC, à **garder mappée** pour les routines système.

### Compatibilité Oric‑1 (vs Atmos)
- Ne pas appeler les vecteurs ROM Atmos `$0245–$024F`.
- Lecture clavier : préférer la lecture VIA directe à la ROM si on veut un binaire identique sur les deux machines.
- `.tap` généré sans flag « Atmos uniquement ».

### Budget CPU et cible nominale (cf. §6)
- 1 MHz × 50 Hz PAL = **20 000 cycles/frame**.
- Hypothèse réaliste pour Bresenham 6502 optimisé : **15–22 c/px** (10 c/px est l'asymptote théorique, pas la réalité hors SMC).
- Sans SMC + déroulage : 30 segments × 20 px × 18 c/px × 2 ≈ **108 % du budget 50 Hz**.
- **Mode 25 Hz acté comme cible nominale** (frame‑skip 1/2). Le 50 Hz reste un objectif de polish, atteignable seulement avec SMC + boucle déroulée + complexité de shapes contrainte (≤ 8 segments par astéroïde, vs 12 sur l'arcade).

## Décisions d'architecture actées (cf. guide)

### Politique C vs ASM (cf. §4)
Le code C de `cc65` est ~5–10× plus lent que de l'asm soigné. Règle : **tout ce qui est appelé > 1×/frame finit en asm**.
- **ASM obligatoire** : tracé de ligne XOR, init/clear HIRES, rotation des sommets, intégration physique 8.8, détection collision, lecture clavier VIA, player AY (sous IRQ).
- **C acceptable** : boucle de jeu, machine à états, écrans titre/score, IA soucoupe (décisions rares), spawning des vagues.

### Wraparound (cf. §5 étape 3)
**Décision = duplication d'instance** (pas de clipping vrai). Quand le centre d'une entité est à < R d'un bord, on dessine l'entité 2× (ou 4× dans les coins) avec offset ±240 / ±200. La routine `line.s` peut **supposer toutes coords en entrée dans `[0, 239] × [0, 199]`**.

### Self‑modifying code (cf. §5 étape 2)
Assumé pour la routine de ligne et autres boucles serrées. Code en RAM. Précaution debug : le désassemblage statique ne reflète plus le code exécuté.

### Synchro vidéo (cf. §5 étape 6)
**Cible finale = vrai signal VSync ULA** (cf. notes Fabrice Frances). Le Timer 1 du VIA 6522 (période 20 ms) est seulement une solution de transition — il dérive et finira par tearing sporadique.

### AY‑3‑8912 (cf. §5 étape 5)
**L'AY n'est pas mappé en mémoire**. On le pilote via le VIA 6522 (port A pour les données, lignes BC1/BDIR pour le contrôle). Référence : routines `ay_*` des samples OSDK, code source d'Oricium pour la version IRQ.

### Format livré (cf. §6)
**`.tap` en priorité** (tout Oric‑1 stock, pas de driver disque résident). `.dsk` en cible secondaire pour confort + sauvegarde high‑scores.

## Choix techniques structurants (du guide)

- **Tracé en XOR (EOR)** via Bresenham 6502 : retracer la frame précédente pour l'effacer puis dessiner la nouvelle. La routine `src/asm/line.s` est **le point de performance n°1** — benchmark obligatoire avant de passer à l'étape 3.
- Tables précalculées attendues : **400 octets** d'adresses de début de ligne HIRES (200 × 2), **8 masques** de bits intra‑octet, **table sin/cos** 256 entrées 8 bits signé (`data/trig_tables.bin`).
- Modèles vectoriels : chaque entité = liste de segments (x,y) en 8 bits signé en coords objet ; pipeline par frame = effacer (re‑XOR) → MAJ pos/angle → rotation+translation → tracer.
- **Logique de jeu transposée du désassemblage Atari arcade rev 4** (CPU 6502, donc **réutilisation de code** et pas seulement de logique). Variables conservées sous leur nom Atari pour traçabilité : `statusShip`, `statusAsteroids[]`, `statusSaucer`, `horzVelShip/vertVelShip` (8.8 fixed point), `angleShip`, `astWaveTimerReload`, `numAstThisWave`, `randomSeed`. Sources primaires : 6502disassembly.com/va‑asteroids (logique), Nick Mikstas (extraction shapes). Computer Archeology et Jed Margolin/DVG en secondaires.
- **Audio** : l'arcade Atari utilise des **oscillateurs analogiques discrets**, pas une puce. Aucune table de registres à porter — chaque effet doit être **recréé** sur l'AY‑3‑8912 de l'Oric à partir de la spec fonctionnelle (tirs, explosions petites/moyennes/grandes, thrust, thump‑thump cadencé par `astWaveTimerReload`, soucoupe, hyperespace, extra ship).

## Stratégie de test (cf. §7)

À 1 MHz et sans print debug, la testabilité s'organise sur 3 couches :
1. **Tests host (x86)** : routines portables (rotation, intégration 8.8, fragmentation, collision) compilent aussi sur PC ; harness en C ou Python avec vecteurs entrée→sortie. Pour l'asm 6502 critique, émulateur minimaliste (lib6502, py65) intégré au harness. Cible : feedback < 1 s, exécutable en CI.
2. **Tests cible Phosphoric headless** : scénarios figés (RNG seedé, positions/vélocités fixées) chargés depuis un `.tap`, exécution N **cycles 6502** (déterministe), **diff bit‑à‑bit** de la capture HIRES (`--screenshot`) contre snapshots de référence. Inputs scriptés via `--type-keys`. Profilage des routines critiques via `--profile`.
3. **Assertions debug** : macro `ASSERT_DBG(cond)` qui émet `BRK` ou code d'erreur HIRES en build debug, compile en zéro octet en release.
4. Playtest manuel à chaque jalon (Phosphoric en mode SDL2 interactif, ou Oric‑1 physique).

## Arborescence cible (proposée par le guide §4, pas encore créée)

```
src/main.c   src/game.{c,h}   src/entities.c   src/input.c   src/sound.c
src/asm/line.s   src/asm/hires.s   src/asm/trig.s
data/shapes.h   data/trig_tables.bin
build/   Makefile
```

## Instructions utilisateur (depuis le CLAUDE.md global)

- Le projet est géré en **méthode agile** : maintenir CHANGELOG et ROADMAP à chaque modification, créer/exécuter les tests à chaque modif, garder la documentation à jour automatiquement.
- Les commits utilisent l'identité git `bmarty <bmarty@mailo.com>`.
- Toute modification doit être tracée et versionnée — il faudra `git init` lors du premier commit puisque le dépôt n'est pas encore initialisé.
