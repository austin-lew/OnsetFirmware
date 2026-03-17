#include "serial_service.h"
#include "freertos_handles.h"
#include <stdbool.h>
#include "usbd_cdc_if.h"
#include "limitswitch_driver.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define WELCOME_MSG "[Onset] Ready\r\n"

typedef enum
{
    SERIAL_IDLE,
    SERIAL_RX,
    SERIAL_TX
} serial_state_t;

typedef struct
{
    volatile limitswitch_event_t switch2_status;
    volatile limitswitch_event_t switch3_status;
    elbow_to_serial_status_t elbow_status;
    precharge_to_serial_status_t precharge_status;
    led_to_serial_status_t led_status;
    loader_to_serial_status_t loader_status;
} tx_data_t;

tx_data_t tx_data;

typedef struct
{
    char buffer[APP_TX_DATA_SIZE];
    uint16_t size;
} tx_buffer_t;

tx_buffer_t tx_buffer;

volatile serial_state_t state = SERIAL_IDLE;
extern USBD_HandleTypeDef hUsbDeviceFS;

static char latest_rx_packet[APP_RX_DATA_SIZE];
static volatile uint32_t limitswitch_change_counter = 0U;
static uint32_t last_transmitted_limitswitch_change_counter = 0U;

/**
 * @brief Returns whether a USB CDC TX transfer is currently in flight.
 *
 * @return true when USB CDC cannot accept a new transmit request.
 */
static bool usb_tx_busy(void)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

    if (hcdc == NULL)
    {
        return true;
    }

    return (hcdc->TxState != 0U);
}

/**
 * @brief Checks whether unread bytes are available in the USB RX ring buffer.
 *
 * @return true when head and tail differ.
 */
static bool rx_buffer_has_data(void)
{
    return (rx_buffer.head != rx_buffer.tail);
}

/**
 * @brief Pops one byte from the USB RX ring buffer.
 *
 * @param byte Destination for the popped byte.
 * @return true when a byte was read.
 */
static bool pop_rx_byte(uint8_t *byte)
{
    if (!rx_buffer_has_data())
    {
        return false;
    }

    uint16_t tail = rx_buffer.tail;
    *byte = rx_buffer.buffer[tail];
    rx_buffer.tail = (uint16_t)((tail + 1U) % APP_RX_DATA_SIZE);
    return true;
}

/**
 * @brief Drains RX bytes and keeps the most recent complete <...> frame.
 *
 * @param packet Output buffer for the latest complete frame.
 * @param packet_size Size of @p packet in bytes.
 * @return true when at least one complete frame was found.
 */
static bool consume_latest_rx_packet(char *packet, uint16_t packet_size)
{
    bool in_frame = false;
    bool found_packet = false;
    uint16_t frame_len = 0;
    uint8_t byte = 0;

    if ((packet == NULL) || (packet_size < 3U))
    {
        return false;
    }

    while (pop_rx_byte(&byte))
    {
        if (byte == '<')
        {
            in_frame = true;
            frame_len = 0;
            packet[frame_len++] = '<';
            continue;
        }

        if (!in_frame)
        {
            continue;
        }

        if (frame_len >= (uint16_t)(packet_size - 1U))
        {
            in_frame = false;
            frame_len = 0;
            continue;
        }

        // capitalize all letters
        if (isalpha(byte))
        {
            byte = (uint8_t)toupper((int)byte);
        }

        packet[frame_len++] = (char)byte;

        if (byte == '>')
        {
            packet[frame_len] = '\0';
            found_packet = true;
            in_frame = false;
            frame_len = 0;
        }
    }

    return found_packet;
}

/**
 * @brief Parses a framed command and forwards it to the elbow service queue.
 *
 * @param packet Null-terminated frame in the form <...>.
 */
