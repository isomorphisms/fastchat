#include "history_web_boundary.h"

#include <string.h>

/* The UI is not allowed to reach account/session operations through history
 * browsing. Auth belongs to a separately consented short-lived provider.
 * No methods, origins, headers or URLs are accepted from application text.
 */
cgw_status fc_history_resolve(const cgw_input *input, cgw_http_request *output)
{
    if (output == NULL) return CGW_INVALID_PARAMETERS;
    memset(output, 0, sizeof(*output));
    if (input == NULL) return CGW_INVALID_PARAMETERS;

    switch (input->operation) {
    case CGW_CONVERSATION_PAGE:
    case CGW_PROJECT_PAGE:
    case CGW_PROJECT_CONVERSATION_PAGE:
    case CGW_SHARED_PAGE:
    case CGW_SHARED_DETAIL:
    case CGW_CONVERSATION_BATCH:
    case CGW_CONVERSATION_CURRENT:
    case CGW_CONVERSATION_MESSAGES:
    case CGW_CONVERSATION_DETAIL:
    case CGW_ASSET_OPEN:
        return cgw_resolve(input, output);
    default:
        return CGW_INVALID_OPERATION;
    }
}
