package org.isomorphisms.fastchat.core

object Fixtures {
    const val unicode = "λ × μ − ν ÷ 2; Idriç; e\u0301; 中文; العربية; 👩🏽‍🔧"
    val response = "# Deterministic response\n\n$unicode\n\nThis response becomes visible after durable completion.\n\n```kotlin\nval message = \"Hello\"\nprintln(message)\n```\n"
    fun events(request: ModelRequest): List<Observation> {
        fun event(kind: Kind, text: String = "") = Observation(request.authority, request.request, request.attempt, kind, text)
        val terminal = when {
            request.prompt == "/fail" -> event(Kind.Failed, "Deterministic provider failure")
            request.prompt == "/uncertain" -> event(Kind.Uncertain, "Connection lost; delivery unknown")
            else -> event(Kind.Completed)
        }
        val chunks = mutableListOf<String>()
        var offset = 0
        while (offset < response.length) {
            var end = minOf(offset + 32, response.length)
            if (end < response.length && response[end - 1].isHighSurrogate()) end--
            chunks += response.substring(offset, end)
            offset = end
        }
        return listOf(event(Kind.Started)) + chunks.map { event(Kind.Chunk, it) } + terminal
    }
    fun longThread(engine: Engine, count: Int = 500) {
        repeat(count) { number ->
            val request = engine.submit("Fixture ${number + 1}: $unicode")
            engine.observe(Observation(request.authority, request.request, request.attempt, Kind.Started))
            engine.observe(Observation(request.authority, request.request, request.attempt, Kind.Chunk, response))
            engine.observe(Observation(request.authority, request.request, request.attempt, Kind.Completed))
        }
    }
}
