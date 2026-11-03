/* The product surface uses real framework Views in its own ViewRoot. Android's
 * NativeActivity surface remains an explicitly selected Canvas comparator. */
static Presentation *material_owner;
static void material_destroy(Presentation *p) {
    if (material_owner == p) material_owner ← NULL;
    JNIEnv *e ← p->activity->env;
    jobject references[] ← {p->native_panel, p->native_listener, p->native_status,
        p->native_prompt, p->native_response, p->native_range, p->native_send};
    for (unsigned i ← 0; i < sizeof(references)/sizeof(references[0]); i++)
        if (references[i]) (*e)->DeleteGlobalRef(e, references[i]);
    for (unsigned i ← 0; i < FC_PAGE_BUTTONS; i++)
        if (p->native_page_buttons[i]) (*e)->DeleteGlobalRef(e, p->native_page_buttons[i]);
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
        if (identity == UI_PREVIOUS_TURN && selected > 1) p->viewed_request ← selected - 1;
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
    jobject root ← m3_column(e, activity);
    (*e)->CallVoidMethod(e, root, method(e, root, "setBackgroundColor", "(I)V"), p->theme.surface);
    (*e)->CallVoidMethod(e, root, method(e, root, "setPadding", "(IIII)V"),
        m3_dp(p->theme, 12), m3_dp(p->theme, 8), m3_dp(p->theme, 12), m3_dp(p->theme, 8));
    jobject heading ← m3_new(e, activity, "android/widget/TextView");
    jstring title ← (*e)->NewStringUTF(e, "FastChat");
    m3_text(e, heading, title, p->theme, 22.0f); m3_add(e, root, heading, -1, -2, 0.0f);
    jobject navigation ← m3_new(e, activity, "android/widget/LinearLayout");
    material_button(p, e, navigation, UI_PREVIOUS_TURN, "Older", 0);
    material_button(p, e, navigation, UI_NEXT_TURN, "Newer", 0);
    material_button(p, e, navigation, UI_LATEST_TURN, "Latest", 0);
    material_button(p, e, navigation, UI_SETUP, "Setup", 0);
    m3_add(e, root, navigation, -1, -2, 0.0f);
    jobject status ← m3_new(e, activity, "android/widget/TextView");
    jstring empty ← (*e)->NewStringUTF(e, "");
    m3_text(e, status, empty, p->theme, 14.0f); m3_add(e, root, status, -1, -2, 0.0f);
    p->native_status ← (*e)->NewGlobalRef(e, status);
    m3_describe(e, status, "FastChat generation status");
    jobject scroll ← m3_new(e, activity, "android/widget/ScrollView");
    jobject content ← m3_column(e, activity);
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
    const char *labels[FC_PAGE_BUTTONS] ← { "Previous", "Next", "Start", "Copy" };
    for (int i ← 0; i < FC_PAGE_BUTTONS; i++) {
        jobject button ← material_button(p, e, pages, UI_PAGE_FIRST + i, labels[i], 0);
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
    material_owner ← p;
    clear_java_failure(p, e);
    (*e)->PopLocalFrame(e, NULL);
}

static void material_present(Presentation *p) {
    if (!p->native_panel || !p->composer_attached || p->settings_visible) return;
    JNIEnv *e ← p->activity->env;
    if ((*e)->PushLocalFrame(e, 32) < 0) return;
    uint64_t request ← p->viewed_request ? p->viewed_request : p->conversation.request;
    HistoryTurn selected ← p->selected_turn;
    if (request == p->conversation.request) {
        selected.phase ← p->conversation.phase; selected.prompt_length ← p->conversation.prompt_length;
        memcpy(selected.prompt, p->conversation.prompt, selected.prompt_length);
    }
    unsigned char bytes[FC_VIEW_BYTES]; size_t length ← 0;
    Result result ← request != p->conversation.request
        ? read_history_window(&p->conversation, &selected, p->page.viewport, bytes, sizeof(bytes), &length)
        : render_stored_window(&p->conversation, p->page.viewport, bytes, sizeof(bytes), &length);
    int valid ← 1;
    length ← complete_utf8_prefix(bytes, length, &valid);
    if (result != FC_OK || !valid) { length ← 0; report_problem(p, "Stored text cannot be displayed"); }
    p->page.next_viewport ← p->page.viewport + length;
    p->page.next_page_available ← 0;
    if (length) response_page_follows(&p->conversation,
        request != p->conversation.request ? &selected : NULL, p->page.next_viewport, &p->page.next_page_available);
    if (p->native_revision != p->conversation.sequence || p->native_request != request || p->native_offset != p->page.viewport) {
        jstring prompt ← material_utf8(e, (const unsigned char *)selected.prompt, request ? selected.prompt_length : 0);
        jstring response ← material_utf8(e, bytes, length);
        (*e)->CallVoidMethod(e, p->native_prompt, method(e, p->native_prompt, "setText", "(Ljava/lang/CharSequence;)V"), prompt);
        (*e)->CallVoidMethod(e, p->native_response, method(e, p->native_response, "setText", "(Ljava/lang/CharSequence;)V"), response);
        p->native_revision ← p->conversation.sequence; p->native_request ← request; p->native_offset ← p->page.viewport;
    }
    char status[256], range[160];
    snprintf(status, sizeof(status), "Turn %llu / %llu · %s%s%s", (unsigned long long)request,
        (unsigned long long)p->conversation.request, request ? phase_name(selected.phase) : "Ready",
        p->message[0] ? "\n" : "", p->message);
    material_label(e, p->native_status, status);
    snprintf(range, sizeof(range), "Page %llu · bytes %llu–%llu%s",
        (unsigned long long)(p->page.page_index + 1), (unsigned long long)p->page.viewport,
        (unsigned long long)p->page.next_viewport, p->page.next_page_available ? "" : " · End of stored text");
    material_label(e, p->native_range, range);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_PREVIOUS], p->page.page_count != 0);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_NEXT], p->page.next_page_available);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_START], p->page.viewport != 0);
    m3_enabled(e, p->native_page_buttons[FC_PAGE_COPY], length != 0);
    const char *action ← p->running ? "Stop" : p->conversation.phase == UNCERTAIN ? "Retry" : "Send";
    material_label(e, p->native_send, action); m3_describe(e, p->native_send, action);
    clear_java_failure(p, e);
    (*e)->PopLocalFrame(e, NULL);
}
