# NOTICE — périmètre de la licence et droits tiers

## Œuvre originale (sous licence EUPL v1.2)

Copyright © 2026 Bénédicte Marty (bmarty@mailo.com)

Le code original de ce projet est publié sous la **Licence Publique de
l'Union européenne v1.2 (EUPL-1.2)** — voir le fichier [LICENSE](LICENSE).
Texte officiel dans toutes les langues de l'UE :
<https://joinup.ec.europa.eu/collection/eupl/eupl-text-eupl-12>

Sont couverts par cette licence, notamment :

- le moteur de rendu HIRES Oric (tracé de ligne XOR Bresenham avec SMC,
  blit sprites, init vidéo — `src/asm/line.s`, `src/asm/hires*`) ;
- les routines d'entrées clavier/joystick IJK (`src/asm/input.s`) ;
- le moteur sonore AY-3-8912 et la **recréation originale** des effets
  sonores et du jingle (`src/asm/sound.s`, `src/sound.h`) — l'arcade
  d'origine utilisait des oscillateurs analogiques discrets, aucun code
  ni donnée audio d'Atari n'est réutilisé ;
- l'adaptation Oric de la boucle de jeu, le HUD 7 segments, l'écran
  titre, les high scores (`src/*.c`) ;
- le système de build, les tests et la documentation.

## Droits tiers — Atari (non couverts par la licence)

Ce projet est un **clone d'étude et de préservation** d'*Asteroids*
(© 1979 Atari, Inc. ; droits actuels : Atari Interactive, Inc.).

- La **logique de jeu** (machine à états, physique, fragmentation,
  IA de la soucoupe, générateur pseudo-aléatoire, barème de score) est
  **adaptée du désassemblage de la ROM arcade rev 4** (source :
  6502disassembly.com). Les noms de variables d'origine sont conservés
  à des fins de traçabilité.
- Les **formes vectorielles** (astéroïdes, vaisseau, soucoupe) dérivent
  des données extraites de la ROM (travaux de Nick Mikstas).

**Aucun droit n'est revendiqué** sur ces éléments dérivés de l'œuvre
d'Atari, qui ne sont pas couverts par la licence EUPL ci-dessus.
*Asteroids* est une marque d'Atari Interactive, Inc. Ce projet est
**non commercial**, fourni gratuitement, et n'est **ni affilié à, ni
approuvé par Atari**. Si vous réutilisez ce code, la réutilisation des
parties dérivées d'Atari est sous votre propre responsabilité.

## Outils et références

- Émulateur de développement : [Phosphoric](https://github.com/benedictemarty/Phosphoric) (MIT).
- Références techniques : 6502disassembly.com, Computer Archeology,
  travaux de Nick Mikstas et Jed Margolin, documentation Oric de
  Fabrice Francès et du forum Defence-Force.
