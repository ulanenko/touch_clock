/*
 * LVGL's allocator (CONFIG_LV_USE_CUSTOM_MALLOC): PSRAM first.
 *
 * Through malloc, everything LVGL allocates under 16 KB (objects, styles, labels' text, draw
 * tasks) came from internal RAM, and LVGL alone kept the internal RAM malloc can use full: the
 * draw threads' stacks, DMA buffers and the network were left the 32 KB reserved for
 * internal-only allocations, about 23 KB of it free. In PSRAM, internal RAM keeps about 90 KB
 * free; frames take 0.3-0.7 ms longer (reading the objects and styles through the cache), the
 * frames that open a page about 1.5 ms. Keeping the blocks a frame makes and drops (its draw
 * tasks) in internal RAM won none of that back. Internal RAM only if PSRAM runs out.
 */

#include <stddef.h>

#include "esp_heap_caps.h"
#include "lvgl.h"

#define LV_MEM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void)
{
}

void lv_mem_deinit(void)
{
}

lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void *lv_malloc_core(size_t size)
{
    void *p = heap_caps_malloc(size, LV_MEM_CAPS);

    return p != NULL ? p : heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    void *q = heap_caps_realloc(p, new_size, LV_MEM_CAPS);

    return q != NULL || new_size == 0 ? q : heap_caps_realloc(p, new_size, MALLOC_CAP_DEFAULT);
}

void lv_free_core(void *p)
{
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    LV_UNUSED(mon_p);
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
