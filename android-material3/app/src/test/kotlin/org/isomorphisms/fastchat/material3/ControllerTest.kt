package org.isomorphisms.fastchat.material3

import kotlinx.coroutines.*
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.test.*
import org.isomorphisms.fastchat.core.*
import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files
import java.io.IOException

@OptIn(ExperimentalCoroutinesApi::class)
class ControllerTest {
    @Test fun acquisitionContinuesWithoutRendererAndReplaysCompletion() = runTest {
        val directory = Files.createTempDirectory("m3-controller").toFile()
        val owner = CoroutineScope(SupervisorJob() + StandardTestDispatcher(testScheduler))
        try {
            val controller = ChatController(owner, { FileEventStore(directory) })
            runCurrent()
            val result = async { controller.send(Fixtures.unicode) }
            runCurrent(); assertTrue(result.await())
            assertEquals("", controller.state.value.messages.last().text)
            // No state collector or Activity exists while the provider completes.
            advanceUntilIdle()
            assertEquals(Fixtures.response, controller.state.value.messages.last().text)
            owner.cancel(); runCurrent()
            FileEventStore(directory).use { assertEquals(Phase.Completed, Engine(it).state.requests.single().phase) }
        } finally { owner.cancel(); directory.deleteRecursively() }
    }
    @Test fun failedStorePreventsTransportAndPreservesNoCanonicalSubmission() = runTest {
        val directory = Files.createTempDirectory("m3-failed-store").toFile()
        val owner = CoroutineScope(SupervisorJob() + StandardTestDispatcher(testScheduler))
        var calls = 0
        val transport = object : Transport {
            override fun events(request: ModelRequest) = flow<Observation> { calls++; error("Must not start") }
            override suspend fun cancel(request: RequestId, attempt: AttemptId) = Unit
        }
        try {
            val controller = ChatController(owner, {
                FileEventStore(directory, writeFrame = { _, _ -> throw IOException("Injected write failure") })
            }, transport)
            runCurrent()
            val result = async { controller.send("keep draft") }
            runCurrent(); assertFalse(result.await())
            assertEquals(0, calls); assertNotNull(controller.state.value.error)
            assertTrue(controller.state.value.messages.isEmpty())
            assertFalse(controller.send("store closed"))
        } finally { owner.cancel(); runCurrent(); directory.deleteRecursively() }
    }
    @Test fun requestScopedStopAndIncompleteTransportBecomeDurableStates() = runTest {
        val directory = Files.createTempDirectory("m3-stop").toFile()
        val owner = CoroutineScope(SupervisorJob() + StandardTestDispatcher(testScheduler))
        val transport = object : Transport {
            override fun events(request: ModelRequest) = flow {
                emit(Observation(request.authority, request.request, request.attempt, Kind.Started))
                emit(Observation(request.authority, request.request, request.attempt, Kind.Chunk, "partial"))
                if (request.prompt == "stop") awaitCancellation() // The other request simply loses transport.
            }
            override suspend fun cancel(request: RequestId, attempt: AttemptId) = Unit
        }
        try {
            val controller = ChatController(owner, { FileEventStore(directory) }, transport)
            runCurrent()
            val first = async { controller.send("stop") }; runCurrent(); assertTrue(first.await())
            val second = async { controller.send("lost") }; runCurrent(); assertTrue(second.await())
            controller.cancel(RequestId(1)); runCurrent()
            assertEquals(Phase.Cancelled, controller.state.value.messages.first { it.key == "1:assistant" }.phase)
            assertEquals(Phase.Uncertain, controller.state.value.messages.first { it.key == "2:assistant" }.phase)
            assertTrue(controller.state.value.messages.filter { !it.fromUser }.all { it.text.isEmpty() })
            owner.cancel(); runCurrent()
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                assertEquals(listOf(Phase.Cancelled, Phase.Uncertain), engine.state.requests.map { it.phase })
            }
        } finally { owner.cancel(); directory.deleteRecursively() }
    }
}
