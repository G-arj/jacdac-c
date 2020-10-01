// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jd_control.h"
#include "jd_services.h"
#include "jd_io.h"
#include "interfaces/jd_tx.h"
#include "interfaces/jd_hw.h"

#include "jd_util.h"

// do not use _state parameter in this file - it can be NULL in bootloader mode

struct srv_state {
    SRV_COMMON;
};

static uint8_t id_counter;
static uint32_t nextblink;

static void identify(void) {
    if (!id_counter)
        return;
    if (!jd_should_sample(&nextblink, 150000))
        return;

    id_counter--;
    jd_led_blink(50000);
}

void jd_ctrl_process(srv_t *_state) {
    identify();
}

static void send_value(jd_packet_t *pkt, uint32_t v) {
    jd_send(JD_SERVICE_NUMBER_CTRL, pkt->service_command, &v, sizeof(v));
}

void jd_ctrl_handle_packet(srv_t *_state, jd_packet_t *pkt) {
    switch (pkt->service_command) {
    case JD_CMD_ADVERTISEMENT_DATA:
        jd_services_announce();
        break;

    case JD_CMD_CTRL_IDENTIFY:
        id_counter = 7;
        nextblink = now;
        identify();
        break;

    case JD_CMD_CTRL_RESET:
        target_reset();
        break;

    case (JD_CMD_GET_REG | JD_REG_CTRL_DEVICE_DESCRIPTION):
        jd_send(JD_SERVICE_NUMBER_CTRL, pkt->service_command, app_dev_class_name,
                strlen(app_dev_class_name));
        break;

    case (JD_CMD_GET_REG | JD_REG_CTRL_FIRMWARE_VERSION):
        jd_send(JD_SERVICE_NUMBER_CTRL, pkt->service_command, app_fw_version,
                strlen(app_fw_version));
        break;

    case (JD_CMD_GET_REG | JD_REG_CTRL_DEVICE_CLASS):
        send_value(pkt, app_get_device_class());
        break;

    case (JD_CMD_GET_REG | JD_REG_CTRL_BL_DEVICE_CLASS):
        send_value(pkt, app_get_device_class());
        break;

    case (JD_CMD_GET_REG | JD_REG_CTRL_MICROS_SINCE_BOOT): {
        uint64_t t = tim_get_micros();
        jd_send(JD_SERVICE_NUMBER_CTRL, pkt->service_command, &t, sizeof(t));
        break;
    }

#if JD_CONFIG_TEMPERATURE == 1
    case (JD_CMD_GET_REG | JD_REG_CTRL_TEMPERATURE):
        send_value(pkt, adc_read_temp());
        break;
#endif
    }
}

SRV_DEF(jd_ctrl, JD_SERVICE_CLASS_CTRL);
void jd_ctrl_init(void) {
    SRV_ALLOC(jd_ctrl);
}
