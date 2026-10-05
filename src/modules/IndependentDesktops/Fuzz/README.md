# Recovery argument parser fuzzing

This libFuzzer target checks the production decimal-handle parser against an
independent bounded arithmetic oracle. It does not invoke any Windows operation
using an input handle. Seeds are synthetic public numbers/signs/overflow cases.
Inputs are capped at 64 bytes and truncated at NUL to reflect the API contract.

In an x64 Visual Studio developer shell with LLVM on PATH:

```powershell
clang++ -std=c++20 -fsanitize=fuzzer,address -DNOMINMAX -DUNICODE -D_UNICODE `
  src/modules/IndependentDesktops/Fuzz/ArgumentFuzzer.cpp `
  src/modules/IndependentDesktops/Recovery.cpp `
  src/modules/IndependentDesktops/WindowOperations.cpp `
  -luser32 -ladvapi32 -o IndependentDesktopsArgumentFuzzer.exe
./IndependentDesktopsArgumentFuzzer.exe -seed=49420 -runs=10000 -max_len=64 src/modules/IndependentDesktops/Fuzz/corpus
```

Keep the compiler/sanitizer version, commit, seed, execution count, crash inputs
and exit code with the run evidence. A bounded fuzz run is not proof of exhaustive
input safety. Unicode argument, IPC settings and long-running application behavior
require additional coverage; this target does not claim to cover them.
