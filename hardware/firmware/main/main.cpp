
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_check.h"
#include "unistd.h"

#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lvgl_port.h"

// Code is built upon this example, allowing touch later
// https://github.com/espressif/esp-idf/blob/master/examples/peripherals/lcd/spi_lcd_touch/


// Check out this example to use the esp32 port of LVGL (for speed)
//https://github.com/espressif/esp-bsp/blob/76cc90336b34955fc76b510557b837e963b6a9e9/components/esp_lvgl_port/examples/touchscreen/main/main.c


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

#define EXAMPLE_LVGL_DRAW_BUF_LINES    50 // number of display lines in each draw buffer

static const char *TAG = "gc9a01_test";

#define TEST_ASSERT_NOT_NULL(a) assert(a != NULL)
#define TEST_ESP_OK(a) assert(a == ESP_OK)

static lv_display_t *lvgl_disp = NULL;
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

void example_lvgl_demo_ui()
{
    lv_obj_t *scr = lv_scr_act();

    btn = lv_button_create(scr);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH" ROTATE");
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    /*Button event*/
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, lvgl_disp);

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
      .data_io_default_level = 0,
      .max_transfer_sz = TEST_LCD_H_RES * 80 * TEST_LCD_BIT_PER_PIXEL / 8,
      .flags = 0,
      .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
      .intr_flags = 0
    };
    
    TEST_ESP_OK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config =     {                                                               \
        .cs_gpio_num = TEST_PIN_NUM_LCD_CS,                                          
        .dc_gpio_num = TEST_PIN_NUM_LCD_DC,                                          
        .spi_mode = 0,                                              
        .pclk_hz = 80 * 1000 * 1000,                                
        .trans_queue_depth = 10,                                    
        .on_color_trans_done = NULL,                            
        .user_ctx = NULL,                                   
        .lcd_cmd_bits = 8,                                          
        .lcd_param_bits = 8,                                        
        .cs_ena_pretrans = 0,        /*!< Amount of SPI bit-cycles the cs should be activated before the transmission (0-16) */
        .cs_ena_posttrans = 0,       /*!< Amount of SPI bit-cycles the cs should stay active after the transmission (0-16) */
        .flags = {}                                                 
    };


    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install gc9a01 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
        .flags = {},
        .vendor_config = nullptr,
    };

    TEST_ESP_OK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_invert_color(panel_handle, true));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, true, false));
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    /* Initialize LVGL */
    ESP_LOGI(TAG, "Initialize LVGL library");
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 4096,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };

    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = TEST_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES,
        .double_buffer = true,
        .hres = TEST_LCD_H_RES,
        .vres = TEST_LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Task lock */
    lvgl_port_lock(0);    
    example_lvgl_demo_ui();
    lvgl_port_unlock();

    //TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    //TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    //TEST_ESP_OK(spi_bus_free(LCD_HOST));
}

extern "C" void app_main()
{
  test_spi();
}
