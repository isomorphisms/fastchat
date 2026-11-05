/* The product surface uses real framework Views in its own ViewRoot. Android's
 * NativeActivity surface remains an explicitly selected Canvas comparator. */
static Presentation *material_owner;
static void material_destroy(Presentation *p) {
    if (material_owner == p) material_owner ← NULL;
    timeline_stop(&p->timeline);
    JNIEnv *e ← p->activity->env;
    jobject references[] ← {p->native_panel, p->native_listener, p->native_status,
        p->native_prompt, p->native_response, p->native_range, p->native_send, p->native_scroll};
    for (unsigned i ← 0; i < sizeof(references)/sizeof(references[0]); i++)
        if (references[i]) (*e)->DeleteGlobalRef(e, references[i]);
    for (unsigned i ← 0; i < FC_PAGE_BUTTONS; i++)
        if (p->native_page_buttons[i]) (*e)->DeleteGlobalRef(e, p->native_page_buttons[i]);
    for (unsigned i ← 0; i < 3; i++)
        if (p->native_history_buttons[i]) (*e)->DeleteGlobalRef(e, p->native_history_buttons[i]);
    for (unsigned i ← 0; i < FC_CONTEXT_TURNS; i++)
        for (unsigned role ← 0; role < 3; role++)
            if (p->native_context[i][role]) (*e)->DeleteGlobalRef(e, p->native_context[i][role]);
}

