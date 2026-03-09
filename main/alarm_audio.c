#include "alarm_audio.h"

#include <string.h>

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

extern const int16_t alarm_pcm_data[];
extern const int32_t ALARM_PCM_SAMPLES;

#define ALARM_SAMPLE_RATE_HZ 44100
#define ALARM_AUDIO_CHUNK_SAMPLES 512
#define ALARM_AUDIO_QUEUE_LEN 8
#define ALARM_AUDIO_TASK_STACK 4096

typedef enum {
    ALARM_AUDIO_CMD_PLAY_ALARM = 0,
    ALARM_AUDIO_CMD_PLAY_TEST,
    ALARM_AUDIO_CMD_STOP,
    ALARM_AUDIO_CMD_SET_VOLUME,
} alarm_audio_cmd_type_t;

typedef enum {
    ALARM_AUDIO_MODE_IDLE = 0,
    ALARM_AUDIO_MODE_ALARM,
    ALARM_AUDIO_MODE_TEST,
} alarm_audio_mode_t;

typedef struct {
    alarm_audio_cmd_type_t type;
    uint8_t volume;
} alarm_audio_cmd_t;

typedef struct {
    QueueHandle_t queue;
    TaskHandle_t task;
    esp_codec_dev_handle_t speaker;
    volatile alarm_audio_mode_t mode;
    volatile uint8_t volume;
    bool initialized;
} alarm_audio_state_t;

static const char *TAG = "alarm_audio";
static alarm_audio_state_t s_alarm_audio = {0};

static uint8_t clamp_volume(uint8_t volume)
{
    return (volume > 100U) ? 100U : volume;
}

static void write_silence(void)
{
    static int16_t silence[ALARM_AUDIO_CHUNK_SAMPLES] = {0};

    if (s_alarm_audio.speaker == NULL) {
        return;
    }

    esp_codec_dev_write(s_alarm_audio.speaker, silence, sizeof(silence));
}

static void apply_volume(uint8_t volume)
{
    if (s_alarm_audio.speaker == NULL) {
        return;
    }

    if (esp_codec_dev_set_out_vol(s_alarm_audio.speaker, volume) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to set alarm volume to %u%%", volume);
    }
}

static void handle_command(const alarm_audio_cmd_t *cmd,
                           alarm_audio_mode_t *mode,
                           size_t *sample_index,
                           uint8_t *volume)
{
    switch (cmd->type) {
    case ALARM_AUDIO_CMD_PLAY_ALARM:
        *volume = clamp_volume(cmd->volume);
        *sample_index = 0;
        *mode = ALARM_AUDIO_MODE_ALARM;
        apply_volume(*volume);
        break;
    case ALARM_AUDIO_CMD_PLAY_TEST:
        *volume = clamp_volume(cmd->volume);
        *sample_index = 0;
        *mode = ALARM_AUDIO_MODE_TEST;
        apply_volume(*volume);
        break;
    case ALARM_AUDIO_CMD_STOP:
        *sample_index = 0;
        *mode = ALARM_AUDIO_MODE_IDLE;
        write_silence();
        break;
    case ALARM_AUDIO_CMD_SET_VOLUME:
        *volume = clamp_volume(cmd->volume);
        apply_volume(*volume);
        break;
    default:
        break;
    }

    s_alarm_audio.volume = *volume;
    s_alarm_audio.mode = *mode;
}

static void alarm_audio_task(void *arg)
{
    alarm_audio_cmd_t cmd;
    alarm_audio_mode_t mode = ALARM_AUDIO_MODE_IDLE;
    size_t sample_index = 0;
    uint8_t volume = s_alarm_audio.volume;
    int16_t chunk[ALARM_AUDIO_CHUNK_SAMPLES];

    (void)arg;

    for (;;) {
        if (mode == ALARM_AUDIO_MODE_IDLE) {
            if (xQueueReceive(s_alarm_audio.queue, &cmd, portMAX_DELAY) == pdTRUE) {
                handle_command(&cmd, &mode, &sample_index, &volume);
            }
            continue;
        }

        while (xQueueReceive(s_alarm_audio.queue, &cmd, 0) == pdTRUE) {
            handle_command(&cmd, &mode, &sample_index, &volume);
        }

        if (mode == ALARM_AUDIO_MODE_IDLE) {
            continue;
        }

        if (sample_index >= (size_t)ALARM_PCM_SAMPLES) {
            if (mode == ALARM_AUDIO_MODE_TEST) {
                mode = ALARM_AUDIO_MODE_IDLE;
                sample_index = 0;
                s_alarm_audio.mode = mode;
                write_silence();
                continue;
            }
            sample_index = 0;
        }

        size_t remaining = (size_t)ALARM_PCM_SAMPLES - sample_index;
        size_t chunk_samples = (remaining > ALARM_AUDIO_CHUNK_SAMPLES) ? ALARM_AUDIO_CHUNK_SAMPLES : remaining;

        memcpy(chunk, &alarm_pcm_data[sample_index], chunk_samples * sizeof(int16_t));
        if (esp_codec_dev_write(s_alarm_audio.speaker, chunk, chunk_samples * sizeof(int16_t)) != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "Alarm audio write failed");
            mode = ALARM_AUDIO_MODE_IDLE;
            sample_index = 0;
            s_alarm_audio.mode = mode;
            write_silence();
            continue;
        }

        sample_index += chunk_samples;
    }
}

