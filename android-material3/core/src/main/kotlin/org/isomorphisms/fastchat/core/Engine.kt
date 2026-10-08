package org.isomorphisms.fastchat.core

interface EventStore : AutoCloseable {
    val conversation: ConversationId
    val authority: TransportAuthority
    val events: List<StoredEvent>
    val extent: Extent
    fun append(events: List<Event>): List<StoredEvent>
}

/** Store and transport lifetime are independent of any renderer or View. Single serialized owner. */
class Engine(private val store: EventStore) {
    var state: State = store.events.fold(State(store.conversation, store.authority)) { state, event -> state.apply(event) }
        private set

    private fun commit(events: List<Event>) {
        // Validate the entire command before writing. Publish only after append's durable acknowledgement.
        val expected = events.mapIndexed { index, event -> StoredEvent(state.cursor.sequence + index + 1, event) }
        val candidate = expected.fold(state) { value, event -> value.apply(event) }
        check(store.append(events) == expected)
        state = candidate
    }
    fun recoverInterruptedAttempts() {
        val interrupted = state.requests.filter { it.active }.map {
            Event(Kind.Uncertain, it.id, it.attempt, "Process ended before a durable terminal event; delivery unknown")
        }
        if (interrupted.isNotEmpty()) commit(interrupted)
    }
    fun submit(prompt: String): ModelRequest {
        require(prompt.isNotBlank())
        val request = RequestId((state.requests.maxOfOrNull { it.id.value } ?: 0) + 1)
        val attempt = AttemptId(state.nextAttempt)
        commit(listOf(Event(Kind.UserSubmitted, request, attempt, prompt), Event(Kind.AttemptSubmitted, request, attempt)))
        return ModelRequest(state.authority, state.conversation, request, attempt, prompt)
    }
    fun retry(request: RequestId): ModelRequest? {
        val previous = state.request(request) ?: return null
        if (previous.phase != Phase.Uncertain) return null
        val attempt = AttemptId(state.nextAttempt)
        commit(listOf(Event(Kind.RetryRequested, request, previous.attempt), Event(Kind.AttemptSubmitted, request, attempt)))
        return ModelRequest(state.authority, state.conversation, request, attempt, previous.prompt)
    }
    fun cancel(request: RequestId): Pair<RequestId, AttemptId>? {
        val previous = state.request(request) ?: return null
        if (!previous.active || previous.phase == Phase.CancellationPending) return null
        commit(listOf(Event(Kind.CancellationRequested, request, previous.attempt)))
        return request to previous.attempt
    }
    fun observe(observation: Observation): Boolean {
        if (observation.authority != state.authority) return false
        val previous = state.request(observation.request) ?: return false
        if (previous.attempt != observation.attempt) return false
        val allowed = when (observation.kind) {
            Kind.Started -> previous.phase == Phase.Submitted || previous.phase == Phase.CancellationPending
            Kind.Chunk, Kind.Tool, Kind.Completed, Kind.Cancelled -> previous.phase == Phase.Streaming || previous.phase == Phase.CancellationPending
            Kind.Failed, Kind.Uncertain -> previous.active
            else -> false
        }
        if (!allowed) return false
        commit(listOf(Event(observation.kind, observation.request, observation.attempt, observation.text)))
        return true
    }
    fun read(view: View): Pair<View, List<StoredEvent>> {
        if (view.conversation != state.conversation) return view to emptyList()
        val visible = store.events.filter { it.sequence > view.cursor.sequence }
        return view.copy(cursor = visible.lastOrNull()?.let { Cursor(it.sequence) } ?: view.cursor) to visible
    }
}
