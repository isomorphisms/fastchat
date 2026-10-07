package org.isomorphisms.fastchat.core

import org.junit.Assert.*
import org.junit.Test
import java.io.File
import java.io.IOException
import java.io.RandomAccessFile
import java.nio.file.Files

class StoreTest {
    private fun directory() = Files.createTempDirectory("fastchat-store").toFile()
    @Test fun completedHistoryReplaysExactlyAndIdsDoNotRepeat() {
        val directory = directory()
        try {
            lateinit var previous: State
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                val request = engine.submit(Fixtures.unicode)
                Fixtures.events(request).forEach(engine::observe)
                previous = engine.state
                assertEquals(store.extent.committed, store.extent.durable)
                assertEquals(store.extent.durable, store.extent.written)
                assertEquals(store.address(), AppendAddress(ConversationId(1), store.extent.committed))
            }
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                engine.recoverInterruptedAttempts()
                assertEquals(previous, engine.state)
                val next = engine.submit("next")
                assertEquals(RequestId(2), next.request)
                assertEquals(AttemptId(2), next.attempt)
            }
        } finally { directory.deleteRecursively() }
    }
    @Test fun everyTruncatedFramePrefixRecoversOnlyWholeCommands() {
        val directory = directory()
        try {
            lateinit var first: ByteArray
            lateinit var both: ByteArray
            FileEventStore(directory).use { store ->
                val engine = Engine(store); engine.submit("first")
                first = File(directory, "conversation-1.fclog").readBytes()
                engine.submit("second ${Fixtures.unicode}")
                both = File(directory, "conversation-1.fclog").readBytes()
            }
            for (cut in first.size until both.size) {
                File(directory, "conversation-1.fclog").writeBytes(both.copyOf(cut))
                FileEventStore(directory).use { store ->
                    assertEquals("prefix $cut", 2, store.events.size)
                    assertEquals(first.size.toLong(), store.extent.committed)
                }
            }
            File(directory, "conversation-1.fclog").writeBytes(both)
            FileEventStore(directory).use { assertEquals(4, it.events.size) }
        } finally { directory.deleteRecursively() }
    }
    @Test fun partialWriteNeverPublishesStateAndPoisonsOwner() {
        val directory = directory()
        try {
            FileEventStore(directory, writeFrame = { file, bytes ->
                file.write(bytes, 0, bytes.size / 2); throw IOException("Injected no space left on device")
            }).use { store ->
                val engine = Engine(store)
                assertThrows(IOException::class.java) { engine.submit("not acknowledged") }
                assertTrue(engine.state.requests.isEmpty()); assertTrue(store.events.isEmpty())
                assertThrows(IllegalStateException::class.java) { engine.submit("poisoned") }
            }
            FileEventStore(directory).use { assertTrue(it.events.isEmpty()) }
        } finally { directory.deleteRecursively() }
    }
    @Test fun syncFailureNeverPublishesState() {
        val directory = directory()
        try {
            var calls = 0
            FileEventStore(directory, syncFile = { file ->
                calls++
                if (calls == 2) throw IOException("Injected sync failure")
                file.fd.sync()
            }).use { store ->
                val engine = Engine(store)
                assertThrows(IOException::class.java) { engine.submit("not durable") }
                assertEquals(Cursor(0), engine.state.cursor)
            }
            FileEventStore(directory).use { assertTrue(it.events.isEmpty()) }
        } finally { directory.deleteRecursively() }
    }
    @Test fun corruptionFailsClosedWithoutDeletingHistory() {
        val directory = directory()
        try {
            FileEventStore(directory).use { Engine(it).submit("original") }
            val path = File(directory, "conversation-1.fclog")
            RandomAccessFile(path, "rw").use { it.seek(it.length() - 1); it.writeByte(0xFF) }
            val corrupt = path.readBytes()
            assertThrows(IllegalStateException::class.java) { FileEventStore(directory).close() }
            assertArrayEquals(corrupt, path.readBytes())
        } finally { directory.deleteRecursively() }
    }
    @Test fun onlyOneStoreOwnerMayAppend() {
        val directory = directory()
        try {
            FileEventStore(directory).use {
                assertThrows(java.nio.channels.OverlappingFileLockException::class.java) { FileEventStore(directory).close() }
            }
            FileEventStore(directory).use { Engine(it).submit("lock released") }
        } finally { directory.deleteRecursively() }
    }
    @Test fun unicodeAndAuthorityMismatchFailWithoutChangingHistory() {
        val directory = directory()
        try {
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                assertThrows(java.nio.charset.CharacterCodingException::class.java) { engine.submit("unpaired \uD800") }
                assertTrue(engine.state.requests.isEmpty()); assertEquals(0, store.extent.committed)
                engine.submit(Fixtures.unicode)
            }
            val path = File(directory, "conversation-1.fclog")
            val bytes = path.readBytes()
            assertThrows(IllegalStateException::class.java) { FileEventStore(directory, authority = TransportAuthority(2)).close() }
            assertArrayEquals(bytes, path.readBytes())
            FileEventStore(directory).use { assertEquals(Fixtures.unicode, Engine(it).state.requests.single().prompt) }
        } finally { directory.deleteRecursively() }
    }
    @Test fun realProcessExitLeavesUncertainAttemptWithoutAutoRetry() {
        val directory = directory()
        try {
            val classpath = listOf(ProcessProbe::class.java, State::class.java, Unit::class.java)
                .map { it.protectionDomain.codeSource.location.toURI().path }.distinct().joinToString(File.pathSeparator)
            val java = File(System.getProperty("java.home"), "bin/java").absolutePath
            val writer = ProcessBuilder(java, "-cp", classpath, ProcessProbe::class.java.name,
                "write", directory.absolutePath).redirectErrorStream(true).start()
            val writerOutput = writer.inputStream.bufferedReader().readText()
            assertEquals(writerOutput, 0, writer.waitFor())
            val reader = ProcessBuilder(java, "-cp", classpath, ProcessProbe::class.java.name,
                "read", directory.absolutePath).redirectErrorStream(true).start()
            val result = reader.inputStream.bufferedReader().readText()
            assertEquals(result, 0, reader.waitFor())
            assertEquals("Uncertain:1:5", result.trim())
            FileEventStore(directory).use {
                val engine = Engine(it); val cursor = engine.state.cursor
                engine.recoverInterruptedAttempts(); assertEquals(cursor, engine.state.cursor)
                assertEquals(Phase.Uncertain, engine.state.requests.single().phase)
                assertEquals(AttemptId(2), engine.retry(RequestId(1))!!.attempt)
            }
        } finally { directory.deleteRecursively() }
    }
}
object ProcessProbe {
    @JvmStatic fun main(args: Array<String>) {
        FileEventStore(File(args[1])).use {
            val engine = Engine(it)
            if (args[0] == "write") {
                val request = engine.submit("interrupted")
                engine.observe(Observation(request.authority, request.request, request.attempt, Kind.Started))
                engine.observe(Observation(request.authority, request.request, request.attempt, Kind.Chunk, "partial"))
                Runtime.getRuntime().halt(0) // No shutdown hook or orderly close.
            } else {
                engine.recoverInterruptedAttempts()
                print("${engine.state.requests.single().phase}:${engine.state.requests.single().attempt.value}:${engine.state.cursor.sequence}")
            }
        }
    }
}
