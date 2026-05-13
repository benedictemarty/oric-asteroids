PROJECT   = asteroids
ROM       = /home/bmarty/Oric1/roms/basic10.rom
EMU       = /home/bmarty/Oric1/oric1-emu
BIN2TAP   = /home/bmarty/Oric1/bin2tap
CC65      = cc65
CA65      = ca65
LD65      = ld65
CC65LIB   = /usr/local/share/cc65/lib/none.lib

LOAD_ADDR = 1280
EXEC_ADDR = 1280

CFG       = cfg/oric1.cfg
BUILD     = build

# CFLAGS cc65 (revue senior 2026-05-13) :
#   -O               optimisations standards
#   -Or              register-load optimisations (regs Y/X dans boucles)
#   -Cl              static-locals : locales en BSS au lieu de c-stack
#                    (10-30 % perf sur fonctions hot comme bullets_update,
#                    asteroids_update, ship_update — appelées chaque frame).
#   --register-vars  permet `register int x;` (utile si on annote certains).
#   -I src           includes du projet.
CFLAGS    = -t none -O -Or -Cl --register-vars -I src
ASFLAGS   = -t none

SRCS_C    = src/main.c src/game.c src/asteroids.c src/hud.c src/ufo.c src/title.c
SRCS_ASM  = src/asm/crt0.s src/asm/line.s src/asm/ship.s \
            src/asm/ship_verts.s src/asm/input.s src/asm/shapes.s \
            src/asm/sound.s

OBJ_CRT0    = $(BUILD)/crt0.o
OBJ_LINE    = $(BUILD)/line.o
OBJ_SHIP    = $(BUILD)/ship.o
OBJ_VERTS   = $(BUILD)/ship_verts.o
OBJ_INPUT   = $(BUILD)/input.o
OBJ_SHAPES  = $(BUILD)/shapes.o
OBJ_SOUND   = $(BUILD)/sound.o
OBJ_MAIN    = $(BUILD)/main.o
OBJ_GAME    = $(BUILD)/game.o
OBJ_ASTER   = $(BUILD)/asteroids.o
OBJ_HUD     = $(BUILD)/hud.o
OBJ_UFO     = $(BUILD)/ufo.o
OBJ_TITLE   = $(BUILD)/title.o
OBJS        = $(OBJ_CRT0) $(OBJ_LINE) $(OBJ_SHIP) $(OBJ_VERTS) \
              $(OBJ_INPUT) $(OBJ_SHAPES) $(OBJ_SOUND) $(OBJ_MAIN) \
              $(OBJ_GAME) $(OBJ_ASTER) $(OBJ_HUD) $(OBJ_UFO) \
              $(OBJ_TITLE)

BIN       = $(BUILD)/$(PROJECT).bin
TAP       = $(BUILD)/$(PROJECT).tap

# Fast load injecte le binaire après ~3M cycles (RAM test Oric BASIC 1.0).
FASTLOAD_DONE  = 3500000
TIME_AFTER     = 4000000
TEST_CYCLES    = $(shell echo $$(($(FASTLOAD_DONE) + $(TIME_AFTER))))
SCREENSHOT     = tests/out/phase9_release.ppm
REF_SHOT       = tests/ref/phase9_release.ppm
BENCH_CYCLES   = $(shell echo $$(($(FASTLOAD_DONE) + 25000000)))
BENCH_PROF     = tests/out/phase6_bench.prof

# Inputs scriptés Phase 3 :
#   après 3.5M cycles : CALL 1280 (lance le jeu)
#   après 4.0M cycles : appui RIGHT ARROW (tourne 0.5 s)
#   après 4.5M cycles : appui UP ARROW (thrust)
#   après 5.5M cycles : appui SPACE (tir)
TEST_INPUT     = "$(FASTLOAD_DONE):CALL $(LOAD_ADDR)\n"

.PHONY: all clean run test ref check bench gen_ship host-test

