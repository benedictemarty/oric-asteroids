# Rapport de bug Phosphoric — glitch zone TEXT du bas en mode HIRES

**Date** : 2026-05-13
**Projet** : Oric Asteroids (portage clone arcade)
**Émulateur** : Phosphoric (`/home/bmarty/Oric1/oric1-emu`)
**Machine cible** : Oric‑1 48 K, ROM BASIC 1.0
**Mode** : HIRES (240×200 monochrome + 3 lignes texte en bas)

---

## Symptôme

Lors du passage en mode HIRES (écriture de `$1C` à `$BB80`), les 3 lignes
de texte affichées en bas de l'écran (scanlines 200-223, soit 24 scanlines
= 3 lignes texte × 8 scanlines) apparaissent altérées :

- **Avant entrée en HIRES** : ces lignes contiennent typiquement le texte
  du prompt BASIC (`Ready`, `CALL 1280`, etc.) sur fond noir, encre blanche
  (couleurs Oric par défaut au boot).
- **Après entrée en HIRES** : les lignes affichent un **pattern de lignes
  verticales** (glitch visuel) sur fond blanc. Le texte BASIC initial
  disparaît, remplacé par ce pattern.

Visuel observé (capture PPM 240×224, décimée par 3 horizontalement,
les lignes 200-223 du PPM, qui correspondent aux scanlines TEXT du bas) :

```
200 #####.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.
201 #####.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.
...
```

`#` = pixel blanc, `.` = pixel noir. Le pattern de 5 colonnes blanches
suivi de `.#.#.#.` répété sur toute la largeur évoque un mélange de :
- Un attribut PAPER hérité (fond blanc plutôt que noir)
- Le rendu d'un caractère affiché en boucle (probablement `$40` = `'@'`)

## Reproduction minimale

1. Démarrer Phosphoric en mode Oric‑1 avec ROM BASIC 1.0 :

   ```bash
   /home/bmarty/Oric1/oric1-emu -m oric1 \
     -r /home/bmarty/Oric1/roms/basic10.rom \
     -t /path/to/asteroids.tap -f
   ```

2. Au prompt BASIC (`Ready`), taper `CALL 1280` puis ENTER pour démarrer
   le binaire.

3. Le binaire fait :
   - `STA $BB80` avec A=`$1C` (active mode HIRES = video mode 4)
   - Remplit `$A000-$BF3F` avec `$40` (octet "pixel vierge" en HIRES :
     bit 6=1 pour éviter la détection attribut, bits 5-0=0 pour aucun
     pixel allumé)

4. Observer les 3 lignes texte du bas : elles affichent le pattern
   ci-dessus au lieu d'être noires (ou de conserver le texte BASIC).

Code 6502 minimal pour reproduction (assemblé avec `ca65`/`xa`) :

```asm
        lda  #$1C
        sta  $BB80              ; passage en mode HIRES

        lda  #$A0
        sta  ptr+1
        lda  #$00
        sta  ptr
        ldx  #$1F               ; 31 pages : $A0..$BE
        ldy  #0
        lda  #$40
fill:   sta  (ptr),y
        iny
        bne  fill
        inc  ptr+1
        dex
        bne  fill
        ; + 64 octets manuels $BF00-$BF3F (omis ici)
        rts
```

## Investigation empirique

### Hypothèse 1 : zone TEXT du bas à `$BF68-$BFDF`

Sur Oric en mode HIRES, l'hypothèse classique est que les 24 scanlines
du bas affichent les **3 dernières lignes du screen TEXT** (lignes
texte 25-27 = `$BB80 + 25*40` à `$BB80 + 27*40 + 39` = `$BF68-$BFDF`,
soit 120 octets en dehors de la zone HIRES `$A000-$BF3F`).

**Test effectué** : écrire `$20` (espace) dans `$BF68-$BFDF` après le
remplissage HIRES :

```asm
        ldx  #119
        lda  #$20
@fill:  sta  $BF68,x
        dex
        bpl  @fill
```

**Résultat** : seules les **5 premières colonnes** de la zone TEXT du
bas changent (passent de "#####" à "....."). Le reste de la ligne garde
le pattern d'origine. Capture après modification :

```
200 .....#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#.#
```

→ Seul l'effet d'**un** octet écrit en `$BF68` est visible (probablement
en tant qu'attribut INK/PAPER pour la première cellule, mais aucun
effet sur les cellules suivantes).

### Hypothèse 2 : zone TEXT du bas à `$BB80-$BBF7`

Si l'ULA Oric lit la zone TEXT du bas depuis **les 3 premières lignes**
du screen TEXT (`$BB80-$BBF7`), alors elle est **dans** la zone HIRES
et nos `$40` (remplissage HIRES) seraient interprétés comme caractères
`'@'`, expliquant le pattern observé.

**Pas testé** car écraser cette zone avec `$20` après le remplissage
HIRES créerait des pixels parasites en HIRES (bit 5 = 1 sur les
scanlines 176-178), polluant la zone de jeu.

### Hypothèse 3 (la plus probable)

Comportement Phosphoric spécifique : l'émulation de l'ULA pour la zone
TEXT du bas en HIRES suit une logique différente du hardware Oric réel,
OU bien l'init HIRES `_hires_init` que nous faisons n'inclut pas une
étape requise (réinitialiser certains attributs d'attribut TEXT, par
exemple la palette ink/paper de la zone TEXT du bas).

## Comportement attendu

Selon notre lecture des notes Defence Force et de la doc Oric HIRES :

1. En HIRES, les 3 lignes texte du bas sont lues depuis le screen TEXT
   habituel (`$BB80-$BFDF`). L'adresse exacte des 3 dernières lignes
   reste à confirmer.
2. Les attributs INK/PAPER de chaque ligne TEXT du bas devraient être
   indépendants des attributs HIRES.
3. Au boot, l'écran TEXT devrait avoir INK=BLANC, PAPER=NOIR par défaut
   (cf. registres Oric). Au passage en HIRES, ces attributs **devraient
   être conservés** pour la zone TEXT du bas.

## Demande à l'équipe Phosphoric

1. Confirmer l'adresse exacte que l'ULA lit pour les 24 scanlines du
   bas en mode HIRES (= `$BB80-$BBF7` ? `$BF68-$BFDF` ? autre ?).
2. Confirmer si le rendu observé est conforme à l'Oric‑1 réel (le bug
   se reproduit-il sur le hardware physique ?).
3. Si c'est un bug d'émulation : préciser quelle release Phosphoric
   est concernée et envisager un correctif.
4. Si c'est le comportement matériel attendu : recommander la séquence
   d'init HIRES qui produit une zone TEXT du bas propre (fond noir,
   contenu maîtrisé).

## Métadonnées build

- Binaire de reproduction : `build/asteroids.tap` (au tag git
  `v1.2.9` + Unreleased de ce dépôt, post-commit
  `feat(ship): vaisseau arcade-fidèle 5 segments`).
- ROM Oric‑1 utilisée : `/home/bmarty/Oric1/roms/basic10.rom`.
- Commandes : `make && make run`.
- Capture du glitch : `tests/out/phase9_release.ppm` (240×224, P6 raw).

## Workaround actuel

Aucun fix appliqué côté projet — la zone TEXT du bas garde son
comportement glitché en attendant retour de l'équipe Phosphoric.
La zone HIRES reste 100 % propre (objectif primaire du jeu).
