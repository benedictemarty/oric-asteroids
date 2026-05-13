# Analyse des sons arcade Asteroids (1979) — calibration AY-3-8912

**Date** : 2026-05-13 (analyse initiale .wav)
**Mise à jour** : 2026-05-13 (re-analyse depuis MP3 22 kHz, qualité supérieure)
**Source initiale** : https://www.classicgaming.cc/classics/asteroids/sounds
  (`.wav` 8-bit mono 11025 Hz datés de 1994, qualité dégradée)
**Source MP3** : fichiers téléchargés ailleurs par l'utilisateur dans
  `~/Téléchargements` (22050 Hz 16-bit, qualité bien supérieure)
**Méthode** : décodage MP3 → WAV mono 22050 Hz via `ffmpeg`, puis analyse
  zero-crossings / RMS par segments de 10 % de la durée.

---

## ⚠ Corrections suite à l'analyse MP3 (mise à jour)

L'analyse initiale sur `.wav` 1994 8-bit/11kHz a donné des fréquences
biaisées par l'aliasing/distortion. Avec les MP3 22 kHz récents, plusieurs
caractéristiques changent significativement :

### extra-ship — TONE PLATEAU, pas sweep

| Segment | Temps | Freq    | RMS    |
|---------|-------|---------|--------|
| 0       |   0ms | (sil)   |    32  |
| 1       |  21ms | (sil)   |    35  |
| 2       |  43ms | **2956 Hz** | 10661 |
| 3       |  64ms | **2956 Hz** | 10852 |
| 4       |  85ms | **2956 Hz** | 10757 |
| 5       | 107ms | 3049 Hz |  6228  |
| 6       | 128ms | 3190 Hz |     7  |

⇒ **Tone plateau ~2956 Hz pendant 85-130 ms** suivi d'un fade rapide.
Pas un sweep descendant comme l'analyse .wav suggérait.

### fire — Noise SWEEP descendant 1200→600 Hz puis remontant

| Segment | Temps | Freq    | RMS    |
|---------|-------|---------|--------|
| 0       |   0ms | (sil)   |    38  |
| 1       |  42ms | 1220 Hz | 14548  |
| 2       |  84ms |  770 Hz | 10587  |
| 3       | 127ms |  687 Hz |  7533  |
| 4       | 169ms |  557 Hz |  5706  |
| 5       | 211ms |  770 Hz |  4673  |
| 6       | 253ms |  947 Hz |  3471  |

⇒ Noise burst avec fréquence centrale **descendante** initiale puis
re-modulée. Notre `R6=3` (~651 Hz constant) reste un compromis acceptable
mais pas parfait.

### Explosions S/M/L — confirmation

Profil RMS : impact initial aigu (917-1124 Hz, ~100 ms) puis **corps grave**
(130-400 Hz) qui décroît sur 600-700 ms, puis fade. Les valeurs `R6=12/8/6`
de l'étape 1 correspondent au **corps grave** (167/246/306 Hz), ce qui est
le bon choix pour AY-3-8912 qui n'a pas d'enveloppe de pitch noise.

### UFO — multiples bips sur 4 secondes

