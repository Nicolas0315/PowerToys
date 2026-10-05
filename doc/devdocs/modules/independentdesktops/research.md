# Research record

Repository inspected at `79830211a8e33e6873270e92dd37417a31b2427e` on 2026-10-05.
The implementation deliberately distinguishes confirmed API behavior from
inference and acceptance work.

| Source | Observation or design consequence |
| --- | --- |
| [Issue #49420](https://github.com/microsoft/PowerToys/issues/49420) | Author identifies independently switchable monitors as the principal request; issue is awaiting triage |
| [Issue #58](https://github.com/microsoft/PowerToys/issues/58) | Long-standing request for independent virtual desktops per monitor |
| [Issue #39839](https://github.com/microsoft/PowerToys/issues/39839) | Related native-desktop request was closed and directed to Windows Feedback Hub |
| [IVirtualDesktopManager](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ivirtualdesktopmanager) | Public interface has window desktop identity, current desktop membership and window movement; it has no per-monitor active-desktop selector |
| [ShowWindowAsync](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindowasync) | A successful return means a visibility operation was started, not completed; acknowledgement must inspect actual visibility |
| [UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute) | Explicit child handle inheritance supports the anonymous recovery mapping without broad inherited handles |
| [GetAncestor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getancestor) | Root-owner relationship is used to keep owned windows together |

At the inspected revision, Workspaces' `workspaces-common/VirtualDesktop.h`
checks membership of one current native desktop. FancyZones'
`FancyZonesLib/VirtualDesktop.cpp` combines the public desktop manager with
Explorer registry identity. Launcher/Command Palette helpers move windows
between desktops. A source search found no existing internal desktop-selector
implementation usable for independent monitor switching.

These findings support a limitation of the public API and existing PowerToys
code. They do not prove every private Windows implementation is impossible.
The independent window-group approach is an engineering alternative, not a
claim that Windows now exposes native per-monitor desktops.

## Contribution expectations

The repository's `CONTRIBUTING.md` expects discussion and agreement with the
team before a larger feature is accepted. `AGENTS.md` requires Windows build,
applicable tests and physical multi-monitor coverage with differing DPI.
Maintainer agreement has not been obtained by this implementation session.
The PR must leave that communication checklist item unchecked and explain the
separate-workspace scope rather than using `Closes #49420`.

The current installer generates `BaseApplications.wxs` using
`installer/PowerToysSetupVNext/generateAllFileComponents.ps1` over root build
outputs. The signing manifest uses explicit file names, so the new executable
and module DLL require entries in `.pipelines/ESRPSigning_core.json`.

## Validation tooling sources

- [Visual Studio developer shells](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell): explicitly select amd64 target and host to avoid mixing x86 libraries with an x64 compiler.
- [VSTest console options](https://learn.microsoft.com/en-us/visualstudio/test/vstest-console-options): force x64 for these assemblies and fail when no tests are discovered; an empty discovery is not a successful test execution.
- [InterlockedCompareExchange](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedcompareexchange): shared journal state uses explicit full memory barriers when publishing and reading slot metadata.
