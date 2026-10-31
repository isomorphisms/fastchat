#define _GNU_SOURCE
#include "conversation.h"
#include "fixture_transport.h"
#include "render_policy.h"
#include "transport_boundary.h"
#include "settings.h"
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window_jni.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifndef FC_SOURCE_REVISION
#define FC_SOURCE_REVISION "UNBOUND"
#endif

/* Platform widgets provide the IME. Canvas/Paint consumes a bounded stored
   window. No application DEX or response-sized allocation is required. */
typedef struct {
    ANativeActivity *activity;
    ANativeWindow *window;
    AInputQueue *input;
    ALooper *looper;
    jobject composer;
    jobject composer_manager, composer_layout;
    int composer_attached;
    jobject settings_panel, settings_layout, settings_fields[6];
    int settings_visible, settings_attached, configuration_failure, acceptance;
    uint64_t draft_check_ns;
    jchar draft_checkpoint[4096];
    size_t draft_checkpoint_size;
    int acceptance_pending;
    Conversation conversation;
    FixtureTransport transport;
    TransportBoundary network;
    int http2;
    char model[129], provider_path[256];
    uint64_t viewed_request;
    HistoryTurn selected_turn;
    int timer, running, cancel, failure;
    FixtureScenario fixture_scenario;
    unsigned fixture_step;
    size_t wire_offset, page_count;
    uint64_t previous_pages[64], began_ns;
    unsigned char previous_fences[64], previous_line_starts[64];
    int fenced, line_start, next_fenced, next_line_start, next_page_available;
    uint64_t created_ns, generation_cpu_ns;
    uint64_t viewport, next_viewport, last_barrier_ns, terminal_ns, first_posted_ns;
    float density;
    int width, height, content_bottom;
    float touch_x, touch_y;
    int touch_down;
    char message[160];
} Presentation;

static void show_settings(Presentation *);
static void save_settings(Presentation *);
static void close_settings(Presentation *);
static void attach_settings(Presentation *);
static void detach_settings(Presentation *);
static void persist_draft(Presentation *);

