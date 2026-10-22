# FC-UI-01: independently composited message input

This candidate repairs the application boundary described in
[SUN FC-UI-01](https://github.com/isomorphisms/flexible-pipes/issues/222).
It starts from the reading/provider candidate at
`410ce85df1eebc06c01768bbc0084ce6292c65c2`, the exact head of
[isomorphisms/fastchat PR #13, “FC-M1/M3/M5: qualify native reading controls and provider formats”](https://github.com/isomorphisms/fastchat/pull/13).
Main remains `442e8135c018ee003021f1c3f681686c80be9fd0` at inspection.

## Diagnosis and intervention

`NativeActivity` takes ownership of the main window's surface and input queue.
Android's [Window contract](https://developer.android.com/reference/android/view/Window#takeSurface(android.view.SurfaceHolder.Callback2))
specifies that its view hierarchy then stops drawing, and `takeInputQueue`
stops normal input dispatch. Adding EditText to that window's DecorView cannot
provide a normally rendered and dispatched editor. This supports the observed
invisible input; it does not prove the state of the historical C67 View tree.

The editor now lives in an attached application panel with a separate platform
ViewRoot, surface and input dispatch. TYPE_APPLICATION_PANEL binds it to the
activity's window token; it requests no system overlay permission.
FLAG_NOT_TOUCH_MODAL leaves touches outside the editor for the native reader
and Send/Stop/Retry targets. The existing native response canvas is retained.
Public Android APIs construct and manage the panel through JNI; no app DEX,
Java/Kotlin source, Gradle or new renderer is introduced.

Attachment waits for a native window and a valid parent token. The editor
resizes with the content rectangle, detaches on stop/window destruction, and
reattaches on resume. Android saved-instance state retains at most 4096 UTF-16
units of unsubmitted draft across recreation. Canonical submitted history
remains in the existing disk store. Empty input, unavailable editor, excessive
UTF-8 length and invalid Unicode receive distinct diagnostics. Named acceptance
fixtures wait until the input layer attaches before submitting.

## Acceptance boundary

This is source implementation, not physical acceptance. Required next checks:

- Exact-source ICK Android compilation and reproducible signed packaging.
- On A1 and C67 separately: visually observe input; tap/focus/type/paste Unicode
  including supplementary characters; open/close IME; ensure input and Send are
  visible; read/page/copy a long answer; Stop; retry uncertainty; rotate/resume;
  kill/reopen and observe canonical history.
- Verify no separate panel remains after leaving the app and that outside
  touches reach the native controls while the editor is focused.
- Keep A1 ARM32 and C67 ARM64 evidence distinct. No C67 ARM64 build is claimed.

Device, live-provider and registered FP dispatch acceptance are NOT_RUN.
The shared producer's separate Grease qualification failure is still owned by
[CGW-FC-01](https://github.com/isomorphisms/flexible-pipes/issues/153).
No merge, release or predecessor approval is supplied by this candidate.
