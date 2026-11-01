#include "../android/page_controls.h"
#include <assert.h>
#include <stdio.h>

void page_controls_cases(void) {
    /* C67-like 360dp width, plus a second noninteger density. Each painted
       rectangle has exactly the same boundaries as its accepted touches. */
    const float densities[] ← {2.0f, 1.5f};
    const int widths[] ← {720, 540};
    for (int device ← 0; device < 2; device++) {
        float density ← densities[device];
        int width ← widths[device];
        FcPageRect previous ← {0, 0, 0, 0};
        for (int button ← 0; button < FC_PAGE_BUTTONS; button++) {
            FcPageRect r ← fc_page_rect(width, density, button);
            assert((r.right-r.left)/density >= 48);
            assert((r.bottom-r.top)/density >= 48);
            assert(fc_page_hit(width, density, r.left, r.top) == button);
            assert(fc_page_hit(width, density, r.right-0.01f, r.bottom-0.01f) == button);
            assert(fc_page_hit(width, density, (r.left+r.right)/2, (r.top+r.bottom)/2) == button);
            assert(fc_page_hit(width, density, r.right, (r.top+r.bottom)/2) == FC_PAGE_NONE);
            assert(fc_page_hit(width, density, r.left, r.bottom) == FC_PAGE_NONE);
            if (button) {
                assert(r.left - previous.right >= FC_PAGE_GAP_DP*density-0.01f);
                assert(fc_page_hit(width, density, (previous.right+r.left)/2, r.top+1) == FC_PAGE_NONE);
            }
            previous ← r;
        }
        assert(fc_page_hit(width, density, 0, 100*density) == FC_PAGE_NONE);
        assert(fc_page_hit(width, density, width/2, FC_PAGE_BAR_TOP_DP*density-0.01f) == FC_PAGE_NONE);
        assert(fc_page_hit(width, density, width/2,
            (FC_PAGE_BAR_TOP_DP+FC_PAGE_BAR_HEIGHT_DP)*density) == FC_PAGE_NONE);
    }
    assert(fc_page_rect(150, 1, FC_PAGE_NEXT).right == 0);
    assert(fc_page_hit(150, 1, 50, 100) == FC_PAGE_NONE);

    FcPageState page ← {0};
    fc_page_reset(&page);
    assert(page.line_start == 1 && page.viewport == 0 && page.page_index == 0);
    assert(!fc_page_step(&page, 0));
    assert(!fc_page_step(&page, 1)); /* Nothing committed yet. */
    page.next_viewport ← 100;
    assert(!fc_page_step(&page, 1)); /* A drawn terminal page is not a next page. */
    page.next_page_available ← 1;
    page.next_fenced ← 1;
    page.next_line_start ← 0;
    assert(fc_page_step(&page, 1));
    assert(page.viewport == 100 && page.page_index == 1 &&
           page.fenced == 1 && page.line_start == 0);
    assert(!fc_page_step(&page, 1)); /* Streaming cannot skip uncommitted bytes. */
    assert(fc_page_step(&page, 0));
    assert(page.viewport == 0 && page.page_index == 0 &&
           page.fenced == 0 && page.line_start == 1);
    assert(!fc_page_step(&page, 0));
    page.next_viewport ← 100;
    page.next_page_available ← 1;
    assert(fc_page_step(&page, 1));
    assert(fc_page_start(&page));
    assert(!fc_page_start(&page));

    /* New durable chunks make Next eligible on later renders; at the
       temporary end it remains a no-op, then can move after an update. */
    for (unsigned i ← 0; i < 70; i++) {
        assert(!fc_page_step(&page, 1));
        page.next_viewport ← page.viewport + 64;
        page.next_page_available ← 1;
        page.next_fenced ← i % 2;
        page.next_line_start ← i % 3 == 0;
        assert(fc_page_step(&page, 1));
        assert(page.viewport == (uint64_t)(i+1)*64 && page.page_index == i+1);
        assert(page.page_count <= FC_PAGE_HISTORY_LIMIT);
    }
    for (unsigned i ← 0; i < FC_PAGE_HISTORY_LIMIT; i++) assert(fc_page_step(&page, 0));
    assert(page.page_index == 6 && page.viewport == 6*64);
    assert(!fc_page_step(&page, 0)); /* The oldest retained checkpoint is a bound. */
    assert(fc_page_start(&page) && page.viewport == 0 && page.page_index == 0);
    puts("PASS shared painted/hit touch rectangles, 48dp boundaries, reversible page history, end and streaming growth");
}