static uint64_t monotonic_ns(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000u + (uint64_t)time.tv_nsec;
}
static uint64_t process_cpu_ns(void) {
    struct timespec time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time);
    return (uint64_t)time.tv_sec * 1000000000u + (uint64_t)time.tv_nsec;
}
static long process_peak_rss_kib(void) {
    FILE *status ← fopen("/proc/self/status", "r");
    if (!status) return -1;
    char line[128];
    long peak ← -1;
    while (fgets(line, sizeof(line), status)) {
        if (sscanf(line, "VmHWM: %ld kB", &peak) == 1) break;
    }
    fclose(status);
    return peak;
}
static void log_measurements(Presentation *presentation) {
    long peak ← process_peak_rss_kib();
    FILE *status ← fopen("/proc/self/statm", "r");
    unsigned long pages, resident;
    long steady ← -1;
    if (status) {
        if (fscanf(status, "%lu %lu", &pages, &resident) == 2)
            steady ← (long)(resident * (unsigned long)sysconf(_SC_PAGESIZE) ÷ 1024);
        fclose(status);
    }
    const StoreMeasurements *measurements ← &presentation->conversation.measurements;
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "source=%s phase=%s peak_rss_kib=%ld steady_rss_kib=%ld data_bytes=%llu journal_bytes=%llu writes=%llu reserve_bytes=%llu reserve_calls=%llu allocation_bytes=%llu allocation_calls=%llu arena_capacity=%llu arena_high_water=%llu max_write=%zu max_view=%zu fixture_buffer_max=%zu generation_cpu_ns=%llu",
        FC_SOURCE_REVISION, phase_name(presentation->conversation.phase), peak, steady,
        (unsigned long long)measurements->data_bytes, (unsigned long long)measurements->journal_bytes,
        (unsigned long long)measurements->write_count,
        (unsigned long long)measurements->reserve_bytes, (unsigned long long)measurements->reserve_calls,
        (unsigned long long)measurements->allocation_bytes, (unsigned long long)measurements->allocation_calls,
        (unsigned long long)presentation->conversation.arena_capacity,
        (unsigned long long)presentation->conversation.arena_high_water,
        measurements->maximum_write, measurements->maximum_view,
        presentation->transport.maximum_buffered, (unsigned long long)(process_cpu_ns() - presentation->generation_cpu_ns));
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "policy_heap_peak=%zu policy_heap_limit=%u", policy_memory_peak(), FC_POLICY_BYTES);
}
static void report_problem(Presentation *presentation, const char *message) {
    snprintf(presentation->message, sizeof(presentation->message), "%s", message);
    __android_log_print(ANDROID_LOG_ERROR, "FastChat", "%s", message);
}
static int clear_java_failure(Presentation *presentation, JNIEnv *environment) {
    if (!(*environment)->ExceptionCheck(environment)) return 0;
    (*environment)->ExceptionDescribe(environment);
    (*environment)->ExceptionClear(environment);
    report_problem(presentation, "Android presentation error; see FastChat log");
    return 1;
}
static jmethodID method(JNIEnv *environment, jobject object, const char *name, const char *signature) {
    jclass type ← (*environment)->GetObjectClass(environment, object);
    jmethodID result ← (*environment)->GetMethodID(environment, type, name, signature);
    (*environment)->DeleteLocalRef(environment, type);
    return result;
}
static void bound_editor(JNIEnv *environment, jobject editor, int length) {
    jclass filter_type ← (*environment)->FindClass(environment, "android/text/InputFilter");
    jclass limit_type ← (*environment)->FindClass(environment, "android/text/InputFilter$LengthFilter");
    jobject limit ← (*environment)->NewObject(environment, limit_type,
        (*environment)->GetMethodID(environment, limit_type, "<init>", "(I)V"), length);
    jobjectArray filters ← (*environment)->NewObjectArray(environment, 1, filter_type, limit);
    (*environment)->CallVoidMethod(environment, editor,
        method(environment, editor, "setFilters", "([Landroid/text/InputFilter;)V"), filters);
    (*environment)->DeleteLocalRef(environment, filters);
    (*environment)->DeleteLocalRef(environment, limit);
    (*environment)->DeleteLocalRef(environment, limit_type);
    (*environment)->DeleteLocalRef(environment, filter_type);
}
static unsigned decode_codepoint(const unsigned char *bytes, size_t *position) {
    unsigned first ← bytes[*position];
    *position ← *position + 1;
    unsigned width ← first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
    unsigned codepoint ← first & (width == 1 ? 0x7f : width == 2 ? 0x1f : width == 3 ? 0x0f : 7);
    for (unsigned part ← 1; part < width; part ← part + 1) {
        codepoint ← (codepoint << 6) | (bytes[*position] & 0x3f);
        *position ← *position + 1;
    }
    return codepoint;
}
static void draw_line(JNIEnv *environment, jobject canvas, jobject paint, const jchar *units,
                       size_t length, float x, float y) {
    jstring text ← (*environment)->NewString(environment, units, (jsize)length);
    (*environment)->CallVoidMethod(environment, canvas,
        method(environment, canvas, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V"),
        text, x, y, paint);
    (*environment)->DeleteLocalRef(environment, text);
}
static size_t draw_bytes(JNIEnv *environment, jobject canvas, jobject paint,
                         const unsigned char *bytes, size_t length, float x, float *y,
                         float line_height, unsigned columns, float bottom,
                         int *fenced, int *logical_line_start) {
    jchar line[256];
    size_t position ← 0;
    while (position < length && *y <= bottom) {
        int heading ← 0;
        if (fenced && *logical_line_start) {
            if (length - position >= 3 && !memcmp(bytes+position, "```", 3)) *fenced ← !*fenced;
            size_t markers ← 0;
            while (markers < 6 && position+markers < length && bytes[position+markers] == '#') markers++;
            heading ← !*fenced && markers && position+markers < length && bytes[position+markers] == ' ';
        }
        (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setFakeBoldText", "(Z)V"), heading ? JNI_TRUE : JNI_FALSE);
        jclass typeface_type ← (*environment)->FindClass(environment, "android/graphics/Typeface");
        const char *face ← fenced && *fenced ? "MONOSPACE" : "DEFAULT";
        jobject typeface ← (*environment)->GetStaticObjectField(environment, typeface_type,
            (*environment)->GetStaticFieldID(environment, typeface_type, face, "Landroid/graphics/Typeface;"));
        jobject prior_face ← (*environment)->CallObjectMethod(environment, paint,
            method(environment, paint, "setTypeface", "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;"), typeface);
        if (prior_face) (*environment)->DeleteLocalRef(environment, prior_face);
        (*environment)->DeleteLocalRef(environment, typeface);
        (*environment)->DeleteLocalRef(environment, typeface_type);
        size_t units ← 0;
        unsigned cells ← 0;
        size_t line_start ← position;
        while (position < length && units < 250) {
            size_t next ← position;
            unsigned codepoint ← decode_codepoint(bytes, &next);
            unsigned width ← codepoint >= 0x1100 ? 2 : 1;
            if (codepoint != '\n' && cells + width > columns && units) break;
            position ← next;
            if (logical_line_start) *logical_line_start ← codepoint == '\n';
            if (codepoint == '\n') break;
            if (codepoint == '\r') continue;
            if (codepoint == '\t') codepoint ← ' ';
            if (codepoint < 0x20) codepoint ← 0xfffd;
            if (codepoint > 0xffff) {
                codepoint ← codepoint - 0x10000;
                line[units] ← (jchar)(0xd800 | (codepoint >> 10));
                units ← units + 1;
                line[units] ← (jchar)(0xdc00 | (codepoint & 0x3ff));
            } else line[units] ← (jchar)codepoint;
            units ← units + 1;
            cells ← cells + width;
        }
        if (position == line_start) break;
        draw_line(environment, canvas, paint, line, units, x, *y);
        *y ← *y + line_height;
    }
    (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setFakeBoldText", "(Z)V"), JNI_FALSE);
    return position;
}
static void draw_label(JNIEnv *environment, jobject canvas, jobject paint, const char *label,
                        float x, float y, float size) {
    (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setTextSize", "(F)V"), size);
    jstring text ← (*environment)->NewStringUTF(environment, label); /* labels are ASCII */
    (*environment)->CallVoidMethod(environment, canvas,
        method(environment, canvas, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V"), text, x, y, paint);
    (*environment)->DeleteLocalRef(environment, text);
}
/* Android's own selectable TextView supplies handles/copy for the current
   bounded disk window. It neither loads nor retains the whole response. */
static void select_page(Presentation *presentation) {
    unsigned char bytes[FC_VIEW_BYTES];
    size_t length ← 0;
    Result result ← presentation->viewed_request
        ? read_history_window(&presentation->conversation, &presentation->selected_turn,
                              presentation->viewport, bytes, sizeof(bytes), &length)
        : render_stored_window(&presentation->conversation, presentation->viewport,
                               bytes, sizeof(bytes), &length);
    if (result != FC_OK || !length) return;
    uint64_t displayed ← presentation->next_viewport - presentation->viewport;
    if (presentation->next_viewport > presentation->viewport && displayed < length) length ← (size_t)displayed;
    int valid;
    if (complete_utf8_prefix(bytes, length, &valid) != length || !valid) {
        report_problem(presentation, "Stored page has invalid UTF-8"); return;
    }
    jchar units[FC_VIEW_BYTES];
    size_t count ← 0;
    for (size_t position ← 0; position < length;) {
        unsigned codepoint ← decode_codepoint(bytes, &position);
        if (codepoint > 0xffff) {
            codepoint ← codepoint - 0x10000;
            units[count++] ← (jchar)(0xd800 | (codepoint >> 10));
            units[count++] ← (jchar)(0xdc00 | (codepoint & 0x3ff));
        } else units[count++] ← (jchar)codepoint;
    }
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 24) < 0) return;
    jclass text_type ← (*environment)->FindClass(environment, "android/widget/TextView");
    jobject text ← (*environment)->NewObject(environment, text_type,
        (*environment)->GetMethodID(environment, text_type, "<init>", "(Landroid/content/Context;)V"), presentation->activity->clazz);
    jstring page ← (*environment)->NewString(environment, units, (jsize)count);
    (*environment)->CallVoidMethod(environment, text, method(environment, text, "setText", "(Ljava/lang/CharSequence;)V"), page);
    (*environment)->CallVoidMethod(environment, text, method(environment, text, "setTextIsSelectable", "(Z)V"), JNI_TRUE);
    (*environment)->CallVoidMethod(environment, text, method(environment, text, "setTextSize", "(F)V"), 18.0);
    (*environment)->CallVoidMethod(environment, text, method(environment, text, "setPadding", "(IIII)V"), 16, 16, 16, 16);
    jclass scroll_type ← (*environment)->FindClass(environment, "android/widget/ScrollView");
    jobject scroll ← (*environment)->NewObject(environment, scroll_type,
        (*environment)->GetMethodID(environment, scroll_type, "<init>", "(Landroid/content/Context;)V"), presentation->activity->clazz);
    (*environment)->CallVoidMethod(environment, scroll, method(environment, scroll, "addView", "(Landroid/view/View;)V"), text);
    jclass builder_type ← (*environment)->FindClass(environment, "android/app/AlertDialog$Builder");
    jobject builder ← (*environment)->NewObject(environment, builder_type,
        (*environment)->GetMethodID(environment, builder_type, "<init>", "(Landroid/content/Context;)V"), presentation->activity->clazz);
    (*environment)->CallObjectMethod(environment, builder, method(environment, builder, "setView", "(Landroid/view/View;)Landroid/app/AlertDialog$Builder;"), scroll);
    jstring title ← (*environment)->NewStringUTF(environment, "Select and copy this stored page");
    (*environment)->CallObjectMethod(environment, builder, method(environment, builder, "setTitle", "(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;"), title);
    jstring done ← (*environment)->NewStringUTF(environment, "Done");
    (*environment)->CallObjectMethod(environment, builder, method(environment, builder, "setPositiveButton", "(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;"), done, NULL);
    (*environment)->CallObjectMethod(environment, builder, method(environment, builder, "show", "()Landroid/app/AlertDialog;"));
    clear_java_failure(presentation, environment);
    (*environment)->PopLocalFrame(environment, NULL);
}
enum { PAGE_BAR_TOP ← 90, PAGE_BAR_HEIGHT ← 48, PAGE_BUTTONS ← 4 };
static int page_button_left(int width, int button) { return width * button ÷ PAGE_BUTTONS; }
static void page_response(Presentation *presentation, int forward) {
    if (!forward) {
        if (presentation->page_count) {
            size_t page ← --presentation->page_count;
            presentation->viewport ← presentation->previous_pages[page];
            presentation->fenced ← presentation->previous_fences[page];
            presentation->line_start ← presentation->previous_line_starts[page];
        } else { presentation->viewport ← 0; presentation->fenced ← 0; presentation->line_start ← 1; }
    } else if (presentation->next_page_available) {
        if (presentation->page_count == 64) {
            memmove(presentation->previous_pages, presentation->previous_pages+1, 63*sizeof(uint64_t));
            memmove(presentation->previous_fences, presentation->previous_fences+1, 63);
            memmove(presentation->previous_line_starts, presentation->previous_line_starts+1, 63);
            presentation->page_count ← 63;
        }
        presentation->previous_pages[presentation->page_count] ← presentation->viewport;
        presentation->previous_fences[presentation->page_count] ← (unsigned char)presentation->fenced;
        presentation->previous_line_starts[presentation->page_count++] ← (unsigned char)presentation->line_start;
        presentation->viewport ← presentation->next_viewport;
        presentation->fenced ← presentation->next_fenced;
        presentation->line_start ← presentation->next_line_start;
    }
}
static void present_stored_response(Presentation *presentation) {
    if (!presentation->window) return;
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 64) < 0) return;
    jobject surface ← ANativeWindow_toSurface(environment, presentation->window);
    jobject canvas ← (*environment)->CallObjectMethod(environment, surface,
        method(environment, surface, "lockCanvas", "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;"), NULL);
    if (clear_java_failure(presentation, environment) || !canvas) {
        (*environment)->PopLocalFrame(environment, NULL);
        return;
    }
    presentation->width ← (*environment)->CallIntMethod(environment, canvas, method(environment, canvas, "getWidth", "()I"));
    presentation->height ← (*environment)->CallIntMethod(environment, canvas, method(environment, canvas, "getHeight", "()I"));
    float density ← presentation->density;
    int bottom ← presentation->content_bottom > 0 && presentation->content_bottom < presentation->height
        ? presentation->content_bottom : presentation->height;
    (*environment)->CallVoidMethod(environment, canvas, method(environment, canvas, "drawColor", "(I)V"), (jint)0xfff6f2e9);
    jclass paint_type ← (*environment)->FindClass(environment, "android/graphics/Paint");
    jobject paint ← (*environment)->NewObject(environment, paint_type,
        (*environment)->GetMethodID(environment, paint_type, "<init>", "(I)V"), 1);
    (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setColor", "(I)V"), (jint)0xff24282b);
    if (presentation->settings_visible) {
        draw_label(environment, canvas, paint, "Provider setup | API access", 16*density, 28*density, 18*density);
        draw_label(environment, canvas, paint, "Use your provider's API key and model name.", 16*density, 52*density, 13*density);
        draw_label(environment, canvas, paint, "Save                         Back", 16*density, 82*density, 18*density);
        if (presentation->message[0]) draw_label(environment, canvas, paint, presentation->message,
            16*density, 106*density, 12*density);
        (*environment)->CallVoidMethod(environment, surface,
            method(environment, surface, "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V"), canvas);
        clear_java_failure(presentation, environment);
        (*environment)->PopLocalFrame(environment, NULL);
        return;
    }
    HistoryTurn selected ← presentation->selected_turn;
    uint64_t selected_request ← presentation->viewed_request ? presentation->viewed_request : presentation->conversation.request;
    Result selected_result ← selected_request ? FC_OK : FC_REJECTED;
    if (selected_request == presentation->conversation.request) {
        selected.prompt_length ← presentation->conversation.prompt_length;
        memcpy(selected.prompt, presentation->conversation.prompt, selected.prompt_length);
        selected.attempt ← presentation->conversation.attempt;
        selected.phase ← presentation->conversation.phase;
        selected.provider ← presentation->conversation.provider;
    }
    char heading[160];
    snprintf(heading, sizeof(heading), "FastChat | %s",
        !selected_request ? (presentation->configuration_failure ? "provider setup required"
            : presentation->http2 ? presentation->model : "test responder (no model configured)")
        : selected.provider.backend == PROVIDER_FIXTURE ? "test responder"
        : selected.provider.backend == PROVIDER_UNRECORDED ? "provider not recorded"
        : selected.provider.model);
    draw_label(environment, canvas, paint, heading, 16*density, 28*density, 14*density);
    draw_label(environment, canvas, paint, phase_name(selected.phase), 16*density, 52*density, 16*density);
    if (!presentation->acceptance) draw_label(environment, canvas, paint, "Setup",
        presentation->width - 76*density, 52*density, 16*density);
    draw_label(environment, canvas, paint, "Older turn       Newer turn       Latest", 16*density, 82*density, 15*density);
    static const char *page_labels[PAGE_BUTTONS] ← { "Previous", "Next", "Start", "Copy" };
    for (int button ← 0; button < PAGE_BUTTONS; button++)
        draw_label(environment, canvas, paint, page_labels[button],
                   page_button_left(presentation->width, button) + 4*density,
                   (PAGE_BAR_TOP + PAGE_BAR_HEIGHT÷2 + 5)*density, 15*density);
    float y ← (PAGE_BAR_TOP + PAGE_BAR_HEIGHT + 20)*density;
    unsigned columns ← (unsigned)((presentation->width - 32*density) ÷ (11*density));
    if (columns < 8) columns ← 8;
    (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setTextSize", "(F)V"), 18*density);
    if (selected_result == FC_OK && selected.prompt_length) {
        draw_bytes(environment, canvas, paint, (const unsigned char *)selected.prompt,
                   selected.prompt_length, 16*density, &y, 24*density, columns, y + 46*density, NULL, NULL);
        y ← y + 12*density;
    }
    unsigned char bytes[FC_VIEW_BYTES];
    size_t length;
    int posted_text ← 0;
    Result read ← selected_result == FC_OK && selected_request != presentation->conversation.request
        ? read_history_window(&presentation->conversation, &selected, presentation->viewport, bytes, sizeof(bytes), &length)
        : render_stored_window(&presentation->conversation, presentation->viewport, bytes, sizeof(bytes), &length);
    presentation->next_page_available ← 0;
    presentation->next_viewport ← presentation->viewport;
    if (read != FC_OK) report_problem(presentation, "Stored text cannot be displayed");
    else if (length) {
        presentation->next_fenced ← presentation->fenced;
        presentation->next_line_start ← presentation->line_start;
        size_t displayed ← draw_bytes(environment, canvas, paint, bytes, length, 16*density, &y,
                                      24*density, columns, bottom - 112*density,
                                      &presentation->next_fenced, &presentation->next_line_start);
        presentation->next_viewport ← presentation->viewport + displayed;
        posted_text ← displayed > 0;
        if (displayed && displayed < length) presentation->next_page_available ← 1;
        else if (displayed && response_page_follows(&presentation->conversation,
                selected_request != presentation->conversation.request ? &selected : NULL,
                presentation->next_viewport, &presentation->next_page_available) != FC_OK)
            report_problem(presentation, "Next stored page cannot be read");
    } else {
        draw_label(environment, canvas, paint, presentation->conversation.phase == UNCERTAIN
            ? "Remote outcome unknown. Retry repeats this request."
            : presentation->http2 ? "Ready for a model response." : "Test responder ready.", 16*density, y, 15*density);
    }
    if (presentation->message[0]) draw_label(environment, canvas, paint, presentation->message,
        16*density, bottom - 86*density, 13*density);
    else if (posted_text) {
        char range[96];
        snprintf(range, sizeof(range), "Bytes %llu-%llu%s",
                 (unsigned long long)(presentation->viewport + 1),
                 (unsigned long long)presentation->next_viewport,
                 presentation->next_page_available ? "" : " | End of stored text");
        draw_label(environment, canvas, paint, range, 16*density, bottom - 86*density, 13*density);
    }
    draw_label(environment, canvas, paint, presentation->running ? "Stop" : presentation->conversation.phase == UNCERTAIN ? "Retry" : "Send",
               presentation->width - 88*density, bottom - 26*density, 19*density);
    (*environment)->CallVoidMethod(environment, surface,
        method(environment, surface, "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V"), canvas);
    int posted ← !clear_java_failure(presentation, environment);
    /* Posting a buffer is an observable proxy, not a physical visible frame. */
    if (posted && posted_text && selected_request == presentation->conversation.request && !presentation->first_posted_ns) {
        presentation->first_posted_ns ← monotonic_ns();
        if (presentation->began_ns) __android_log_print(ANDROID_LOG_INFO, "FastChat", "first_posted_ns=%llu",
            (unsigned long long)(presentation->first_posted_ns - presentation->began_ns));
        if (presentation->began_ns) log_measurements(presentation);
    }
    if (posted && presentation->terminal_ns && presentation->conversation.phase == COMPLETED) {
        __android_log_print(ANDROID_LOG_INFO, "FastChat", "terminal_to_completed_post_ns=%llu request=%llu attempt=%llu",
            (unsigned long long)(monotonic_ns() - presentation->terminal_ns),
            (unsigned long long)presentation->conversation.request, (unsigned long long)presentation->conversation.attempt);
        presentation->terminal_ns ← 0;
        log_measurements(presentation);
    }
    (*environment)->PopLocalFrame(environment, NULL);
}
/* NativeActivity takes the main surface AND input queue. A child of its
   DecorView cannot draw or receive normal widget dispatch. An attached panel
   has its own ViewRoot/surface/input queue; outside touches still reach the
   native reader and Send/Stop targets. It needs no application DEX. */
static void detach_composer(Presentation *presentation) {
    if (!presentation->composer_attached) return;
    JNIEnv *environment ← presentation->activity->env;
    (*environment)->CallVoidMethod(environment, presentation->composer_manager,
        method(environment, presentation->composer_manager, "removeViewImmediate", "(Landroid/view/View;)V"), presentation->composer);
    clear_java_failure(presentation, environment);
    presentation->composer_attached ← 0;
}
static void attach_composer(Presentation *presentation) {
    if (presentation->settings_visible) { attach_settings(presentation); return; }
    if (!presentation->composer || presentation->composer_attached || !presentation->window) return;
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 12) < 0) return;
    jobject activity ← presentation->activity->clazz;
    jobject window ← (*environment)->CallObjectMethod(environment, activity, method(environment, activity, "getWindow", "()Landroid/view/Window;"));
    jobject decor ← (*environment)->CallObjectMethod(environment, window, method(environment, window, "getDecorView", "()Landroid/view/View;"));
    jobject token ← (*environment)->CallObjectMethod(environment, decor, method(environment, decor, "getWindowToken", "()Landroid/os/IBinder;"));
    if (!clear_java_failure(presentation, environment) && token) {
        int width ← (*environment)->CallIntMethod(environment, decor, method(environment, decor, "getWidth", "()I"));
        int available ← width - (int)(104*presentation->density);
        if (available > 0) {
            jclass layout_type ← (*environment)->GetObjectClass(environment, presentation->composer_layout);
            (*environment)->SetObjectField(environment, presentation->composer_layout,
                (*environment)->GetFieldID(environment, layout_type, "token", "Landroid/os/IBinder;"), token);
            (*environment)->SetIntField(environment, presentation->composer_layout,
                (*environment)->GetFieldID(environment, layout_type, "width", "I"), available);
            (*environment)->CallVoidMethod(environment, presentation->composer_manager,
                method(environment, presentation->composer_manager, "addView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"),
                presentation->composer, presentation->composer_layout);
            if (!clear_java_failure(presentation, environment)) {
                presentation->composer_attached ← 1;
                __android_log_print(ANDROID_LOG_INFO, "FastChat", "composer_layer=attached width=%d height=%d", available, (int)(64*presentation->density));
            }
        }
    }
    (*environment)->PopLocalFrame(environment, NULL);
}
static void create_composer(Presentation *presentation) {
    JNIEnv *environment ← presentation->activity->env;
    (*environment)->PushLocalFrame(environment, 32);
    jobject activity ← presentation->activity->clazz;
    jobject resources ← (*environment)->CallObjectMethod(environment, activity,
        method(environment, activity, "getResources", "()Landroid/content/res/Resources;"));
    jobject metrics ← (*environment)->CallObjectMethod(environment, resources,
        method(environment, resources, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;"));
    jclass metrics_type ← (*environment)->GetObjectClass(environment, metrics);
    presentation->density ← (*environment)->GetFloatField(environment, metrics,
        (*environment)->GetFieldID(environment, metrics_type, "density", "F"));
    jclass edit_type ← (*environment)->FindClass(environment, "android/widget/EditText");
    jobject edit ← (*environment)->NewObject(environment, edit_type,
        (*environment)->GetMethodID(environment, edit_type, "<init>", "(Landroid/content/Context;)V"), activity);
    bound_editor(environment, edit, 4096);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setInputType", "(I)V"), 0x20001); /* text + multiline */
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setFocusableInTouchMode", "(Z)V"), JNI_TRUE);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setMinLines", "(I)V"), 1);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setMaxLines", "(I)V"), 3);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setImeOptions", "(I)V"), 0x10000000); /* no fullscreen extraction */
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setTextSize", "(F)V"), 18.0);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setTextColor", "(I)V"), (jint)0xff24282b);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setBackgroundColor", "(I)V"), (jint)0xffe4e6df);
    jstring hint ← (*environment)->NewStringUTF(environment, "Message");
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setHint", "(Ljava/lang/CharSequence;)V"), hint);
    jclass layout_type ← (*environment)->FindClass(environment, "android/view/WindowManager$LayoutParams");
    jobject layout ← (*environment)->NewObject(environment, layout_type,
        (*environment)->GetMethodID(environment, layout_type, "<init>", "(IIIII)V"), 1,
        (int)(64*presentation->density), 1000, 32, -3); /* application panel, NOT_TOUCH_MODAL, translucent */
    (*environment)->SetIntField(environment, layout,
        (*environment)->GetFieldID(environment, layout_type, "gravity", "I"), 83); /* BOTTOM | LEFT */
    (*environment)->SetIntField(environment, layout,
        (*environment)->GetFieldID(environment, layout_type, "softInputMode", "I"), 16); /* ADJUST_RESIZE */
    jobject window ← (*environment)->CallObjectMethod(environment, activity, method(environment, activity, "getWindow", "()Landroid/view/Window;"));
    (*environment)->CallVoidMethod(environment, window, method(environment, window, "setSoftInputMode", "(I)V"), 16);
    jobject manager ← (*environment)->CallObjectMethod(environment, window,
        method(environment, window, "getWindowManager", "()Landroid/view/WindowManager;"));
    if (!clear_java_failure(presentation, environment) && manager) {
        presentation->composer ← (*environment)->NewGlobalRef(environment, edit);
        presentation->composer_layout ← (*environment)->NewGlobalRef(environment, layout);
        presentation->composer_manager ← (*environment)->NewGlobalRef(environment, manager);
    }
    (*environment)->PopLocalFrame(environment, NULL);
}
static Result selected_provider(Presentation *presentation, AttemptProvider *provider) {
    if (presentation->http2)
        return transport_boundary_provider(&presentation->network, presentation->model, presentation->provider_path, provider);
    memset(provider, 0, sizeof(*provider));
    provider->backend ← PROVIDER_FIXTURE;
    return FC_OK;
}
static Result editor_bytes(Presentation *presentation, jobject editor, char *bytes, size_t capacity, size_t *written) {
    *written ← 0;
    JNIEnv *environment ← presentation->activity->env;
    jobject editable ← (*environment)->CallObjectMethod(environment, editor,
        method(environment, editor, "getText", "()Landroid/text/Editable;"));
    if (clear_java_failure(presentation, environment) || !editable) return FC_REJECTED;
    jstring text ← (*environment)->CallObjectMethod(environment, editable, method(environment, editable, "toString", "()Ljava/lang/String;"));
    if (clear_java_failure(presentation, environment) || !text) {
        (*environment)->DeleteLocalRef(environment, editable); return FC_REJECTED;
    }
    const jchar *units ← (*environment)->GetStringChars(environment, text, NULL);
    jsize count ← (*environment)->GetStringLength(environment, text);
    size_t length ← 0;
    Result result ← FC_OK;
    if (!units) result ← FC_REJECTED;
    for (jsize index ← 0; result == FC_OK && index < count; index ← index + 1) {
        unsigned codepoint ← units[index];
        if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
            if (index + 1 >= count || units[index+1] < 0xdc00 || units[index+1] > 0xdfff) { result ← FC_INVALID_TEXT; break; }
            index ← index + 1;
            codepoint ← 0x10000 + ((codepoint - 0xd800) << 10) + units[index] - 0xdc00;
        } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) { result ← FC_INVALID_TEXT; break; }
        unsigned width ← codepoint < 0x80 ? 1 : codepoint < 0x800 ? 2 : codepoint < 0x10000 ? 3 : 4;
        if (length + width > capacity) { result ← FC_REJECTED; break; }
        if (width == 1) bytes[length] ← (char)codepoint;
        else {
            bytes[length] ← (char)((width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (codepoint >> (6*(width-1))));
            for (unsigned part ← 1; part < width; part ← part + 1)
                bytes[length+part] ← (char)(0x80 | ((codepoint >> (6*(width-part-1))) & 0x3f));
        }
        length ← length + width;
    }
    if (units) (*environment)->ReleaseStringChars(environment, text, units);
    (*environment)->DeleteLocalRef(environment, text);
    (*environment)->DeleteLocalRef(environment, editable);
    if (clear_java_failure(presentation, environment)) result ← FC_REJECTED;
    if (result == FC_OK) *written ← length;
    return result;
}
static Result submit_composer(Presentation *presentation) {
    if (!presentation->composer || !presentation->composer_attached) {
        report_problem(presentation, "Message input unavailable"); return FC_REJECTED;
    }
    JNIEnv *environment ← presentation->activity->env;
    char bytes[FC_PROMPT_BYTES];
    size_t length;
    Result result ← editor_bytes(presentation, presentation->composer, bytes, sizeof(bytes), &length);
    if (result == FC_REJECTED) report_problem(presentation, "Message exceeds 1024 UTF-8 bytes");
    if (result == FC_INVALID_TEXT) report_problem(presentation, "Message contains invalid Unicode");
    if (result == FC_OK && !length) {
        report_problem(presentation, "Type a message first"); result ← FC_REJECTED;
    }
    AttemptProvider provider;
    if (result == FC_OK) result ← selected_provider(presentation, &provider);
    if (result == FC_OK) result ← submit_text_to(&presentation->conversation, bytes, length, &provider);
    if (result == FC_OK) {
        jstring empty ← (*environment)->NewStringUTF(environment, "");
        (*environment)->CallVoidMethod(environment, presentation->composer,
            method(environment, presentation->composer, "setText", "(Ljava/lang/CharSequence;)V"), empty);
        (*environment)->DeleteLocalRef(environment, empty);
        persist_draft(presentation);
    }
    return result;
}
static void send_or_cancel(Presentation *presentation) {
    if (presentation->failure || presentation->timer < 0) return;
    if (presentation->configuration_failure || presentation->settings_visible) {
        report_problem(presentation, "Open Setup to configure the provider"); return;
    }
    presentation->message[0] ← 0;
    ComposerAction action;
    AttemptProvider provider;
    if (selected_provider(presentation, &provider) != FC_OK) {
        report_problem(presentation, "Provider profile is invalid"); return;
    }
    if (policy_composer_action(presentation->conversation.phase, presentation->running, &action) != FC_OK) {
        presentation->failure ← 1;
        report_problem(presentation, "Policy unavailable: submission disabled");
        return;
    }
    if (action == POLICY_CANCEL) {
        if (admit_event(&presentation->conversation, presentation->conversation.request,
                         presentation->conversation.attempt, CANCEL_REQUEST) == FC_OK) {
            presentation->cancel ← 1;
            if (presentation->http2 && transport_boundary_cancel(&presentation->network) != FC_OK)
                report_problem(presentation, "Cancellation transport unavailable; outcome remains pending");
        }
    } else if ((action == POLICY_RETRY ? retry_request_to(&presentation->conversation, &provider) :
                action == POLICY_SUBMIT ? submit_composer(presentation) : FC_REJECTED) == FC_OK) {
        presentation->viewport ← 0;
        presentation->fenced ← 0;
        presentation->line_start ← 1;
        presentation->viewed_request ← 0;
        presentation->first_posted_ns ← 0;
        presentation->terminal_ns ← 0;
        presentation->fixture_step ← 0;
        presentation->wire_offset ← 0;
        presentation->page_count ← 0;
        presentation->began_ns ← monotonic_ns();
        presentation->generation_cpu_ns ← process_cpu_ns();
        if (!presentation->http2 && policy_fixture_scenario(presentation->conversation.prompt, presentation->conversation.prompt_length,
                action == POLICY_RETRY, &presentation->fixture_scenario) != FC_OK) {
            admit_event(&presentation->conversation, presentation->conversation.request, presentation->conversation.attempt, TRANSPORT_LOSS);
            presentation->failure ← 1;
            report_problem(presentation, "Policy unavailable: request outcome uncertain");
            return;
        }
        presentation->cancel ← 0;
        presentation->running ← 1;
        presentation->message[0] ← 0;
        presentation->last_barrier_ns ← monotonic_ns();
        fixture_transport_open(&presentation->transport, &presentation->conversation);
        if (presentation->http2 && transport_boundary_send(&presentation->network, presentation->model, presentation->provider_path) != FC_OK) {
            /* Request adaptation refused before fc_submit. It is a known local
               refusal, not an unknown remote outcome and not a retry trigger. */
            admit_event(&presentation->conversation, presentation->conversation.request, presentation->conversation.attempt, FAILURE);
            presentation->running ← 0;
            report_problem(presentation, "Model request not sent: context or profile unavailable");
        }
    } else if (!presentation->message[0]) report_problem(presentation, "Request could not be submitted; see FastChat log");
    present_stored_response(presentation);
}
static int on_input(int descriptor, int events, void *context) {
    (void)descriptor; (void)events;
    Presentation *presentation ← context;
    AInputEvent *event;
    while (AInputQueue_getEvent(presentation->input, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(presentation->input, event)) continue;
        int handled ← 0;
        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
            (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_DOWN) {
            presentation->touch_x ← AMotionEvent_getX(event, 0);
            presentation->touch_y ← AMotionEvent_getY(event, 0);
            presentation->touch_down ← 1;
            int bottom ← presentation->content_bottom > 0 ? presentation->content_bottom : presentation->height;
            handled ← presentation->touch_y >= PAGE_BAR_TOP*presentation->density && presentation->touch_y < bottom - 112*presentation->density;
        }
        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
            (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_CANCEL)
            presentation->touch_down ← 0;
        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION &&
            (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_UP) {
            float x ← AMotionEvent_getX(event, 0), y ← AMotionEvent_getY(event, 0);
            int bottom ← presentation->content_bottom > 0 ? presentation->content_bottom : presentation->height;
            float vertical ← y - presentation->touch_y, horizontal ← x - presentation->touch_x;
            if (presentation->settings_visible) {
                if (y >= 60*presentation->density && y < 94*presentation->density) {
                    if (x < presentation->width÷2) save_settings(presentation);
                    else close_settings(presentation);
                }
                handled ← 1;
            } else if (!presentation->acceptance && y >= 30*presentation->density && y < 60*presentation->density
                && x > presentation->width - 96*presentation->density) {
                show_settings(presentation); handled ← 1;
            } else if (presentation->touch_down && presentation->touch_y >= (PAGE_BAR_TOP + PAGE_BAR_HEIGHT)*presentation->density &&
                presentation->touch_y < bottom - 112*presentation->density &&
                (vertical > 48*presentation->density || vertical < -48*presentation->density) &&
                vertical*vertical > horizontal*horizontal) {
                page_response(presentation, vertical < 0);
                present_stored_response(presentation); handled ← 1;
            } else if (presentation->touch_down &&
                       (vertical*vertical + horizontal*horizontal > 256*presentation->density*presentation->density)) {
                handled ← 1; /* A drag must not activate a toolbar or Send. */
            } else if (y > bottom - 72*presentation->density && x > presentation->width - 104*presentation->density) {
                send_or_cancel(presentation); handled ← 1;
            } else if (y > 60*presentation->density && y < 90*presentation->density) {
                uint64_t selected ← presentation->viewed_request ? presentation->viewed_request : presentation->conversation.request;
                if (x < presentation->width÷3 && selected > 1) presentation->viewed_request ← selected - 1;
                else if (x < 2*presentation->width÷3 && selected < presentation->conversation.request) presentation->viewed_request ← selected + 1;
                else if (x >= 2*presentation->width÷3) presentation->viewed_request ← 0;
                if (presentation->viewed_request && history_turn(&presentation->conversation,
                    presentation->viewed_request, &presentation->selected_turn) != FC_OK) {
                    presentation->viewed_request ← 0;
                    report_problem(presentation, "Stored history cannot be selected");
                }
                presentation->viewport ← 0;
                presentation->fenced ← 0;
                presentation->line_start ← 1;
                presentation->page_count ← 0;
                present_stored_response(presentation); handled ← 1;
            } else if (y >= PAGE_BAR_TOP*presentation->density && y < (PAGE_BAR_TOP + PAGE_BAR_HEIGHT)*presentation->density) {
                if (x < page_button_left(presentation->width, 1)) page_response(presentation, 0);
                else if (x < page_button_left(presentation->width, 2)) page_response(presentation, 1);
                else if (x < page_button_left(presentation->width, 3)) {
                    presentation->viewport ← 0; presentation->page_count ← 0;
                    presentation->fenced ← 0; presentation->line_start ← 1;
                }
                else select_page(presentation);
                present_stored_response(presentation); handled ← 1;
            }
            presentation->touch_down ← 0;
        }
        AInputQueue_finishEvent(presentation->input, event, handled);
    }
    return 1;
}
static int on_tick(int descriptor, int events, void *context) {
    (void)events;
    Presentation *presentation ← context;
    if (presentation->acceptance_pending && presentation->composer_attached) {
        presentation->acceptance_pending ← 0;
        send_or_cancel(presentation);
    }
    uint64_t ticks;
    if (read(descriptor, &ticks, sizeof(ticks)) != sizeof(ticks)) return 1;
    uint64_t draft_now ← monotonic_ns();
    if (draft_now - presentation->draft_check_ns >= 1000000000u) {
        presentation->draft_check_ns ← draft_now;
        persist_draft(presentation);
    }
    if (!presentation->running) return 1;
    Result result ← FC_OK;
    if (presentation->http2) {
        result ← transport_boundary_poll(&presentation->network);
        if (result == FC_BACKPRESSURE) { present_stored_response(presentation); return 1; }
        Phase phase ← presentation->conversation.phase;
        if (phase == COMPLETED || phase == CANCELLED || phase == FAILED || phase == UNCERTAIN) {
            presentation->running ← 0;
            presentation->terminal_ns ← monotonic_ns();
        }
    } else if (presentation->cancel) {
        result ← admit_event(&presentation->conversation, presentation->conversation.request, presentation->conversation.attempt, CANCEL_ACK);
        presentation->running ← 0;
    } else {
        FixturePlan plan;
        result ← policy_fixture_frame(presentation->fixture_step, presentation->fixture_scenario, &plan);
        if (result != FC_OK) {
            presentation->running ← 0;
            fixture_transport_lost(&presentation->transport);
            report_problem(presentation, "Policy stopped; request is not completed");
            return 1;
        }
        if (plan.terminal && !presentation->wire_offset) presentation->terminal_ns ← monotonic_ns();
        WriteResult offered ← fixture_transport_offer(&presentation->transport, plan.bytes + presentation->wire_offset,
                                                       plan.length - presentation->wire_offset);
        presentation->wire_offset ← presentation->wire_offset + offered.consumed;
        result ← offered.result;
        if (result == FC_BACKPRESSURE) { present_stored_response(presentation); return 1; }
        if (presentation->wire_offset != plan.length && result == FC_OK) result ← FC_STORAGE_ERROR;
        if (result == FC_OK) { presentation->fixture_step ← presentation->fixture_step + 1; presentation->wire_offset ← 0; }
        if (plan.terminal && result == FC_OK) presentation->running ← 0;
        if (presentation->fixture_step % 128 == 0) log_measurements(presentation);
    }
    uint64_t now ← monotonic_ns();
    int barrier ← 0;
    if (result == FC_OK) result ← policy_barrier_due(presentation->conversation.phase, now - presentation->last_barrier_ns,
        presentation->conversation.extents.written - presentation->conversation.extents.committed, &barrier);
    if (result == FC_OK && barrier) {
        result ← commit_stored_prefix(&presentation->conversation);
        presentation->last_barrier_ns ← now;
        if (result == FC_OK) __android_log_print(ANDROID_LOG_INFO, "FastChat",
            "source=%s prefix request=%llu attempt=%llu committed=%llu session_ns=%llu",
            FC_SOURCE_REVISION, (unsigned long long)presentation->conversation.request,
            (unsigned long long)presentation->conversation.attempt,
            (unsigned long long)presentation->conversation.extents.committed,
            (unsigned long long)presentation->created_ns);
    }
    if (result != FC_OK) {
        presentation->running ← 0;
        if (result != FC_STORAGE_ERROR) admit_event(&presentation->conversation,
            presentation->conversation.request, presentation->conversation.attempt, TRANSPORT_LOSS);
        report_problem(presentation, "Transport stopped; request is not completed");
    }
    present_stored_response(presentation);
    return 1;
}
static void window_created(ANativeActivity *activity, ANativeWindow *window) {
    Presentation *presentation ← activity->instance;
    presentation->window ← window;
    ANativeWindow_acquire(window);
    attach_composer(presentation);
    present_stored_response(presentation);
}
static void window_destroyed(ANativeActivity *activity, ANativeWindow *window) {
    Presentation *presentation ← activity->instance;
    if (presentation->window == window) { detach_settings(presentation); detach_composer(presentation); ANativeWindow_release(window); presentation->window ← NULL; }
}
static void redraw(ANativeActivity *activity, ANativeWindow *window) {
    (void)window; present_stored_response(activity->instance);
}
static void content_changed(ANativeActivity *activity, const ARect *rectangle) {
    Presentation *presentation ← activity->instance;
    presentation->content_bottom ← rectangle->bottom;
    if (presentation->settings_visible) {
        attach_settings(presentation);
        present_stored_response(presentation); return;
    }
    /* Reattach against the new parent frame after resize/IME/rotation. */
    if (presentation->composer_attached) {
        JNIEnv *environment ← activity->env;
        jclass layout_type ← (*environment)->GetObjectClass(environment, presentation->composer_layout);
        int available ← rectangle->right - rectangle->left - (int)(104*presentation->density);
        if (available > 0) {
            (*environment)->SetIntField(environment, presentation->composer_layout,
                (*environment)->GetFieldID(environment, layout_type, "width", "I"), available);
            (*environment)->CallVoidMethod(environment, presentation->composer_manager,
                method(environment, presentation->composer_manager, "updateViewLayout", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"), presentation->composer, presentation->composer_layout);
        }
        (*environment)->DeleteLocalRef(environment, layout_type);
        clear_java_failure(presentation, environment);
    } else attach_composer(presentation);
    present_stored_response(presentation);
}
static void input_created(ANativeActivity *activity, AInputQueue *queue) {
    Presentation *presentation ← activity->instance;
    presentation->input ← queue;
    AInputQueue_attachLooper(queue, presentation->looper, ALOOPER_POLL_CALLBACK, on_input, presentation);
}
static void input_destroyed(ANativeActivity *activity, AInputQueue *queue) {
    Presentation *presentation ← activity->instance;
    AInputQueue_detachLooper(queue);
    presentation->input ← NULL;
}
static void resumed(ANativeActivity *activity) { attach_composer(activity->instance); }
static void stopped(ANativeActivity *activity) {
    Presentation *presentation ← activity->instance;
    persist_draft(presentation);
    detach_settings(presentation);
    detach_composer(presentation);
}
static void *save_draft(ANativeActivity *activity, size_t *size) {
    Presentation *presentation ← activity->instance;
    *size ← 0;
    if (!presentation->composer) return NULL;
    JNIEnv *environment ← activity->env;
    if ((*environment)->PushLocalFrame(environment, 8) < 0) return NULL;
    jobject editable ← (*environment)->CallObjectMethod(environment, presentation->composer,
        method(environment, presentation->composer, "getText", "()Landroid/text/Editable;"));
    jstring text ← (*environment)->CallObjectMethod(environment, editable, method(environment, editable, "toString", "()Ljava/lang/String;"));
    void *copy ← NULL;
    if (!clear_java_failure(presentation, environment) && text) {
        jsize length ← (*environment)->GetStringLength(environment, text);
        /* Bound saved presentation state separately from the 1024-byte send
           limit. Submitted conversation history remains in the disk store. */
        if (length > 0 && length <= 4096) {
            copy ← malloc((size_t)length*sizeof(jchar));
            if (copy) {
                (*environment)->GetStringRegion(environment, text, 0, length, copy);
                if (clear_java_failure(presentation, environment)) { free(copy); copy ← NULL; }
                else *size ← (size_t)length*sizeof(jchar);
            }
        }
    }
    (*environment)->PopLocalFrame(environment, NULL);
    return copy;
}
static void destroyed(ANativeActivity *activity) {
    Presentation *presentation ← activity->instance;
    detach_settings(presentation);
    detach_composer(presentation);
    if (presentation->http2) transport_boundary_close(&presentation->network);
    else if (presentation->running && !presentation->conversation.poisoned) fixture_transport_lost(&presentation->transport);
    if (presentation->timer >= 0) { ALooper_removeFd(presentation->looper, presentation->timer); close(presentation->timer); }
    if (presentation->composer) (*activity->env)->DeleteGlobalRef(activity->env, presentation->composer);
    if (presentation->composer_layout) (*activity->env)->DeleteGlobalRef(activity->env, presentation->composer_layout);
    if (presentation->composer_manager) (*activity->env)->DeleteGlobalRef(activity->env, presentation->composer_manager);
    if (presentation->settings_panel) (*activity->env)->DeleteGlobalRef(activity->env, presentation->settings_panel);
    if (presentation->settings_layout) (*activity->env)->DeleteGlobalRef(activity->env, presentation->settings_layout);
    for (unsigned index ← 0; index < 6; index++)
        if (presentation->settings_fields[index]) (*activity->env)->DeleteGlobalRef(activity->env, presentation->settings_fields[index]);
    if (presentation->window) ANativeWindow_release(presentation->window);
    conversation_close(&presentation->conversation);
    policy_close();
    free(presentation);
    activity->instance ← NULL;
}
static Result configure_legacy_transport(Presentation *presentation) {
    /* Missing configuration selects the credential-free acceptance backend.
       A present but malformed configuration fails closed; no network fallback. */
    char filename[4096];
    int count ← snprintf(filename, sizeof(filename), "%s/transport.profile.tsv", presentation->activity->internalDataPath);
    if (count < 0 || (size_t)count >= sizeof(filename)) return FC_REJECTED;
    FILE *file ← fopen(filename, "r");
    if (!file) return errno == ENOENT ? FC_OK : FC_REJECTED;
    char origin[2048] ← {0}, ca[2048] ← {0}, line[4096];
    unsigned seen ← 0;
    ProviderRequestFormat format ← PROVIDER_CHAT_COMPLETIONS;
    Result result ← FC_OK;
    while (fgets(line, sizeof(line), file)) {
        size_t length ← strlen(line);
        if (!length || line[length-1] != '\n') { result ← FC_REJECTED; break; }
        line[length-1] ← 0;
        char *value ← strchr(line, '\t');
        if (!value || strchr(value+1, '\t')) { result ← FC_REJECTED; break; }
        *value ← 0; value ← value + 1;
        if (!strcmp(line, "format")) {
            if (seen & 16) { result ← FC_REJECTED; break; }
            if (!strcmp(value, "responses")) format ← PROVIDER_RESPONSES;
            else if (strcmp(value, "chat-completions")) { result ← FC_REJECTED; break; }
            seen ← seen | 16;
            continue;
        }
        char *destination ← NULL;
        size_t capacity ← 0;
        unsigned bit ← 0;
        if (!strcmp(line, "origin")) { destination ← origin; capacity ← sizeof(origin); bit ← 1; }
        if (!strcmp(line, "ca")) { destination ← ca; capacity ← sizeof(ca); bit ← 2; }
        if (!strcmp(line, "model")) { destination ← presentation->model; capacity ← sizeof(presentation->model); bit ← 4; }
        if (!strcmp(line, "path")) { destination ← presentation->provider_path; capacity ← sizeof(presentation->provider_path); bit ← 8; }
        if (!destination || (seen & bit) || !*value || strlen(value) >= capacity) { result ← FC_REJECTED; break; }
        strcpy(destination, value); seen ← seen | bit;
    }
    if (ferror(file)) result ← FC_REJECTED;
    fclose(file);
    if (result != FC_OK || (seen & 15) != 15 || ca[0] != '/' || presentation->provider_path[0] != '/') return FC_REJECTED;
    result ← transport_boundary_open(&presentation->network, &presentation->conversation, origin, ca);
    if (result != FC_OK) return result;
    presentation->network.request_format ← format;
    presentation->http2 ← 1;
    count ← snprintf(filename, sizeof(filename), "%s/authorization.header", presentation->activity->internalDataPath);
    if (count < 0 || (size_t)count >= sizeof(filename)) return FC_REJECTED;
    file ← fopen(filename, "r");
    if (!file) return errno == ENOENT ? FC_OK : FC_REJECTED;
    char header[2048];
    size_t length ← fread(header, 1, sizeof(header)-1, file);
    if (ferror(file) || !feof(file)) result ← FC_REJECTED;
    header[length] ← 0;
    if (result == FC_OK) result ← transport_boundary_authorization(&presentation->network, header);
    memset(header, 0, sizeof(header));
    fclose(file);
    return result;
}
#include "provider_setup.inc"
static int acceptance_intent(Presentation *presentation, char *store, size_t capacity, char *fixture) {
    JNIEnv *environment ← presentation->activity->env;
    if ((*environment)->PushLocalFrame(environment, 12) < 0) return -1;
    jobject activity ← presentation->activity->clazz;
    jobject intent ← (*environment)->CallObjectMethod(environment, activity,
        method(environment, activity, "getIntent", "()Landroid/content/Intent;"));
    if (clear_java_failure(presentation, environment) || !intent) {
        (*environment)->PopLocalFrame(environment, NULL); return -1;
    }
    jstring key ← (*environment)->NewStringUTF(environment, "fastchat_acceptance_store");
    jstring value ← (*environment)->CallObjectMethod(environment, intent,
        method(environment, intent, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;"), key);
    int result ← 0;
    if (value) {
        const char *name ← (*environment)->GetStringUTFChars(environment, value, NULL);
        result ← -1;
        if (name && strlen(name) > 6 && strlen(name) < capacity && !strncmp(name, "fc-m4-", 6)) {
            result ← 1;
            for (const char *part ← name; *part; part++)
                if (!((*part >= 'a' && *part <= 'z') || (*part >= 'A' && *part <= 'Z') ||
                      (*part >= '0' && *part <= '9') || *part == '-')) result ← -1;
            if (result == 1) strcpy(store, name);
        }
        if (name) (*environment)->ReleaseStringUTFChars(environment, value, name);
    }
    key ← (*environment)->NewStringUTF(environment, "fastchat_acceptance_fixture");
    value ← (*environment)->CallObjectMethod(environment, intent,
        method(environment, intent, "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;"), key);
    if (value) {
        const char *name ← (*environment)->GetStringUTFChars(environment, value, NULL);
        if (result != 1 || !name || (strcmp(name, ":long") && strcmp(name, ":lost") && strcmp(name, ":fail"))) result ← -1;
        else strcpy(fixture, name); /* longest accepted literal is six bytes */
        if (name) (*environment)->ReleaseStringUTFChars(environment, value, name);
    }
    if (clear_java_failure(presentation, environment)) result ← -1;
    (*environment)->PopLocalFrame(environment, NULL);
    return result;
}
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity, void *saved, size_t saved_size) {
    Presentation *presentation ← calloc(1, sizeof(*presentation));
    if (!presentation) { ANativeActivity_finish(activity); return; }
    presentation->activity ← activity;
    presentation->timer ← -1;
    presentation->draft_checkpoint_size ← SIZE_MAX;
    presentation->density ← 1;
    presentation->line_start ← 1;
    presentation->conversation.journal ← -1;
    presentation->conversation.response ← -1;
    presentation->created_ns ← monotonic_ns();
    activity->instance ← presentation;
    activity->callbacks->onDestroy ← destroyed;
    activity->callbacks->onResume ← resumed;
    activity->callbacks->onStop ← stopped;
    activity->callbacks->onSaveInstanceState ← save_draft;
    activity->callbacks->onNativeWindowCreated ← window_created;
    activity->callbacks->onNativeWindowDestroyed ← window_destroyed;
    activity->callbacks->onNativeWindowRedrawNeeded ← redraw;
    activity->callbacks->onNativeWindowResized ← redraw;
    activity->callbacks->onContentRectChanged ← content_changed;
    activity->callbacks->onInputQueueCreated ← input_created;
    activity->callbacks->onInputQueueDestroyed ← input_destroyed;
    char path[4096];
    char acceptance_store[80] ← {0}, acceptance_fixture[8] ← {0};
    int acceptance ← acceptance_intent(presentation, acceptance_store, sizeof(acceptance_store), acceptance_fixture);
    presentation->acceptance ← acceptance;
    int count ← snprintf(path, sizeof(path), "%s/%s", activity->internalDataPath,
                         acceptance == 1 ? acceptance_store : "conversation");
    if (acceptance < 0 || count < 0 || (size_t)count >= sizeof(path) || conversation_open(&presentation->conversation, path) != FC_OK || policy_open() != FC_OK) {
        presentation->failure ← 1;
        report_problem(presentation, "Store unavailable: submission disabled");
    }
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "source=%s replay_ns=%llu", FC_SOURCE_REVISION,
        (unsigned long long)(monotonic_ns() - presentation->created_ns));
    /* A named acceptance store always selects the deterministic backend.
       It never opens the ordinary conversation or reads account settings. */
    if (!presentation->failure && !acceptance && configure_transport(presentation) != FC_OK) {
        presentation->configuration_failure ← 1;
        report_problem(presentation, "Explicit transport profile unavailable; submission disabled");
    }
    create_composer(presentation);
    jchar disk_draft[4096];
    size_t disk_draft_size ← 0;
    if (!acceptance && !saved && private_file_read(activity->internalDataPath, "composer.draft",
        disk_draft, sizeof(disk_draft), &disk_draft_size) == FC_OK && disk_draft_size % sizeof(jchar) == 0) {
        saved ← disk_draft; saved_size ← disk_draft_size;
    }
    if (presentation->composer && saved && saved_size && saved_size <= 4096*sizeof(jchar) && saved_size % sizeof(jchar) == 0) {
        JNIEnv *environment ← activity->env;
        jstring draft ← (*environment)->NewString(environment, saved, (jsize)(saved_size ÷ sizeof(jchar)));
        (*environment)->CallVoidMethod(environment, presentation->composer,
            method(environment, presentation->composer, "setText", "(Ljava/lang/CharSequence;)V"), draft);
        (*environment)->DeleteLocalRef(environment, draft);
        clear_java_failure(presentation, environment);
    }
    presentation->looper ← ALooper_forThread();
    if (!presentation->looper) presentation->looper ← ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    presentation->timer ← timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (presentation->timer >= 0) {
        struct itimerspec interval ← { {0, 120000000}, {0, 120000000} };
        timerfd_settime(presentation->timer, 0, &interval, NULL);
        ALooper_addFd(presentation->looper, presentation->timer, ALOOPER_POLL_CALLBACK, ALOOPER_EVENT_INPUT, on_tick, presentation);
    } else report_problem(presentation, "Fixture timer unavailable");
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "native_slice=ready policy=%s recovered_partial=%d",
        renderer_follows_prefixes() ? "streaming" : "disk-first", presentation->conversation.recovered_partial);
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "source=%s store=%s phase=%s request=%llu attempt=%llu committed=%llu session_ns=%llu",
        FC_SOURCE_REVISION, acceptance == 1 ? acceptance_store : "conversation", phase_name(presentation->conversation.phase),
        (unsigned long long)presentation->conversation.request, (unsigned long long)presentation->conversation.attempt,
        (unsigned long long)presentation->conversation.extents.committed, (unsigned long long)presentation->created_ns);
    if (!presentation->failure && presentation->composer && acceptance_fixture[0]) {
        JNIEnv *environment ← activity->env;
        jstring prompt ← (*environment)->NewStringUTF(environment, acceptance_fixture);
        (*environment)->CallVoidMethod(environment, presentation->composer,
            method(environment, presentation->composer, "setText", "(Ljava/lang/CharSequence;)V"), prompt);
        (*environment)->DeleteLocalRef(environment, prompt);
        if (!clear_java_failure(presentation, environment)) presentation->acceptance_pending ← 1;
    }
}
