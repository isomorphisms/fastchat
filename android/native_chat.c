#define _GNU_SOURCE
#include "conversation.h"
#include "render_policy.h"
#include "fixture_transport.h"
#include <android/input.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    struct android_app *application;
    Conversation conversation;
    FixtureTransport transport;
    char composer[FC_PROMPT_BYTES + 1];
    char presentation_error[160];
    size_t composer_length, fixture_offset;
    unsigned fixture_stage, fixture_repetition;
    uint64_t viewport_offset, next_viewport_offset;
    double next_fragment, last_barrier, terminal_time;
    int dirty, storage_ready, final_receipt;
} ChatApplication;
typedef struct {
    JNIEnv *environment;
    jobject surface, canvas, paint;
    jmethodID draw_color, draw_text, set_color, set_size, break_text, unlock;
    int detach, local_frame;
    float width, height, font, row;
} CanvasFrame;
static ARect content_bounds(struct android_app *application) {
    ARect bounds ← application->contentRect;
    if (bounds.right <= bounds.left || bounds.bottom <= bounds.top)
        bounds ← (ARect){ 0, 0, ANativeWindow_getWidth(application->window), ANativeWindow_getHeight(application->window) };
    return bounds;
}

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}
static int attach_android(struct android_app *application, CanvasFrame *frame) {
    JavaVM *machine ← application->activity->vm;
    jint status ← (*machine)->GetEnv(machine, (void **)&frame->environment, JNI_VERSION_1_6);
    frame->detach ← status == JNI_EDETACHED;
    if (frame->detach) status ← (*machine)->AttachCurrentThread(machine, &frame->environment, NULL);
    return status == JNI_OK;
}
static int android_exception(JNIEnv *environment) {
    if (!(*environment)->ExceptionCheck(environment)) return 0;
    (*environment)->ExceptionDescribe(environment);
    (*environment)->ExceptionClear(environment);
    return 1;
}
static int open_canvas(ChatApplication *chat, CanvasFrame *frame) {
    memset(frame, 0, sizeof(*frame));
    if (!chat->application->window || !attach_android(chat->application, frame)) return 0;
    JNIEnv *environment ← frame->environment;
    if ((*environment)->PushLocalFrame(environment, 64) < 0) return 0;
    frame->local_frame ← 1;
    frame->surface ← ANativeWindow_toSurface(environment, chat->application->window);
    if (!frame->surface) return 0;
    jclass surface_class ← (*environment)->GetObjectClass(environment, frame->surface);
    jmethodID lock ← (*environment)->GetMethodID(environment, surface_class, "lockCanvas", "(Landroid/graphics/Rect;)Landroid/graphics/Canvas;");
    frame->unlock ← (*environment)->GetMethodID(environment, surface_class, "unlockCanvasAndPost", "(Landroid/graphics/Canvas;)V");
    if (!lock || !frame->unlock) return 0;
    frame->canvas ← (*environment)->CallObjectMethod(environment, frame->surface, lock, NULL);
    jclass canvas_class ← (*environment)->FindClass(environment, "android/graphics/Canvas");
    jclass paint_class ← (*environment)->FindClass(environment, "android/graphics/Paint");
    if (!frame->canvas || !canvas_class || !paint_class) return 0;
    jmethodID construct ← (*environment)->GetMethodID(environment, paint_class, "<init>", "(I)V");
    frame->paint ← (*environment)->NewObject(environment, paint_class, construct, 1);
    frame->draw_color ← (*environment)->GetMethodID(environment, canvas_class, "drawColor", "(I)V");
    frame->draw_text ← (*environment)->GetMethodID(environment, canvas_class, "drawText", "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");
    frame->set_color ← (*environment)->GetMethodID(environment, paint_class, "setColor", "(I)V");
    frame->set_size ← (*environment)->GetMethodID(environment, paint_class, "setTextSize", "(F)V");
    frame->break_text ← (*environment)->GetMethodID(environment, paint_class, "breakText", "(Ljava/lang/String;ZF[F)I");
    ARect bounds ← content_bounds(chat->application);
    frame->width ← (float)(bounds.right - bounds.left);
    frame->height ← (float)(bounds.bottom - bounds.top);
    jmethodID translate ← (*environment)->GetMethodID(environment, canvas_class, "translate", "(FF)V");
    if (!translate) return 0;
    (*environment)->CallVoidMethod(environment, frame->canvas, translate, (float)bounds.left, (float)bounds.top);
    frame->font ← frame->width / 25.0f;
    if (frame->font < 20.0f) frame->font ← 20.0f;
    frame->row ← frame->font * 1.6f;
    return frame->paint && frame->draw_text && frame->draw_color && frame->break_text
        && frame->set_color && frame->set_size && !android_exception(environment);
}
static int close_canvas(ChatApplication *chat, CanvasFrame *frame) {
    JNIEnv *environment ← frame->environment;
    if (!environment) return 0;
    int exception ← android_exception(environment);
    if (frame->canvas && frame->surface && frame->unlock)
        (*environment)->CallVoidMethod(environment, frame->surface, frame->unlock, frame->canvas);
    if (android_exception(environment)) exception ← 1;
    if (frame->local_frame) (*environment)->PopLocalFrame(environment, NULL);
    if (frame->detach) (*chat->application->activity->vm)->DetachCurrentThread(chat->application->activity->vm);
    return !exception;
}
/* JNI NewString consumes UTF-16, so supplementary Unicode survives intact.
   NewStringUTF's modified UTF-8 would corrupt the provider's four-byte scalars. */
