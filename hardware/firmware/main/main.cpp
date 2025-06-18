
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_lcd_gc9a01.h"
#include "unistd.h"
#include "lvgl.h"

#define LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (240)
#define TEST_LCD_V_RES              (240)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_4)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_5)
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_6)
#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_7)
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_15)

#define TEST_DELAY_TIME_MS          (3000)

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#define EXAMPLE_LVGL_DRAW_BUF_LINES    20 // number of display lines in each draw buffer

static const char *TAG = "gc9a01_test";
static SemaphoreHandle_t refresh_finish = NULL;

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

#define TEST_ASSERT_NOT_NULL(a) assert(a != NULL)
#define TEST_ESP_OK(a) assert(a == ESP_OK)



static lv_obj_t * btn;
static lv_display_rotation_t rotation = LV_DISP_ROTATION_0;

static void btn_cb(lv_event_t * e)
{
//    lv_display_t *disp = lv_event_get_user_data(e);
//    rotation++;
//    if (rotation > LV_DISP_ROTATION_270) {
//        rotation = LV_DISP_ROTATION_0;
//    }
//    lv_disp_set_rotation(disp, rotation);
}
static void set_angle(void * obj, int32_t v)
{
    lv_arc_set_value((lv_obj_t*)obj, v);
}

void example_lvgl_demo_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    btn = lv_button_create(scr);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH" ROTATE");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    /*Button event*/
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, disp);

    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(scr);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);   /*Be sure the knob is not displayed*/
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);  /*To not allow adjusting by click*/
    lv_obj_center(arc);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, set_angle);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);    /*Just for the demo*/
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_start(&a);
}

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    refresh_finish = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(refresh_finish);

    uint16_t row_line = TEST_LCD_V_RES / TEST_LCD_BIT_PER_PIXEL;
    uint8_t byte_per_pixel = TEST_LCD_BIT_PER_PIXEL / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * TEST_LCD_H_RES * byte_per_pixel, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < TEST_LCD_BIT_PER_PIXEL; j++) {
        for (int i = 0; i < row_line * TEST_LCD_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), TEST_LCD_BIT_PER_PIXEL) >> (k * 8)) & 0xff;
            }
        }
        TEST_ESP_OK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, TEST_LCD_H_RES, (j + 1) * row_line, color));
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }
    free(color);
    vSemaphoreDelete(refresh_finish);
}

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        // in case of triggering a task watch dog time out
        time_till_next_ms = (time_till_next_ms > EXAMPLE_LVGL_TASK_MIN_DELAY_MS) ? time_till_next_ms : EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        // in case of lvgl display not ready yet
        time_till_next_ms = (time_till_next_ms < EXAMPLE_LVGL_TASK_MAX_DELAY_MS) ? time_till_next_ms : EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        usleep(1000 * time_till_next_ms);
    }
}

void test_spi()
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
      .mosi_io_num = TEST_PIN_NUM_LCD_DATA0,                                  
      .miso_io_num = -1,                                      
      .sclk_io_num = TEST_PIN_NUM_LCD_PCLK,                            
      .quadwp_io_num = -1,                                    
      .quadhd_io_num = -1,                                    
      .data4_io_num = -1,                                     
      .data5_io_num = -1,                                     
      .data6_io_num = -1,                                     
      .data7_io_num = -1,
      .max_transfer_sz = TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8,
      .flags = 0,
      .isr_cpu_id = INTR_CPU_ID_AUTO,
      .intr_flags = 0
    };
    
    TEST_ESP_OK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
            test_notify_refresh_ready, NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install gc9a01 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_invert_color(panel_handle, true));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, true, false));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));


    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_display_t *display = lv_display_create(TEST_LCD_H_RES, TEST_LCD_V_RES);
    size_t draw_buffer_sz = TEST_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    

    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf1);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);
    ESP_LOGI(TAG, "Install LVGL tick timer");

    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    /* Register done callback */
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);


    _lock_acquire(&lvgl_api_lock);
    example_lvgl_demo_ui(display);
    _lock_release(&lvgl_api_lock);

    //test_draw_bitmap(panel_handle);
    //vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    //TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    //TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    //TEST_ESP_OK(spi_bus_free(LCD_HOST));
}

extern "C" void app_main()
{
  test_spi();
}
