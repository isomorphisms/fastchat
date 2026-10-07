package org.isomorphisms.fastchat.core

import org.junit.Assert.*
import org.junit.Test
import java.nio.file.Files

class CoreTest {
    private fun fixture(test: (Engine) -> Unit) {
        val directory = Files.createTempDirectory("fastchat-core").toFile()
        try { FileEventStore(directory).use { test(Engine(it)) } } finally { directory.deleteRecursively() }
    }
    private fun event(request: ModelRequest, kind: Kind, text: String = "") =
        Observation(request.authority, request.request, request.attempt, kind, text)

    @Test fun interleavedRequestsMatchIdricFixture() = fixture { engine ->
        val first = engine.submit("first"); val second = engine.submit("second")
        listOf(event(first, Kind.Started), event(second, Kind.Started), event(first, Kind.Chunk, "first answer"),
            event(second, Kind.Chunk, "second answer"), event(second, Kind.Completed), event(first, Kind.Completed)).forEach {
            assertTrue(engine.observe(it))
        }
        assertNotEquals(first.request, second.request); assertNotEquals(first.attempt, second.attempt)
        assertTrue(engine.state.requests.all { it.phase == Phase.Completed })
        assertEquals(10, engine.state.cursor.sequence)
    }
    @Test fun independentViewsMatchIdricFixture() = fixture { engine ->
        val request = engine.submit("one shared history")
        listOf(event(request, Kind.Started), event(request, Kind.Chunk, "response"), event(request, Kind.Completed)).forEach(engine::observe)
        val first = View(ViewId(1), engine.state.conversation, Cursor(0))
        val second = View(ViewId(2), engine.state.conversation, Cursor(0))
        val (advanced, visible) = engine.read(first)
        assertEquals(5, visible.size); assertEquals(0, engine.read(advanced).second.size)
        assertEquals(5, engine.read(second).second.size)
        assertTrue(engine.read(first.copy(conversation = ConversationId(2))).second.isEmpty())
    }
    @Test fun firstDurableTerminalWinsBothCancellationRaces() = fixture { engine ->
        val first = engine.submit("race")
        engine.observe(event(first, Kind.Started)); engine.cancel(first.request)
        assertTrue(engine.observe(event(first, Kind.Completed)))
        assertFalse(engine.observe(event(first, Kind.Cancelled)))
        assertEquals(Phase.Completed, engine.state.request(first.request)?.phase)
        assertEquals(5, engine.state.cursor.sequence)
        val second = engine.submit("reverse race")
        engine.observe(event(second, Kind.Started)); engine.cancel(second.request)
        assertTrue(engine.observe(event(second, Kind.Cancelled)))
        assertFalse(engine.observe(event(second, Kind.Completed)))
        assertFalse(engine.observe(event(second, Kind.Chunk, "late")))
        assertEquals(Phase.Cancelled, engine.state.request(second.request)?.phase)
    }
    @Test fun uncertainRetryIsExplicitAndRejectsOldAttempt() = fixture { engine ->
        val old = engine.submit("uncertain")
        listOf(event(old, Kind.Started), event(old, Kind.Chunk, "partial"), event(old, Kind.Uncertain, "unknown")).forEach(engine::observe)
        assertEquals(Phase.Uncertain, engine.state.request(old.request)?.phase)
        assertEquals("", engine.state.messages().last().text)
        val retried = engine.retry(old.request)!!
        assertNotEquals(old.attempt, retried.attempt)
        assertFalse(engine.observe(event(old, Kind.Chunk, "stale")))
        assertFalse(engine.observe(event(old, Kind.Completed)))
        listOf(event(retried, Kind.Started), event(retried, Kind.Chunk, "response"), event(retried, Kind.Completed)).forEach(engine::observe)
        assertEquals(10, engine.state.cursor.sequence)
        assertEquals("response", engine.state.messages().last().text)
        assertNull(engine.retry(retried.request))
    }
    @Test fun cancellationBeforeStartAndAdmissionBoundaries() = fixture { engine ->
        val request = engine.submit("boundary")
        assertFalse(engine.observe(event(request, Kind.Chunk, "before start")))
        assertFalse(engine.observe(event(request, Kind.Started).copy(authority = TransportAuthority(9))))
        assertFalse(engine.observe(event(request, Kind.Started).copy(request = RequestId(9))))
        assertEquals(request.request to request.attempt, engine.cancel(request.request))
        assertNull(engine.cancel(request.request))
        assertTrue(engine.observe(event(request, Kind.Cancelled)))
        assertEquals(Phase.Cancelled, engine.state.request(request.request)?.phase)
    }
    @Test fun responseProjectionCanUseSameStoredStreamingSequence() = fixture { engine ->
        val request = engine.submit("Unicode")
        val events = Fixtures.events(request)
        events.dropLast(1).forEach(engine::observe)
        assertEquals("", engine.state.messages().last().text)
        assertEquals(Fixtures.response, engine.state.messages(ProjectionMode.StoredStreaming).last().text)
        engine.observe(events.last())
        assertEquals(Fixtures.response, engine.state.messages().last().text)
        assertTrue(engine.state.messages().last().text.contains(Fixtures.unicode))
    }
    @Test fun failuresHideIncompleteResponseAndDoNotEnableUncertainRetry() = fixture { engine ->
        val request = engine.submit("/fail")
        Fixtures.events(request).forEach(engine::observe)
        assertEquals(Phase.Failed, engine.state.request(request.request)?.phase)
        assertEquals("", engine.state.messages().last().text)
        assertNull(engine.retry(request.request))
    }
    @Test fun longThreadAndMarkdownRemainExact() = fixture { engine ->
        Fixtures.longThread(engine)
        assertEquals(1000, engine.state.messages().size)
        assertEquals(2500, engine.state.cursor.sequence)
        assertEquals(Fixtures.response, engine.state.messages().last().text)
        val blocks = markdownBlocks(Fixtures.response)
        assertEquals(TextBlock.Heading("Deterministic response"), blocks.first())
        assertEquals(TextBlock.Code("val message = \"Hello\"\nprintln(message)"), blocks.last())
        assertEquals(listOf(TextBlock.Code("α\n")), markdownBlocks("```\nα\n"))
    }
    @Test fun malformedOrTruncatedTransportUnicodeIsNeverReplacedSilently() {
        assertThrows(java.nio.charset.CharacterCodingException::class.java) { Utf8Chunks().accept(byteArrayOf(0xFF.toByte())) }
        val incomplete = Utf8Chunks()
        assertEquals("", incomplete.accept(byteArrayOf(0xE2.toByte(), 0x82.toByte())))
        assertThrows(java.nio.charset.CharacterCodingException::class.java) { incomplete.finish() }
    }
}
