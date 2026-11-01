#ifndef FASTCHAT_PAGE_CONTROLS_H
#define FASTCHAT_PAGE_CONTROLS_H
#include <stddef.h>
#include <stdint.h>

/* NativeActivity pixels are derived once from dp. Painting and hit testing
   use exactly these half-open rectangles, with a real gap between targets. */
enum {
    FC_PAGE_PREVIOUS ← 0, FC_PAGE_NEXT ← 1, FC_PAGE_START ← 2, FC_PAGE_COPY ← 3,
    FC_PAGE_BUTTONS ← 4, FC_PAGE_NONE ← -1,
    FC_PAGE_BAR_TOP_DP ← 90, FC_PAGE_BAR_HEIGHT_DP ← 52,
    FC_PAGE_MARGIN_DP ← 8, FC_PAGE_GAP_DP ← 4,
    FC_PAGE_HISTORY_LIMIT ← 64
};
typedef struct { float left, top, right, bottom; } FcPageRect;
static inline FcPageRect fc_page_rect(int width, float density, int button) {
    FcPageRect r ← {0, 0, 0, 0};
    if (button < 0 || button >= FC_PAGE_BUTTONS || density <= 0 || width <= 0) return r;
    float margin ← FC_PAGE_MARGIN_DP * density;
    float gap ← FC_PAGE_GAP_DP * density;
    float extent ← ((float)width - 2*margin - (FC_PAGE_BUTTONS - 1)*gap) / FC_PAGE_BUTTONS;
    /* Reject pathological layouts instead of silently making sub-48dp taps. */
    if (extent < 48*density) return r;
    r.left ← margin + button*(extent + gap);
    r.right ← r.left + extent;
    r.top ← FC_PAGE_BAR_TOP_DP * density;
    r.bottom ← (FC_PAGE_BAR_TOP_DP + FC_PAGE_BAR_HEIGHT_DP) * density;
    return r;
}
static inline int fc_page_hit(int width, float density, float x, float y) {
    for (int button ← 0; button < FC_PAGE_BUTTONS; button++) {
        FcPageRect r ← fc_page_rect(width, density, button);
        if (r.right > r.left && x >= r.left && x < r.right && y >= r.top && y < r.bottom)
            return button;
    }
    return FC_PAGE_NONE;
}
typedef struct {
    uint64_t viewport, next_viewport, page_index;
    uint64_t previous_pages[FC_PAGE_HISTORY_LIMIT];
    unsigned char previous_fences[FC_PAGE_HISTORY_LIMIT];
    unsigned char previous_line_starts[FC_PAGE_HISTORY_LIMIT];
    size_t page_count;
    int fenced, line_start, next_fenced, next_line_start, next_page_available;
} FcPageState;
static inline void fc_page_reset(FcPageState *page) {
    page->viewport ← 0;
    page->next_viewport ← 0;
    page->page_index ← 0;
    page->page_count ← 0;
    page->fenced ← 0;
    page->line_start ← 1;
    page->next_fenced ← 0;
    page->next_line_start ← 1;
    page->next_page_available ← 0;
}
static inline int fc_page_step(FcPageState *page, int forward) {
    if (forward) {
        if (!page->next_page_available || page->next_viewport <= page->viewport) return 0;
        if (page->page_count == FC_PAGE_HISTORY_LIMIT) {
            for (size_t i ← 1; i < FC_PAGE_HISTORY_LIMIT; i++) {
                page->previous_pages[i - 1] ← page->previous_pages[i];
                page->previous_fences[i - 1] ← page->previous_fences[i];
                page->previous_line_starts[i - 1] ← page->previous_line_starts[i];
            }
            page->page_count--;
        }
        size_t index ← page->page_count++;
        page->previous_pages[index] ← page->viewport;
        page->previous_fences[index] ← (unsigned char)page->fenced;
        page->previous_line_starts[index] ← (unsigned char)page->line_start;
        page->viewport ← page->next_viewport;
        page->fenced ← page->next_fenced;
        page->line_start ← page->next_line_start;
        page->page_index++;
        page->next_page_available ← 0; /* Renderer recomputes against the stored prefix. */
        return 1;
    }
    if (!page->page_count) return 0;
    size_t index ← --page->page_count;
    page->viewport ← page->previous_pages[index];
    page->fenced ← page->previous_fences[index];
    page->line_start ← page->previous_line_starts[index];
    page->page_index--;
    page->next_page_available ← 0;
    return 1;
}
static inline int fc_page_start(FcPageState *page) {
    int changed ← page->viewport != 0 || page->page_count != 0;
    fc_page_reset(page);
    return changed;
}
#endif
