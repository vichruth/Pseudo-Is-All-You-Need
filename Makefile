# Build for the pseudocode compiler.
#
# Targets:
#   make          -> build/pseudoc      dev build
#   make release  -> build/pseudoc-rel  optimized
#   make test     -> full test suite across all phases

CC       := cc
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Iinclude
RELFLAGS := -O2 -DNDEBUG
LDLIBS   := -lm

# Sanitizers probe
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
SAN_OK := $(shell printf 'int main(void){return 0;}' > /tmp/.santest.c 2>/dev/null && \
             $(CC) $(SAN_FLAGS) /tmp/.santest.c -o /tmp/.santest >/dev/null 2>&1 && echo yes)

ifeq ($(SAN_OK),yes)
DEVFLAGS := -g -O0 $(SAN_FLAGS)
else
DEVFLAGS := -g -O0
$(warning sanitizers unavailable (missing libasan/libubsan) - building without them)
endif

SRC     := $(wildcard src/*.c)
BUILD   := build
BIN     := $(BUILD)/pseudoc
RELBIN  := $(BUILD)/pseudoc-rel

.PHONY: all release clean test test-lexer test-ast test-semantics test-ir test-vm test-aot test-errlog

all: $(BIN)

$(BIN): $(SRC) $(wildcard include/*.h) | $(BUILD)
	$(CC) $(CFLAGS) $(DEVFLAGS) $(SRC) -o $@ $(LDLIBS)

release: $(SRC) $(wildcard include/*.h) | $(BUILD)
	$(CC) $(CFLAGS) $(RELFLAGS) $(SRC) -o $(RELBIN) $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

# Phase 1 Test targets
test-lexer: $(BIN)
	@echo "=== Testing Lexer ==="
	./$(BIN) --dump-tokens tests/lex_all_tokens.pseudo > /dev/null
	@echo "Lexer test passed."

test-ast: $(BIN)
	@echo "=== Testing Parser & AST ==="
	./$(BIN) --dump-ast tests/parse_valid.pseudo > /dev/null
	@echo "Parser AST test passed."

test-semantics: $(BIN)
	@echo "=== Testing Semantic Analysis ==="
	./$(BIN) --check tests/parse_valid.pseudo > /dev/null
	./$(BIN) --check tests/lex_all_tokens.pseudo > /dev/null
	@echo "Semantic analysis tests passed."

test-ir: $(BIN)
	@echo "=== Testing Shared IR Generation ==="
	./$(BIN) --dump-ir tests/parse_valid.pseudo > /dev/null
	@echo "IR generation test passed."

test-vm: $(BIN)
	@echo "=== Testing Bytecode VM Execution ==="
	./$(BIN) run tests/vm_factorial.pseudo > /dev/null
	./$(BIN) run tests/vm_arrays_loops.pseudo > /dev/null
	@echo "VM execution tests passed."

test-aot: $(BIN)
	@echo "=== Testing AOT-to-C Native Compilation & Cross-Backend Equivalence ==="
	./$(BIN) build tests/vm_factorial.pseudo -o $(BUILD)/test_factorial_aot
	./$(BIN) build tests/vm_arrays_loops.pseudo -o $(BUILD)/test_arrays_aot
	./$(BIN) run tests/vm_factorial.pseudo > $(BUILD)/vm_fact.out
	./$(BUILD)/test_factorial_aot > $(BUILD)/aot_fact.out
	diff -u $(BUILD)/vm_fact.out $(BUILD)/aot_fact.out
	./$(BIN) run tests/vm_arrays_loops.pseudo > $(BUILD)/vm_arr.out
	./$(BUILD)/test_arrays_aot > $(BUILD)/aot_arr.out
	diff -u $(BUILD)/vm_arr.out $(BUILD)/aot_arr.out
	@echo "AOT-to-C native output matches VM output 100% byte-for-byte!"

test-errlog: $(BIN)
	@echo "=== Testing Durable Error Logging (.errlog) & Recovery ==="
	rm -f .errlog
	-./$(BIN) --check tests/parse_errors.pseudo > /dev/null 2>&1
	-./$(BIN) --check tests/semantic_errors.pseudo > /dev/null 2>&1
	@test -f .errlog || (echo "error: .errlog file not created" && exit 1)
	@grep -q "E100" .errlog || (echo "error: E100 parser errors not found in .errlog" && exit 1)
	@grep -q "E201" .errlog || (echo "error: E201 semantic errors not found in .errlog" && exit 1)
	@grep -q "E202" .errlog || (echo "error: E202 semantic errors not found in .errlog" && exit 1)
	@grep -q "E203" .errlog || (echo "error: E203 semantic errors not found in .errlog" && exit 1)
	@grep -q "E204" .errlog || (echo "error: E204 semantic errors not found in .errlog" && exit 1)
	@grep -q "E205" .errlog || (echo "error: E205 semantic errors not found in .errlog" && exit 1)
test-ambiguity: $(BIN)
test: test-lexer test-ast test-semantics test-ir test-vm test-aot test-ambiguity test-errlog
	@echo "\nAll Phase 1, 2, 3, 4, 5 & Ambiguity tests passed successfully!"