Le fichier `asteroid-ufo.mp3` fait **4181 ms** ⇒ enregistrement de plusieurs
bips successifs (l'UFO arcade émet en continu). Les bips individuels
oscillent **700-1100 Hz**, donc nos valeurs `1259→879` (large) sont un
poil trop aiguës. Acceptable mais pourrait être abaissé.

---

---

## Résumé des 10 effets arcade

| Effet         | Durée    | Type   | Fréq / sweep             | Notes                                      |
|---------------|---------:|--------|--------------------------|--------------------------------------------|
| fire          |  267 ms  | NOISE  | ~740 Hz                  | Bruit filtré, pas une tone aigu            |
| bangLarge     |  863 ms  | NOISE  | ~167 Hz                  | Noise grave (gros asteroid)                |
| bangMedium    |  984 ms  | NOISE  | ~246 Hz                  | Noise moyen                                |
| bangSmall     |  867 ms  | NOISE  | ~306 Hz                  | Noise aigu (petit asteroid)                |
| thrust        |  288 ms  | TONE   | ~82 Hz                   | Rumble grave (cv=0.4, un peu noisy)        |
| extraShip     |  136 ms  | SWEEP  | 2786 → 0 Hz              | Whoosh descendant rapide                   |
| saucerBig     |  168 ms  | SWEEP  | 1259 → 879 Hz            | Bip descendant grave (UFO menaçant)        |
| saucerSmall   |  124 ms  | SWEEP  | 983 → 1354 Hz            | Bip **montant** aigu (UFO petit nerveux)   |
| beat1         |   74 ms  | SWEEP  | 134 → 81 Hz              | "thump" cadencé #1 — sweep grave           |
| beat2         |   78 ms  | SWEEP  | 129 → 77 Hz              | "thump" cadencé #2 — sym. au beat1         |

---

## Comparaison avec notre `src/asm/sound.s` actuel (Phase 8+11+…)

| ID         | Notre version       | Arcade authentique     | Verdict       |
|------------|---------------------|------------------------|---------------|
| FX_FIRE    | Tone aigu (~6 fr)   | NOISE 740 Hz (267 ms)  | **Divergent** : type différent |
| FX_EXPLODE | Noise unique        | 3 noises 167/246/306 Hz| **Divergent** : pas de S/M/L  |
| FX_THUMP   | Tone grave statique | SWEEP 134→81 Hz (74 ms)| **Divergent** : pas de sweep  |
| FX_HYPER   | Tone+noise          | (pas d'arcade direct)  | OK (effet additionnel)         |
| FX_THRUST  | Noise court (~3 fr) | TONE 82 Hz (288 ms)    | **Divergent** : tone vs noise |
| FX_LIFE    | Tone aigu (~20 fr)  | SWEEP 2786→0 Hz (136 ms)| **Divergent** : pas de sweep |
| FX_UFO     | Bip-bip statique    | SWEEP S/L différenciés | **Divergent** : pas de sweep, pas de S/L |

---

## Recommandations de portage AY-3-8912

L'AY-3-8912 supporte nativement :
- **Tone** (3 canaux A/B/C) : registres R0..R5 (fréquence 12-bit par canal).
- **Noise** : registre R6 (fréquence 5-bit, partagé par tous canaux).
- **Mixer** (R7) : routage tone/noise par canal.
- **Volume** (R8..R10) : amplitude par canal (4-bit ou enveloppe).
- **Enveloppe** (R11..R13) : forme + fréquence.

### Mapping recommandé

| Effet           | Canal | R7 mixer            | Fréq tone/noise               | Volume/env       | Durée   |
|-----------------|-------|---------------------|-------------------------------|------------------|---------|
| FX_FIRE         | A     | noise A only        | R6 ≈ 740 Hz mapped (= ~5)     | Vol 12 → décrémente | ~7 fr |
| FX_BANG_LARGE   | A     | noise A only        | R6 grave (≈ 2-3)              | Env. décay long  | ~22 fr |
| FX_BANG_MEDIUM  | A     | noise A only        | R6 moyen (≈ 5-6)              | Env. décay moyen | ~25 fr |
| FX_BANG_SMALL   | A     | noise A only        | R6 aigu (≈ 8-10)              | Env. décay court | ~22 fr |
| FX_THRUST       | B     | tone B only         | R2..R3 → 82 Hz (period ≈ 7570)| Vol moyen        | ~7 fr (re-déclenché) |
| FX_LIFE         | C     | tone C only         | R4..R5 sweep 2786→0 Hz        | Vol décroissant  | ~3-4 fr|
| FX_UFO_LARGE    | C     | tone C only         | Sweep 1259→879 Hz             | Vol fixe         | ~4 fr (re-décl.) |
| FX_UFO_SMALL    | C     | tone C only         | Sweep 983→1354 Hz             | Vol fixe         | ~3 fr (re-décl.) |
| FX_THUMP_1      | B     | tone B only         | Sweep 134→81 Hz (rapide)      | Vol fort         | ~2 fr  |
| FX_THUMP_2      | B     | tone B only         | Sweep 129→77 Hz (rapide)      | Vol fort         | ~2 fr  |

### Conversion fréquence → registres AY

À 1 MHz horloge AY (Oric standard) :
- **Tone period** = `clk / (16 × freq_Hz)` → ex. 82 Hz = 1e6 / (16 × 82) = **762** = `$2FA`
  R0 = $FA (low), R1 = $02 (high)
- **Noise period** (R6) = 0..31, freq = `clk / (16 × 32 × period)` → 740 Hz noise = period 2.6 → **R6 = 3**
  - Pour 167 Hz : period ~12 → R6 = 12 (large bang)
  - Pour 246 Hz : period ~8 → R6 = 8 (med bang)
  - Pour 306 Hz : period ~6 → R6 = 6 (small bang)

### Sweeps via SMC ou table

Pour les sweeps (extraShip, saucer, thumps), deux approches :
1. **Table d'écritures PSG** : à chaque tick, écrire R0/R1 (ou R6) avec une
   valeur précalculée depuis une table de 4-8 entrées. Coût ≈ 30 cycles/tick.
2. **Linéaire en asm** : décrémenter R0/R1 avec un step fixe à chaque tick.
   Plus compact mais moins flexible.

Approche 1 recommandée pour fidélité.

---

## Plan de portage suggéré (priorité décroissante)

1. **Triple explosion S/M/L** (gameplay-pertinent : feedback différencié
   sur la taille de l'asteroid détruit). Touche `collisions_*_asteroids`
   pour passer la taille à `sound_play_fx`.
2. **FX_FIRE → noise** (le tone aigu actuel est "computer game" mais
   pas du tout Asteroids — gros gain d'immersion).
3. **FX_LIFE sweep descendant** (whoosh "tum-ti-ti-tum-tum" caractéristique).
4. **FX_THUMP sweep grave** + alternance THUMP_1/THUMP_2 (vrai pulse arcade).
5. **FX_UFO S/L différenciés** (small = sweep montant nerveux, large =
   sweep descendant menaçant).
6. **FX_THRUST tone 82 Hz** (au lieu de noise — moins urgent visuellement).

Étapes 1-3 ont le plus gros impact perceptif.
