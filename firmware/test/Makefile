# Firefly firmware verification
#
#   make            everything below; results land in ../docs/results/
#   make unit       host unit tests of src/firefly.c and the thresholds
#   make sweep      exhaustive two-board search over sync constants
#   make sim        the real firmware binary on simavr (needs libsimavr-dev, libelf-dev)
#   make swarm      many-board simulation (python3; matplotlib for plots)
#   make quick      swarm with 20 trials instead of 100

CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Werror -std=gnu99
RESULTS = ../docs/results
ELF     = ../build/firefly.elf
PY      = python3

all: unit sweep sim swarm

$(RESULTS):
	mkdir -p $(RESULTS)

unit: | $(RESULTS)
	$(CC) $(CFLAGS) -o /tmp/firefly_ut test_firefly.c ../src/firefly.c
	/tmp/firefly_ut | tee $(RESULTS)/unit.txt

sweep: | $(RESULTS)
	$(CC) $(CFLAGS) -o /tmp/firefly_sweep param_sweep.c ../src/firefly.c
	/tmp/firefly_sweep | tee $(RESULTS)/param-sweep.md

$(ELF):
	$(MAKE) -C ..

sim: $(ELF) | $(RESULTS)
	$(CC) -O2 -Wall -o /tmp/firefly_sim sim/fwsim.c $$(pkg-config --cflags --libs simavr) -lelf -lm
	ram=$$(avr-size -A $(ELF) | awk '/^\.data/{d=$$2} /^\.bss/{b=$$2} END{print d+b}'); \
	/tmp/firefly_sim $(ELF) $(RESULTS)/sim.md $$ram | grep -v '^Loaded' | tee $(RESULTS)/sim.txt

swarm: | $(RESULTS)
	$(PY) swarm_report.py

quick: | $(RESULTS)
	$(PY) swarm_report.py --quick

clean:
	rm -f /tmp/firefly_ut /tmp/firefly_sweep /tmp/firefly_sim /tmp/firefly_swarm*

.PHONY: all unit sweep sim swarm quick clean
SHELL = /bin/bash
.SHELLFLAGS = -o pipefail -c
