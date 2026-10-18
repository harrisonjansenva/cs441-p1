#!/bin/sh
# Regression tests for isaSim. Run from anywhere:  sh sim/tests/run_tests.sh   (or: make test)
# Work files go to sim/build/test_tmp (removed by `make clean`).
here=$(cd "$(dirname "$0")" && pwd)
sim=$(dirname "$here")
bench=$(dirname "$sim")/benchmarks
isa="$sim/isaSim"
tmp="$sim/build/test_tmp"
mkdir -p "$tmp"
cd "$tmp" || exit 1

pass=0; fail=0
ok()   { pass=$((pass + 1)); echo "  ok   $1"; }
bad()  { fail=$((fail + 1)); echo "  FAIL $1"; }
check() { # check <name> <command...>
    name=$1; shift
    if "$@" >/dev/null 2>&1; then ok "$name"; else bad "$name"; fi
}
# word at hex address in a .mem.out file ("" if absent)
word() { grep -i "^0x$(printf '%07X' "$2")," "$1" | sed 's/.*, *//'; }

echo "== benchmarks vs checked-in expected output"
for b in maxfinder-int maxfinder-fp benchmark1; do
    "$isa" -r "$bench/$b.asm" "$bench/$b.mem" </dev/null >/dev/null 2>&1
    check "$b -r matches $b.mem.out" diff "$b.mem.out" "$bench/$b.mem.out"
    cp "$b.mem.out" "$b.run.out"; cp "$b.instrCount" "$b.run.count"
    printf 'c\n' | "$isa" -s "$bench/$b.asm" "$bench/$b.mem" >/dev/null 2>&1
    check "$b -s == -r (memory)" diff "$b.mem.out" "$b.run.out"
    check "$b -s == -r (instrCount)" diff "$b.instrCount" "$b.run.count"
    printf '\n\n\n5\nc\n' | "$isa" -s "$bench/$b.asm" "$bench/$b.mem" >/dev/null 2>&1
    check "$b -s mixed step sizes == -r" diff "$b.mem.out" "$b.run.out"
done

echo "== benchmark1 results vs values derived by hand (independent of the simulator)"
missing=0
while IFS= read -r line; do
    grep -qxF "$line" benchmark1.run.out || { missing=$((missing + 1)); echo "    missing/different: $line"; }
done < "$here/benchmark1.expected"
[ "$missing" -eq 0 ] && ok "all $(wc -l < "$here/benchmark1.expected" | tr -d ' ') expected benchmark1 words present" \
                     || bad "benchmark1 expected words ($missing wrong)"

echo "== labeled sources behave exactly like the resolved (address-only) benchmarks"
for b in maxfinder-int maxfinder-fp benchmark1; do
    "$isa" -r "$bench/labeled/$b.asm" "$bench/$b.mem" </dev/null >/dev/null 2>&1
    check "$b labeled == resolved (memory)" diff "$b.mem.out" "$b.run.out"
    check "$b labeled == resolved (instrCount)" diff "$b.instrCount" "$b.run.count"
done
for b in maxfinder-int maxfinder-fp benchmark1; do   # no labels left in the submitted files
    if grep -vE '^\s*(#|;|$)' "$bench/$b.asm" | grep -qE '^\s*[A-Za-z_][A-Za-z0-9_]*\s*:'; then bad "$b.asm has label definitions"
    else ok "$b.asm uses concrete addresses only"; fi
done

echo "== coverage: every instruction executed at least once in benchmark1"
if grep -E ': 0$' benchmark1.run.count >/dev/null; then bad "benchmark1 covers every instruction"
else ok "benchmark1 covers every instruction"; fi

