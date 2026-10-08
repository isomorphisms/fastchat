package org.isomorphisms.fastchat.material3

import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flow
import org.isomorphisms.fastchat.core.*

/** Real adapters implement this boundary. No HTTP, credentials, or socket ownership in the screen. */
interface Transport {
    fun events(request: ModelRequest): Flow<Observation>
    suspend fun cancel(request: RequestId, attempt: AttemptId)
}
class DeterministicTransport : Transport {
    override fun events(request: ModelRequest) = flow {
        for (event in Fixtures.events(request)) { delay(80); emit(event) }
    }
    override suspend fun cancel(request: RequestId, attempt: AttemptId) = Unit
}
