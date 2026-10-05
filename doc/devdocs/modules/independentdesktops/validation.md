# Validation and test data

This document is a reproduction guide and a coverage ledger. An unexecuted
case is a requirement, not a passed result. The PR description links the exact
tested commits and final CI attempts; a previous commit's green run is not proof
for a newer source change.

## Published inputs

All automated inputs are synthetic and checked into `Tests/`. No real application
titles, desktop screenshots, user profiles, credentials or private data are used.

| Dataset | Values |
| --- | --- |
| Window identities | Sequential synthetic HWND values, PID 10, thread 20, creation marker 30; reuse cases alter creation time or incarnation |
| Native desktop identifiers | `native-1`, `native-2` |
| Monitor identifiers | `left`, `right` |
| Workspace identifiers | Zero-based 0..3 in the model; displayed as 1..4 |
| Owned group | Synthetic window 3 owned by window 2; same-monitor and cross-monitor variants |
| Capacity | 1,025 synthetic roots against a limit of 1,024 |
| Deterministic sequence | `std::mt19937` seed 49420; 20 windows across two monitors; 10,000 independently targeted switches compared with a reference visibility model |
| Windows runtime fixture | A newly created native STATIC top-level test window; no application installation required |
| Crash fixture | A child owner process hides the test process's window, then terminates itself with exit 88 while bypassing destructors |
| Double-failure fixture | The child also terminates its watchdog before exiting; the next-start restoration routine must restore retained window intent |

`CoreCases.h` is shared by the supplemental console test and native Visual Studio
unit-test adapter. It covers target-monitor isolation, returning visibility,
originally hidden windows, app-hidden windows, new-window membership, whole-group
moves, cross-monitor dialogs, nonmutating plans, stale plans, native desktop scope,
reused handles, dragged windows, wraparound, reset and capacity refusal.

`RuntimeTests.cpp` exercises real Win32 identities, guardian readiness, hide/show
acknowledgement, exclusion of originally hidden windows, marker mismatch, minimized
state, guardian death, foreign-process crash restoration, double-failure recovery
and the invisible installer's close endpoint. There are 19 model cases and 10
runtime cases after these additions; passing evidence must identify the commit.

## Reproduction commands

Portable supplemental model checks (macOS/Linux):

```sh
cmake -S src/modules/IndependentDesktops -B /tmp/independent-desktops-build
cmake --build /tmp/independent-desktops-build
ctest --test-dir /tmp/independent-desktops-build --output-on-failure
```

Supplemental Windows runtime/model checks (isolated Windows test session):

```powershell
cmake -S src/modules/IndependentDesktops -B independent-desktops-build -A x64
cmake --build independent-desktops-build --config Release
ctest --test-dir independent-desktops-build -C Release --output-on-failure --output-junit results.xml
```

This builds the runtime executable as well as the tests. The fixture tests run
against windows they create; they do not certify the application's complete
multi-monitor/Task View interaction. Do not run orphan-recovery tests alongside
an active Independent Desktops manager. Native DLL settings tests also require
the module disabled in an isolated test session, since lifecycle events are shared.

Repository build and native test path:

```powershell
tools/build/build-essentials.ps1 -Platform x64 -Configuration Release
tools/build/build.ps1 -Platform x64 -Configuration Release -Path src/modules/IndependentDesktops
tools/build/build.ps1 -Platform x64 -Configuration Release -Path src/modules/IndependentDesktops/ModuleInterface
tools/build/build.ps1 -Platform x64 -Configuration Release -Path src/modules/IndependentDesktops/Tests
# After all builds exit 0, use VS Test Explorer or vstest.console.exe:
vstest.console.exe x64/Release/tests/IndependentDesktops/IndependentDesktopsUnitTests.dll /Logger:trx /Platform:x64 -- RunConfiguration.TreatNoTestsAsError=true
```

The current official Azure pipeline discovers native DLLs using `*UnitTest*.dll`
in `.pipelines/v2/templates/job-build-project.yml`; the native test project matches
that pattern. Installer root-output harvesting and explicit signing include both
new binaries. The obsolete paths in the PR template are not used as evidence.

Settings model tests live in `Settings.UI.UnitTests/ModelsTests/IndependentDesktopsSettingsTests.cs`.
Build the Settings unit-test project with repository scripts before running
the full Settings regression suite, since module metadata and GeneralSettings are shared:

```powershell
tools/build/build.ps1 -Platform x64 -Configuration Release -Path src/settings-ui/Settings.UI.UnitTests
vstest.console.exe Release/x64/tests/SettingsTests/Settings.UI.UnitTests.dll /Logger:trx /Platform:x64 -- RunConfiguration.TreatNoTestsAsError=true
```
 The existing Settings navigation smoke suite includes the new page.
Follow the repository's supported test runner instructions; do not substitute
`dotnet test` for the required Windows build/test flow.

The supplemental GitHub workflow publishes JUnit and detailed test output, and
the integration job publishes repository build logs/TRX. Both failed and passed
attempts are retained and linked. ARM64 execution is not covered by an x64 run.

## Observed development results

- Initial model stub: 11 behavioral failures out of 15 cases, as expected before
  implementation. The subsequent model implementation passed those cases locally.
- First Windows build caught a void-pointer handle cast (`C26471`); it was fixed
  before claiming a test execution result.
