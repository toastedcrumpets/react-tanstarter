
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

#include "esp_lcd_gc9a01.h"

#define TEST_LCD_HOST               SPI2_HOST
#define TEST_LCD_H_RES              (240)
#define TEST_LCD_V_RES              (240)
#define TEST_LCD_BIT_PER_PIXEL      (16)

#define TEST_PIN_NUM_LCD_PCLK       (GPIO_NUM_4)
#define TEST_PIN_NUM_LCD_DATA0      (GPIO_NUM_5)
#define TEST_PIN_NUM_LCD_DC         (GPIO_NUM_6)
#define TEST_PIN_NUM_LCD_CS         (GPIO_NUM_7)
#define TEST_PIN_NUM_LCD_RST        (GPIO_NUM_15)

#define TEST_DELAY_TIME_MS          (3000)

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
    
    TEST_ESP_OK(spi_bus_initialize(TEST_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(TEST_PIN_NUM_LCD_CS, TEST_PIN_NUM_LCD_DC,
            test_notify_refresh_ready, NULL);
    // Attach the LCD to the SPI bus
    TEST_ESP_OK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TEST_LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install gc9a01 panel driver");
    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TEST_PIN_NUM_LCD_RST,
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
#else
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
#endif
        .bits_per_pixel = TEST_LCD_BIT_PER_PIXEL,
    };
    TEST_ESP_OK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    TEST_ESP_OK(esp_lcd_panel_reset(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_init(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_invert_color(panel_handle, true));
    TEST_ESP_OK(esp_lcd_panel_mirror(panel_handle, true, false));
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    TEST_ESP_OK(esp_lcd_panel_disp_off(panel_handle, false));
#else
    TEST_ESP_OK(esp_lcd_panel_disp_on_off(panel_handle, true));
#endif

    test_draw_bitmap(panel_handle);
    vTaskDelay(pdMS_TO_TICKS(TEST_DELAY_TIME_MS));

    TEST_ESP_OK(esp_lcd_panel_del(panel_handle));
    TEST_ESP_OK(esp_lcd_panel_io_del(io_handle));
    TEST_ESP_OK(spi_bus_free(TEST_LCD_HOST));
}

extern "C" void app_main()
{
  test_spi();
}
