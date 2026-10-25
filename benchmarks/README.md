## Editing Benchmarks

Each benchmark exists in two forms:

* `labeled/<name>.asm`: the readable source, with names like `LOOP:` and `DONE:`. **Edit only these.**
* `<name>.asm`: the generated, address-only version we use in the simulator. Do NOT edit these by hand.

After changing a labeled file, regenerate the submitted version from this directory:

```
python3 ../sim/tools/resolve_labels.py labeled/<name>.asm > <name>.asm
```

Once you have regenerated the unlabeled .mem files, copy them back here.

Then, refresh the expected output (`cd ../sim && ./isaSim -r ../benchmarks/<name>.asm ../benchmarks/<name>.mem`,
Copy the new `<name>.mem.out` over the checked-in one) and run `make test` in `sim/`. The tests confirm
the labeled and resolved versions behave identically and that no label definitions remain in the submitted files.