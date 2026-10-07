package org.isomorphisms.fastchat.material3

import android.app.Application
import android.os.Bundle
import android.system.Os
import android.system.OsConstants
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.lightColorScheme
import kotlinx.coroutines.*
import org.isomorphisms.fastchat.core.FileEventStore

class ChatApplication : Application() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    val controller by lazy {
        ChatController(scope, {
            FileEventStore(filesDir, syncDirectory = { directory ->
                val descriptor = Os.open(directory.absolutePath, OsConstants.O_RDONLY, 0)
                try { Os.fsync(descriptor) } finally { Os.close(descriptor) }
            })
        })
    }
}
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val controller = (application as ChatApplication).controller
        setContent { MaterialTheme(colorScheme = lightColorScheme()) { ConversationScreen(controller) } }
    }
}
