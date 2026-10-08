package org.isomorphisms.fastchat.core

import java.nio.ByteBuffer
import java.nio.CharBuffer
import java.nio.charset.CodingErrorAction

/** Adapter helper: transport byte chunks are not complete Unicode code points. No HTTP/SSE logic. */
class Utf8Chunks {
    private val decoder = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
        .onUnmappableCharacter(CodingErrorAction.REPORT)
    private var pending = ByteArray(0)
    private var ended = false
    fun accept(bytes: ByteArray): String {
        check(!ended)
        require(bytes.size <= 64 * 1024) { "Adapter must bound its transport input chunks" }
        return decode(bytes, false)
    }
    fun finish(): String {
        check(!ended); ended = true
        return decode(ByteArray(0), true)
    }
    private fun decode(bytes: ByteArray, final: Boolean): String {
        val input = ByteBuffer.wrap(pending + bytes)
        val output = CharBuffer.allocate(input.remaining() + 4)
        val result = decoder.decode(input, output, final)
        if (result.isError) result.throwException()
        check(!result.isOverflow)
        pending = ByteArray(input.remaining()).also(input::get)
        check(pending.size <= 3)
        if (final) {
            val flushed = decoder.flush(output)
            if (flushed.isError) flushed.throwException()
            check(pending.isEmpty() && !flushed.isOverflow)
        }
        output.flip()
        return output.toString()
    }
}
