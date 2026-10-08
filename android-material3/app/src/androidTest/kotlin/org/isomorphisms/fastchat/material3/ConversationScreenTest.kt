package org.isomorphisms.fastchat.material3

import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.test.*
import androidx.compose.ui.test.junit4.StateRestorationTester
import androidx.compose.ui.test.junit4.createComposeRule
import org.isomorphisms.fastchat.core.*
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test

/** Instrumented checks are compiled on the host; their execution needs an Android runtime receipt. */
class ConversationScreenTest {
    @get:Rule val compose = createComposeRule()
    @Test fun unicodeInputSurvivesStateRestorationAndSendsExactText() {
        var sent = ""
        val restoration = StateRestorationTester(compose)
        restoration.setContent {
            MaterialTheme {
                ConversationContent(ScreenState(loading = false), { sent = it; true }, {}, {}, {})
            }
        }
        compose.onNodeWithTag("composer").performTextInput(Fixtures.unicode)
        restoration.emulateSavedInstanceStateRestore()
        compose.onNodeWithTag("composer").assertTextContains(Fixtures.unicode)
        compose.onNodeWithTag("send").performClick()
        compose.runOnIdle { assertEquals(Fixtures.unicode, sent) }
        compose.onNodeWithTag("composer").assert(SemanticsMatcher.expectValue(SemanticsProperties.EditableText, AnnotatedString("")))
    }
    @Test fun failedDurableSendKeepsDraft() {
        compose.setContent { MaterialTheme { ConversationContent(ScreenState(loading = false), { false }, {}, {}, {}) } }
        compose.onNodeWithTag("composer").performTextInput("keep this draft")
        compose.onNodeWithTag("send").performClick()
        compose.onNodeWithTag("composer").assertTextContains("keep this draft")
    }
    @Test fun completedOnlyStateUpdatesAndUncertainRetryRemainVisible() {
        val request = RequestId(1)
        val state = mutableStateOf(ScreenState(loading = false, messages = listOf(
            Message("1:assistant", request, false, "", Phase.Streaming))))
        var cancelled: RequestId? = null; var retried: RequestId? = null
        compose.setContent { MaterialTheme { ConversationContent(state.value, { true }, { cancelled = it }, { retried = it }, {}) } }
        compose.onNodeWithText("Generating…").assertIsDisplayed()
        compose.onNodeWithText("Stop").performClick()
        compose.runOnIdle {
            assertEquals(request, cancelled)
            state.value = ScreenState(loading = false, messages = listOf(
                Message("1:assistant", request, false, "", Phase.Uncertain, "unknown")))
        }
        compose.onNodeWithText("Retry explicitly").performClick()
        compose.runOnIdle {
            assertEquals(request, retried)
            state.value = ScreenState(loading = false, messages = listOf(
                Message("1:assistant", request, false, Fixtures.response, Phase.Completed)))
        }
        compose.onNodeWithText("Deterministic response").assertIsDisplayed()
        compose.onNodeWithText("Copy").assertExists()
        compose.onNodeWithText("val message = \"Hello\"\nprintln(message)").assertExists()
    }
    @Test fun longThreadKeepsOlderReadingPositionDuringUpdates() {
        val messages = (1..500).map { Message("$it:user", RequestId(it.toLong()), true, "Message $it", Phase.Completed) }
        val state = mutableStateOf(ScreenState(loading = false, messages = messages))
        compose.setContent { MaterialTheme { ConversationContent(state.value, { true }, {}, {}, {}) } }
        compose.onNodeWithTag("messages").performTouchInput { swipeDown() }
        compose.onNodeWithText("Latest messages").assertExists()
        compose.runOnIdle { state.value = state.value.copy(messages = messages + Message("501:assistant", RequestId(501), false, "", Phase.Streaming)) }
        compose.onNodeWithText("Latest messages").assertExists()
        compose.onNodeWithText("Latest messages").performClick()
        compose.onNodeWithText("Generating…").assertIsDisplayed()
    }
}