static esp_err_t enqueue_command(alarm_audio_cmd_type_t type, uint8_t volume)
{
    alarm_audio_cmd_t cmd = {
        .type = type,
        .volume = clamp_volume(volume),
    };
    alarm_audio_mode_t previous_mode = s_alarm_audio.mode;
    uint8_t previous_volume = s_alarm_audio.volume;

    if (!s_alarm_audio.initialized || s_alarm_audio.queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (type == ALARM_AUDIO_CMD_PLAY_ALARM) {
        s_alarm_audio.mode = ALARM_AUDIO_MODE_ALARM;
    } else if (type == ALARM_AUDIO_CMD_PLAY_TEST) {
        s_alarm_audio.mode = ALARM_AUDIO_MODE_TEST;
    } else if (type == ALARM_AUDIO_CMD_STOP) {
        s_alarm_audio.mode = ALARM_AUDIO_MODE_IDLE;
    } else if (type == ALARM_AUDIO_CMD_SET_VOLUME) {
        s_alarm_audio.volume = cmd.volume;
    }

    if (xQueueSend(s_alarm_audio.queue, &cmd, 0) != pdTRUE) {
        s_alarm_audio.mode = previous_mode;
        s_alarm_audio.volume = previous_volume;
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t alarm_audio_init(uint8_t initial_volume)
{
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
        .sample_rate = ALARM_SAMPLE_RATE_HZ,
        .mclk_multiple = 384,
    };
    i2s_std_config_t i2s_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(ALARM_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = BSP_I2S_MCLK,
            .bclk = BSP_I2S_SCLK,
            .ws = BSP_I2S_LCLK,
            .dout = BSP_I2S_DOUT,
            .din = BSP_I2S_DSIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    esp_err_t err;

    if (s_alarm_audio.initialized) {
        alarm_audio_set_volume(initial_volume);
        return ESP_OK;
    }

    err = bsp_i2c_init();
    if (err != ESP_OK) {
        return err;
    }

    err = bsp_audio_init(&i2s_config);
    if (err != ESP_OK) {
        return err;
    }

    s_alarm_audio.speaker = bsp_audio_codec_speaker_init();
    if (s_alarm_audio.speaker == NULL) {
        return ESP_FAIL;
    }

    if (esp_codec_set_disable_when_closed(s_alarm_audio.speaker, false) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to keep speaker codec active between writes");
    }
    if (esp_codec_dev_open(s_alarm_audio.speaker, &sample_info) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }

    s_alarm_audio.volume = clamp_volume(initial_volume);
    apply_volume(s_alarm_audio.volume);
    s_alarm_audio.queue = xQueueCreate(ALARM_AUDIO_QUEUE_LEN, sizeof(alarm_audio_cmd_t));
    if (s_alarm_audio.queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(alarm_audio_task,
                    "alarm_audio",
                    ALARM_AUDIO_TASK_STACK,
                    NULL,
                    4,
                    &s_alarm_audio.task) != pdPASS) {
        vQueueDelete(s_alarm_audio.queue);
        s_alarm_audio.queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_alarm_audio.initialized = true;
    return ESP_OK;
}

void alarm_audio_set_volume(uint8_t volume)
{
    if (!s_alarm_audio.initialized) {
        return;
    }

    (void)enqueue_command(ALARM_AUDIO_CMD_SET_VOLUME, volume);
}

esp_err_t alarm_audio_start_alarm(uint8_t volume)
{
    return enqueue_command(ALARM_AUDIO_CMD_PLAY_ALARM, volume);
}

esp_err_t alarm_audio_start_test(uint8_t volume)
{
    return enqueue_command(ALARM_AUDIO_CMD_PLAY_TEST, volume);
}

void alarm_audio_stop(void)
{
    if (!s_alarm_audio.initialized) {
        return;
    }

    (void)enqueue_command(ALARM_AUDIO_CMD_STOP, s_alarm_audio.volume);
}

bool alarm_audio_is_alarm_active(void)
{
    return s_alarm_audio.mode == ALARM_AUDIO_MODE_ALARM;
}

bool alarm_audio_is_test_active(void)
{
    return s_alarm_audio.mode == ALARM_AUDIO_MODE_TEST;
}