- [Windows RED run](https://github.com/Nicolas0315/PowerToys/actions/runs/37319871400),
  commit `7a70a20d202ec9e7d39c543ee9b8288654d36b35`: build passed; seven runtime
  contracts failed with the unimplemented recovery adapter.
- [First implemented recovery run](https://github.com/Nicolas0315/PowerToys/actions/runs/37320850709),
  commit `68f4b54b987165391921ad8d0e879f3f1d2fa95a`: foreign-process crash recovery
  passed, but three same-thread async visibility cases failed. Message processing
  for the target window was corrected; the test expectations were retained.
- [Windows GREEN run](https://github.com/Nicolas0315/PowerToys/actions/runs/37322480845),
  commit `f553e9e50e`: runtime executable compiled, 17 model cases and eight
  runtime cases passed, including minimized and foreign-process crash restoration.
  This run precedes integration and the added double-failure test; it does not
  certify those later changes.

- [Double-failure RED run](https://github.com/Nicolas0315/PowerToys/actions/runs/37323230182),
  commit `19f793f417`: the new next-start recovery test failed while the previous
  eight runtime cases passed. Retained per-window restoration intent was added.
- [Later supplemental run](https://github.com/Nicolas0315/PowerToys/actions/runs/37325867073),
  commit `b30feb68ca`: 18 model and nine runtime cases passed. Repository integration
  and fuzz did not pass at this commit.
- [First repository integration attempt](https://github.com/Nicolas0315/PowerToys/actions/runs/37322745586):
  solution restore and native Runner build passed, but three StyleCop errors in
  new Settings files stopped the Settings build. Headers and spacing were fixed.
- [First fuzz attempt](https://github.com/Nicolas0315/PowerToys/actions/runs/37324656134):
  a PowerShell unquoted comma in the sanitizer option caused a parser error before
  compilation. The option was quoted without removing sanitizers or reducing inputs.
- A subsequent fuzz compile exposed an x86 library environment with an x64 LLVM
  target (`LNK4272` / unresolved runtime symbols). The workflow now explicitly
  launches an x64 Visual Studio developer shell; sanitizers remain enabled.
- Independent specification review found an app-hidden owned dialog on another
  monitor could permit a partial transition. A new model regression failed before
  the fix; validation now rejects the whole affected group for both Select and Move.
- Code inspection found installer force-termination could kill both manager and
  guardian. A hidden manager close endpoint and a protected installer path were
  added. Endpoint execution is automated; actual uninstall/update remains unexecuted.

- Independent quality review reproduced a foreground dialog moved onto a different
  monitor selecting that monitor's unrelated workspace. The new foreground
  regression failed before the fix; activation now requires matching assignment
  and an unsplit group, and automatic cross-monitor refusals do not stop recovery.
- Shared journal state reads now use Interlocked barriers as well as writes;
  plain volatile reads were insufficient to establish ARM64 acquire ordering.
  x64 tests do not certify ARM64 execution or prove absence of every timing race.

- The next Settings UI compile caught seven new ViewModel spacing diagnostics
  (SA1513/SA1516). Blank lines were added without changing behavior.
- The sanitized fuzzer compiled, but its first launch lacked the matching ASan
  runtime DLL. After staging that DLL, output showed an unquoted argument also
  included the checkout directory as a corpus. Explicit argument-array passing
  limits the intended input corpus to the five checked-in synthetic seed files;
  that earlier run is not counted as the intended corpus validation.

- SettingsAPI can throw `winrt::hresult_error`, which is not caught by
  `std::exception`. The module's settings boundaries now catch these failures.
  A native integration regression loads the built module DLL without enabling it
  and sends 11 published malformed JSON/type/range/modifier inputs, preserving
  the read-only baseline of all five shortcuts after each rejection. Parsing commits shortcuts only
  after validating the whole input; rejected input is not saved. The 19 shared
  model cases plus this DLL-boundary case make 20 native adapter tests.

- Review caught a test fixture that would use the production saver to establish a
  custom shortcut and overwrite a local tester's settings. The fixture now captures
  the current hotkeys without saving anything and asserts every field is unchanged
  after each rejected input. This avoids a crash-sensitive save/restore workaround.

## Required physical/interactive acceptance

Every row below remains **unexecuted** until recorded with the tested commit,
Windows build, PowerToys configuration, monitor topology/DPI and observed output.

| Scenario | Required observation |
| --- | --- |
| Two physical monitors at 100% / 150% DPI | Switch A then B; opposite monitor's windows and selected group remain unchanged |
| Owned modal dialog on one monitor | Root/dialog hide and return together; dialog remains usable |
| Owned dialog spanning two monitors | Refused switch leaves both monitors' visibility unchanged |
| Cross-monitor drag | Visible root joins destination monitor's selected group without changing the other selection |
| New windows and application activation | New windows join the selected group; activating an existing group selects only its monitor |
| Native Task View switching | Native switch restores prior hidden state and reconciles destination groups; never claim native monitor independence |
| Native pinned windows | Verify exclusion or document the precise unsupported pin configuration |
| Elevated and inaccessible applications | Unsupported windows remain unchanged; no unprotected hiding |
| Minimized, maximized and full-screen applications | Preserve state and coordinates through switching and disable |
| Hung application | Timeout is reported as failure; restoration intent remains until acknowledgement; window returns after the app resumes |
| Unplug/replug monitor | Managed windows restored, assignments reset, no window permanently hidden |
| Disable, Runner exit and manager crash | All managed windows restored without closing the application |
| Watchdog crash | Manager detects loss, restores windows and exits; further hides are refused |
| Simultaneous manager/watchdog failure | Next-start recovery restores tagged intent before new workspace management begins |
| Repeated enable/disable, Settings hotkey edits | Correct lifecycle, no orphaned handles, shortcuts update through Runner |
| GPO disabled/unconfigured, uninstall/update | Policy respected, normal startup unchanged, recovery and installer behavior verified |
| ARM64 build and runtime | Build and execute on ARM64, rather than infer from x64 success |

Before marking ready for review/merge, obtain maintainer agreement on the module,
complete these acceptance cases, resolve independent review findings, and attach
the final-head repository CI evidence. These are explicitly outstanding gates.
