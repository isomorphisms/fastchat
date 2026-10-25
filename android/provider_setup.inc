/* Included after the existing NativeActivity primitives. Platform widgets own
   editing, this panel owns settings, and TransportBoundary owns the connection. */
static Result system_trust(Presentation *presentation, char *path, size_t capacity) {
    const char *directory ← "/apex/com.android.conscrypt/cacerts";
    DIR *roots ← opendir(directory);
    if (!roots && errno == ENOENT) { directory ← "/system/etc/security/cacerts"; roots ← opendir(directory); }
    if (!roots) return FC_STORAGE_ERROR;
    char temporary[4096];
    int count ← snprintf(path, capacity, "%s/system-roots.pem", presentation->activity->internalDataPath);
    int pending ← snprintf(temporary, sizeof(temporary), "%s/system-roots.pending", presentation->activity->internalDataPath);
    if (count < 0 || (size_t)count >= capacity || pending < 0 || (size_t)pending >= sizeof(temporary)) {
        closedir(roots); return FC_REJECTED;
    }
    int output ← open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output < 0) { closedir(roots); return FC_STORAGE_ERROR; }
    Result result ← FC_OK;
    unsigned certificates ← 0;
    size_t total ← 0;
    struct dirent *entry;
    while (result == FC_OK && (entry ← readdir(roots))) {
        if (entry->d_name[0] == '.') continue;
        int file ← openat(dirfd(roots), entry->d_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        struct stat status;
        if (file < 0 || fstat(file, &status) != 0 || !S_ISREG(status.st_mode)
            || status.st_size <= 0 || status.st_size > 65536 || certificates >= 1024) {
            if (file >= 0) close(file);
            result ← FC_STORAGE_ERROR; break;
        }
        char bytes[4096];
        ssize_t amount;
        while ((amount ← read(file, bytes, sizeof(bytes))) != 0) {
            if (amount < 0 && errno == EINTR) continue;
            if (amount < 0 || total + (size_t)amount > 4*1024*1024) { result ← FC_STORAGE_ERROR; break; }
            size_t written ← 0;
            while (written < (size_t)amount) {
                ssize_t moved ← write(output, bytes + written, (size_t)amount - written);
                if (moved < 0 && errno == EINTR) continue;
                if (moved <= 0) { result ← FC_STORAGE_ERROR; break; }
                written ← written + (size_t)moved;
            }
            if (result != FC_OK) break;
            total ← total + (size_t)amount;
        }
        close(file);
        if (result == FC_OK && write(output, "\n", 1) != 1) result ← FC_STORAGE_ERROR;
        certificates++;
    }
    closedir(roots);
    if (!certificates || fsync(output) != 0) result ← FC_STORAGE_ERROR;
    if (close(output) != 0) result ← FC_STORAGE_ERROR;
    if (result == FC_OK && rename(temporary, path) != 0) result ← FC_STORAGE_ERROR;
    int parent ← open(presentation->activity->internalDataPath, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent < 0 || (result == FC_OK && fsync(parent) != 0)) result ← FC_STORAGE_ERROR;
    if (parent >= 0) close(parent);
    if (result != FC_OK) unlink(temporary);
    return result;
}
static Result prepare_settings(Presentation *presentation, const ProviderSettings *settings, TransportBoundary *network) {
    if (!settings_valid(settings)) return FC_REJECTED;
    char ca[4096];
    const char *trust ← settings->ca;
    if (!strcmp(trust, "system")) {
        Result result ← system_trust(presentation, ca, sizeof(ca));
        if (result != FC_OK) return result;
        trust ← ca;
    }
    Result result ← transport_boundary_open(network, &presentation->conversation, settings->provider.origin, trust);
    if (result != FC_OK) return result;
    network->request_format ← settings->provider.backend == PROVIDER_RESPONSE ? PROVIDER_RESPONSES : PROVIDER_CHAT_COMPLETIONS;
    if (settings->authorization[0]) result ← transport_boundary_authorization(network, settings->authorization);
    if (result != FC_OK) transport_boundary_close(network);
    return result;
}
static void adopt_settings(Presentation *presentation, const ProviderSettings *settings, const TransportBoundary *network) {
    if (presentation->http2) transport_boundary_close(&presentation->network);
    presentation->network ← *network;
    strcpy(presentation->model, settings->provider.model);
    strcpy(presentation->provider_path, settings->provider.path);
    presentation->http2 ← 1;
    presentation->configuration_failure ← 0;
}
static Result configure_transport(Presentation *presentation) {
    ProviderSettings settings;
    int present;
    Result result ← settings_load(presentation->activity->internalDataPath, &settings, &present);
    if (result == FC_OK && !present) result ← configure_legacy_transport(presentation);
    else if (result == FC_OK) {
        TransportBoundary network;
        result ← prepare_settings(presentation, &settings, &network);
        if (result == FC_OK) adopt_settings(presentation, &settings, &network);
    }
    private_bytes_clear(&settings, sizeof(settings));
    return result;
}
static void set_field(Presentation *presentation, jobject field, const char *value) {
    JNIEnv *environment ← presentation->activity->env;
    jchar units[2048];
    size_t length ← strlen(value), position ← 0, count ← 0;
    int valid;
    if (length > 2048 || complete_utf8_prefix((const unsigned char *)value, length, &valid) != length || !valid) return;
    while (position < length) {
        unsigned codepoint ← decode_codepoint((const unsigned char *)value, &position);
        if (codepoint < 0x10000) units[count++] ← (jchar)codepoint;
        else {
            codepoint ← codepoint - 0x10000;
            units[count++] ← (jchar)(0xd800 + (codepoint >> 10));
            units[count++] ← (jchar)(0xdc00 + (codepoint & 0x3ff));
        }
    }
    jstring text ← (*environment)->NewString(environment, units, (jsize)count);
    if (text) (*environment)->CallVoidMethod(environment, field,
        method(environment, field, "setText", "(Ljava/lang/CharSequence;)V"), text);
    if (text) (*environment)->DeleteLocalRef(environment, text);
}
static void detach_settings(Presentation *presentation) {
    if (!presentation->settings_attached) return;
    JNIEnv *environment ← presentation->activity->env;
    (*environment)->CallVoidMethod(environment, presentation->composer_manager,
        method(environment, presentation->composer_manager, "removeViewImmediate", "(Landroid/view/View;)V"), presentation->settings_panel);
    clear_java_failure(presentation, environment);
    presentation->settings_attached ← 0;
}
static void attach_settings(Presentation *presentation) {
    if (!presentation->settings_visible || presentation->settings_attached || !presentation->window || !presentation->settings_panel) return;
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 12) < 0) return;
    jobject activity ← presentation->activity->clazz;
    jobject window ← (*environment)->CallObjectMethod(environment, activity, method(environment, activity, "getWindow", "()Landroid/view/Window;"));
    jobject decor ← (*environment)->CallObjectMethod(environment, window, method(environment, window, "getDecorView", "()Landroid/view/View;"));
    jobject token ← (*environment)->CallObjectMethod(environment, decor, method(environment, decor, "getWindowToken", "()Landroid/os/IBinder;"));
    jclass layout_type ← (*environment)->GetObjectClass(environment, presentation->settings_layout);
    int width ← (*environment)->CallIntMethod(environment, decor, method(environment, decor, "getWidth", "()I"));
    int bottom ← presentation->content_bottom > 0 ? presentation->content_bottom : presentation->height;
    int height ← bottom - (int)(136*presentation->density);
    if (height < (int)(48*presentation->density)) height ← (int)(48*presentation->density);
    if (!clear_java_failure(presentation, environment) && token && width > 0) {
        (*environment)->SetObjectField(environment, presentation->settings_layout,
            (*environment)->GetFieldID(environment, layout_type, "token", "Landroid/os/IBinder;"), token);
        (*environment)->SetIntField(environment, presentation->settings_layout,
            (*environment)->GetFieldID(environment, layout_type, "width", "I"), width);
        (*environment)->SetIntField(environment, presentation->settings_layout,
            (*environment)->GetFieldID(environment, layout_type, "height", "I"), height);
        (*environment)->CallVoidMethod(environment, presentation->composer_manager,
            method(environment, presentation->composer_manager, "addView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"),
            presentation->settings_panel, presentation->settings_layout);
        if (!clear_java_failure(presentation, environment)) presentation->settings_attached ← 1;
    }
    (*environment)->PopLocalFrame(environment, NULL);
}
static void show_settings(Presentation *presentation) {
    if (presentation->running || presentation->acceptance) {
        report_problem(presentation, "Stop generation before changing providers"); return;
    }
    if (presentation->failure || !presentation->composer_manager) { report_problem(presentation, "Android editor or store unavailable"); return; }
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 64) < 0) return;
    if (!presentation->settings_panel) {
        jobject activity ← presentation->activity->clazz;
        jclass scroll_type ← (*environment)->FindClass(environment, "android/widget/ScrollView");
        jobject scroll ← (*environment)->NewObject(environment, scroll_type,
            (*environment)->GetMethodID(environment, scroll_type, "<init>", "(Landroid/content/Context;)V"), activity);
        jclass linear_type ← (*environment)->FindClass(environment, "android/widget/LinearLayout");
        jobject linear ← (*environment)->NewObject(environment, linear_type,
            (*environment)->GetMethodID(environment, linear_type, "<init>", "(Landroid/content/Context;)V"), activity);
        (*environment)->CallVoidMethod(environment, linear, method(environment, linear, "setOrientation", "(I)V"), 1);
        (*environment)->CallVoidMethod(environment, scroll, method(environment, scroll, "setBackgroundColor", "(I)V"), (jint)0xfff6f2e9);
        const char *labels[] ← {"HTTPS origin", "Endpoint path", "Model", "Format: chat-completions or responses",
            "Authorization header (optional)", "CA: system or an absolute PEM path"};
        for (unsigned index ← 0; index < 6; index++) {
            jclass label_type ← (*environment)->FindClass(environment, "android/widget/TextView");
            jobject label ← (*environment)->NewObject(environment, label_type,
                (*environment)->GetMethodID(environment, label_type, "<init>", "(Landroid/content/Context;)V"), activity);
            set_field(presentation, label, labels[index]);
            (*environment)->CallVoidMethod(environment, linear, method(environment, linear, "addView", "(Landroid/view/View;)V"), label);
            jclass edit_type ← (*environment)->FindClass(environment, "android/widget/EditText");
            jobject edit ← (*environment)->NewObject(environment, edit_type,
                (*environment)->GetMethodID(environment, edit_type, "<init>", "(Landroid/content/Context;)V"), activity);
            (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setInputType", "(I)V"), index == 4 ? 0x81 : 0x80001);
            (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setSingleLine", "(Z)V"), JNI_TRUE);
            (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setImeOptions", "(I)V"), 0x10000000);
            (*environment)->CallVoidMethod(environment, linear, method(environment, linear, "addView", "(Landroid/view/View;)V"), edit);
            presentation->settings_fields[index] ← (*environment)->NewGlobalRef(environment, edit);
        }
        (*environment)->CallVoidMethod(environment, scroll, method(environment, scroll, "addView", "(Landroid/view/View;)V"), linear);
        jclass layout_type ← (*environment)->FindClass(environment, "android/view/WindowManager$LayoutParams");
        jobject layout ← (*environment)->NewObject(environment, layout_type,
            (*environment)->GetMethodID(environment, layout_type, "<init>", "(IIIII)V"), 1, 1, 1000, 32 | 0x2000, -3); /* also SECURE: exclude credential screenshots */
        (*environment)->SetIntField(environment, layout,
            (*environment)->GetFieldID(environment, layout_type, "gravity", "I"), 51); /* TOP | LEFT */
        (*environment)->SetIntField(environment, layout,
            (*environment)->GetFieldID(environment, layout_type, "y", "I"), (int)(120*presentation->density));
        (*environment)->SetIntField(environment, layout,
            (*environment)->GetFieldID(environment, layout_type, "softInputMode", "I"), 16);
        presentation->settings_panel ← (*environment)->NewGlobalRef(environment, scroll);
        presentation->settings_layout ← (*environment)->NewGlobalRef(environment, layout);
    }
    ProviderSettings settings;
    int present;
    Result loaded ← settings_load(presentation->activity->internalDataPath, &settings, &present);
    if (loaded != FC_OK || !present) {
        memset(&settings, 0, sizeof(settings));
        settings.provider.backend ← presentation->http2 && presentation->network.request_format == PROVIDER_RESPONSES ? PROVIDER_RESPONSE : PROVIDER_CHAT;
        strcpy(settings.provider.origin, presentation->http2 ? presentation->network.origin : "https://api.openai.com");
        strcpy(settings.provider.path, presentation->http2 ? presentation->provider_path : "/v1/chat/completions");
        strcpy(settings.provider.model, presentation->model);
        strcpy(settings.ca, "system");
    }
    set_field(presentation, presentation->settings_fields[0], settings.provider.origin);
    set_field(presentation, presentation->settings_fields[1], settings.provider.path);
    set_field(presentation, presentation->settings_fields[2], settings.provider.model);
    set_field(presentation, presentation->settings_fields[3], settings.provider.backend == PROVIDER_RESPONSE ? "responses" : "chat-completions");
    set_field(presentation, presentation->settings_fields[4], settings.authorization);
    set_field(presentation, presentation->settings_fields[5], settings.ca);
    private_bytes_clear(&settings, sizeof(settings));
    if (!clear_java_failure(presentation, environment)) {
        persist_draft(presentation);
        detach_composer(presentation);
        presentation->message[0] ← 0;
        presentation->settings_visible ← 1;
        attach_settings(presentation);
    }
    (*environment)->PopLocalFrame(environment, NULL);
    present_stored_response(presentation);
}
static void close_settings(Presentation *presentation) {
    detach_settings(presentation);
    presentation->settings_visible ← 0;
    if (presentation->settings_fields[4]) set_field(presentation, presentation->settings_fields[4], "");
    attach_composer(presentation);
    present_stored_response(presentation);
}
static void save_settings(Presentation *presentation) {
    ProviderSettings settings;
    memset(&settings, 0, sizeof(settings));
    char format[32];
    char *destinations[] ← {settings.provider.origin, settings.provider.path, settings.provider.model,
        format, settings.authorization, settings.ca};
    size_t capacities[] ← {sizeof(settings.provider.origin), sizeof(settings.provider.path), sizeof(settings.provider.model),
        sizeof(format), sizeof(settings.authorization), sizeof(settings.ca)};
    Result result ← FC_OK;
    for (unsigned index ← 0; result == FC_OK && index < 6; index++) {
        size_t length;
        result ← editor_bytes(presentation, presentation->settings_fields[index], destinations[index], capacities[index]-1, &length);
        if (result == FC_OK) destinations[index][length] ← 0;
    }
    if (result == FC_OK) {
        if (!strcmp(format, "responses")) settings.provider.backend ← PROVIDER_RESPONSE;
        else if (!strcmp(format, "chat-completions")) settings.provider.backend ← PROVIDER_CHAT;
        else result ← FC_REJECTED;
    }
    TransportBoundary network;
    int prepared ← 0;
    if (result == FC_OK) { result ← prepare_settings(presentation, &settings, &network); prepared ← result == FC_OK; }
    if (result == FC_OK) {
        result ← settings_save(presentation->activity->internalDataPath, &settings);
        if (result != FC_OK) presentation->configuration_failure ← 1; /* failed barrier: no further submissions */
    }
    if (result == FC_OK) adopt_settings(presentation, &settings, &network);
    else if (prepared) transport_boundary_close(&network);
    private_bytes_clear(&settings, sizeof(settings));
    if (result == FC_OK) {
        close_settings(presentation);
        snprintf(presentation->message, sizeof(presentation->message), "Provider saved. Type a message to connect.");
    } else report_problem(presentation, "Settings invalid or unavailable; check fields and storage");
    present_stored_response(presentation);
}
static void persist_draft(Presentation *presentation) {
    if (presentation->acceptance || !presentation->composer) return;
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 8) < 0) return;
    jobject editable ← (*environment)->CallObjectMethod(environment, presentation->composer,
        method(environment, presentation->composer, "getText", "()Landroid/text/Editable;"));
    jstring text ← editable ? (*environment)->CallObjectMethod(environment, editable,
        method(environment, editable, "toString", "()Ljava/lang/String;")) : NULL;
    if (!clear_java_failure(presentation, environment) && text) {
        jsize length ← (*environment)->GetStringLength(environment, text);
        if (length <= 4096) {
            jchar units[4096];
            (*environment)->GetStringRegion(environment, text, 0, length, units);
            size_t size ← (size_t)length*sizeof(jchar);
            if (!clear_java_failure(presentation, environment)
                && (size != presentation->draft_checkpoint_size || memcmp(units, presentation->draft_checkpoint, size))) {
                if (private_file_write(presentation->activity->internalDataPath, "composer.draft", units, size) != FC_OK)
                    report_problem(presentation, "Draft could not be saved");
                else {
                    memcpy(presentation->draft_checkpoint, units, size);
                    presentation->draft_checkpoint_size ← size;
                }
            }
        } else report_problem(presentation, "Draft exceeds 4096 UTF-16 units; shorten it to save");
    }
    (*environment)->PopLocalFrame(environment, NULL);
}
