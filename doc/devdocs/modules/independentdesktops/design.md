# Independent Desktops design

## Contract

Selecting a workspace on monitor B must change only window groups assigned to B,
while leaving A's selection and visibility unchanged. This applies symmetrically
to both monitors. The implementation uses public Win32 visibility operations,
with its own workspace membership; it does not switch the Windows native desktop.

## Components

| Component | Responsibility |
| --- | --- |
| `WorkspaceModel.h` | Pure C++ membership, native-desktop/monitor selection, owned-group planning and nonmutating transitions |
| `WindowOperations.*` | Window identity, process creation time, per-HWND incarnation properties, visibility acknowledgement and orphan recovery |
| `Recovery.*` | Anonymous page-file mapping, inherited-handle watchdog, recovery registration and visibility-intent ownership |
| `main.cpp` | COM desktop queries, window enumeration, monitor reconciliation, event dispatch and a nonactivating status indicator |
| `ModuleInterface/` | Runner lifecycle, readiness handshake, five hotkeys, Settings JSON and GPO integration |
| Settings UI/library | Explicit opt-in, localizable explanation and configurable shortcuts |

No third-party runtime dependency is introduced. The code uses the Windows SDK
and existing PowerToys Settings/logging dependencies in the module interface.

## Membership and transitions

The model stores a window's identity, root owner, physical monitor, native
desktop, assigned monitor, workspace and whether the utility hid it. Native
desktop/monitor pairs each have a selected workspace. Roots are adopted before
owned windows so enumeration order cannot split a group.

Planning a switch produces hide/show lists and a proposed selection without
modifying the model. The Windows adapter applies and acknowledges visibility
changes before committing that transition. A failed or stale transition cannot
advance the model selection. On an adapter failure, restoration takes precedence
over continuing workspace operations.

A group spanning monitors is refused if the proposed visibility changes would
touch another monitor. App-hidden windows are never shown merely because their
workspace becomes selected. A visible root dragged between monitors joins the
destination selection. Foreground activation of an assigned group selects that
group's workspace; it does not select workspaces on other monitors.

At most 1,024 windows can be registered. Capacity exhaustion refuses further
workspace operation instead of partially managing the desktop.

## Recovery protocol

1. Create an anonymous page-file-backed journal, ready event and synchronizable
   manager-process handle. Pass exactly those three handles to a second copy of
   the executable using `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`.
2. The recovery process maps the journal, validates its version, then signals
   ready. Until that acknowledgement and a live-process check succeed, hiding
   is prohibited.
3. Register each window using HWND, owning PID, owning thread, process creation
   time and a dedicated incarnation property. Window destruction removes its
   properties; a reused HWND is not accepted as the old registration.
4. Before posting a hide, publish recovery intent with the saved nonactivating
   show command. Wait for the actual visibility state, with a bounded timeout.
5. When showing, keep recovery intent until visibility is acknowledged. A failed
   show is retained for recovery retry rather than discarded.
6. On manager exit or ownership transfer, the watchdog validates each identity,
   restores only armed registrations and removes its own properties. It retries
   unresponsive windows until they become visible or their identities cease to
   match. It does not terminate the application.
7. If both processes disappear, tagged per-window restoration intent survives in
   the still-running application. The next manager, after exclusive acquisition
   of the manager mutex, restores those windows before creating new membership.
   Registration alone does not authorize showing an app-hidden window.

The journal contains window identities and show states, not application content.
It is not stored on disk. Command-line recovery handles are process-local object
references and do not contain authentication tokens. Neither the mapping nor its
handles are published as diagnostic artifacts.

## Integration and gates

The feature is disabled by default and respects its module GPO. New enum members
and wrapper methods are appended to preserve existing values and member order.
Runner loads the module DLL; the DLL launches the executable by absolute path
and waits for ready or process exit with a bound. Both binaries are in the explicit
signing manifest. The installer harvests root build outputs through the existing
generation process. The manager owns an invisible top-level shutdown window;
the installer requests `WM_CLOSE` and waits up to five seconds in its own session.
It never forcibly terminates this executable because the guardian shares its name
and must remain alive until restoration finishes. Across sessions, Runner exit
is the shutdown signal; a hung application may keep recovery alive and leave files
in use. Installer execution and file-in-use handling remain acceptance gates.

The draft contribution requires maintainer agreement on the new module and its
experimental interaction with Task View. Hosted tests establish specific logical
and Win32 recovery contracts; they do not establish physical-monitor behavior,
complete application compatibility, installer execution or UI acceptance.
