package org.isomorphisms.fastchat.core

@JvmInline value class ConversationId(val value: Long)
@JvmInline value class ViewId(val value: Long)
@JvmInline value class RequestId(val value: Long)
@JvmInline value class AttemptId(val value: Long)
@JvmInline value class TransportAuthority(val value: Long)
@JvmInline value class Cursor(val sequence: Long)
data class View(val id: ViewId, val conversation: ConversationId, val cursor: Cursor)
data class AppendAddress(val stream: ConversationId, val offset: Long)
data class Extent(val committed: Long, val durable: Long, val written: Long, val capacity: Long) {
    init { require(0 <= committed && committed <= durable && durable <= written && written <= capacity) }
}

enum class Phase { Submitted, Streaming, CancellationPending, Completed, Cancelled, Failed, Uncertain }
enum class Kind { UserSubmitted, AttemptSubmitted, Started, Chunk, Tool, CancellationRequested, Completed,
    Cancelled, Failed, Uncertain, RetryRequested }
data class Event(val kind: Kind, val request: RequestId, val attempt: AttemptId, val text: String = "")
data class StoredEvent(val sequence: Long, val event: Event)
data class ModelRequest(val authority: TransportAuthority, val conversation: ConversationId,
    val request: RequestId, val attempt: AttemptId, val prompt: String)
data class Observation(val authority: TransportAuthority, val request: RequestId, val attempt: AttemptId,
    val kind: Kind, val text: String = "")
data class Request(val id: RequestId, val attempt: AttemptId, val prompt: String,
    val phase: Phase = Phase.Submitted, val chunks: List<String> = emptyList(), val detail: String = "") {
    val active: Boolean get() = phase in setOf(Phase.Submitted, Phase.Streaming, Phase.CancellationPending)
}
data class State(val conversation: ConversationId, val authority: TransportAuthority,
    val requests: List<Request> = emptyList(), val cursor: Cursor = Cursor(0), val nextAttempt: Long = 1) {
    fun request(id: RequestId) = requests.firstOrNull { it.id == id }
}

/** Canonical replay is strict. Transport admission happens before the journal boundary. */
fun State.apply(stored: StoredEvent): State {
    require(stored.sequence == cursor.sequence + 1) { "Non-contiguous event cursor" }
    val event = stored.event
    require(event.request.value > 0 && event.attempt.value > 0)
    val previous = request(event.request)
    val updated = when (event.kind) {
        Kind.UserSubmitted -> {
            require(previous == null) { "Duplicate request" }
            Request(event.request, event.attempt, event.text)
        }
        else -> {
            requireNotNull(previous) { "Unknown request in journal" }
            if (event.kind != Kind.AttemptSubmitted) require(previous.attempt == event.attempt) { "Stale journal attempt" }
            when (event.kind) {
                Kind.AttemptSubmitted -> {
                    require(previous.phase == Phase.Submitted || previous.phase == Phase.Uncertain)
                    if (previous.phase == Phase.Uncertain) require(event.attempt.value >= nextAttempt)
                    else require(event.attempt == previous.attempt)
                    previous.copy(attempt = event.attempt, phase = Phase.Submitted, chunks = emptyList(), detail = "")
                }
                Kind.RetryRequested -> { require(previous.phase == Phase.Uncertain); previous }
                Kind.Started -> {
                    require(previous.phase == Phase.Submitted || previous.phase == Phase.CancellationPending)
                    previous.copy(phase = if (previous.phase == Phase.Submitted) Phase.Streaming else previous.phase)
                }
                Kind.Chunk, Kind.Tool -> {
                    require(previous.phase == Phase.Streaming || previous.phase == Phase.CancellationPending)
                    if (event.kind == Kind.Chunk) previous.copy(chunks = previous.chunks + event.text) else previous
                }
                Kind.CancellationRequested -> { require(previous.active); previous.copy(phase = Phase.CancellationPending) }
                Kind.Completed, Kind.Cancelled -> {
                    require(previous.phase == Phase.Streaming || previous.phase == Phase.CancellationPending)
                    previous.copy(phase = if (event.kind == Kind.Completed) Phase.Completed else Phase.Cancelled)
                }
                Kind.Failed, Kind.Uncertain -> {
                    require(previous.active)
                    previous.copy(phase = if (event.kind == Kind.Failed) Phase.Failed else Phase.Uncertain, detail = event.text)
                }
                Kind.UserSubmitted -> error("Unreachable")
            }
        }
    }
    return copy(requests = if (previous == null) requests + updated else requests.map { if (it.id == updated.id) updated else it },
        cursor = Cursor(stored.sequence), nextAttempt = maxOf(nextAttempt, event.attempt.value + 1))
}

enum class ProjectionMode { CompletedResponses, StoredStreaming }
data class Message(val key: String, val request: RequestId, val fromUser: Boolean, val text: String,
    val phase: Phase, val detail: String = "")
/** Both modes consume the same durable state; selecting streaming never bypasses the store. */
fun State.messages(mode: ProjectionMode = ProjectionMode.CompletedResponses): List<Message> = requests.flatMap { request ->
    val visible = request.phase == Phase.Completed || mode == ProjectionMode.StoredStreaming
    listOf(Message("${request.id.value}:user", request.id, true, request.prompt, request.phase),
        Message("${request.id.value}:assistant", request.id, false,
            if (visible) request.chunks.joinToString("") else "", request.phase, request.detail))
}
