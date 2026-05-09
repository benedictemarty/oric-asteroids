# Roadmap

Feuille de route du portage *Asteroids* (arcade Atari 1979) sur Oric‑1 48 Ko.
Document vivant, mis à jour à chaque jalon. Référence de cadrage technique
détaillée : [`asteroids-oric1-48k-guide.md`](./asteroids-oric1-48k-guide.md) (§10).

---

## Vue d'ensemble

| Statut | Phase | Livrable | Optimiste | Réaliste |
|---|---|---|---|---|
| ✅ | 1 | Squelette sans OSDK + HIRES + triangle statique | 1 sem. | 2 sem. |
| 🔜 | 2 | Bresenham XOR optimisé + benchmark cycles + SMC | 2 sem. | 4–6 sem. |
| ⏳ | 3 | Vaisseau rotatif + tirs + input clavier | 2 sem. | 3 sem. |
| ⏳ | 4 | Astéroïdes (formes, mouvement, fragmentation, wraparound) | 2 sem. | 3 sem. |
| ⏳ | 5 | Collisions + gestion vies/score | 1 sem. | 2 sem. |
| ⏳ | 6 | Soucoupe (grande/petite) + IA de tir | 1 sem. | 2 sem. |
| ⏳ | 7 | Hyperespace + écran titre + high scores | 1 sem. | 2 sem. |
| ⏳ | 8 | Son AY‑3‑8912 (effets + thump) | 1 sem. | 3 sem. |
| ⏳ | 9 | Synchro VSync « propre » + polish + `.dsk` final | 1 sem. | 2 sem. |
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

### Phase 2 — Routine de ligne XOR optimisée

**Définition de fin** :
- `line.s` trace un segment quelconque sur HIRES en EOR, écriture bit 7 = 1.
- Benchmark mesuré sur Phosphoric (`--profile`) : **≤ 18 cycles/pixel**
  sur scénario standardisé (segments aléatoires couvrant tous les angles).
- Tracé idempotent : retracer la même liste de segments efface l'image.
- **Décision arrêtée** : 50 Hz tenable ou bascule officielle 25 Hz.
- Tests : 10 scénarios géométriques (lignes verticales, horizontales,
  diagonales, courtes, longues, traversant les colonnes d'attributs)
  versionnés.

**Risque calendrier principal du projet**. Si l'objectif n'est pas tenu :
arbitrage entre baisse de complexité visuelle, bascule 25 Hz définitive,
ou refonte algorithmique (tracé de polygones plutôt que segments).

### Phase 3 — Vaisseau rotatif + tirs + input clavier

**Définition de fin** :
- Vaisseau triangulaire à rotation continue (table sin/cos 256 entrées),
  thrust, friction lente, wraparound aux bords.
- Tirs : ≤ 4 simultanés, durée de vie limitée, vélocité fixée.
- Lecture clavier VIA directe (pas de ROM Atmos) : rotation, thrust, tir,
  hyperespace, pause.
- Maintien du framerate cible avec ~5 entités à l'écran.

### Phase 4 — Astéroïdes

**Définition de fin** :
- 4 formes × 3 tailles d'astéroïdes extraites du disasm Atari (Nick Mikstas),
  reformatées en `data/shapes.h`.
- Mouvement linéaire avec wraparound par duplication d'instance.
- Fragmentation : taille N → 2 × taille N‑1, angles ±θ.
- Spawn de la vague initiale selon table Atari.
- Maintien du framerate cible avec ~10 entités.

### Phase 5 — Collisions + vies + score

**Définition de fin** :
- Détection collision cercle‑cercle 8 bits signé en asm.
- Tir vs astéroïde, tir vs soucoupe, vaisseau vs astéroïde, vaisseau vs soucoupe.
- Scoring conforme à l'arcade : 20/50/100 pour grand/moyen/petit,
  200/1000 pour soucoupe grande/petite.
- 3 vies de départ, ship extra à 10 000 pts.
- Affichage HUD score + vies en HIRES.

### Phase 6 — Soucoupe + IA

**Définition de fin** :
- Soucoupe grande (cible facile, tirs aléatoires) et petite (visée précise).
- Apparition selon la table Atari (`statusSaucer`, RNG).
- IA de tir transposée du désassemblage 6502 rev 4.
- Sortie de l'écran, respawn sur le côté opposé.

### Phase 7 — Hyperespace + écran titre + high scores

**Définition de fin** :
- Hyperespace : disparition, téléportation aléatoire, probabilité de mort.
- Écran titre vectoriel avec démo passive (RAM seedée).
- Table de 10 high scores avec saisie initiales (3 lettres) au clavier.
- Persistance high scores en `.tap` (saisie + relecture).

### Phase 8 — Son AY‑3‑8912

**Définition de fin** :
- Player AY sous IRQ (Timer 1 VIA), piloté en page zéro.
- Effets : tirs, explosions petites/moyennes/grandes, thrust, soucoupe,
  hyperespace, extra ship, thump‑thump cadencé par `astWaveTimerReload`.
- Pas de pop / glitch audible.
- Mix supportable à volume Oric standard.

### Phase 9 — Polish + `.dsk` final

**Définition de fin** :
- Bascule sur le vrai signal VSync ULA, mesure absence de tearing.
- Image `.dsk` Microdisc avec sauvegarde high scores native.
- README + manuel jeu utilisateur.
- Pack final `.tap` + `.dsk` + sources sous tag `v1.0.0`.

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
