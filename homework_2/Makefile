# Makefile - Simulador de Gerenciamento de Memoria Continua
#
# Alvos disponiveis:
#   make            compila o simulador em build/memory-simulator
#   make run        compila e executa a interface de terminal
#   make test       compila e executa todos os testes automatizados
#   make asan       executa os testes com AddressSanitizer e UndefinedBehaviorSanitizer
#   make clean      remove os artefatos de build
#   make help       mostra esta ajuda

CC        := gcc
CSTD      := -std=c11
WARNINGS  := -Wall -Wextra -Wpedantic

SRC_DIR   := src
TEST_DIR  := tests
BUILD_DIR := build
SAN_DIR   := $(BUILD_DIR)/san

CFLAGS    := $(CSTD) $(WARNINGS) -O2 -I$(SRC_DIR) -I$(TEST_DIR)
SANFLAGS  := $(CSTD) $(WARNINGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I$(SRC_DIR) -I$(TEST_DIR)

MAIN_SRC  := $(SRC_DIR)/main.c
LIB_SRCS  := $(filter-out $(MAIN_SRC), $(wildcard $(SRC_DIR)/*.c))
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)

TARGET       := $(BUILD_DIR)/memory-simulator
SAN_TARGET   := $(SAN_DIR)/memory-simulator
TEST_BINS    := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%,$(TEST_SRCS))
SAN_TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(SAN_DIR)/%,$(TEST_SRCS))

.PHONY: all run test asan clean help

all: $(TARGET)

$(BUILD_DIR) $(SAN_DIR):
	mkdir -p $@

$(TARGET): $(LIB_SRCS) $(MAIN_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LIB_SRCS) $(MAIN_SRC) -o $@

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(LIB_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/$*.c $(LIB_SRCS) -o $@

$(SAN_TARGET): $(LIB_SRCS) $(MAIN_SRC) | $(SAN_DIR)
	$(CC) $(SANFLAGS) $(LIB_SRCS) $(MAIN_SRC) -o $@

$(SAN_DIR)/%: $(TEST_DIR)/%.c $(LIB_SRCS) | $(SAN_DIR)
	$(CC) $(SANFLAGS) $(TEST_DIR)/$*.c $(LIB_SRCS) -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TEST_BINS)
	@falha=0; \
	for teste in $(TEST_BINS); do \
		echo "=== $$teste ==="; \
		./$$teste || falha=1; \
	done; \
	if [ $$falha -eq 0 ]; then echo "TODOS OS TESTES PASSARAM"; else echo "HOUVE FALHAS NOS TESTES"; fi; \
	exit $$falha

asan: $(SAN_TARGET) $(SAN_TEST_BINS)
	@falha=0; \
	for teste in $(SAN_TEST_BINS); do \
		echo "=== (sanitizer) $$teste ==="; \
		./$$teste || falha=1; \
	done; \
	if [ $$falha -eq 0 ]; then echo "TESTES COM SANITIZER PASSARAM"; else echo "HOUVE FALHAS NOS TESTES COM SANITIZER"; fi; \
	exit $$falha

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Simulador de Gerenciamento de Memoria Continua"
	@echo ""
	@echo "  make       - compila o simulador"
	@echo "  make run   - compila e executa o simulador"
	@echo "  make test  - compila e roda os testes"
	@echo "  make asan  - roda os testes com ASan + UBSan"
	@echo "  make clean - remove os artefatos"