echo "== MAXFINDER edge cases"
mkmem() { # mkmem <file> <out-addr-ignored> <values...> ; array at 0x20, N at 0x11
    f=$1; shift
    { echo "# t"; echo "26, 32"; echo "0x10, 0x30"; echo "0x11, $#"; echo "0x12, 0x20"
      a=32; for v in "$@"; do printf '0x%X, %s\n' "$a" "$v"; a=$((a + 1)); done; } > "$f"
}
t_int() { # t_int <name> <expected> <values...>
    n=$1; e=$2; shift 2
    mkmem "$n.mem" "$@"
    "$isa" -r "$bench/maxfinder-int.asm" "$n.mem" >/dev/null 2>&1
    [ "$(word "$n.mem.out" 0x30)" = "$e" ] && ok "int: $n" || bad "int: $n (got '$(word "$n.mem.out" 0x30)', want $e)"
}
t_fp() {
    n=$1; e=$2; shift 2
    mkmem "$n.mem" "$@"
    "$isa" -r "$bench/maxfinder-fp.asm" "$n.mem" >/dev/null 2>&1
    [ "$(word "$n.mem.out" 0x30)" = "$e" ] && ok "fp: $n" || bad "fp: $n (got '$(word "$n.mem.out" 0x30)', want $e)"
}
t_int int_max_first 0x00000009 9 1 2 3
t_int int_max_last 0x00000009 1 2 3 9
t_int int_all_negative 0xFFFFFFFF 0xFFFFFFF0 0xFFFFFFFF 0xFFFFFFF5
t_int int_single 0x00000007 7
t_fp fp_max_first 0x41100000 0x41100000 0x3FC00000 0x40000000   # 9.0 1.5 2.0
t_fp fp_max_last 0x41100000 0x3FC00000 0x40000000 0x41100000
t_fp fp_all_negative 0xBFC00000 0xC0400000 0xBFC00000 0xC0000000  # -3 -1.5 -2
t_fp fp_neg_zero_vs_neg 0x80000000 0xBFC00000 0x80000000 0xC0000000 # -1.5 -0.0 -2

echo "== assembler / loader errors (must fail, must not run)"
expect_fail() { # expect_fail <name> <pattern> ; asm on stdin file t.asm
    name=$1; pat=$2
    out=$("$isa" -r t.asm t.mem 2>&1 </dev/null); rc=$?
    if [ $rc -ne 0 ] && echo "$out" | grep -qi "$pat"; then ok "$name"; else bad "$name (rc=$rc: $out)"; fi
}
printf '# t\n26, 32\n0x10, 0x1\n' > t.mem
printf 'LDR R32, 1\nHALT\n'          > t.asm; expect_fail "register R32 rejected" "out of range"
printf 'LDR R2, 70000\nHALT\n'       > t.asm; expect_fail "imm16 overflow rejected" "16 bits"
printf 'JUMP nowhere\nHALT\n'        > t.asm; expect_fail "undefined label rejected" "undefined label"
printf 'FOO R2, R3\nHALT\n'          > t.asm; expect_fail "unknown mnemonic rejected" "unknown instruction"
printf 'ADD R2, R3\nHALT\n'          > t.asm; expect_fail "wrong operand count rejected" "operand"
printf 'L:\nL:\nHALT\n'              > t.asm; expect_fail "duplicate label rejected" "duplicate"
printf 'LDR R2, 1\n'                 > t.asm; expect_fail "falling off the end faults" "illegal"
printf 'L: JUMP L\n'                 > t.asm
out=$("$isa" -r --max-instr 1000 t.asm t.mem 2>&1 </dev/null); rc=$?
[ $rc -ne 0 ] && echo "$out" | grep -q "stopped before HALT" && ok "infinite loop stopped by --max-instr" || bad "max-instr"
printf 'HALT\n' > t.asm
printf '# t\n26, 32\n(e) 0x1\n' > t.mem; expect_fail "bad .mem token rejected" "expected"
printf '# t\n24, 32\n' > t.mem;            expect_fail "wrong .mem header rejected" "26, 32"
printf '# t\n26, 32\n' > t.mem
check "usage error without args" sh -c "! '$isa' -r"

echo "== semantics spot checks"
printf '# t\n26, 32\n' > t.mem
cat > t.asm <<'EOF'
LDR R0, 5          # write to R0 is discarded
LDR R1, 9          # write to R1 is discarded
ADD R2, R0, R1     # 0 + 1
LDR R3, 0x100
ST  R2, R3, 0
ST  R0, R3, 1
ST  R1, R3, 2
LDR R4, 0
DEC R4, R4         # 0xFFFFFFFF
LDR R5, 0
CLZ R5, R5         # clz(0) = 32
ST  R5, R3, 3
CNTT0 R6, R0       # ctz(0) = 32
ST  R6, R3, 4
HALT
EOF
"$isa" -r t.asm t.mem >/dev/null 2>&1
[ "$(word t.mem.out 0x100)" = "0x00000001" ] && ok "R0/R1 constants preserved" || bad "R0/R1 constants"
[ "$(word t.mem.out 0x103)" = "0x00000020" ] && ok "CLZ(0) = 32" || bad "CLZ(0)"
[ "$(word t.mem.out 0x104)" = "0x00000020" ] && ok "CNTT0(0) = 32" || bad "CNTT0(0)"

echo
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
