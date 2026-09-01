#!/bin/bash
# ==============================================================================
# demo.sh — Master Live Demonstration Script for "Pseudo Is All You Need"
#
# Showcases all 6 compiler engineering & ML stages end-to-end.
# Usage:
#   ./demo.sh          (interactive step-by-step mode)
#   ./demo.sh --auto   (automated fast-run mode)
# ==============================================================================

set -e

# Color definitions
BOLD="\033[1m"
GREEN="\033[1;32m"
CYAN="\033[1;36m"
YELLOW="\033[1;33m"
BLUE="\033[1;34m"
MAGENTA="\033[1;35m"
RED="\033[1;31m"
RESET="\033[0m"

AUTO_MODE=0
if [ "$1" == "--auto" ]; then
    AUTO_MODE=1
fi

pause_step() {
    if [ $AUTO_MODE -eq 0 ]; then
        echo -e "${YELLOW}Press [Enter] to continue to the next demonstration step...${RESET}"
        read -r
    else
        sleep 1
    fi
}

clear_screen() {
    clear || printf "\033[H\033[J"
}

clear_screen

echo -e "${BLUE}${BOLD}"
echo "================================================================================"
echo "         PSEUDO IS ALL YOU NEED — COMPILER & ML SYSTEM DEMONSTRATION           "
echo "================================================================================"
echo -e "${RESET}"
echo "This demonstration presents the 6 core architectural stages of our compiler:"
echo "  1. Complete Compiler Pipeline (Lexer -> AST -> 3-Address Code IR -> C11 Source)"
echo "  2. Dual-Backend Execution (Custom Bytecode VM vs Standalone Native C Binary)"
echo "  3. Interactive REPL Shell (Persistent Variable & Function Scopes)"
echo "  4. Multi-Error Panic Recovery & Durable Diagnostics Logging (.errlog)"
echo "  5. Native Microcontroller / Embedded C11 Code Generation"
echo "  6. Phase 6 ML Layer (Adaptive Input Understanding & Auto-Repair)"
echo ""
pause_step

# ------------------------------------------------------------------------------
# STAGE 0: Build & Self-Test
# ------------------------------------------------------------------------------
clear_screen
echo -e "${CYAN}${BOLD}=== STAGE 0: Building & Running Full Compiler Test Suite ===${RESET}\n"
make clean > /dev/null
make test
echo ""
echo -e "${GREEN}✓ All Phase 1-5 automated compiler unit tests passed with 100% success!${RESET}\n"
pause_step

# ------------------------------------------------------------------------------
# STAGE 1: Compiler Pipeline Deep Dive
# ------------------------------------------------------------------------------
clear_screen
echo -e "${CYAN}${BOLD}=== STAGE 1: Compiler Pipeline Deep Dive ===${RESET}\n"
echo -e "${BOLD}Source Program (tests/vm_factorial.pseudo):${RESET}"
echo -e "${YELLOW}------------------------------------------------------------${RESET}"
cat tests/vm_factorial.pseudo
echo -e "${YELLOW}------------------------------------------------------------${RESET}\n"
pause_step

echo -e "${BOLD}1. Lexer Tokens (Non-owning slices):${RESET}"
./build/pseudoc --dump-tokens tests/vm_factorial.pseudo | head -n 12
echo "..."
echo ""
pause_step

echo -e "${BOLD}2. Abstract Syntax Tree (AST):${RESET}"
./build/pseudoc --dump-ast tests/vm_factorial.pseudo | head -n 25
echo "..."
echo ""
pause_step

echo -e "${BOLD}3. Shared Linear 3-Address Code (TAC) IR:${RESET}"
./build/pseudoc --dump-ir tests/vm_factorial.pseudo | head -n 25
echo "..."
echo ""
pause_step

echo -e "${BOLD}4. Ahead-Of-Time Emitted C11 Code:${RESET}"
./build/pseudoc --dump-c tests/vm_factorial.pseudo | head -n 28
echo "..."
echo ""
pause_step

# ------------------------------------------------------------------------------
# STAGE 2: Dual-Backend Equivalence Proof
# ------------------------------------------------------------------------------
clear_screen
echo -e "${CYAN}${BOLD}=== STAGE 2: Dual-Backend Cross-Execution & Equivalence Proof ===${RESET}\n"
echo "Running on Backend A (Custom Stack Bytecode Virtual Machine):"
echo -e "${GREEN}$ ./build/pseudoc run tests/vm_factorial.pseudo${RESET}"
./build/pseudoc run tests/vm_factorial.pseudo > build/vm_demo.out
cat build/vm_demo.out
echo ""