static jstring material_utf8(JNIEnv *e, const unsigned char *bytes, size_t length) {
    jbyteArray input ← (*e)->NewByteArray(e, (jsize)length);
    (*e)->SetByteArrayRegion(e, input, 0, (jsize)length, (const jbyte *)bytes);
    jclass type ← (*e)->FindClass(e, "java/lang/String");
    jstring encoding ← (*e)->NewStringUTF(e, "UTF-8");
    jstring result ← (*e)->NewObject(e, type,
        (*e)->GetMethodID(e, type, "<init>", "([BLjava/lang/String;)V"), input, encoding);
    (*e)->DeleteLocalRef(e, input); (*e)->DeleteLocalRef(e, type); (*e)->DeleteLocalRef(e, encoding);
    return result;
}
static void material_label(JNIEnv *e, jobject view, const char *label) {
    jstring text ← material_utf8(e, (const unsigned char *)label, strlen(label));
    (*e)->CallVoidMethod(e, view, method(e, view, "setText", "(Ljava/lang/CharSequence;)V"), text);
    (*e)->DeleteLocalRef(e, text);
}
static void timeline_present(Presentation *p, JNIEnv *e, uint64_t focus) {
    TimelineProjection *projection ← &p->timeline;
    if (!projection->started || pthread_mutex_trylock(&projection->lock) != 0) return;
    if (p->native_context_focus != focus) {
        projection->source ← p->conversation;
        projection->focus ← focus; projection->requested++; projection->pending ← 1; projection->ready ← 0;
        p->native_context_focus ← focus;
        pthread_cond_signal(&projection->changed);
        pthread_mutex_unlock(&projection->lock);
        for (int i ← 0; i < FC_CONTEXT_TURNS; i++) {
            p->native_context_requests[i] ← 0;
            for (int role ← 0; role < 3; role++)
                (*e)->CallVoidMethod(e, p->native_context[i][role], method(e, p->native_context[i][role], "setVisibility", "(I)V"), 8);
        }
        return;
    }
    if (!projection->ready || projection->completed != projection->requested) {
        pthread_mutex_unlock(&projection->lock); return;
    }
    TimelineTurn turns[FC_CONTEXT_TURNS];
    memcpy(turns, projection->turns, sizeof(turns));
    size_t count ← projection->count;
    Result result ← projection->result;
    projection->ready ← 0;
    pthread_mutex_unlock(&projection->lock);
    if (result != FC_OK) { report_problem(p, "Earlier turns cannot be verified; reopen history"); return; }
    for (size_t i ← 0; i < count; i++) {
        if ((*e)->PushLocalFrame(e, 16) < 0) return;
        TimelineTurn *turn ← &turns[i];
        jstring user ← material_utf8(e, (const unsigned char *)turn->turn.prompt, turn->turn.prompt_length);
        jstring assistant ← material_utf8(e, turn->bytes, turn->length);
        (*e)->CallVoidMethod(e, p->native_context[i][0], method(e, p->native_context[i][0], "setText", "(Ljava/lang/CharSequence;)V"), user);
        (*e)->CallVoidMethod(e, p->native_context[i][1], method(e, p->native_context[i][1], "setText", "(Ljava/lang/CharSequence;)V"), assistant);
        char description[128];
        snprintf(description, sizeof(description), "Earlier user message, turn %llu", (unsigned long long)turn->turn.request);
        m3_describe(e, p->native_context[i][0], description);
        snprintf(description, sizeof(description), "Earlier assistant response preview, turn %llu", (unsigned long long)turn->turn.request);
        m3_describe(e, p->native_context[i][1], description);
        snprintf(description, sizeof(description), "Read turn %llu · %s%s", (unsigned long long)turn->turn.request,
            phase_name(turn->turn.phase), turn->length < turn->turn.extent ? " · more text" : "");
        material_label(e, p->native_context[i][2], description);
        m3_describe(e, p->native_context[i][2], description);
        p->native_context_requests[i] ← turn->turn.request;
        for (int role ← 0; role < 3; role++)
            (*e)->CallVoidMethod(e, p->native_context[i][role], method(e, p->native_context[i][role], "setVisibility", "(I)V"), 0);
        (*e)->PopLocalFrame(e, NULL);
    }
    p->native_scroll_pending ← 2;
}
static void material_copy(Presentation *p, JNIEnv *e) {
    jobject text ← (*e)->CallObjectMethod(e, p->native_response,
        method(e, p->native_response, "getText", "()Ljava/lang/CharSequence;"));
    jstring service ← (*e)->NewStringUTF(e, "clipboard");
    jobject clipboard ← (*e)->CallObjectMethod(e, p->activity->clazz,
        method(e, p->activity->clazz, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"), service);
    jclass type ← (*e)->FindClass(e, "android/content/ClipData");
    jstring label ← (*e)->NewStringUTF(e, "FastChat response page");
    jobject clip ← (*e)->CallStaticObjectMethod(e, type, (*e)->GetStaticMethodID(e, type,
        "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;"), label, text);
    (*e)->CallVoidMethod(e, clipboard, method(e, clipboard, "setPrimaryClip", "(Landroid/content/ClipData;)V"), clip);
    if (!clear_java_failure(p, e)) snprintf(p->message, sizeof(p->message), "Response page copied");
}
static void material_action(JNIEnv *e, jobject listener, jobject clicked) {
    (void)listener;
    Presentation *p ← material_owner;
    if (!p || e != p->activity->env || !p->native_panel) return;
    if ((*e)->PushLocalFrame(e, 48) < 0) return;
    jobject context ← (*e)->CallObjectMethod(e, clicked,
        method(e, clicked, "getContext", "()Landroid/content/Context;"));
    if (!(*e)->IsSameObject(e, context, p->activity->clazz)) {
        (*e)->PopLocalFrame(e, NULL); return; /* retired Activity cannot command a new owner */
    }
    jint identity ← (*e)->CallIntMethod(e, clicked, method(e, clicked, "getId", "()I"));
    if (identity == UI_SEND) send_or_cancel(p);
    else if (identity == UI_SETUP) show_settings(p);
    else if (identity == UI_SAVE) save_settings(p);
    else if (identity == UI_CLOSE) close_settings(p);
    else if (identity >= UI_PAGE_FIRST && identity < UI_PAGE_FIRST + FC_PAGE_BUTTONS) {
        int action ← identity - UI_PAGE_FIRST;
        if (action == FC_PAGE_PREVIOUS) page_response(p, 0);
        else if (action == FC_PAGE_NEXT) page_response(p, 1);
        else if (action == FC_PAGE_START) fc_page_start(&p->page);
        else material_copy(p, e);
    } else {
        uint64_t selected ← p->viewed_request ? p->viewed_request : p->conversation.request;
        if (identity >= UI_CONTEXT_FIRST && identity < UI_CONTEXT_FIRST + FC_CONTEXT_TURNS) {
            uint64_t requested ← p->native_context_requests[identity - UI_CONTEXT_FIRST];
            if (requested && requested < selected) p->viewed_request ← requested;
        } else if (identity == UI_PREVIOUS_TURN && selected > 1) p->viewed_request ← selected - 1;
        else if (identity == UI_NEXT_TURN && selected < p->conversation.request) p->viewed_request ← selected + 1;
        else if (identity == UI_LATEST_TURN) p->viewed_request ← 0;
        if (p->viewed_request && history_turn(&p->conversation, p->viewed_request, &p->selected_turn) != FC_OK) {
            p->viewed_request ← 0; report_problem(p, "Stored history cannot be selected");
        }
        fc_page_reset(&p->page);
    }
    material_present(p);
    (*e)->PopLocalFrame(e, NULL);
}
static jobject material_button(Presentation *p, JNIEnv *e, jobject row, int identity, const char *label, int filled) {
    jobject button ← m3_button(e, p->activity->clazz, p->native_listener, p->theme, identity, label, filled);
    m3_add(e, row, button, 0, -2, 1.0f);
    return button;
}
static void create_material_panel(Presentation *p) {
    JNIEnv *e ← p->activity->env;
    if ((*e)->PushLocalFrame(e, 80) < 0) return;
    jobject activity ← p->activity->clazz;
    /* NativeActivity is a framework class. Resolve the companion DEX through
     * the application's ClassLoader rather than FindClass's boot loader. */
    jobject loader ← (*e)->CallObjectMethod(e, activity, method(e, activity, "getClassLoader", "()Ljava/lang/ClassLoader;"));
    jstring name ← (*e)->NewStringUTF(e, "org.isomorphisms.nativeui.Action");
    jclass action_type ← (jclass)(*e)->CallObjectMethod(e, loader,
        method(e, loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"), name);
    if (!action_type || clear_java_failure(p, e)) { (*e)->PopLocalFrame(e, NULL); return; }
    JNINativeMethod callbacks[] ← {{ "onClick", "(Landroid/view/View;)V", (void *)material_action }};
    if ((*e)->RegisterNatives(e, action_type, callbacks, 1) != 0) { (*e)->PopLocalFrame(e, NULL); return; }
    jobject listener ← (*e)->NewObject(e, action_type, (*e)->GetMethodID(e, action_type, "<init>", "()V"));
    p->native_listener ← (*e)->NewGlobalRef(e, listener);
    p->theme ← m3_theme(e, activity);
    jobject window ← (*e)->CallObjectMethod(e, activity, method(e, activity, "getWindow", "()Landroid/view/Window;"));
    (*e)->CallVoidMethod(e, window, method(e, window, "setStatusBarColor", "(I)V"), p->theme.surface);
    (*e)->CallVoidMethod(e, window, method(e, window, "setNavigationBarColor", "(I)V"), p->theme.surface);
    jobject decor ← (*e)->CallObjectMethod(e, window, method(e, window, "getDecorView", "()Landroid/view/View;"));
    jint flags ← (*e)->CallIntMethod(e, decor, method(e, decor, "getSystemUiVisibility", "()I"));
    (*e)->CallVoidMethod(e, decor, method(e, decor, "setSystemUiVisibility", "(I)V"),
        (flags & ~0x2010) | (p->theme.dark ? 0 : 0x2010));
    jobject root ← m3_column(e, activity);
    (*e)->CallVoidMethod(e, root, method(e, root, "setBackgroundColor", "(I)V"), p->theme.surface);
    (*e)->CallVoidMethod(e, root, method(e, root, "setPadding", "(IIII)V"),
        m3_dp(p->theme, 12), m3_dp(p->theme, 8), m3_dp(p->theme, 12), m3_dp(p->theme, 8));
    jobject heading ← m3_new(e, activity, "android/widget/TextView");
    jstring title ← (*e)->NewStringUTF(e, "FastChat");
    m3_text(e, heading, title, p->theme, 22.0f); m3_add(e, root, heading, -1, -2, 0.0f);
    jobject navigation ← m3_new(e, activity, "android/widget/LinearLayout");
    const int history_actions[3] ← { UI_PREVIOUS_TURN, UI_NEXT_TURN, UI_LATEST_TURN };
    const char *history_labels[3] ← { "Older", "Newer", "Latest" };
    for (int i ← 0; i < 3; i++) {
        jobject button ← material_button(p, e, navigation, history_actions[i], history_labels[i], 0);
        p->native_history_buttons[i] ← (*e)->NewGlobalRef(e, button);
    }
    material_button(p, e, navigation, UI_SETUP, "Setup", 0);
    m3_add(e, root, navigation, -1, -2, 0.0f);
    jobject status ← m3_new(e, activity, "android/widget/TextView");
    jstring empty ← (*e)->NewStringUTF(e, "");
    m3_text(e, status, empty, p->theme, 14.0f); m3_add(e, root, status, -1, -2, 0.0f);
    p->native_status ← (*e)->NewGlobalRef(e, status);
    m3_describe(e, status, "FastChat generation status");
    jobject scroll ← m3_new(e, activity, "android/widget/ScrollView");
    m3_describe(e, scroll, "Conversation timeline");
    jobject content ← m3_column(e, activity);
    p->native_scroll ← (*e)->NewGlobalRef(e, scroll);
    for (int i ← 0; i < FC_CONTEXT_TURNS; i++) {
        for (int role ← 0; role < 3; role++) {
            jobject view ← role == 2
                ? m3_button(e, activity, listener, p->theme, UI_CONTEXT_FIRST + i, "Read turn", 0)
                : m3_new(e, activity, "android/widget/TextView");
            if (role < 2) {
                m3_text(e, view, empty, p->theme, 16.0f);
                (*e)->CallVoidMethod(e, view, method(e, view, "setTextIsSelectable", "(Z)V"), JNI_TRUE);
                (*e)->CallVoidMethod(e, view, method(e, view, "setMaxLines", "(I)V"), 6);
                (*e)->CallVoidMethod(e, view, method(e, view, "setPadding", "(IIII)V"),
                    m3_dp(p->theme, 8), m3_dp(p->theme, 8), m3_dp(p->theme, 8), m3_dp(p->theme, 8));
                if (!role) (*e)->CallVoidMethod(e, view, method(e, view, "setBackgroundColor", "(I)V"), p->theme.container);
            }
            (*e)->CallVoidMethod(e, view, method(e, view, "setVisibility", "(I)V"), 8);
            m3_add(e, content, view, -1, -2, 0.0f);
            p->native_context[i][role] ← (*e)->NewGlobalRef(e, view);
            (*e)->DeleteLocalRef(e, view);
        }
    }
    jobject user ← m3_new(e, activity, "android/widget/TextView");
    jobject assistant ← m3_new(e, activity, "android/widget/TextView");
    m3_text(e, user, empty, p->theme, 16.0f); m3_text(e, assistant, empty, p->theme, 16.0f);
    (*e)->CallVoidMethod(e, user, method(e, user, "setBackgroundColor", "(I)V"), p->theme.container);
    (*e)->CallVoidMethod(e, user, method(e, user, "setPadding", "(IIII)V"), 16, 12, 16, 12);
    (*e)->CallVoidMethod(e, assistant, method(e, assistant, "setPadding", "(IIII)V"), 0, m3_dp(p->theme, 12), 0, m3_dp(p->theme, 20));
    (*e)->CallVoidMethod(e, user, method(e, user, "setTextIsSelectable", "(Z)V"), JNI_TRUE);
    (*e)->CallVoidMethod(e, assistant, method(e, assistant, "setTextIsSelectable", "(Z)V"), JNI_TRUE);
    m3_describe(e, user, "User message"); m3_describe(e, assistant, "Assistant response, selectable text");
    m3_add(e, content, user, -1, -2, 0.0f); m3_add(e, content, assistant, -1, -2, 0.0f);
    (*e)->CallVoidMethod(e, scroll, method(e, scroll, "addView", "(Landroid/view/View;)V"), content);
    m3_add(e, root, scroll, -1, 0, 1.0f);
    p->native_prompt ← (*e)->NewGlobalRef(e, user);
    p->native_response ← (*e)->NewGlobalRef(e, assistant);
    jobject pages ← m3_new(e, activity, "android/widget/LinearLayout");
    const char *labels[FC_PAGE_BUTTONS] ← { "Prev", "Next", "Start", "Copy" };
    const char *descriptions[FC_PAGE_BUTTONS] ← { "Previous response page", "Next response page", "Start of response", "Copy displayed response page" };
    for (int i ← 0; i < FC_PAGE_BUTTONS; i++) {
        jobject button ← material_button(p, e, pages, UI_PAGE_FIRST + i, labels[i], 0);
        m3_describe(e, button, descriptions[i]);
        p->native_page_buttons[i] ← (*e)->NewGlobalRef(e, button);
    }
    m3_add(e, root, pages, -1, -2, 0.0f);
    jobject range ← m3_new(e, activity, "android/widget/TextView");
    m3_text(e, range, empty, p->theme, 12.0f); m3_add(e, root, range, -1, -2, 0.0f);
    p->native_range ← (*e)->NewGlobalRef(e, range);
    m3_describe(e, range, "Response page position");
    jobject composer ← m3_new(e, activity, "android/widget/LinearLayout");
    (*e)->CallVoidMethod(e, p->composer, method(e, p->composer, "setTextColor", "(I)V"), p->theme.on_surface);
    (*e)->CallVoidMethod(e, p->composer, method(e, p->composer, "setBackgroundColor", "(I)V"), p->theme.container);
    (*e)->CallVoidMethod(e, p->composer, method(e, p->composer, "setHintTextColor", "(I)V"), p->theme.secondary);
    m3_describe(e, p->composer, "Message, multiline draft");
    m3_add(e, composer, p->composer, 0, -2, 3.0f);
    jobject send ← material_button(p, e, composer, UI_SEND, "Send", 1);
    p->native_send ← (*e)->NewGlobalRef(e, send);
    m3_add(e, root, composer, -1, -2, 0.0f);
    jclass layout_type ← (*e)->GetObjectClass(e, p->composer_layout);
    (*e)->SetIntField(e, p->composer_layout, (*e)->GetFieldID(e, layout_type, "width", "I"), -1);
    (*e)->SetIntField(e, p->composer_layout, (*e)->GetFieldID(e, layout_type, "height", "I"), -1);
    (*e)->SetIntField(e, p->composer_layout, (*e)->GetFieldID(e, layout_type, "gravity", "I"), 48); /* TOP */
    p->native_panel ← (*e)->NewGlobalRef(e, root);
    p->native_revision ← UINT64_MAX;
    timeline_start(&p->timeline);
    material_owner ← p;
    clear_java_failure(p, e);
    (*e)->PopLocalFrame(e, NULL);
}

static void material_present(Presentation *p) {
    if (!p->native_panel || !p->composer_attached || p->settings_visible) return;
    JNIEnv *e ← p->activity->env;
    if ((*e)->PushLocalFrame(e, 32) < 0) return;
    uint64_t request ← p->viewed_request ? p->viewed_request : p->conversation.request;
    timeline_present(p, e, request);
    if (p->native_scroll_pending && --p->native_scroll_pending == 0)
        (*e)->CallBooleanMethod(e, p->native_scroll, method(e, p->native_scroll, "fullScroll", "(I)Z"), 130);
    HistoryTurn selected ← p->selected_turn;
    if (request == p->conversation.request) {
        selected.phase ← p->conversation.phase; selected.prompt_length ← p->conversation.prompt_length;
        memcpy(selected.prompt, p->conversation.prompt, selected.prompt_length);
    }
    unsigned char bytes[FC_VIEW_BYTES]; size_t length ← p->native_response_length;
    int changed ← p->native_revision != p->conversation.sequence || p->native_request != request || p->native_offset != p->page.viewport;
    Result result ← FC_OK;
    if (changed) result ← request != p->conversation.request
        ? read_history_window(&p->conversation, &selected, p->page.viewport, bytes, sizeof(bytes), &length)
        : render_stored_window(&p->conversation, p->page.viewport, bytes, sizeof(bytes), &length);
    int valid ← 1;
    if (changed) length ← complete_utf8_prefix(bytes, length, &valid);
    if (result != FC_OK || !valid) { length ← 0; report_problem(p, "Stored text cannot be displayed"); }
    if (changed) {
        p->page.next_viewport ← p->page.viewport + length;
        p->page.next_page_available ← 0;
        if (length) response_page_follows(&p->conversation,
            request != p->conversation.request ? &selected : NULL, p->page.next_viewport, &p->page.next_page_available);
        if (p->native_request != request) {
            jstring prompt ← material_utf8(e, (const unsigned char *)selected.prompt, request ? selected.prompt_length : 0);
            (*e)->CallVoidMethod(e, p->native_prompt, method(e, p->native_prompt, "setText", "(Ljava/lang/CharSequence;)V"), prompt);
            jstring prefix ← (*e)->NewStringUTF(e, "User message. ");
            jstring description ← (*e)->CallObjectMethod(e, prefix, method(e, prefix, "concat", "(Ljava/lang/String;)Ljava/lang/String;"), prompt);
            (*e)->CallVoidMethod(e, p->native_prompt, method(e, p->native_prompt, "setContentDescription", "(Ljava/lang/CharSequence;)V"), description);
        }
        /* A stream may append beyond an already full visible page. Keep its
         * View, layout and selection intact when those displayed bytes match. */
        if (p->native_request != request || p->native_offset != p->page.viewport ||
                p->native_response_length != length || memcmp(p->native_response_bytes, bytes, length)) {
            jstring response ← material_utf8(e, bytes, length);
            (*e)->CallVoidMethod(e, p->native_response, method(e, p->native_response, "setText", "(Ljava/lang/CharSequence;)V"), response);
            jstring prefix ← (*e)->NewStringUTF(e, "Assistant response, selectable text. ");
            jstring description ← (*e)->CallObjectMethod(e, prefix, method(e, prefix, "concat", "(Ljava/lang/String;)Ljava/lang/String;"), response);
            (*e)->CallVoidMethod(e, p->native_response, method(e, p->native_response, "setContentDescription", "(Ljava/lang/CharSequence;)V"), description);
            memcpy(p->native_response_bytes, bytes, length); p->native_response_length ← length;
        }
        p->native_revision ← p->conversation.sequence; p->native_request ← request; p->native_offset ← p->page.viewport;
    }
    (*e)->CallVoidMethod(e, p->native_prompt, method(e, p->native_prompt, "setVisibility", "(I)V"), request ? 0 : 8);
    m3_enabled(e, p->native_history_buttons[0], request > 1);
    m3_enabled(e, p->native_history_buttons[1], request && request < p->conversation.request);
    m3_enabled(e, p->native_history_buttons[2], p->viewed_request && p->conversation.request);
    char status[256], range[160];
    snprintf(status, sizeof(status), "Turn %llu / %llu · %s%s%s", (unsigned long long)request,
        (unsigned long long)p->conversation.request, request ? phase_name(selected.phase) : "Ready",
        p->message[0] ? "\n" : "", p->message);
    material_label(e, p->native_status, status);
    char accessible[320];
    snprintf(accessible, sizeof(accessible), "FastChat generation status. %s", status);
    m3_describe(e, p->native_status, accessible);
    snprintf(range, sizeof(range), "Page %llu · bytes %llu–%llu%s",
        (unsigned long long)(p->page.page_index + 1), (unsigned long long)p->page.viewport,
        (unsigned long long)p->page.next_viewport, p->page.next_page_available ? "" : " · End of stored text");
    material_label(e, p->native_range, range);
    snprintf(accessible, sizeof(accessible), "Response page position. %s", range);
    m3_describe(e, p->native_range, accessible);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_PREVIOUS], p->page.page_count != 0);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_NEXT], p->page.next_page_available);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_START], p->page.viewport != 0);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_COPY], length != 0);
    const char *action ← p->running ? "Stop" : p->conversation.phase == UNCERTAIN ? "Retry" : "Send";
    material_label(e, p->native_send, action); m3_describe(e, p->native_send, action);
    clear_java_failure(p, e);
    (*e)->PopLocalFrame(e, NULL);
}
