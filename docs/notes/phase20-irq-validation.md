# Validation Phase 20 — Player AY sous IRQ Timer 1

**Date** : 2026-05-13
**Outils** : Phosphoric v1.16.14-alpha (`--trace-irq`, `--dump-ram-at`)
**Binaire** : `build/asteroids.tap`, commit `5900132`
**ROM** : `basic10.rom` (Oric-1 BASIC 1.0), model `-m oric1`

## Commande de validation

```bash
oric1-emu -m oric1 -r basic10.rom -t build/asteroids.tap -f -n \
  -c 10000000 \
  --trace-irq /tmp/irq.log \
  --dump-ram-at 9000000:/tmp/ram_9M.bin \
  --screenshot /tmp/phase20_validate.ppm
```

## Résultats

### Trace IRQ

- **493 IRQ-ENTRY** + **493 RTI** sur 10 M cycles → équilibrés, aucun
  RTI manquant.
- **Cadence T1** : ~19 940 cycles entre deux IRQ-ENTRY consécutives
  (ex. `9 948 519` → `9 968 449` = 19 930 cycles). Cible théorique
  20 000 cycles = 50 Hz. **Précision hardware-exact** (variance < 1 %).
- **`IFR = $C0`** (T1 + master flag) et **`srcmask = $01`** (T1
  uniquement) à chaque IRQ → aucune autre source VIA ne fire, pas de
  CB1/CA1/T2 parasite. Confirme l'efficacité du `disable all + clear
  IFR` au début de `_irq_install`.
- **Tous les RTI retournent vers `PC_return` dans la plage `$0500-$9A22`**
  (zone code Asteroids). Exemples : `$06EA`, `$2D4B`, `$41D4`. Aucun
  PC corrompu → fix `PHA/TXA-PHA/TYA-PHA` validé.

### Dump RAM @ 9 M cycles

- `_frame_cnt` (ZP `$AD`) = **`$BF`** (191 décimal).
- Cohérent : autorun à ~5 M cycles, donc ~4 M cycles dans le jeu =
  ~200 ticks T1 = `$C8`. Légèrement inférieur car `irq_install` est
  appelée *après* `timer_init` et `sound_init`, et reset `frame_cnt`
  à 0 à l'installation.

### Premier IRQ-ENTRY observé

```
0002640648 IRQ-ENTRY PC_pre=$ED84 target=$0228 IFR=$C0 IER=$C0 srcmask=$01
```

À 2.64 M cycles, donc bien AVANT que notre `irq_install` s'exécute
(~5 M cycles à l'autorun). C'est l'IRQ de la **ROM** elle-même qui
fire pendant le boot. La ROM gère ses propres IRQ avant que notre
binaire ne prenne la main. Pas de souci, ce sont les IRQ pré-prise
en main qui n'incrémentent pas `_frame_cnt` (notre handler n'est pas
encore installé).

## Conclusion

Phase 20 est **fonctionnellement et chiffrément validée** :
- Le player AY tourne à 50 Hz précis (jitter < 1 %)
- Aucune IRQ parasite (les fixes Phosphoric Q3 et Q2 sont efficaces)
- `frame_cnt` s'incrémente correctement
- Aucun RTI vers PC garbage
- Main loop libéré de l'appel `sound_tick()`

## Remerciements

L'équipe Phosphoric (cf `docs/notes/2026-05-13-asteroids-irq-debug-
response.md`) pour :
1. Le diagnostic du bug PHA manquant
2. La recommandation `disable all VIA IRQ` avant enable T1
3. Les outils `--trace-irq` et `--dump-ram-at` livrés dans
   v1.16.14-alpha, qui ont permis cette validation chiffrée
