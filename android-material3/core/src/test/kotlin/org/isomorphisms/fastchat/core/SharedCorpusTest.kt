package org.isomorphisms.fastchat.core

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.Parameterized
import java.nio.file.Files

/** Byte-for-byte FC-D1/FC-S1 corpus, including splits inside both 3- and 4-byte code points. */
@RunWith(Parameterized::class)
class SharedCorpusTest(private val name: String, private val bytes: ByteArray, private val sizes: List<Int>) {
    @Test fun diskFirstAndStoredStreamingAgreeAfterDurableCompletionAndReplay() {
        val directory = Files.createTempDirectory("m3-shared-corpus").toFile()
        try {
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                val request = engine.submit(name)
                fun observe(kind: Kind, text: String = "") = engine.observe(
                    Observation(request.authority, request.request, request.attempt, kind, text))
                observe(Kind.Started)
                val decoder = Utf8Chunks()
                var offset = 0
                for (size in sizes) {
                    val decoded = decoder.accept(bytes.copyOfRange(offset, offset + size))
                    offset += size
                    if (decoded.isNotEmpty()) observe(Kind.Chunk, decoded)
                    assertEquals("", engine.state.messages().last().text)
                }
                assertEquals(bytes.size, offset)
                val tail = decoder.finish()
                if (tail.isNotEmpty()) observe(Kind.Chunk, tail)
                assertArrayEquals(bytes, engine.state.messages(ProjectionMode.StoredStreaming).last().text.toByteArray(Charsets.UTF_8))
                observe(Kind.Completed)
                assertArrayEquals(bytes, engine.state.messages().last().text.toByteArray(Charsets.UTF_8))
            }
            FileEventStore(directory).use { store ->
                val engine = Engine(store)
                engine.recoverInterruptedAttempts()
                assertEquals(Phase.Completed, engine.state.requests.single().phase)
                assertArrayEquals(bytes, engine.state.messages().last().text.toByteArray(Charsets.UTF_8))
            }
        } finally { directory.deleteRecursively() }
    }
    companion object {
        @JvmStatic @Parameterized.Parameters(name = "{0}") fun cases(): List<Array<Any>> =
            SharedCorpusTest::class.java.getResourceAsStream("/comparison-corpus.tsv")!!.bufferedReader().useLines { lines ->
                lines.drop(1).map { line ->
                    val fields = line.split('\t')
                    val bytes = if (fields[1] == "-") ByteArray(0) else fields[1].chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                    arrayOf<Any>(fields[0], bytes, fields[2].split(',').map(String::toInt))
                }.toList()
            }
    }
}
