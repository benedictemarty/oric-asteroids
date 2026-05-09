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

SRCS_C    = src/main.c
SRCS_ASM  = src/asm/crt0.s

OBJ_CRT0  = $(BUILD)/crt0.o
OBJ_MAIN  = $(BUILD)/main.o
OBJS      = $(OBJ_CRT0) $(OBJ_MAIN)

BIN       = $(BUILD)/$(PROJECT).bin
TAP       = $(BUILD)/$(PROJECT).tap

# Fast load injecte le binaire après ~3M cycles (RAM test Oric BASIC 1.0).
# CALL_CYCLES = instant d'envoi de "CALL 1280" : assez tard pour que le code
# soit en RAM, assez tôt pour laisser TIME_AFTER cycles de jeu avant capture.
FASTLOAD_DONE  = 3500000
TIME_AFTER     = 2500000
TEST_CYCLES    = $(shell echo $$(($(FASTLOAD_DONE) + $(TIME_AFTER))))
SCREENSHOT     = tests/out/phase1_triangle.ppm
REF_SHOT       = tests/ref/phase1_triangle.ppm

.PHONY: all clean run test ref check

all: $(TAP)

$(BUILD):
	mkdir -p $(BUILD)

$(OBJ_CRT0): $(SRCS_ASM) | $(BUILD)
	$(CA65) -t none -o $@ $<

$(BUILD)/main.s: $(SRCS_C) | $(BUILD)
	$(CC65) -t none -O -o $@ $<

$(OBJ_MAIN): $(BUILD)/main.s
	$(CA65) -t none -o $@ $<

$(BIN): $(OBJS) $(CFG)
	$(LD65) -C $(CFG) -o $@ $(OBJS) $(CC65LIB)

$(TAP): $(BIN)
	$(BIN2TAP) $(BIN) --start $(LOAD_ADDR) --exec $(EXEC_ADDR) \
	           --name "$(PROJECT)" -o $@
	@echo ">>> $(TAP) prêt"

run: $(TAP)
	$(EMU) -m oric1 -r $(ROM) -t $(TAP) -f \
	       --type-keys "$(FASTLOAD_DONE):CALL $(LOAD_ADDR)\n"

test: $(TAP)
	@mkdir -p tests/out
	$(EMU) --headless -m oric1 -r $(ROM) -t $(TAP) -f \
	       --type-keys "$(FASTLOAD_DONE):CALL $(LOAD_ADDR)\n" \
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

clean:
	rm -rf $(BUILD) tests/out
