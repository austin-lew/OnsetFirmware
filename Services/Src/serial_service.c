#include "serial_service.h"
#include "freertos_handles.h"
#include <stdbool.h>
#include "usbd_cdc_if.h"
#include "limitswitch_driver.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

typedef enum{
    SERIAL_IDLE,
    SERIAL_RX,
    SERIAL_TX
}serial_state_t;

typedef struct{
    volatile limitswitch_event_t switch2_status;
    volatile limitswitch_event_t switch3_status;
    elbow_to_serial_status_t elbow_status;
}tx_data_t;

tx_data_t tx_data;

typedef struct {
    char buffer[APP_TX_DATA_SIZE];
    uint16_t size;
} tx_buffer_t;

tx_buffer_t tx_buffer;

volatile serial_state_t state = SERIAL_IDLE;
extern USBD_HandleTypeDef hUsbDeviceFS;

static char latest_rx_packet[APP_RX_DATA_SIZE];

static bool usb_tx_busy(void){
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    if (hcdc == NULL) {
        return true;
    }

    return (hcdc->TxState != 0U);
}

static bool rx_buffer_has_data(void){
    return (rx_buffer.head != rx_buffer.tail);
}

static bool pop_rx_byte(uint8_t *byte){
    if (!rx_buffer_has_data()) {
        return false;
    }

    uint16_t tail = rx_buffer.tail;
    *byte = rx_buffer.buffer[tail];
    rx_buffer.tail = (uint16_t)((tail + 1U) % APP_RX_DATA_SIZE);
    return true;
}

static bool consume_latest_rx_packet(char *packet, uint16_t packet_size){
    bool in_frame = false;
    bool found_packet = false;
    uint16_t frame_len = 0;
    uint8_t byte = 0;

    if ((packet == NULL) || (packet_size < 3U)) {
        return false;
    }

    while (pop_rx_byte(&byte)) {
        if (byte == '<') {
            in_frame = true;
            frame_len = 0;
            packet[frame_len++] = '<';
            continue;
        }

        if (!in_frame) {
            continue;
        }

        if (frame_len >= (uint16_t)(packet_size - 1U)) {
            in_frame = false;
            frame_len = 0;
            continue;
        }

        packet[frame_len++] = (char)byte;

        if (byte == '>') {
            packet[frame_len] = '\0';
            found_packet = true;
            in_frame = false;
            frame_len = 0;
        }
    }

    return found_packet;
}

static void parse_rx_packet(char *packet){
    switch(packet[1]){
        case 'H':
            serial_to_elbow_msg_t msg = {
                .command = CMD_HOME,
                .value = 0
            };
            osMessageQueuePut(elbow_to_serialHandle, &msg, 0, 0);
            break;
        case 'M':
            // match format <M,VALUE>
            float value = 0;
            if (sscanf(packet, "<M,%f>", &value) == 1) {
                serial_to_elbow_msg_t msg = {
                    .command = CMD_MOVE,
                    .value = value
                };
                osMessageQueuePut(elbow_to_serialHandle, &msg, 0, 0);
            }
            break;
    }
}

static void limitswitch2_event_callback(limitswitch_event_t event){
    tx_data.switch2_status = event;
}

static void limitswitch3_event_callback(limitswitch_event_t event){
    tx_data.switch3_status = event;
}

limitswitch_config_t limitswitch2_config = {
    .gpio_port = LIMIT_SW_2_GPIO_Port,
    .gpio_pin = LIMIT_SW_2_Pin,
    .pressed_state = GPIO_PIN_RESET,
    .callback = limitswitch2_event_callback
};

limitswitch_config_t limitswitch3_config = {
    .gpio_port = LIMIT_SW_3_GPIO_Port,
    .gpio_pin = LIMIT_SW_3_Pin,
    .pressed_state = GPIO_PIN_RESET,
    .callback = limitswitch3_event_callback
};

static serial_state_t init_serial_service(){
    register_limitswitch(LIMITSWITCH_2, limitswitch2_config);
    register_limitswitch(LIMITSWITCH_3, limitswitch3_config);

    tx_data.switch2_status = (HAL_GPIO_ReadPin(LIMIT_SW_2_GPIO_Port, LIMIT_SW_2_Pin) == limitswitch2_config.pressed_state) ? LIMITSWITCH_PRESSED : LIMITSWITCH_RELEASED;
    tx_data.switch3_status = (HAL_GPIO_ReadPin(LIMIT_SW_3_GPIO_Port, LIMIT_SW_3_Pin) == limitswitch3_config.pressed_state) ? LIMITSWITCH_PRESSED : LIMITSWITCH_RELEASED;
    tx_data.elbow_status = STATUS_NEEDS_HOME;
    return SERIAL_IDLE;
}

static void update_tx_buffer(void){
    limitswitch_event_t switch2_status = tx_data.switch2_status;
    limitswitch_event_t switch3_status = tx_data.switch3_status;
    elbow_to_serial_status_t elbow_status = tx_data.elbow_status;

    char local_buffer[APP_TX_DATA_SIZE];
    int written = snprintf(local_buffer,
                           sizeof(local_buffer),
                           "<%u,%u,%u>",
                           (unsigned int)switch2_status,
                           (unsigned int)switch3_status,
                           (unsigned int)elbow_status);

    if (written <= 0) {
        tx_buffer.size = 0;
        tx_buffer.buffer[0] = '\0';
        return;
    }

    uint16_t payload_len = (written >= (int)sizeof(local_buffer))
        ? (uint16_t)(sizeof(local_buffer) - 1U)
        : (uint16_t)written;

    memcpy((void *)tx_buffer.buffer, local_buffer, (size_t)payload_len + 1U);
    tx_buffer.size = payload_len;
}

static serial_state_t state_machine(serial_state_t state){
    switch(state){
        case SERIAL_IDLE:
            if (rx_buffer_has_data()) {
                return SERIAL_RX;
            }

            if (osMessageQueueGetCount(elbow_to_serialHandle) > 0) {
                osMessageQueueGet(elbow_to_serialHandle, &tx_data.elbow_status, NULL, 0);
                return SERIAL_TX;
            }
            return SERIAL_IDLE;
        case SERIAL_RX:
            if(consume_latest_rx_packet(latest_rx_packet, sizeof(latest_rx_packet)) == false) {
                return SERIAL_IDLE;
            }
            parse_rx_packet(latest_rx_packet);
            return SERIAL_IDLE;
        case SERIAL_TX:
            if (usb_tx_busy()) {
                return SERIAL_TX;
            }

            update_tx_buffer();

            if (tx_buffer.size == 0U) {
                return SERIAL_IDLE;
            }

            if (CDC_Transmit_FS((uint8_t *)(void *)tx_buffer.buffer, tx_buffer.size) != USBD_OK) {
                return SERIAL_TX;
            }
            
            return SERIAL_IDLE;
        default:
            return SERIAL_IDLE;
    }
}

void start_serial_service(void *argument){
    state = init_serial_service();
    while (true) {
        state = state_machine(state);
        osDelay(10);
    }
}