static serial_state_t parse_rx_packet(char *packet)
{
    switch (packet[1])
    {
    case 'S':
    {
        return SERIAL_TX;
    }
    case 'H':
    {
        serial_to_elbow_msg_t msg = {
            .command = CMD_SERIAL_ELBOW_HOME,
            .value = 0};
        osMessageQueuePut(serial_to_elbowHandle, &msg, 0, 0);
        return SERIAL_IDLE;
    }
    case 'M':
    {
        float value = 0;
        if (sscanf(packet, "<M,%f>", &value) == 1)
        {
            serial_to_elbow_msg_t msg = {
                .command = CMD_SERIAL_ELBOW_MOVE,
                .value = value};
            osMessageQueuePut(serial_to_elbowHandle, &msg, 0, 0);
        }
        return SERIAL_IDLE;
    }
    case 'P':
    {
        int value = 0;
        if (sscanf(packet, "<P,%d>", &value) == 1)
        {
            serial_to_precharge_msg_t msg = {
                .command = (value == 0) ? CMD_SERIAL_PRECHARGE_OFF : CMD_SERIAL_PRECHARGE_ON};
            osMessageQueuePut(serial_to_prechargeHandle, &msg, 0, 0);
        }
        return SERIAL_IDLE;
    }
    case 'L':
    {
        int value = 0;
        if (sscanf(packet, "<L,%d>", &value) == 1)
        {
            serial_to_led_msg_t msg = {.command = CMD_SERIAL_LED_OFF, .r = 0U, .g = 0U, .b = 0U};
            if (value == (int)CMD_SERIAL_LED_LAUNCH)
            {
                msg.command = CMD_SERIAL_LED_LAUNCH;
            }
            else if (value == (int)CMD_SERIAL_LED_SINGLE_COLOUR)
            {
                int r = 0, g = 0, b = 0;
                if (sscanf(packet, "<L,%d,%d,%d,%d>", &value, &r, &g, &b) == 4)
                {
                    msg.command = CMD_SERIAL_LED_SINGLE_COLOUR;
                    msg.r = (uint8_t)r;
                    msg.g = (uint8_t)g;
                    msg.b = (uint8_t)b;
                }
            }
            osMessageQueuePut(serial_to_ledHandle, &msg, 0, 0);
        }
        return SERIAL_IDLE;
    }
    case 'B':
    {
        serial_to_loader_msg_t msg = {
            .command = CMD_SERIAL_LOADER_LOAD};
        osMessageQueuePut(serial_to_loaderHandle, &msg, 0, 0);
        return SERIAL_IDLE;
    }
    default:
        return SERIAL_IDLE;
    }
}

/**
 * @brief Updates switch 2 status on limit switch interrupt event.
 *
 * @param event New switch event.
 */
static void limitswitch2_event_callback(limitswitch_event_t event)
{
    tx_data.switch2_status = event;
    limitswitch_change_counter++;
}

/**
 * @brief Updates switch 3 status on limit switch interrupt event.
 *
 * @param event New switch event.
 */
static void limitswitch3_event_callback(limitswitch_event_t event)
{
    tx_data.switch3_status = event;
    limitswitch_change_counter++;
}

limitswitch_config_t limitswitch2_config = {
    .gpio_port = LIMIT_SW_2_GPIO_Port,
    .gpio_pin = LIMIT_SW_2_Pin,
    .pressed_state = GPIO_PIN_SET,
    .callback = limitswitch2_event_callback};

limitswitch_config_t limitswitch3_config = {
    .gpio_port = LIMIT_SW_3_GPIO_Port,
    .gpio_pin = LIMIT_SW_3_Pin,
    .pressed_state = GPIO_PIN_SET,
    .callback = limitswitch3_event_callback};

/**
 * @brief Initializes serial service dependencies and initial TX status state.
 *
 * @return Initial serial service state.
 */
static serial_state_t init_serial_service()
{
    register_limitswitch(LIMITSWITCH_2, limitswitch2_config);
    register_limitswitch(LIMITSWITCH_3, limitswitch3_config);

    tx_data.switch2_status = (HAL_GPIO_ReadPin(LIMIT_SW_2_GPIO_Port, LIMIT_SW_2_Pin) == limitswitch2_config.pressed_state) ? LIMITSWITCH_PRESSED : LIMITSWITCH_RELEASED;
    tx_data.switch3_status = (HAL_GPIO_ReadPin(LIMIT_SW_3_GPIO_Port, LIMIT_SW_3_Pin) == limitswitch3_config.pressed_state) ? LIMITSWITCH_PRESSED : LIMITSWITCH_RELEASED;
    tx_data.elbow_status = STATUS_ELBOW_SERIAL_NEEDS_HOME;
    tx_data.precharge_status = STATUS_PRECHARGE_SERIAL_OFF;
    tx_data.led_status = STATUS_LED_SERIAL_DISABLED;
    tx_data.loader_status = STATUS_LOADER_SERIAL_IDLE;
    last_transmitted_limitswitch_change_counter = limitswitch_change_counter;

    while (usb_tx_busy())
    {
        osDelay(10);
    }
    CDC_Transmit_FS((uint8_t *)WELCOME_MSG, sizeof(WELCOME_MSG) - 1U);

    return SERIAL_IDLE;
}

