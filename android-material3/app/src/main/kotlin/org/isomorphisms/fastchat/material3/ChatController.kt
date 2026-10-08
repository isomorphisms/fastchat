package org.isomorphisms.fastchat.material3

import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.isomorphisms.fastchat.core.*

data class ScreenState(val loading: Boolean = true, val messages: List<Message> = emptyList(),
    val error: String? = null, val loadingFixture: Boolean = false)

/** Application-scoped owner. Destroying a screen does not cancel acquisition or release the store. */
class ChatController(private val scope: CoroutineScope, private val storeFactory: () -> EventStore,
    private val transport: Transport = DeterministicTransport()) {
    private sealed interface Command {
        data class Send(val text: String, val result: CompletableDeferred<Boolean>) : Command
        data class Cancel(val request: RequestId) : Command
        data class Retry(val request: RequestId) : Command
        data class Receive(val observation: Observation) : Command
        data object LongThread : Command
    }
    private val commands = Channel<Command>(64)
    private val mutableState = MutableStateFlow(ScreenState())
    val state = mutableState.asStateFlow()
    private val attempts = mutableMapOf<AttemptId, Job>()
    init {
        scope.launch {
            var store: EventStore? = null
            try {
                store = storeFactory()
                val engine = Engine(store)
                engine.recoverInterruptedAttempts()
                fun publish() { mutableState.value = ScreenState(loading = false, messages = engine.state.messages()) }
                fun start(request: ModelRequest) {
                    attempts[request.attempt] = scope.launch {
                        try {
                            transport.events(request).collect { commands.send(Command.Receive(it)) }
                            // A stream closing without a terminal observation is uncertain, never complete.
                            commands.send(Command.Receive(Observation(request.authority, request.request, request.attempt,
                                Kind.Uncertain, "Transport ended without a completion acknowledgement")))
                        } catch (cancelled: CancellationException) { throw cancelled }
                        catch (failure: Exception) {
                            commands.send(Command.Receive(Observation(request.authority, request.request, request.attempt,
                                Kind.Uncertain, failure.message ?: "Transport failed; delivery unknown")))
                        }
                    }
                }
                publish()
                for (command in commands) {
                    try {
                        when (command) {
                            is Command.Send -> {
                                val outbound = engine.submit(command.text)
                                publish()
                                command.result.complete(true)
                                start(outbound)
                            }
                            is Command.Cancel -> engine.cancel(command.request)?.let { (request, attempt) ->
                                publish()
                                attempts.remove(attempt)?.cancel()
                                scope.launch {
                                    val terminal = try { transport.cancel(request, attempt); Kind.Cancelled }
                                        catch (failure: Exception) { Kind.Uncertain }
                                    commands.send(Command.Receive(Observation(engine.state.authority, request, attempt,
                                        terminal, if (terminal == Kind.Uncertain) "Cancellation delivery unknown" else "")))
                                }
                            }
                            is Command.Retry -> engine.retry(command.request)?.let { publish(); start(it) }
                            is Command.Receive -> {
                                if (engine.observe(command.observation)) {
                                    publish()
                                    if (engine.state.request(command.observation.request)?.active == false)
                                        attempts.remove(command.observation.attempt)?.cancel()
                                }
                            }
                            Command.LongThread -> if (engine.state.requests.isEmpty()) {
                                mutableState.value = mutableState.value.copy(loadingFixture = true)
                                Fixtures.longThread(engine)
                                publish()
                            }
                        }
                    } catch (failure: Exception) {
                        if (command is Command.Send) command.result.complete(false)
                        throw failure // Fail closed: transport cannot continue after failed persistence.
                    }
                }
            } catch (cancelled: CancellationException) { throw cancelled }
            catch (failure: Exception) {
                mutableState.value = mutableState.value.copy(loading = false, loadingFixture = false,
                    error = "Local history unavailable: ${failure.message ?: failure.javaClass.simpleName}")
            } finally {
                attempts.values.forEach { it.cancel() }
                commands.close()
                for (pending in commands) if (pending is Command.Send) pending.result.complete(false)
                store?.close()
            }
        }
    }
    suspend fun send(text: String): Boolean {
        val result = CompletableDeferred<Boolean>()
        if (!commands.trySend(Command.Send(text, result)).isSuccess) return false
        return result.await()
    }
    fun cancel(request: RequestId) { commands.trySend(Command.Cancel(request)) }
    fun retry(request: RequestId) { commands.trySend(Command.Retry(request)) }
    fun loadLongThread() { commands.trySend(Command.LongThread) }
}
