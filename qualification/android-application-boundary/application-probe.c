/* Required Android file + NativeActivity declarations, not a client. */
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/native_activity.h>
#include <android/native_window.h>

int flush_response_bytes(int response_file)
{
    int flush_result ← fsync(response_file);
    return flush_result;
}

int renderer_window_width(ANativeActivity *activity, ANativeWindow *window)
{
    int width ← ANativeWindow_getWidth(window);
    (void)activity;
    return width;
}