/**
 * @brief Serializes TX status fields into an ASCII frame: <s2,s3,elbow,precharge,led,loader>.
 */
static void update_tx_buffer(void)
{
    limitswitch_event_t switch2_status = tx_data.switch2_status;
    limitswitch_event_t switch3_status = tx_data.switch3_status;
    elbow_to_serial_status_t elbow_status = tx_data.elbow_status;
    precharge_to_serial_status_t precharge_status = tx_data.precharge_status;
    led_to_serial_status_t led_status = tx_data.led_status;
    loader_to_serial_status_t loader_status = tx_data.loader_status;

    char local_buffer[APP_TX_DATA_SIZE];
    int written = snprintf(local_buffer,
                           sizeof(local_buffer),
                           "<%u,%u,%u,%u,%u,%u>",
                           (unsigned int)switch2_status,
                           (unsigned int)switch3_status,
                           (unsigned int)elbow_status,
                           (unsigned int)precharge_status,
                           (unsigned int)led_status,
                           (unsigned int)loader_status);

    if (written <= 0)
    {
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

/**
 * @brief Handles SERIAL_IDLE state transitions.
 *
 * @return Next serial state.
 */
static serial_state_t handle_idle(void)
{
    if (rx_buffer_has_data())
    {
        return SERIAL_RX;
    }

    if (limitswitch_change_counter != last_transmitted_limitswitch_change_counter)
    {
        return SERIAL_TX;
    }

    if (osMessageQueueGetCount(elbow_to_serialHandle) > 0)
    {
        elbow_to_serial_msg_t msg;
        osMessageQueueGet(elbow_to_serialHandle, &msg, NULL, 0);
        tx_data.elbow_status = msg.status;
        return SERIAL_TX;
    }

    if (osMessageQueueGetCount(precharge_to_serialHandle) > 0)
    {
        precharge_to_serial_msg_t msg;
        osMessageQueueGet(precharge_to_serialHandle, &msg, NULL, 0);
        tx_data.precharge_status = msg.status;
        return SERIAL_TX;
    }

    if (osMessageQueueGetCount(led_to_serialHandle) > 0)
    {
        led_to_serial_msg_t msg;
        osMessageQueueGet(led_to_serialHandle, &msg, NULL, 0);
        tx_data.led_status = msg.status;
        return SERIAL_TX;
    }

    if (osMessageQueueGetCount(loader_to_serialHandle) > 0)
    {
        loader_to_serial_msg_t msg;
        osMessageQueueGet(loader_to_serialHandle, &msg, NULL, 0);
        tx_data.loader_status = msg.status;
        return SERIAL_TX;
    }

    return SERIAL_IDLE;
}

/**
 * @brief Handles SERIAL_RX state work and transitions.
 *
 * @return Next serial state.
 */
static serial_state_t handle_rx(void)
{
    if (!consume_latest_rx_packet(latest_rx_packet, sizeof(latest_rx_packet)))
    {
        return SERIAL_IDLE;
    }

    return parse_rx_packet(latest_rx_packet);
}

/**
 * @brief Handles SERIAL_TX state work and transitions.
 *
 * @return Next serial state.
 */
static serial_state_t handle_tx(void)
{
    if (usb_tx_busy())
    {
        return SERIAL_TX;
    }

    update_tx_buffer();

    if (tx_buffer.size == 0U)
    {
        return SERIAL_IDLE;
    }

    if (CDC_Transmit_FS((uint8_t *)(void *)tx_buffer.buffer, tx_buffer.size) != USBD_OK)
    {
        return SERIAL_TX;
    }

    last_transmitted_limitswitch_change_counter = limitswitch_change_counter;
    return SERIAL_IDLE;
}

/**
 * @brief Executes one iteration of the serial service state machine.
 *
 * @param state Current state.
 * @return Next state.
 */
static serial_state_t state_machine(serial_state_t state)
{
    switch (state)
    {
    case SERIAL_IDLE:
        return handle_idle();
    case SERIAL_RX:
        return handle_rx();
    case SERIAL_TX:
        return handle_tx();
    default:
        return SERIAL_IDLE;
    }
}

/**
 * @brief Serial service task entry point.
 *
 * @param argument Unused RTOS task argument.
 */
void start_serial_service(void *argument)
{
    (void)argument;
    state = init_serial_service();

    while (true)
    {
        state = state_machine(state);
        osDelay(10);
    }
}
