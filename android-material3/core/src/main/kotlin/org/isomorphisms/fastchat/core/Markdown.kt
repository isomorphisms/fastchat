package org.isomorphisms.fastchat.core

sealed interface TextBlock {
    data class Paragraph(val text: String) : TextBlock
    data class Heading(val text: String) : TextBlock
    data class Code(val text: String) : TextBlock
}
fun markdownBlocks(text: String): List<TextBlock> {
    val blocks = mutableListOf<TextBlock>()
    val pending = mutableListOf<String>()
    var code = false
    fun flush() {
        if (pending.isNotEmpty()) {
            val body = pending.joinToString("\n")
            blocks += if (code) TextBlock.Code(body) else TextBlock.Paragraph(body)
            pending.clear()
        }
    }
    for (line in text.lines()) {
        when {
            line.startsWith("```") -> { flush(); code = !code }
            !code && line.isBlank() -> flush()
            !code && Regex("^#{1,6} ").containsMatchIn(line) -> {
                flush(); blocks += TextBlock.Heading(line.substringAfter(' '))
            }
            else -> pending += line
        }
    }
    flush()
    return blocks
}
