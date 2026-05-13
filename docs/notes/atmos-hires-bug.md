# Bug Atmos — rendu HIRES en pattern de points régulier

**État** : dette technique, non bloquant pour Oric‑1. Découvert en testant
la portabilité Atmos après `JMP ($FFFC)` (commit à venir).

## Symptôme

Sur **Atmos** uniquement (BASIC 1.1, ROM `basic11b.rom`), à 10–30M cycles
après le boot avec `asteroids.tap` en fast-load Phosphoric :
- ✅ Le binaire est chargé en RAM et exécuté (PC dans la zone code asteroids,
  pas de crash).
- ❌ L'écran HIRES affiche un pattern régulier de points blancs alignés
  (≈ 1 pixel actif tous les 6 colonnes, sur toutes les lignes), au lieu de
  l'écran titre ASTEROIDS attendu.
- Le pixel count d'octets non-noirs sur Atmos est ~5760 (motif régulier)
  vs ~1062 sur Oric‑1 (titre vectoriel rendu).

Sur **Oric‑1** (BASIC 1.0, `basic10.rom`), même binaire, même 10M cycles :
écran titre ASTEROIDS visible normalement, deux astéroïdes animés.

## Tests reproductibles

```bash
# Oric-1 (OK)
/home/bmarty/Oric1/oric1-emu -m oric1 -r /home/bmarty/Oric1/roms/basic10.rom \
  -t build/asteroids.tap -f -n -c 10000000 --screenshot /tmp/oric1.ppm

# Atmos (KO — pattern de points)
/home/bmarty/Oric1/oric1-emu -m atmos -r /home/bmarty/Oric1/roms/basic11b.rom \
  -t build/asteroids.tap -f -n -c 30000000 --screenshot /tmp/atmos.ppm
```

Bug reproduit aussi bien avec `JMP $F800` (ancien crt0) qu'avec
`JMP ($FFFC)` (nouveau) — donc indépendant du fix vecteur reset.

## Pistes d'investigation

1. **`_hires_init` interaction avec timing Atmos** : la routine copie
   `$B400 → $9800` (charset) et fill `$A000-$BF3F` avec `$40`. Sur Atmos,
   peut-être que l'autorun `$C7` exécute le binaire **avant** que la ROM
   ait posé l'état RAM attendu, ou avec la zone $A000 dans un état
   différent au démarrage.

2. **Charset `$B400 → $9800`** : sur Atmos, le charset à `$B400` peut être
   différent (BASIC 1.1 alt charset). La copie pourrait écraser du code
   ou des données importantes.

3. **Pattern « point tous les 6 » suggère** : un byte HIRES = `$41` (bit 6
   safety + bit 0 = pixel le plus à droite). Si le fill HIRES écrit `$41`
   au lieu de `$40` à cause d'un débordement de boucle, on obtiendrait
   exactement ce pattern.

4. **Autorun `$C7`** : ce byte type SEDORIC/Oric déclenche un `RUN` après
   le `CLOAD`. Le comportement diffère légèrement BASIC 1.0 vs 1.1. Tester
   un `.tap` sans flag autorun et chargement manuel pour isoler.

5. **VIA Port B bit 1 / 2** : pilote l'attribut HIRES vs TEXT. Vérifier
   que `_hires_init` configure correctement DDRB sur les deux machines.

## Étapes du debug futur

1. Charger le binaire en debugger Phosphoric (`-D`) sur Atmos, placer un
   breakpoint sur `_hires_init` et tracer.
2. Comparer le contenu de `$A000-$A040` (premières lignes HIRES) après
   `_hires_init` Oric‑1 vs Atmos.
3. Vérifier le contenu de `$9800-$9FFF` après la copie charset.
4. Tester un `.tap` sans autorun + `CLOAD ""` + `CALL #0500` manuel pour
   éliminer la variable autorun.

## Priorité

**Différé.** Le projet cible Oric‑1 48 Ko nominal (cf. CLAUDE.md).
La compat Atmos est un bonus, pas un objectif primaire. Ce bug n'empêche
pas de tagger le fix vecteur reset comme amélioration de portabilité.
