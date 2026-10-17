#include "history_web_boundary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;

static void require(int ok, const char *why)
{
    ++checks;
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", why);
        exit(1);
    }
}

int main(void)
{
    cgw_http_request out;
    cgw_input input = {0};
    input.operation = CGW_CONVERSATION_PAGE;
    input.arguments.conversation_page.offset = 0;
    input.arguments.conversation_page.limit = 10;
    require(fc_history_resolve(&input, &out) == CGW_OK, "history listing allowed");
    require(out.method == CGW_GET, "history listing GET");
    require(strcmp(out.path,
        "/backend-api/conversations?offset=0&limit=10&order=updated") == 0,
        "same source contract used");
    require(out.requires_authentication == 1, "requires external session");

    input.arguments.conversation_page.limit = 101;
    require(fc_history_resolve(&input, &out) == CGW_INVALID_PARAMETERS,
            "excessive page rejected");
    require(out.path[0] == '\0', "reject without authority");

    input.operation = CGW_ACCOUNT_ARTIFACT;
    input.arguments.artifact.kind = CGW_MEMORIES;
    require(fc_history_resolve(&input, &out) == CGW_INVALID_OPERATION,
            "account memories excluded from thread UI");
    require(out.path[0] == '\0' && out.body[0] == '\0', "account refusal clears data");

    input.operation = CGW_SESSION_PROBE;
    require(fc_history_resolve(&input, &out) == CGW_INVALID_OPERATION,
            "session access excluded from history UI");

    const char *ids[] = {"first", "second"};
    input.operation = CGW_CONVERSATION_BATCH;
    input.arguments.batch.conversation_ids = ids;
    input.arguments.batch.count = 2;
    require(fc_history_resolve(&input, &out) == CGW_OK, "bounded read-only POST allowed");
    require(out.method == CGW_POST, "batch uses POST");
    require(strcmp(out.path, "/backend-api/conversations/batch") == 0,
            "batch path allowlisted");
    require(strcmp(out.body, "{\"conversation_ids\":[\"first\",\"second\"]}") == 0,
            "batch payload bounded");

    input.operation = CGW_ASSET_OPEN;
    input.arguments.asset.file_id = "file-2";
    input.arguments.asset.parent_id = "thread-2";
    input.arguments.asset.scope = CGW_ASSET_CONVERSATION;
    require(fc_history_resolve(&input, &out) == CGW_OK, "thread asset allowed");
    require(strcmp(out.path,
        "/backend-api/files/download/file-2?conversation_id=thread-2&inline=false") == 0,
        "asset path fixed");

    input.operation = (cgw_operation)999;
    require(fc_history_resolve(&input, &out) == CGW_INVALID_OPERATION,
            "arbitrary operation rejected");
    require(out.path[0] == '\0', "rejected unknown clears path");

    printf("PASS %u FastChat native history boundary checks (no network)\n", checks);
    return 0;
}
