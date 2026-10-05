# Independent Desktops (experimental)

Independent Desktops provides four independent **window workspaces** on each
connected monitor. It does not create native Windows virtual desktops or replace
Task View. The feature is disabled by default.

This implements the independently switchable monitor workflow discussed in
[#49420](https://github.com/microsoft/PowerToys/issues/49420), using a separately
approved window-group approach. It does not implement the other features listed
in that issue, and does not claim to resolve native per-monitor virtual desktops.

## Using the module

Enable Independent Desktops in PowerToys Settings. Each monitor starts on
workspace 1. New eligible application windows join the workspace selected on
their monitor. Moving an existing visible window between monitors assigns it to
the destination monitor's selected workspace.

| Action | Default shortcut | Target |
| --- | --- | --- |
| Previous workspace | Win+Ctrl+Alt+Left | Monitor under the pointer |
| Next workspace | Win+Ctrl+Alt+Right | Monitor under the pointer |
| Move window group to previous workspace | Win+Ctrl+Alt+Shift+Left | Foreground window's monitor |
| Move window group to next workspace | Win+Ctrl+Alt+Shift+Right | Foreground window's monitor |
| Restore all managed windows | Win+Ctrl+Alt+Home | All monitors; resets module workspace assignments |

All five shortcuts can be changed in Settings. Moving a window group changes its
membership without changing the selected workspace. Owned dialogs move with the
root window. An operation affecting a dialog on another monitor is refused.

For example, keep communication windows on monitor A, move development windows
between workspaces on monitor B, and switch only monitor B using the pointer
and workspace shortcuts. Monitor A's selected workspace stays unchanged.

The module stores membership and selections only for the running session.
There are no saved application titles, persistent window layouts, screenshots,
or credentials. Settings store only the module's hotkeys and enablement.

## Compatibility and recovery

- Windows native desktops still switch all monitors. The module maintains its
  own monitor selections separately within each native desktop. Switching native
  desktops restores the module's hidden-window state before reconciling the
  selected workspace on the destination desktop.
- Removing/reconfiguring monitors restores managed windows and resets module
  assignments. The module does not reposition application windows.
- Already hidden windows are not adopted. Shell surfaces and PowerToys windows
  are excluded. Windows that cannot be queried or marked are left untouched;
  this includes elevated windows when the manager lacks sufficient access.
- The public desktop API cannot reliably report every application's pinning
  configuration. Windows whose reported native desktop ID differs from the
  current desktop are excluded. Native pinning interactions remain an explicit
  acceptance test; they are not certified by the hosted tests.
- Visibility requests are asynchronous and bounded. If an operation cannot be
  acknowledged, the module stops and hands restoration to its recovery process.
  An unresponsive application may display the window only after its message
  queue resumes processing. A submitted request is not a restoration success.
- The recovery process is armed before any window is hidden. Manager death
  triggers restoration. Recovery-process death causes the manager to restore
  and exit. Simultaneous termination requires next-start recovery; it cannot
  provide immediate recovery while both processes are absent.

The module changes visibility, not application lifetime. It never closes a
managed application. Disable the module or use Restore to bring back managed
windows. When recovering from a simultaneous failure, re-enable the module;
startup checks tagged restoration intent before accepting new work.

See [design](design.md), [test data and reproduction](validation.md), and
[research sources](research.md). Physical two-monitor acceptance and maintainer
approval are required before this experiment can be considered ready to merge.
