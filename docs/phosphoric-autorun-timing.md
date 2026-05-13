# Rapport bug Phosphoric — autorun .tap déclenche le code avant init ROM complète

**Date** : 2026-05-13
**Projet** : Oric Asteroids — portage clone arcade Atari 1979
**Émulateur** : Phosphoric v1.16.3-alpha
**Machine cible** : Oric‑1 48 K, ROM BASIC 1.0
**Outils** : `bin2tap` (binaire fourni avec Phosphoric)

---

## Résumé

Le binaire produit par `bin2tap` (v1.16.3-alpha) inclut un **flag autorun
`$C7`** dans le header `.tap` (byte 7). Avec `oric1-emu -t file.tap -f`,
l'auto-exec saute à `start_addr` **avant que la ROM Oric ait fini ses
initialisations VIA/ULA**. Conséquences pour un binaire qui n'attend
pas explicitement :

1. `VIA_PCR` peut avoir bit 4 = 1 (CB1 rising edge) ⇒ le flag CB1 IFR
   ne se latche jamais sur le VSync ULA (qui est falling edge) ⇒ tout
   code qui poll IFR bit 4 (synchro frame) boucle infiniment.
2. La zone HIRES présente une **bande blanche persistante aux lignes
   192-199** (240×8 = 1920 pixels), comme si une partie de la zone
   bitmap HIRES (`$BE00-$BF18`) n'était pas correctement effacée.
3. Tearing/glitch graphiques aléatoires sur l'écran HIRES.

**Workaround projet** : patch du byte 7 du `.tap` à `$00` après
`bin2tap` pour désactiver l'autorun. L'utilisateur doit alors taper
`CALL 1280` manuellement depuis le BASIC. Dans cet état, le BASIC
ROM est dans son flux idle stable au moment du `CALL`, et le binaire
démarre correctement (HIRES + démo titre + gameplay OK).

## Reproduction

### Avec autorun (cassé)

```bash
# bin2tap par défaut → header byte 7 = $C7
bin2tap binary.bin --start 1280 --exec 1280 --name "demo" -o demo.tap
xxd demo.tap | head -1
# 1616 1624 0000 80c7 ...
#                ^^ flag autorun

oric1-emu -m oric1 -r basic10.rom -t demo.tap -f
# Le binaire s'auto-exécute à l'injection (~3M cycles).
# Symptômes selon le binaire :
#   - frame_wait sur CB1 ⇒ boucle infinie
#   - bande blanche y=192-199 dans la zone HIRES
#   - glitch aléatoires
```

### Sans autorun (OK)

```bash
# Patch byte 7 → $00
python3 -c "d=open('demo.tap','r+b');d.seek(7);d.write(b'\x00');d.close()"
xxd demo.tap | head -1
# 1616 1624 0000 8000 ...
#                ^^ autorun off

oric1-emu -m oric1 -r basic10.rom -t demo.tap -f
# Le binaire est injecté mais NON exécuté. Le BASIC affiche READY.
# L'utilisateur tape : CALL 1280
# → le binaire démarre, HIRES OK, démo titre animée.
```

## Hypothèse de cause racine

Le 6502 saute à `start_addr` (= $0500 ici) immédiatement après
l'injection deferred fast-load. À ce point :

- La ROM BASIC vient potentiellement de finir son RAM test ($FA1F-$FA45)
  et est en cours de configuration des vecteurs IRQ / VIA.
- Le code utilisateur prend la main avec `SEI; LDX #$FF; TXS` mais
  n'a aucun moyen de savoir si la ROM a terminé son init.
- L'état du VIA reflète ce que la ROM était en train de configurer
  (PCR, IER, IFR, ACR partiellement initialisés).

L'équivalent avec `CLOAD ""` manuel : le BASIC est en boucle prompt
idle, la ROM IRQ tourne stable, PCR/IER ont leurs valeurs définitives
quand l'utilisateur tape `CALL 1280`.

## Demande à l'équipe Phosphoric

Plusieurs options, par préférence décroissante :

### Option 1 — Délai d'auto-exec
Phosphoric pourrait attendre la fin de l'init ROM Oric avant de
sauter à `start_addr`. Critère possible : attendre que la ROM atteigne
sa boucle BASIC prompt (typiquement détectable par un PC qui revient
plusieurs fois à une adresse fixe de la ROM, ou par un signal interne).

### Option 2 — Reset VIA avant auto-exec
Avant le `JMP start_addr`, Phosphoric pourrait forcer les registres
VIA à des valeurs Oric standard (PCR=$CD, IER=$7F, IFR=$7F). Ainsi
le binaire utilisateur démarre dans un état VIA prévisible.

### Option 3 — Option `bin2tap --no-autorun`
La plus simple : ajouter un flag explicite à `bin2tap` pour désactiver
le flag autorun, comme c'était le cas avant v1.16.3. L'utilisateur
final tape `CALL n` manuellement après chargement.

### Option 4 — Documentation
Au minimum, documenter dans le manuel/changelog Phosphoric que :
- Le flag autorun est désormais activé par défaut.
- Les binaires qui dépendent de l'init complète ROM doivent attendre
  explicitement (boucle de polling stable, délai cycles, ou reset VIA
  explicite dans leur startup).

## Workaround Makefile actuel

```makefile
$(TAP): $(BIN)
	$(BIN2TAP) $(BIN) --start $(LOAD_ADDR) --exec $(EXEC_ADDR) \
	           --name "$(PROJECT)" -o $@
	@# Désactive le flag autorun (byte 7 = $C7 → $00) — workaround
	@# Phosphoric v1.16.3-alpha autorun timing issue.
	@python3 -c "d=open('$@','r+b');d.seek(7);d.write(b'\x00');d.close()"
```

Le rapport ci-dessus reproduit les symptômes observés et propose
plusieurs voies de fix côté Phosphoric. Disponible pour collaborer
sur le diagnostic et le test du fix retenu.
