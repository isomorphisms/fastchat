package org.isomorphisms.fastchat.material3

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.interaction.DragInteraction
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.coroutines.launch
import org.isomorphisms.fastchat.core.*

@Composable
fun ConversationScreen(controller: ChatController) {
    val state by controller.state.collectAsStateWithLifecycle()
    ConversationContent(state, controller::send, controller::cancel, controller::retry, controller::loadLongThread)
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConversationContent(state: ScreenState, onSend: suspend (String) -> Boolean,
    onCancel: (RequestId) -> Unit, onRetry: (RequestId) -> Unit, onLongThread: () -> Unit) {
    // Preserve the actual editing value (selection and composing region), without filtering IME updates.
    var draft by rememberSaveable(stateSaver = TextFieldValue.Saver) { mutableStateOf(TextFieldValue()) }
    var submitting by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()
    val listState = rememberLazyListState()
    var followLatest by rememberSaveable { mutableStateOf(true) }
    LaunchedEffect(listState) {
        listState.interactionSource.interactions.collect { interaction ->
            if (interaction is DragInteraction.Start) followLatest = false
        }
    }
    // Stable row identities and explicit following keep updates from pulling a reader out of older text.
    LaunchedEffect(state.messages, followLatest) {
        if (followLatest && state.messages.isNotEmpty() && !listState.isScrollInProgress)
            listState.scrollToItem(state.messages.lastIndex)
    }
    Scaffold(topBar = { TopAppBar(title = { Column { Text("FastChat"); Text("Demo provider", style = MaterialTheme.typography.labelMedium) } }) }) { padding ->
        Column(Modifier.fillMaxSize().padding(padding).imePadding()) {
            when {
                state.loading -> Text("Opening history…", Modifier.padding(16.dp))
                state.error != null -> Text(state.error, Modifier.padding(16.dp), color = MaterialTheme.colorScheme.error)
                state.messages.isEmpty() -> Column(Modifier.padding(16.dp)) {
                    Text("Send a message to try the local demo. /fail shows a provider failure; /uncertain shows delivery uncertainty.")
                    TextButton(onClick = onLongThread, enabled = !state.loadingFixture) {
                        Text(if (state.loadingFixture) "Loading sample…" else "Load long sample thread")
                    }
                }
            }
            LazyColumn(state = listState, modifier = Modifier.weight(1f).fillMaxWidth().testTag("messages"),
                contentPadding = PaddingValues(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                items(state.messages, key = { it.key }) { message -> MessageCard(message, onCancel, onRetry) }
            }
            if (!followLatest && state.messages.isNotEmpty()) TextButton(onClick = { followLatest = true }) { Text("Latest messages") }
            Row(Modifier.fillMaxWidth().padding(8.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(value = draft, onValueChange = { draft = it }, label = { Text("Message") },
                    minLines = 1, maxLines = 5, modifier = Modifier.weight(1f).testTag("composer"))
                Button(enabled = !state.loading && state.error == null && !state.loadingFixture && !submitting && draft.text.isNotBlank(),
                    modifier = Modifier.testTag("send"), onClick = {
                        val submitted = draft
                        submitting = true
                        scope.launch {
                            try {
                                if (onSend(submitted.text)) {
                                    if (draft == submitted) draft = TextFieldValue()
                                    followLatest = true
                                }
                            } finally { submitting = false }
                        }
                    }) { Text("Send") }
            }
        }
    }
}

@Composable
private fun MessageCard(message: Message, onCancel: (RequestId) -> Unit, onRetry: (RequestId) -> Unit) {
    val clipboard = LocalClipboardManager.current
    Card(colors = CardDefaults.cardColors(containerColor = if (message.fromUser)
        MaterialTheme.colorScheme.secondaryContainer else MaterialTheme.colorScheme.surfaceContainer)) {
        Column(Modifier.fillMaxWidth().padding(12.dp)) {
            Text(if (message.fromUser) "You" else "Assistant", style = MaterialTheme.typography.labelLarge)
            if (message.text.isNotEmpty()) {
                MarkdownText(message.text)
                TextButton(onClick = { clipboard.setText(AnnotatedString(message.text)) }) { Text("Copy") }
            }
            if (!message.fromUser) {
                val label = when (message.phase) {
                    Phase.Submitted -> "Submitted"
                    Phase.Streaming -> "Generating…"
                    Phase.CancellationPending -> "Stopping…"
                    Phase.Completed -> "Complete"
                    Phase.Cancelled -> "Stopped"
                    Phase.Failed -> "Failed: ${message.detail}"
                    Phase.Uncertain -> "Delivery uncertain: ${message.detail}"
                }
                Text(label, style = MaterialTheme.typography.labelMedium,
                    color = if (message.phase in setOf(Phase.Failed, Phase.Uncertain)) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant)
                if (message.phase == Phase.Submitted || message.phase == Phase.Streaming)
                    TextButton(onClick = { onCancel(message.request) }) { Text("Stop") }
                if (message.phase == Phase.Uncertain) {
                    Text("Retry may duplicate work at the provider.", style = MaterialTheme.typography.bodySmall)
                    TextButton(onClick = { onRetry(message.request) }) { Text("Retry explicitly") }
                }
            }
        }
    }
}

/** Small useful subset: headings, paragraphs, fenced code; other Markdown remains readable verbatim. */
@Composable
private fun MarkdownText(text: String) {
    val blocks = remember(text) { markdownBlocks(text) }
    SelectionContainer {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            blocks.forEach { block ->
                when (block) {
                    is TextBlock.Code -> Surface(color = MaterialTheme.colorScheme.surfaceVariant) {
                        Text(block.text, fontFamily = FontFamily.Monospace,
                            modifier = Modifier.horizontalScroll(rememberScrollState()).padding(8.dp))
                    }
                    is TextBlock.Heading -> Text(block.text, style = MaterialTheme.typography.titleMedium)
                    is TextBlock.Paragraph -> Text(block.text)
                }
            }
        }
    }
}
