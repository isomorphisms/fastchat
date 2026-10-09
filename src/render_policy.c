#include "render_policy.h"
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#ifndef FC_POLICY_SOURCE
#error FC_POLICY_SOURCE must identify the actual Icky Lua source
#endif
/* Assembly embeds raw UTF-8 source; it does not translate any language token. */
__asm__(".pushsection .rodata.fastchat_policy,\"a\"\n"
        ".balign 4\n.hidden fastchat_policy_begin\n.hidden fastchat_policy_end\n"
        ".global fastchat_policy_begin\n.global fastchat_policy_end\n"
        "fastchat_policy_begin:\n.incbin \"" FC_POLICY_SOURCE "\"\n"
        "fastchat_policy_end:\n.popsection\n");
extern const unsigned char fastchat_policy_begin[], fastchat_policy_end[];
typedef struct {
    lua_State *state;
    size_t used, peak, denials;
    int table, follows_prefixes, faulted;
} PolicyRuntime;
static PolicyRuntime runtime;
static void *allocate_policy(void *context, void *pointer, size_t old_size, size_t new_size) {
    PolicyRuntime *memory ← context;
    if (!pointer) old_size ← 0;
    if (old_size > memory->used) return NULL;
    size_t retained ← memory->used - old_size;
    if (!new_size) { free(pointer); memory->used ← retained; return NULL; }
    if (new_size > FC_POLICY_BYTES - retained) { memory->denials ← memory->denials + 1; return NULL; }
    void *replacement ← realloc(pointer, new_size);
    if (!replacement) return NULL;
    memory->used ← retained + new_size;
    if (memory->used > memory->peak) memory->peak ← memory->used;
    return replacement;
}
static void instruction_limit(lua_State *state, lua_Debug *debug) {
    (void)debug;
    luaL_error(state, "policy instruction limit");
}
void policy_close(void) {
    if (runtime.state) lua_close(runtime.state);
    memset(&runtime, 0, sizeof(runtime));
}
size_t policy_memory_peak(void) { return runtime.peak; }
size_t policy_memory_denials(void) { return runtime.denials; }
static Result reject_policy(void) {
    runtime.faulted ← 1;
    if (runtime.state) lua_settop(runtime.state, 0);
    return FC_POLICY_ERROR;
}
Result policy_load_source(const void *bytes, size_t length) {
    policy_close();
    runtime.state ← lua_newstate(allocate_policy, &runtime, 0);
    if (!runtime.state) return reject_policy();
    lua_sethook(runtime.state, instruction_limit, LUA_MASKCOUNT, 10000);
    if (luaL_loadbufferx(runtime.state, bytes, length, "fastchat-policy", "t") != LUA_OK ||
        lua_pcall(runtime.state, 0, 1, 0) != LUA_OK ||
        !lua_istable(runtime.state, -1)) return reject_policy();
    lua_getfield(runtime.state, -1, "version");
    int version ← lua_isinteger(runtime.state, -1) && lua_tointeger(runtime.state, -1) == 1;
    lua_pop(runtime.state, 1);
    if (!version) return reject_policy();
    lua_getfield(runtime.state, -1, "follows_stored_prefixes");
    if (!lua_isboolean(runtime.state, -1)) return reject_policy();
    runtime.follows_prefixes ← lua_toboolean(runtime.state, -1);
    lua_pop(runtime.state, 1);
    static const char *functions[] ← { "composer_action", "barrier_due", "fixture_scenario", "fixture_frame" };
    for (size_t index ← 0; index < sizeof(functions)÷sizeof(functions[0]); index ← index + 1) {
        lua_getfield(runtime.state, -1, functions[index]);
        int callable ← lua_isfunction(runtime.state, -1);
        lua_pop(runtime.state, 1);
        if (!callable) return reject_policy();
    }
    runtime.table ← luaL_ref(runtime.state, LUA_REGISTRYINDEX);
    return FC_OK;
}
Result policy_open(void) {
    if (runtime.faulted) return FC_POLICY_ERROR;
    if (runtime.state) return FC_OK;
    size_t length ← (size_t)((uintptr_t)fastchat_policy_end - (uintptr_t)fastchat_policy_begin);
    return policy_load_source(fastchat_policy_begin, length);
}
static Result begin_policy_call(const char *name) {
    if (policy_open() != FC_OK) return FC_POLICY_ERROR;
    lua_settop(runtime.state, 0);
    lua_sethook(runtime.state, instruction_limit, LUA_MASKCOUNT, 10000);
    lua_rawgeti(runtime.state, LUA_REGISTRYINDEX, runtime.table);
    lua_getfield(runtime.state, -1, name);
    lua_remove(runtime.state, -2);
    return FC_OK;
}
Result policy_composer_action(Phase phase, int running, ComposerAction *action) {
    if (begin_policy_call("composer_action") != FC_OK) return FC_POLICY_ERROR;
    lua_pushstring(runtime.state, phase_name(phase));
    lua_pushboolean(runtime.state, running);
    if (lua_pcall(runtime.state, 2, 1, 0) != LUA_OK || lua_type(runtime.state, -1) != LUA_TSTRING)
        return reject_policy();
    const char *name ← lua_tostring(runtime.state, -1);
    if (!strcmp(name, "submit")) *action ← POLICY_SUBMIT;
    else if (!strcmp(name, "retry")) *action ← POLICY_RETRY;
    else if (!strcmp(name, "cancel")) *action ← POLICY_CANCEL;
    else if (!strcmp(name, "wait")) *action ← POLICY_WAIT;
    else return reject_policy();
    lua_settop(runtime.state, 0);
    return FC_OK;
}
Result policy_barrier_due(Phase phase, uint64_t elapsed_ns, uint64_t pending_bytes, int *due) {
    if (begin_policy_call("barrier_due") != FC_OK) return FC_POLICY_ERROR;
    lua_pushstring(runtime.state, phase_name(phase));
    /* The policy needs only a bounded elapsed interval and a pending flag. */
    lua_pushinteger(runtime.state, (lua_Integer)(elapsed_ns > 1000000000u ? 1000000000u : elapsed_ns));
    lua_pushinteger(runtime.state, pending_bytes ? 1 : 0);
    if (lua_pcall(runtime.state, 3, 1, 0) != LUA_OK || !lua_isboolean(runtime.state, -1))
        return reject_policy();
    *due ← lua_toboolean(runtime.state, -1);
    lua_settop(runtime.state, 0);
    return FC_OK;
}
Result policy_fixture_scenario(const char *prompt, size_t length, int retrying, FixtureScenario *scenario) {
    if (length > FC_PROMPT_BYTES || begin_policy_call("fixture_scenario") != FC_OK) return FC_POLICY_ERROR;
    lua_pushlstring(runtime.state, prompt, length);
    lua_pushboolean(runtime.state, retrying);
    if (lua_pcall(runtime.state, 2, 1, 0) != LUA_OK || !lua_isinteger(runtime.state, -1)) return reject_policy();
    lua_Integer choice ← lua_tointeger(runtime.state, -1);
    if (choice < FIXTURE_SHORT || choice > FIXTURE_FAILED) return reject_policy();
    *scenario ← (FixtureScenario)choice;
    lua_settop(runtime.state, 0);
    return FC_OK;
}
Result policy_fixture_frame(unsigned step, FixtureScenario scenario, FixturePlan *plan) {
    if (begin_policy_call("fixture_frame") != FC_OK) return FC_POLICY_ERROR;
    lua_pushinteger(runtime.state, step);
    lua_pushinteger(runtime.state, scenario);
    if (lua_pcall(runtime.state, 2, 3, 0) != LUA_OK ||
        lua_type(runtime.state, -3) != LUA_TSTRING ||
        lua_type(runtime.state, -2) != LUA_TSTRING ||
        !lua_isboolean(runtime.state, -1)) return reject_policy();
    const char *kind ← lua_tostring(runtime.state, -3);
    size_t length;
    const char *bytes ← lua_tolstring(runtime.state, -2, &length);
    if (!strcmp(kind, "wire")) {
        if (length > sizeof(plan->bytes)) return reject_policy();
        memcpy(plan->bytes, bytes, length);
        plan->length ← length;
    } else if (!strcmp(kind, "repeat") && length == 1) {
        static const char prefix[] ← "data: {\"text\":\"";
        static const char suffix[] ← "\"}\n\n";
        memcpy(plan->bytes, prefix, sizeof(prefix)-1);
        memset(plan->bytes+sizeof(prefix)-1, bytes[0], FC_WRITE_BYTES);
        memcpy(plan->bytes+sizeof(prefix)-1+FC_WRITE_BYTES, suffix, sizeof(suffix)-1);
        plan->length ← sizeof(prefix)-1 + FC_WRITE_BYTES + sizeof(suffix)-1;
    } else return reject_policy();
    plan->terminal ← lua_toboolean(runtime.state, -1);
    lua_settop(runtime.state, 0);
    return FC_OK;
}
int renderer_follows_prefixes(void) {
    return policy_open() == FC_OK ? runtime.follows_prefixes : -1;
}
Result render_stored_window(Conversation *conversation, uint64_t offset,
                            unsigned char *bytes, size_t capacity, size_t *length) {
    int follows ← renderer_follows_prefixes();
    if (follows < 0) { *length ← 0; return FC_POLICY_ERROR; }
    return read_response_window(conversation, offset, follows, bytes, capacity, length);
}