# ── Tests host (x86) ──────────────────────────────────────────────────
# Vérifient le déterminisme et les bornes des routines portables
# (rng8 / rand_offset). Compile en quelques ms, indépendant du SDK Oric.
HOSTCC      = gcc
HOSTCFLAGS  = -Wall -Wextra -std=c90 -O2
HOST_TESTS  = $(BUILD)/test_rng

host-test: $(HOST_TESTS)
	@for t in $(HOST_TESTS); do \
	  echo "=== $$t ==="; \
	  $$t || exit 1; \
	done
	@echo ">>> Tests host PASS"

$(BUILD)/test_rng: tests/host/test_rng.c | $(BUILD)
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

all: $(TAP)

$(BUILD):
	mkdir -p $(BUILD)

# Régénération des tables vaisseau (32 angles précalculés)
gen_ship: tools/gen_ship.py
	python3 tools/gen_ship.py > src/asm/ship_verts.s
	@echo ">>> src/asm/ship_verts.s régénéré"

# Régénération des tables d'astéroïdes (4 formes × 3 tailles)
gen_shapes: tools/gen_shapes.py
	python3 tools/gen_shapes.py > src/asm/shapes.s
	@echo ">>> src/asm/shapes.s régénéré"

$(OBJ_CRT0): src/asm/crt0.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_LINE): src/asm/line.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_SHIP): src/asm/ship.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_VERTS): src/asm/ship_verts.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_INPUT): src/asm/input.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_SHAPES): src/asm/shapes.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(OBJ_SOUND): src/asm/sound.s | $(BUILD)
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/main.s: src/main.c | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_MAIN): $(BUILD)/main.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/game.s: src/game.c src/asteroids.h src/hud.h src/ufo.h src/sound.h | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_GAME): $(BUILD)/game.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/asteroids.s: src/asteroids.c src/asteroids.h | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_ASTER): $(BUILD)/asteroids.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/hud.s: src/hud.c src/hud.h | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_HUD): $(BUILD)/hud.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/ufo.s: src/ufo.c src/ufo.h src/asteroids.h | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_UFO): $(BUILD)/ufo.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BUILD)/title.s: src/title.c src/title.h | $(BUILD)
	$(CC65) $(CFLAGS) -o $@ $<

$(OBJ_TITLE): $(BUILD)/title.s
	$(CA65) $(ASFLAGS) -o $@ $<

$(BIN): $(OBJS) $(CFG)
	$(LD65) -C $(CFG) -o $@ $(OBJS) $(CC65LIB)

$(TAP): $(BIN)
	$(BIN2TAP) $(BIN) --start $(LOAD_ADDR) --exec $(EXEC_ADDR) \
	           --name "$(PROJECT)" -o $@
	@echo ">>> $(TAP) prêt"

run: $(TAP)
	$(EMU) -m oric1 -r $(ROM) -t $(TAP) -f \
	       --type-keys $(TEST_INPUT)

test: $(TAP)
	@mkdir -p tests/out
	$(EMU) --headless -m oric1 -r $(ROM) -t $(TAP) -f \
	       --type-keys $(TEST_INPUT) \
	       --cycles $(TEST_CYCLES) \
	       --screenshot $(SCREENSHOT)
	@echo ">>> capture : $(SCREENSHOT)"

ref: test
	cp $(SCREENSHOT) $(REF_SHOT)
	@echo ">>> référence mise à jour : $(REF_SHOT)"

check: test
	@cmp -s $(REF_SHOT) $(SCREENSHOT) && \
	  echo "PASS — capture identique à la référence" || \
	  (echo "FAIL — divergence avec la référence"; exit 1)

bench: $(TAP)
	@mkdir -p tests/out
	$(EMU) --headless -m oric1 -r $(ROM) -t $(TAP) -f \
	       --type-keys $(TEST_INPUT) \
	       --cycles $(BENCH_CYCLES) \
	       --profile $(BENCH_PROF)
	@echo "=== Rapport profiler ==="
	@cat $(BENCH_PROF)

clean:
	rm -rf $(BUILD) tests/out