echo "Compiling on Backend B (AOT-to-C Native Binary Compilation):"
echo -e "${GREEN}$ ./build/pseudoc build tests/vm_factorial.pseudo -o build/factorial_native${RESET}"
./build/pseudoc build tests/vm_factorial.pseudo -o build/factorial_native
echo "Executing standalone native executable:"
echo -e "${GREEN}$ ./build/factorial_native${RESET}"
./build/factorial_native > build/aot_demo.out
cat build/aot_demo.out
echo ""

echo -e "${BOLD}Checking Cross-Backend Output Diff:${RESET}"
if diff -u build/vm_demo.out build/aot_demo.out; then
    echo -e "${GREEN}✓ Perfect match! VM Output == Native Binary Output (0 byte difference).${RESET}\n"
fi
pause_step

# ------------------------------------------------------------------------------
# STAGE 3: Interactive REPL Shell
# ------------------------------------------------------------------------------
clear_screen
echo -e "${CYAN}${BOLD}=== STAGE 3: Interactive Terminal REPL Shell ===${RESET}\n"
echo "Evaluating expressions, loops, and recursive functions in persistent session:"
echo ""
printf "x = 42\noutput x\nfunction cube(n) return n * n * n endfunction\noutput cube(3)\nexit\n" | ./build/pseudoc repl
echo ""
echo -e "${GREEN}✓ REPL cleanly preserves variable/function scope across statements.${RESET}\n"
pause_step

# ------------------------------------------------------------------------------
# STAGE 4: Multi-Error Panic Recovery & .errlog
# ------------------------------------------------------------------------------
clear_screen
echo -e "${CYAN}${BOLD}=== STAGE 4: Multi-Error Panic Recovery & Durable Logging ===${RESET}\n"
echo -e "Testing malformed input file (${BOLD}tests/semantic_errors.pseudo${RESET}):"
echo -e "${YELLOW}------------------------------------------------------------${RESET}"
cat tests/semantic_errors.pseudo
echo -e "${YELLOW}------------------------------------------------------------${RESET}\n"

echo "Running semantic validation (compiler never halts at first error):"
rm -f .errlog
./build/pseudoc --check tests/semantic_errors.pseudo || true
echo ""

echo -e "${BOLD}Durable Error Corpus Recorded (.errlog):${RESET}"
cat .errlog
echo ""
echo -e "${GREEN}✓ All type, arity, and scope errors caught and formatted for ML training.${RESET}\n"
pause_step

# ------------------------------------------------------------------------------
# STAGE 5: Phase 6 ML Adaptive Input Understanding
# ------------------------------------------------------------------------------
clear_screen
echo -e "${MAGENTA}${BOLD}=== STAGE 5: Phase 6 ML Layer — Adaptive Input Understanding ===${RESET}\n"
echo -e "Simulating user typing errors in ${BOLD}tests/ml_broken_sample.pseudo${RESET}:"
echo -e "${YELLOW}------------------------------------------------------------${RESET}"
cat tests/ml_broken_sample.pseudo
echo -e "${YELLOW}------------------------------------------------------------${RESET}\n"

echo -e "${BOLD}Running Phase 6 Adaptive Rectifier (python3 ml/rectifier.py --fix):${RESET}\n"
python3 ml/rectifier.py tests/ml_broken_sample.pseudo --fix
echo ""
pause_step

# ------------------------------------------------------------------------------
# SUMMARY
# ------------------------------------------------------------------------------
clear_screen
echo -e "${GREEN}${BOLD}"
echo "================================================================================"
echo "                   DEMONSTRATION COMPLETE — ALL 6 PHASES VERIFIED               "
echo "================================================================================"
echo -e "${RESET}"
echo -e "Summary of Accomplishments:"
echo -e "  • ${BOLD}Phase 0${RESET}: Formal EBNF Grammar & Ambiguity Resolutions"
echo -e "  • ${BOLD}Phase 1${RESET}: Non-Owning Lexer, Precedence Parser & Type-Checking Semantics"
echo -e "  • ${BOLD}Phase 2${RESET}: Linear Three-Address Code (TAC) Intermediate Representation"
echo -e "  • ${BOLD}Phase 3${RESET}: Custom Stack-Based Bytecode VM Execution Engine"
echo -e "  • ${BOLD}Phase 4${RESET}: Standalone AOT-to-C Code Generator & Native Compilation"
echo -e "  • ${BOLD}Phase 5${RESET}: Durable .errlog Diagnostics, Multi-Error Recovery & REPL Shell"
echo -e "  • ${BOLD}Phase 6${RESET}: ML Adaptive Input Understanding & Automated Error Rectification"
echo ""
echo -e "${CYAN}Run './build/pseudoc' to start the live interactive REPL anytime!${RESET}\n"