static size_t utf8_to_utf16(const unsigned char *bytes, size_t length, jchar *characters,
                             size_t capacity, size_t *byte_ends) {
    size_t position ← 0;
    size_t count ← 0;
    while (position < length && count + 2 <= capacity) {
        unsigned first ← bytes[position];
        position ← position + 1;
        unsigned point ← first;
        unsigned continuation ← 0;
        if (first >= 0xf0) { point ← first & 7; continuation ← 3; }
        else if (first >= 0xe0) { point ← first & 15; continuation ← 2; }
        else if (first >= 0xc0) { point ← first & 31; continuation ← 1; }
        while (continuation && position < length) {
            point ← (point << 6) | (bytes[position] & 63);
            position ← position + 1;
            continuation ← continuation - 1;
        }
        if (point > 0xffff) {
            point ← point - 0x10000;
            characters[count] ← (jchar)(0xd800 | (point >> 10));
            byte_ends[count] ← position;
            count ← count + 1;
            characters[count] ← (jchar)(0xdc00 | (point & 1023));
        } else characters[count] ← (jchar)point;
        byte_ends[count] ← position;
        count ← count + 1;
    }
    return count;
}
static void draw_characters(CanvasFrame *frame, const jchar *characters, size_t length, float left, float baseline) {
    JNIEnv *environment ← frame->environment;
    jstring text ← (*environment)->NewString(environment, characters, (jsize)length);
    if (text) {
        (*environment)->CallVoidMethod(environment, frame->canvas, frame->draw_text, text, left, baseline, frame->paint);
        (*environment)->DeleteLocalRef(environment, text);
    }
}
static void draw_label(CanvasFrame *frame, const char *text, float left, float baseline) {
    jchar characters[FC_PROMPT_BYTES + 1];
    size_t byte_ends[FC_PROMPT_BYTES + 1];
    size_t length ← strlen(text);
    if (length > FC_PROMPT_BYTES) length ← FC_PROMPT_BYTES;
    size_t count ← utf8_to_utf16((const unsigned char *)text, length, characters,
                                 sizeof(characters) / sizeof(characters[0]), byte_ends);
    draw_characters(frame, characters, count, left, baseline);
}
static uint64_t draw_response(ChatApplication *chat, CanvasFrame *frame, float bottom) {
    unsigned char bytes[FC_VIEW_BYTES];
    size_t length;
    Result result ← render_stored_window(&chat->conversation, chat->viewport_offset, bytes, sizeof(bytes), &length);
    if (result != FC_OK) { draw_label(frame, "Response text error", 12, frame->row * 5); return chat->viewport_offset; }
    jchar characters[FC_VIEW_BYTES];
    size_t byte_ends[FC_VIEW_BYTES];
    size_t count ← utf8_to_utf16(bytes, length, characters, FC_VIEW_BYTES, byte_ends);
    size_t position ← 0;
    float baseline ← frame->row * 5;
    JNIEnv *environment ← frame->environment;
    while (position < count && baseline < bottom) {
        size_t available ← count - position;
        if (available > 128) available ← 128;
        for (size_t index ← 0; index < available; index ← index + 1)
            if (characters[position + index] == '\n') { available ← index; break; }
        size_t shown ← available;
        if (available) {
            jstring candidate ← (*environment)->NewString(environment, characters + position, (jsize)available);
            if (!candidate) break;
            jint fitting ← (*environment)->CallIntMethod(environment, frame->paint, frame->break_text,
                                                         candidate, JNI_TRUE, frame->width - 24.0f, NULL);
            (*environment)->DeleteLocalRef(environment, candidate);
            if (fitting <= 0) break;
            if ((size_t)fitting < shown) shown ← (size_t)fitting;
            if (shown && characters[position + shown - 1] >= 0xd800
                && characters[position + shown - 1] <= 0xdbff) shown ← shown - 1;
            if (!shown) break;
            draw_characters(frame, characters + position, shown, 12, baseline);
        }
        position ← position + shown;
        if (position < count && characters[position] == '\n') position ← position + 1;
        baseline ← baseline + frame->row;
    }
    return position ? chat->viewport_offset + byte_ends[position - 1] : chat->viewport_offset;
}
static void draw_chat(ChatApplication *chat) {
    CanvasFrame frame;
    if (!open_canvas(chat, &frame)) { close_canvas(chat, &frame); return; }
    JNIEnv *environment ← frame.environment;
    (*environment)->CallVoidMethod(environment, frame.canvas, frame.draw_color, (jint)0xff111820u);
    (*environment)->CallVoidMethod(environment, frame.paint, frame.set_color, (jint)0xfff3f6f8u);
    (*environment)->CallVoidMethod(environment, frame.paint, frame.set_size, frame.font);
    draw_label(&frame, renderer_follows_prefixes() ? "FastChat / stored prefixes" : "FastChat / completed answers", 12, frame.row);
    draw_label(&frame, chat->presentation_error[0] ? chat->presentation_error
        : chat->storage_ready ? phase_name(chat->conversation.phase) : "Local store unavailable", 12, frame.row * 2);
    static const char *actions[] ← { "Cancel", "Loss", "Retry", "Next" };
    for (unsigned action ← 0; action < 4; action ← action + 1)
        draw_label(&frame, actions[action], frame.width * (float)action / 4.0f + 12, frame.row * 3);
    if (chat->storage_ready) draw_label(&frame, chat->conversation.prompt, 12, frame.row * 4);
    float composer_top ← frame.height - frame.row * 6;
    if (chat->storage_ready) chat->next_viewport_offset ← draw_response(chat, &frame, composer_top - frame.row);
    draw_label(&frame, chat->composer_length ? chat->composer : "Type below; Send uses a fake fixture", 12, composer_top);
    static const char *rows[] ← { "qwertyuiop", "asdfghjkl", "zxcvbnm", "0123456789" };
    for (unsigned row ← 0; row < 4; row ← row + 1) {
        size_t keys ← strlen(rows[row]);
        for (size_t key ← 0; key < keys; key ← key + 1) {
            char label[2] ← { rows[row][key], 0 };
            draw_label(&frame, label, frame.width * (float)key / (float)keys + 12,
                       composer_top + frame.row * (float)(row + 1));
        }
    }
    static const char *composer_actions[] ← { "Space", "Delete", "Paste", "Send" };
    for (unsigned action ← 0; action < 4; action ← action + 1)
        draw_label(&frame, composer_actions[action], frame.width * (float)action / 4.0f + 12,
                   frame.height - frame.row / 2);
    if (!close_canvas(chat, &frame)) return;
    chat->dirty ← 0;
    if (chat->terminal_time && chat->conversation.phase == COMPLETED && !chat->final_receipt) {
        __android_log_print(ANDROID_LOG_INFO, "FastChat", "completed_post_ms=%.3f bytes=%llu writes=%llu",
                            (monotonic_seconds() - chat->terminal_time) * 1000.0,
                            (unsigned long long)chat->conversation.extents.committed,
                            (unsigned long long)chat->conversation.measurements.write_count);
        chat->final_receipt ← 1;
    }
}
static void start_fixture(ChatApplication *chat) {
    chat->presentation_error[0] ← 0;
    fixture_transport_open(&chat->transport, &chat->conversation);
    chat->fixture_stage ← 0;
    chat->fixture_repetition ← 0;
    chat->fixture_offset ← 0;
    chat->viewport_offset ← 0;
    chat->terminal_time ← 0;
    chat->final_receipt ← 0;
    chat->next_fragment ← monotonic_seconds();
    chat->last_barrier ← chat->next_fragment;
}
static void tick_fixture(ChatApplication *chat) {
    if (!chat->storage_ready || chat->fixture_stage >= 3 || chat->conversation.phase == UNCERTAIN
        || chat->conversation.phase == CANCELLED || chat->conversation.phase == FAILED) return;
    double now ← monotonic_seconds();
    if (now < chat->next_fragment) return;
    if (chat->conversation.phase == CANCEL_PENDING) {
        admit_event(&chat->conversation, chat->conversation.request, chat->conversation.attempt, CANCEL_ACK);
        chat->fixture_stage ← 3; chat->dirty ← 1; return;
    }
    static const char *frames[] ← {
        "data: {\"type\":\"start\"}\n\n",
        "data: {\"text\":\"Stored bytes arrive first. This is a deterministic fixture. \\u20ac \\uD83D\\uDE42\\n**Plain Markdown** and ```code``` remain exact text.\\n\\n\"}\n\n",
        "data: [DONE]\n\n"
    };
    const char *frame ← frames[chat->fixture_stage];
    size_t remaining ← strlen(frame) - chat->fixture_offset;
    size_t amount ← remaining > 37 ? 37 : remaining;
    uint64_t previous ← chat->conversation.extents.committed;
    if (chat->fixture_stage == 2 && !chat->terminal_time) chat->terminal_time ← now;
    WriteResult received ← fixture_transport_feed(&chat->transport, frame + chat->fixture_offset, amount);
    chat->fixture_offset ← chat->fixture_offset + received.consumed;
    if (received.result != FC_OK && received.result != FC_BACKPRESSURE) {
        if (received.result == FC_STORAGE_ERROR)
            snprintf(chat->presentation_error, sizeof(chat->presentation_error), "Store failed: %s; restart to replay", strerror(errno));
        else {
            fixture_transport_finish(&chat->transport);
            snprintf(chat->presentation_error, sizeof(chat->presentation_error), "Fixture framing failed; delivery uncertain");
        }
        chat->fixture_stage ← 3; chat->dirty ← 1; return;
    }
    if (chat->fixture_offset == strlen(frame)) {
        chat->fixture_offset ← 0;
        if (chat->fixture_stage == 1) chat->fixture_repetition ← chat->fixture_repetition + 1;
        if (chat->fixture_stage != 1 || chat->fixture_repetition == 48) chat->fixture_stage ← chat->fixture_stage + 1;
    }
    /* A shared timed barrier batches small fixtures in both visibility branches. */
    if (now - chat->last_barrier >= 0.1 && chat->conversation.phase == GENERATING) {
        if (commit_stored_prefix(&chat->conversation) != FC_OK) {
            snprintf(chat->presentation_error, sizeof(chat->presentation_error), "Store barrier failed: %s", strerror(errno));
            chat->fixture_stage ← 3; chat->dirty ← 1; return;
        }
        chat->last_barrier ← now;
    }
    if (chat->conversation.extents.committed != previous || chat->fixture_stage >= 3) chat->dirty ← 1;
    chat->next_fragment ← now + 0.02;
}
static void paste_clipboard(ChatApplication *chat) {
    CanvasFrame session;
    memset(&session, 0, sizeof(session));
    if (!attach_android(chat->application, &session)) return;
    JNIEnv *environment ← session.environment;
    if ((*environment)->PushLocalFrame(environment, 32) < 0) { close_canvas(chat, &session); return; }
    session.local_frame ← 1;
    jobject activity ← chat->application->activity->clazz;
    jclass activity_class ← (*environment)->GetObjectClass(environment, activity);
    jmethodID service_method ← (*environment)->GetMethodID(environment, activity_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring service_name ← (*environment)->NewStringUTF(environment, "clipboard");
    jobject service ← service_method ? (*environment)->CallObjectMethod(environment, activity, service_method, service_name) : NULL;
    jobject item ← NULL;
    if (service) {
        jclass service_class ← (*environment)->GetObjectClass(environment, service);
        jmethodID primary ← (*environment)->GetMethodID(environment, service_class, "getPrimaryClip", "()Landroid/content/ClipData;");
        jobject clip ← primary ? (*environment)->CallObjectMethod(environment, service, primary) : NULL;
        if (clip) {
            jclass clip_class ← (*environment)->GetObjectClass(environment, clip);
            jmethodID count ← (*environment)->GetMethodID(environment, clip_class, "getItemCount", "()I");
            jmethodID at ← (*environment)->GetMethodID(environment, clip_class, "getItemAt", "(I)Landroid/content/ClipData$Item;");
            if (count && at && (*environment)->CallIntMethod(environment, clip, count) > 0)
                item ← (*environment)->CallObjectMethod(environment, clip, at, 0);
        }
    }
    jstring text ← NULL;
    if (item) {
        jclass item_class ← (*environment)->GetObjectClass(environment, item);
        jmethodID coerce ← (*environment)->GetMethodID(environment, item_class, "coerceToText", "(Landroid/content/Context;)Ljava/lang/CharSequence;");
        jobject sequence ← coerce ? (*environment)->CallObjectMethod(environment, item, coerce, activity) : NULL;
        if (sequence) {
            jclass sequence_class ← (*environment)->GetObjectClass(environment, sequence);
            jmethodID as_string ← (*environment)->GetMethodID(environment, sequence_class, "toString", "()Ljava/lang/String;");
            if (as_string) text ← (*environment)->CallObjectMethod(environment, sequence, as_string);
        }
    }
    unsigned char pasted[FC_PROMPT_BYTES];
    size_t pasted_length ← 0;
    if (text && !android_exception(environment)) {
        jchar characters[FC_PROMPT_BYTES];
        jsize count ← (*environment)->GetStringLength(environment, text);
        if (count > FC_PROMPT_BYTES) count ← FC_PROMPT_BYTES;
        (*environment)->GetStringRegion(environment, text, 0, count, characters);
        for (jsize index ← 0; index < count; index ← index + 1) {
            unsigned point ← characters[index];
            if (point >= 0xd800 && point <= 0xdbff) {
                if (index + 1 == count) break;
                unsigned low ← characters[index + 1];
                if (low < 0xdc00 || low > 0xdfff) break;
                point ← 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                index ← index + 1;
            }
            unsigned char encoded[4];
            size_t length ← encode_utf8_scalar(point, encoded);
            if (!length || length > FC_PROMPT_BYTES - chat->composer_length - pasted_length) break;
            memcpy(pasted + pasted_length, encoded, length);
            pasted_length ← pasted_length + length;
        }
        if (!android_exception(environment)) {
            memcpy(chat->composer + chat->composer_length, pasted, pasted_length);
            chat->composer_length ← chat->composer_length + pasted_length;
            chat->composer[chat->composer_length] ← 0;
        }
    } else snprintf(chat->presentation_error, sizeof(chat->presentation_error), "Clipboard text unavailable");
    close_canvas(chat, &session);
}
static int32_t composer_touch(struct android_app *application, AInputEvent *event) {
    ChatApplication *chat ← application->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION
        || (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK) != AMOTION_EVENT_ACTION_UP) return 0;
    if (!application->window) return 0;
    ARect bounds ← content_bounds(application);
    float width ← (float)(bounds.right - bounds.left);
    float height ← (float)(bounds.bottom - bounds.top);
    float font ← width / 25.0f;
    if (font < 20) font ← 20;
    float row ← font * 1.6f;
    float x ← AMotionEvent_getX(event, 0) - (float)bounds.left;
    float y ← AMotionEvent_getY(event, 0) - (float)bounds.top;
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    if (y < row * 3.5f && y >= row * 2.2f && chat->storage_ready) {
        unsigned action ← (unsigned)(x * 4 / width);
        if (action == 0) admit_event(&chat->conversation, chat->conversation.request, chat->conversation.attempt, CANCEL_REQUEST);
        if (action == 1) admit_event(&chat->conversation, chat->conversation.request, chat->conversation.attempt, TRANSPORT_LOSS);
        if (action == 2 && retry_request(&chat->conversation) == FC_OK) start_fixture(chat);
        if (action == 3) chat->viewport_offset ← chat->next_viewport_offset > chat->viewport_offset
            && chat->next_viewport_offset < chat->conversation.extents.eligible ? chat->next_viewport_offset : 0;
    } else if (y >= height - row * 1.5f) {
        unsigned action ← (unsigned)(x * 4 / width);
        if (action == 0 && chat->composer_length < FC_PROMPT_BYTES) {
            chat->composer[chat->composer_length] ← ' ';
            chat->composer_length ← chat->composer_length + 1;
        }
        if (action == 1 && chat->composer_length) {
            int valid;
            chat->composer_length ← complete_utf8_prefix((const unsigned char *)chat->composer,
                                                         chat->composer_length - 1, &valid);
        }
        if (action == 2) paste_clipboard(chat);
        if (action == 3 && chat->storage_ready
            && submit_text(&chat->conversation, chat->composer, chat->composer_length) == FC_OK) {
            chat->composer_length ← 0; start_fixture(chat);
        }
        chat->composer[chat->composer_length] ← 0;
    } else if (y >= height - row * 5.5f) {
        static const char *rows[] ← { "qwertyuiop", "asdfghjkl", "zxcvbnm", "0123456789" };
        int index ← (int)((y - (height - row * 5.5f)) / row);
        if (index >= 0 && index < 4) {
            size_t count ← strlen(rows[index]);
            size_t key ← (size_t)(x * (float)count / width);
            if (key < count && chat->composer_length < FC_PROMPT_BYTES) {
                chat->composer[chat->composer_length] ← rows[index][key];
                chat->composer_length ← chat->composer_length + 1;
                chat->composer[chat->composer_length] ← 0;
            }
        }
    }
    chat->dirty ← 1;
    return 1;
}
static void window_changed(struct android_app *application, int32_t command) {
    ChatApplication *chat ← application->userData;
    if (command == APP_CMD_INIT_WINDOW || command == APP_CMD_WINDOW_RESIZED
        || command == APP_CMD_GAINED_FOCUS || command == APP_CMD_CONTENT_RECT_CHANGED) chat->dirty ← 1;
}
void android_main(struct android_app *application) {
    app_dummy();
    ChatApplication chat;
    memset(&chat, 0, sizeof(chat));
    chat.application ← application;
    chat.fixture_stage ← 3;
    application->userData ← &chat;
    application->onAppCmd ← window_changed;
    application->onInputEvent ← composer_touch;
    char directory[4096];
    int length ← snprintf(directory, sizeof(directory), "%s/conversation", application->activity->internalDataPath);
    if (length > 0 && (size_t)length < sizeof(directory)) chat.storage_ready ← conversation_open(&chat.conversation, directory) == FC_OK;
    chat.dirty ← 1;
    while (!application->destroyRequested) {
        struct android_poll_source *source;
        int events;
        int identifier ← ALooper_pollOnce(20, NULL, &events, (void **)&source);
        if (identifier >= 0 && source) source->process(application, source);
        tick_fixture(&chat);
        if (chat.dirty && application->window) draw_chat(&chat);
    }
    if (chat.storage_ready) conversation_close(&chat.conversation);
}
