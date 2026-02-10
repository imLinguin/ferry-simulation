CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS :=

BUILDDIR := buildDir

# Common library sources / objects
COMMON_SRC := src/common/ipc.c src/common/logging.c
COMMON_OBJ := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(COMMON_SRC))

# Process sources
MAIN_SRC           := src/processes/main.c
FERRY_MANAGER_SRC  := src/processes/ferry_manager.c
PASSENGER_SRC      := src/processes/passenger.c
PORT_MANAGER_SRC   := src/processes/port_manager.c

MAIN_OBJ           := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(MAIN_SRC))
FERRY_MANAGER_OBJ  := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(FERRY_MANAGER_SRC))
PASSENGER_OBJ      := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(PASSENGER_SRC))
PORT_MANAGER_OBJ   := $(patsubst src/%.c,$(BUILDDIR)/%.o,$(PORT_MANAGER_SRC))

# Targets
TARGETS := \
	$(BUILDDIR)/ferry-simulation \
	$(BUILDDIR)/ferry-manager \
	$(BUILDDIR)/port-manager \
	$(BUILDDIR)/passenger

.PHONY: all clean

all: $(TARGETS)

$(BUILDDIR)/ferry-simulation: $(MAIN_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/ferry-manager: $(FERRY_MANAGER_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/port-manager: $(PORT_MANAGER_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/passenger: $(PASSENGER_OBJ) $(COMMON_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)
