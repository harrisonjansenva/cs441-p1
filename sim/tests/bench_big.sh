#!/bin/sh
# Time the integer and FP MAXFINDER benchmarks on a large random array and check the answer.
#   usage: sh tests/bench_big.sh [N]        (default N = 1000000)
# "load" = a HALT-only program on the same .mem file (parse + memory build), so
# execute time ~= total - load. Work files go to sim/build/big (removed by `make clean`).
here=$(cd "$(dirname "$0")" && pwd)
sim=$(dirname "$here")
bench=$(dirname "$sim")/benchmarks
isa="$sim/build/isaSim_bin"
[ -x "$isa" ] || make -s -C "$sim" || exit 1
n=${1:-1000000}
work="$sim/build/big"
mkdir -p "$work"
cd "$work" || exit 1
printf 'HALT\n' > halt.asm

now() { python3 -c 'import time; print(time.time())'; }
secs() { python3 -c "print(f'{$2 - $1:.3f}')"; }

status=0
for kind in int fp; do
    exp=$(python3 "$here/gen_big.py" $kind "$n" big-$kind.mem)
    asm="$bench/maxfinder-$kind.asm"

    t0=$(now); "$isa" -r halt.asm big-$kind.mem >/dev/null 2>&1; t1=$(now)
    "$isa" -r "$asm" big-$kind.mem > run-$kind.txt 2>&1; t2=$(now)

    got=$(grep -i '^0x0000030,' big-$kind.mem.out | sed 's/.*, *//')
    total=$(grep '^TOTAL:' run-$kind.txt | sed 's/TOTAL: //')
    load=$(secs "$t0" "$t1"); all=$(secs "$t1" "$t2")
    exec=$(python3 -c "print(f'{max($all - $load, 0):.3f}')")
    rate=$(python3 -c "e=max($all - $load, 1e-9); print(f'{$total / e / 1e6:.0f}')")
    if [ "$got" = "$exp" ]; then verdict=ok; else verdict="WRONG (want $exp)"; status=1; fi
    printf '%-4s N=%-9s instructions=%-11s load=%ss total=%ss exec~%ss (~%s M instr/s)  max=%s %s\n' \
        "$kind" "$n" "$total" "$load" "$all" "$exec" "$rate" "$got" "$verdict"
done
exit $status
