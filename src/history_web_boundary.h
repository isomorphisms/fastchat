#ifndef FASTCHAT_HISTORY_WEB_BOUNDARY_H
#define FASTCHAT_HISTORY_WEB_BOUNDARY_H

/* Pinned source dependency: ChatGPTExporter native/cgw_web.h.
 * This only selects read-only history operations, never model inference.
 */
#include "cgw_web.h"

cgw_status fc_history_resolve(const cgw_input *input, cgw_http_request *output);

#endif
