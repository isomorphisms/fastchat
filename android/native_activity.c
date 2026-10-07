#define _GNU_SOURCE
#include "conversation.h"
#include "fixture_transport.h"
#include "render_policy.h"
#include "transport_boundary.h"
#include <android/input.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window_jni.h>
#include <errno.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timerfd.h>
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
    uint64_t created_ns, generation_cpu_ns;
    uint64_t viewport, next_viewport, last_barrier_ns, terminal_ns, first_posted_ns;
    float density;
    int width, height, content_bottom;
    char message[160];
} Presentation;

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
            steady ← (long)(resident * (unsigned long)sysconf(_SC_PAGESIZE) / 1024);
        fclose(status);
    }
    const StoreMeasurements *measurements ← &presentation->conversation.measurements;
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "source=%s phase=%s peak_rss_kib=%ld steady_rss_kib=%ld data_bytes=%llu journal_bytes=%llu writes=%llu reserve_bytes=%llu reserve_calls=%llu arena_capacity=%llu arena_high_water=%llu max_write=%zu max_view=%zu fixture_buffer_max=%zu generation_cpu_ns=%llu",
        FC_SOURCE_REVISION, phase_name(presentation->conversation.phase), peak, steady,
        (unsigned long long)measurements->data_bytes, (unsigned long long)measurements->journal_bytes,
        (unsigned long long)measurements->write_count,
        (unsigned long long)measurements->reserve_bytes, (unsigned long long)measurements->reserve_calls,
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
                         float line_height, unsigned columns, float bottom) {
    jchar line[256];
    size_t position ← 0;
    while (position < length && *y <= bottom) {
        size_t units ← 0;
        unsigned cells ← 0;
        size_t line_start ← position;
        while (position < length && units < 250) {
            size_t next ← position;
            unsigned codepoint ← decode_codepoint(bytes, &next);
            unsigned width ← codepoint >= 0x1100 ? 2 : 1;
            if (codepoint != '\n' && cells + width > columns && units) break;
            position ← next;
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
    HistoryTurn selected ← presentation->selected_turn;
    uint64_t selected_request ← presentation->viewed_request ? presentation->viewed_request : presentation->conversation.request;
    Result selected_result ← selected_request ? FC_OK : FC_REJECTED;
    if (selected_request == presentation->conversation.request) {
        selected.prompt_length ← presentation->conversation.prompt_length;
        memcpy(selected.prompt, presentation->conversation.prompt, selected.prompt_length);
        selected.attempt ← presentation->conversation.attempt;
        selected.phase ← presentation->conversation.phase;
    }
    char heading[160];
    snprintf(heading, sizeof(heading), "FastChat %s | request %llu / attempt %llu",
        renderer_follows_prefixes() ? "live disk" : "disk first",
        (unsigned long long)selected_request, (unsigned long long)selected.attempt);
    draw_label(environment, canvas, paint, heading, 16*density, 28*density, 14*density);
    draw_label(environment, canvas, paint, phase_name(selected.phase), 16*density, 52*density, 16*density);
    draw_label(environment, canvas, paint, "Older turn       Newer turn       Latest", 16*density, 82*density, 15*density);
    draw_label(environment, canvas, paint, "Previous page     Next page     Beginning", 16*density, 106*density, 15*density);
    float y ← 140*density;
    unsigned columns ← (unsigned)((presentation->width - 32*density) / (11*density));
    if (columns < 8) columns ← 8;
    (*environment)->CallVoidMethod(environment, paint, method(environment, paint, "setTextSize", "(F)V"), 18*density);
    if (selected_result == FC_OK && selected.prompt_length) {
        draw_bytes(environment, canvas, paint, (const unsigned char *)selected.prompt,
                   selected.prompt_length, 16*density, &y, 24*density, columns, 186*density);
        y ← y + 12*density;
    }
    unsigned char bytes[FC_VIEW_BYTES];
    size_t length;
    int posted_text ← 0;
    Result read ← selected_result == FC_OK && selected_request != presentation->conversation.request
        ? read_history_window(&presentation->conversation, &selected, presentation->viewport, bytes, sizeof(bytes), &length)
        : render_stored_window(&presentation->conversation, presentation->viewport, bytes, sizeof(bytes), &length);
    if (read != FC_OK) report_problem(presentation, "Stored text cannot be displayed");
    else if (length) {
        size_t displayed ← draw_bytes(environment, canvas, paint, bytes, length, 16*density, &y,
                                      24*density, columns, bottom - 112*density);
        presentation->next_viewport ← presentation->viewport + displayed;
        posted_text ← displayed > 0;
    } else {
        presentation->next_viewport ← presentation->viewport;
        draw_label(environment, canvas, paint, presentation->conversation.phase == UNCERTAIN
            ? "Remote outcome unknown. Send retries this request."
            : "The deterministic fixture is ready.", 16*density, y, 15*density);
    }
    if (presentation->message[0]) draw_label(environment, canvas, paint, presentation->message,
        16*density, bottom - 86*density, 13*density);
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
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setSingleLine", "(Z)V"), JNI_TRUE);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setTextSize", "(F)V"), 18.0);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setTextColor", "(I)V"), (jint)0xff24282b);
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setBackgroundColor", "(I)V"), (jint)0xffe4e6df);
    jstring hint ← (*environment)->NewStringUTF(environment, "Message (:long, :lost, :fail fixtures)");
    (*environment)->CallVoidMethod(environment, edit, method(environment, edit, "setHint", "(Ljava/lang/CharSequence;)V"), hint);
    jclass layout_type ← (*environment)->FindClass(environment, "android/widget/FrameLayout$LayoutParams");
    jobject layout ← (*environment)->NewObject(environment, layout_type,
        (*environment)->GetMethodID(environment, layout_type, "<init>", "(III)V"), -1,
        (int)(64*presentation->density), 80); /* Gravity.BOTTOM */
    (*environment)->SetIntField(environment, layout,
        (*environment)->GetFieldID(environment, layout_type, "rightMargin", "I"), (int)(104*presentation->density));
    jobject window ← (*environment)->CallObjectMethod(environment, activity, method(environment, activity, "getWindow", "()Landroid/view/Window;"));
    (*environment)->CallVoidMethod(environment, window, method(environment, window, "setSoftInputMode", "(I)V"), 16);
    jobject decor ← (*environment)->CallObjectMethod(environment, window, method(environment, window, "getDecorView", "()Landroid/view/View;"));
    (*environment)->CallVoidMethod(environment, decor, method(environment, decor, "addView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"), edit, layout);
    if (!clear_java_failure(presentation, environment)) presentation->composer ← (*environment)->NewGlobalRef(environment, edit);
    (*environment)->PopLocalFrame(environment, NULL);
}
static Result submit_composer(Presentation *presentation) {
    if (!presentation->composer) return FC_REJECTED;
    JNIEnv *environment ← presentation->activity->env;
    jobject editable ← (*environment)->CallObjectMethod(environment, presentation->composer,
        method(environment, presentation->composer, "getText", "()Landroid/text/Editable;"));
    jstring text ← (*environment)->CallObjectMethod(environment, editable, method(environment, editable, "toString", "()Ljava/lang/String;"));
    const jchar *units ← (*environment)->GetStringChars(environment, text, NULL);
    jsize count ← (*environment)->GetStringLength(environment, text);
    char bytes[FC_PROMPT_BYTES];
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
        if (length + width > sizeof(bytes)) { result ← FC_REJECTED; break; }
        if (width == 1) bytes[length] ← (char)codepoint;
        else {
            bytes[length] ← (char)((width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (codepoint >> (6*(width-1))));
            for (unsigned part ← 1; part < width; part ← part + 1)
                bytes[length+part] ← (char)(0x80 | ((codepoint >> (6*(width-part-1))) & 0x3f));
        }
        length ← length + width;
    }
    if (units) (*environment)->ReleaseStringChars(environment, text, units);
    if (result == FC_OK) result ← submit_text(&presentation->conversation, bytes, length);
    (*environment)->DeleteLocalRef(environment, text);
    (*environment)->DeleteLocalRef(environment, editable);
    if (result == FC_OK) {
        jstring empty ← (*environment)->NewStringUTF(environment, "");
        (*environment)->CallVoidMethod(environment, presentation->composer,
            method(environment, presentation->composer, "setText", "(Ljava/lang/CharSequence;)V"), empty);
        (*environment)->DeleteLocalRef(environment, empty);
    }
    return result;
}
static void send_or_cancel(Presentation *presentation) {
    if (presentation->failure || presentation->timer < 0) return;
    ComposerAction action;
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
    } else if ((action == POLICY_RETRY ? retry_request(&presentation->conversation) :
                action == POLICY_SUBMIT ? submit_composer(presentation) : FC_REJECTED) == FC_OK) {
        presentation->viewport ← 0;
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
            admit_event(&presentation->conversation, presentation->conversation.request, presentation->conversation.attempt, TRANSPORT_LOSS);
            presentation->running ← 0;
            report_problem(presentation, "Transport submission unavailable; outcome uncertain");
        }
    } else report_problem(presentation, "Message rejected (empty, too long, or active request)");
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
            (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_UP) {
            float x ← AMotionEvent_getX(event, 0), y ← AMotionEvent_getY(event, 0);
            int bottom ← presentation->content_bottom > 0 ? presentation->content_bottom : presentation->height;
            if (y > bottom - 72*presentation->density && x > presentation->width - 104*presentation->density) {
                send_or_cancel(presentation); handled ← 1;
            } else if (y > 60*presentation->density && y < 90*presentation->density) {
                uint64_t selected ← presentation->viewed_request ? presentation->viewed_request : presentation->conversation.request;
                if (x < presentation->width/3 && selected > 1) presentation->viewed_request ← selected - 1;
                else if (x < 2*presentation->width/3 && selected < presentation->conversation.request) presentation->viewed_request ← selected + 1;
                else if (x >= 2*presentation->width/3) presentation->viewed_request ← 0;
                if (presentation->viewed_request && history_turn(&presentation->conversation,
                    presentation->viewed_request, &presentation->selected_turn) != FC_OK) {
                    presentation->viewed_request ← 0;
                    report_problem(presentation, "Stored history cannot be selected");
                }
                presentation->viewport ← 0;
                presentation->page_count ← 0;
                present_stored_response(presentation); handled ← 1;
            } else if (y >= 90*presentation->density && y < 120*presentation->density) {
                if (x < presentation->width/3) {
                    if (presentation->page_count) {
                        presentation->page_count ← presentation->page_count - 1;
                        presentation->viewport ← presentation->previous_pages[presentation->page_count];
                    } else presentation->viewport ← 0;
                } else if (x < 2*presentation->width/3 && presentation->next_viewport > presentation->viewport) {
                    if (presentation->page_count == 64) {
                        memmove(presentation->previous_pages, presentation->previous_pages+1, 63*sizeof(uint64_t));
                        presentation->page_count ← 63;
                    }
                    presentation->previous_pages[presentation->page_count] ← presentation->viewport;
                    presentation->page_count ← presentation->page_count + 1;
                    presentation->viewport ← presentation->next_viewport;
                } else if (x >= 2*presentation->width/3) { presentation->viewport ← 0; presentation->page_count ← 0; }
                present_stored_response(presentation); handled ← 1;
            }
        }
        AInputQueue_finishEvent(presentation->input, event, handled);
    }
    return 1;
}
static int on_tick(int descriptor, int events, void *context) {
    (void)events;
    Presentation *presentation ← context;
    uint64_t ticks;
    if (read(descriptor, &ticks, sizeof(ticks)) != sizeof(ticks)) return 1;
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
    present_stored_response(presentation);
}
static void window_destroyed(ANativeActivity *activity, ANativeWindow *window) {
    Presentation *presentation ← activity->instance;
    if (presentation->window == window) { ANativeWindow_release(window); presentation->window ← NULL; }
}
static void redraw(ANativeActivity *activity, ANativeWindow *window) {
    (void)window; present_stored_response(activity->instance);
}
static void content_changed(ANativeActivity *activity, const ARect *rectangle) {
    Presentation *presentation ← activity->instance;
    presentation->content_bottom ← rectangle->bottom;
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
static void destroyed(ANativeActivity *activity) {
    Presentation *presentation ← activity->instance;
    if (presentation->http2) transport_boundary_close(&presentation->network);
    else if (presentation->running && !presentation->conversation.poisoned) fixture_transport_lost(&presentation->transport);
    if (presentation->timer >= 0) { ALooper_removeFd(presentation->looper, presentation->timer); close(presentation->timer); }
    if (presentation->composer) (*activity->env)->DeleteGlobalRef(activity->env, presentation->composer);
    if (presentation->window) ANativeWindow_release(presentation->window);
    conversation_close(&presentation->conversation);
    policy_close();
    free(presentation);
    activity->instance ← NULL;
}
static Result configure_transport(Presentation *presentation) {
    /* Missing configuration selects the credential-free acceptance backend.
       A present but malformed configuration fails closed; no network fallback. */
    char filename[4096];
    int count ← snprintf(filename, sizeof(filename), "%s/transport.profile.tsv", presentation->activity->internalDataPath);
    if (count < 0 || (size_t)count >= sizeof(filename)) return FC_REJECTED;
    FILE *file ← fopen(filename, "r");
    if (!file) return errno == ENOENT ? FC_OK : FC_REJECTED;
    char origin[2048] ← {0}, ca[2048] ← {0}, line[4096];
    unsigned seen ← 0;
    Result result ← FC_OK;
    while (fgets(line, sizeof(line), file)) {
        size_t length ← strlen(line);
        if (!length || line[length-1] != '\n') { result ← FC_REJECTED; break; }
        line[length-1] ← 0;
        char *value ← strchr(line, '\t');
        if (!value || strchr(value+1, '\t')) { result ← FC_REJECTED; break; }
        *value ← 0; value ← value + 1;
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
    if (result != FC_OK || seen != 15 || ca[0] != '/' || presentation->provider_path[0] != '/') return FC_REJECTED;
    result ← transport_boundary_open(&presentation->network, &presentation->conversation, origin, ca);
    if (result != FC_OK) return result;
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
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity, void *saved, size_t saved_size) {
    (void)saved; (void)saved_size;
    Presentation *presentation ← calloc(1, sizeof(*presentation));
    if (!presentation) { ANativeActivity_finish(activity); return; }
    presentation->activity ← activity;
    presentation->timer ← -1;
    presentation->density ← 1;
    presentation->conversation.journal ← -1;
    presentation->conversation.response ← -1;
    presentation->created_ns ← monotonic_ns();
    activity->instance ← presentation;
    activity->callbacks->onDestroy ← destroyed;
    activity->callbacks->onNativeWindowCreated ← window_created;
    activity->callbacks->onNativeWindowDestroyed ← window_destroyed;
    activity->callbacks->onNativeWindowRedrawNeeded ← redraw;
    activity->callbacks->onNativeWindowResized ← redraw;
    activity->callbacks->onContentRectChanged ← content_changed;
    activity->callbacks->onInputQueueCreated ← input_created;
    activity->callbacks->onInputQueueDestroyed ← input_destroyed;
    char path[4096];
    int count ← snprintf(path, sizeof(path), "%s/conversation", activity->internalDataPath);
    if (count < 0 || (size_t)count >= sizeof(path) || conversation_open(&presentation->conversation, path) != FC_OK || policy_open() != FC_OK) {
        presentation->failure ← 1;
        report_problem(presentation, "Store unavailable: submission disabled");
    }
    __android_log_print(ANDROID_LOG_INFO, "FastChat", "source=%s replay_ns=%llu", FC_SOURCE_REVISION,
        (unsigned long long)(monotonic_ns() - presentation->created_ns));
    if (!presentation->failure && configure_transport(presentation) != FC_OK) {
        presentation->failure ← 1;
        report_problem(presentation, "Explicit transport profile unavailable; submission disabled");
    }
    create_composer(presentation);
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
}
