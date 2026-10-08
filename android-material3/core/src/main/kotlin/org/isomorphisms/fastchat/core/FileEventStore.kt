package org.isomorphisms.fastchat.core

import java.io.*
import java.nio.channels.FileChannel
import java.nio.charset.CodingErrorAction
import java.nio.file.StandardOpenOption
import java.util.zip.CRC32

/** Versioned length/CRC frames are command transactions. A torn final frame is never replayed. */
class FileEventStore(
    directory: File,
    override val conversation: ConversationId = ConversationId(1),
    override val authority: TransportAuthority = TransportAuthority(1),
    private val syncDirectory: (File) -> Unit = { FileChannel.open(it.toPath(), StandardOpenOption.READ).use { channel -> channel.force(true) } },
    private val writeFrame: (RandomAccessFile, ByteArray) -> Unit = { file, bytes -> file.write(bytes) },
    private val syncFile: (RandomAccessFile) -> Unit = { it.fd.sync() },
) : EventStore {
    private val file: RandomAccessFile
    private val lock: java.nio.channels.FileLock
    private val history = mutableListOf<StoredEvent>()
    private var poisoned = false
    override val events: List<StoredEvent> get() = history.toList()
    override var extent = Extent(0, 0, 0, Long.MAX_VALUE)
        private set
    init {
        require(directory.isDirectory) { "Store directory must already exist" }
        val path = File(directory, "conversation-${conversation.value}.fclog")
        val created = !path.exists()
        file = RandomAccessFile(path, "rw")
        try {
            lock = file.channel.tryLock() ?: error("Conversation store already has an owner")
            if (created) { syncFile(file); syncDirectory(directory) }
            replay()
        } catch (failure: Throwable) { file.close(); throw failure }
    }
    private fun replay() {
        var boundary = 0L
        var state = State(conversation, authority)
        while (boundary < file.length()) {
            file.seek(boundary)
            if (file.length() - boundary < HEADER_BYTES) break
            check(file.readInt() == MAGIC) { "Corrupt journal magic at $boundary" }
            val length = file.readInt()
            check(length in 1..MAX_FRAME_BYTES) { "Invalid journal length at $boundary" }
            val checksum = file.readLong()
            if (file.length() - file.filePointer < length) break
            val bytes = ByteArray(length).also(file::readFully)
            check(CRC32().apply { update(bytes) }.value == checksum) { "Corrupt journal checksum at $boundary" }
            val batch = decode(bytes)
            batch.forEach { state = state.apply(it) }
            history.addAll(batch)
            boundary = file.filePointer
        }
        if (boundary != file.length()) { file.setLength(boundary); syncFile(file) }
        extent = Extent(boundary, boundary, boundary, Long.MAX_VALUE)
    }
    override fun append(events: List<Event>): List<StoredEvent> {
        check(!poisoned) { "Store unavailable after failed write; reopen required" }
        require(events.isNotEmpty())
        val batch = events.mapIndexed { index, event -> StoredEvent(history.size.toLong() + index + 1, event) }
        val payload = encode(batch)
        require(payload.size <= MAX_FRAME_BYTES)
        val frame = ByteArrayOutputStream().also { stream ->
            DataOutputStream(stream).use { out ->
                out.writeInt(MAGIC); out.writeInt(payload.size)
                out.writeLong(CRC32().apply { update(payload) }.value); out.write(payload)
            }
        }.toByteArray()
        val start = extent.committed
        try {
            file.seek(start)
            writeFrame(file, frame)
            check(file.filePointer == start + frame.size) { "Short journal write" }
            extent = Extent(start, start, file.filePointer, Long.MAX_VALUE)
            syncFile(file)
            extent = Extent(start, file.filePointer, file.filePointer, Long.MAX_VALUE)
            history.addAll(batch)
            extent = Extent(file.filePointer, file.filePointer, file.filePointer, Long.MAX_VALUE)
            return batch
        } catch (failure: Throwable) {
            poisoned = true
            try {
                file.setLength(start); syncFile(file)
                extent = Extent(start, start, start, Long.MAX_VALUE)
            } catch (rollback: Throwable) { failure.addSuppressed(rollback) }
            throw failure
        }
    }
    fun address() = AppendAddress(conversation, extent.committed)
    override fun close() { try { lock.release() } finally { file.close() } }
    private fun encode(events: List<StoredEvent>): ByteArray = ByteArrayOutputStream().also { stream ->
        DataOutputStream(stream).use { out ->
            out.writeInt(1); out.writeLong(conversation.value); out.writeLong(authority.value); out.writeInt(events.size)
            events.forEach {
                out.writeLong(it.sequence); out.writeInt(it.event.kind.ordinal)
                out.writeLong(it.event.request.value); out.writeLong(it.event.attempt.value)
                val encoded = Charsets.UTF_8.newEncoder().onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT).encode(java.nio.CharBuffer.wrap(it.event.text))
                val text = ByteArray(encoded.remaining()).also(encoded::get)
                require(text.size <= MAX_TEXT_BYTES)
                out.writeInt(text.size); out.write(text)
            }
        }
    }.toByteArray()
    private fun decode(bytes: ByteArray): List<StoredEvent> = DataInputStream(ByteArrayInputStream(bytes)).use { input ->
        check(input.readInt() == 1) { "Unsupported journal version" }
        check(input.readLong() == conversation.value && input.readLong() == authority.value) { "Journal identity mismatch" }
        val count = input.readInt()
        check(count in 1..10000)
        val result = List(count) {
            val sequence = input.readLong()
            val kind = Kind.entries.getOrNull(input.readInt()) ?: error("Invalid journal event kind")
            val request = RequestId(input.readLong()); val attempt = AttemptId(input.readLong())
            val length = input.readInt(); check(length in 0..MAX_TEXT_BYTES && length <= input.available())
            val textBytes = ByteArray(length).also(input::readFully)
            val text = Charsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT)
                .onUnmappableCharacter(CodingErrorAction.REPORT).decode(java.nio.ByteBuffer.wrap(textBytes)).toString()
            StoredEvent(sequence, Event(kind, request, attempt, text))
        }
        check(input.available() == 0) { "Trailing journal data" }
        result
    }
    companion object {
        private const val MAGIC = 0x46434D31
        private const val HEADER_BYTES = 16L
        private const val MAX_FRAME_BYTES = 4 * 1024 * 1024
        private const val MAX_TEXT_BYTES = 512 * 1024
    }
}
