# Bounded native conversation timeline

The existing `HistoryTurn` is the semantic input: request/attempt identity,
phase, exact prompt bytes, provider identity and the response arena interval.
The presentation projects the three preceding turns in increasing request
order beside the selected turn. It owns no journal, serializer or transport.
Earlier responses are explicitly bounded previews; a real Button opens the
existing full-response paging controller. TextViews retain exact Unicode text
and framework selection. An empty response remains empty rather than invented.

The platform worker receives a value copy of `Conversation` restricted to its
published journal prefix. Existing `history_turn` and `read_history_window`
use positional reads and only that copy. It borrows file descriptors until a
joined shutdown before the controller closes them. No JNI or renderer work
runs on the worker. The UI never waits for a busy projection mutex: it retries
at a later tick. A monotonically increasing projection revision discards
superseded work before publication; a focus change immediately retires old
Button identities. Each Activity has a separate projection owner.

Desired interface: `project : committed history × focused request → bounded
ordered previous turns`, with errors distinct from empty history. No new
Idriç language feature or fallback is needed: the existing typed history
authority is reused, and C is confined to the established Android platform
worker/JNI adapter. This is not a new conversation model.

The worker has fixed memory: three 1024-byte response previews and the existing
bounded prompt records. Nine context Views are allocated once, then reused.
Streaming beyond an unchanged active page preserves its View and selection.
Unchanged ticks no longer reread the visible response from disk. Full
virtualized history, Markdown, Share, attachment presentation, controller/store
I/O migration, IME and physical performance qualification remain unfinished.

Qualification must compile this actual platform code through the existing
ICK producer, exercise the signed APK on API 29/34 and independently inspect
ordered prior roles/text and navigation. Source inspection is not execution.

The independent API34 replay at ai-ci 2c6c1339 / FastChat 0c5288a2 retained
an accessibility tree containing only the NativeActivity FrameLayout/surface,
while the framework application-panel controls were attached and generation
continued. The earlier screenshot is not the failure-time hierarchy. The repair
puts both the Material panel and provider settings in android.R.id.content with
ordinary FrameLayout parameters. Resize/IME follows that Activity hierarchy;
the native Canvas comparison keeps its existing window path. Controllers,
stores, credentials, semantic actions and checked callback DEX are unchanged.
The same independent composer/timeline assertions remain required on both APIs.
