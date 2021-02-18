// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jd_services.h"
#include "interfaces/jd_pins.h"
#include "jacdac/dist/c/arcadegamepad.h"

#define BUTTON_FLAGS 0
#define NUM_PLAYERS 1

struct srv_state {
    SENSOR_COMMON;
    uint8_t inited;
    uint32_t btn_state;

    uint8_t pin;
    uint8_t pressed;
    uint8_t prev_pressed;
    uint8_t num_pins;
    uint8_t active;
    const uint8_t *button_pins;
    const uint8_t *led_pins;
    uint32_t press_time;
    uint32_t nextSample;
};

static uint32_t buttons_state(srv_t *state) {
    if (!state->inited) {
        state->inited = true;
        for (int i = 0; i < state->num_pins; ++i) {
            pin_setup_input(state->button_pins[i], state->active ? -1 : 1);
            if (state->led_pins)
                pin_setup_output(state->led_pins[i]);
        }
    }

    uint32_t r = 0;

    for (int i = 0; i < state->num_pins; ++i) {
        if (state->button_pins[i] == 0xff)
            continue;
        if (pin_get(state->button_pins[i]) == state->active) {
            r |= (1 << i);
        }
    }

    return r;
}

static void update(srv_t *state) {
    uint32_t newstate = buttons_state(state);
    if (newstate != state->btn_state) {
        for (int i = 0; i < state->num_pins; ++i) {
            uint32_t isPressed = (newstate & (1 << i));
            uint32_t wasPressed = (state->btn_state & (1 << i));
            if (isPressed != wasPressed) {
                if (state->led_pins && state->led_pins[i] != 0xff)
                    pin_set(state->led_pins[i], isPressed);
                uint32_t arg = i + 1;
                jd_send_event_ext(state, isPressed ? JD_ARCADE_GAMEPAD_EV_DOWN : JD_ARCADE_GAMEPAD_EV_UP, &arg,
                                  sizeof(arg));
            }
        }
        state->btn_state = newstate;
    }
}

static void send_report(srv_t *state) {
    jd_arcade_gamepad_buttons_t reports[state->num_pins], *report;
    report = reports;

    for (int i = 0; i < state->num_pins; ++i) {
        if (state->btn_state & (1 << i)) {
            report->button = i + 1;
            report->pressure = 0xff;
            report++;
        }
    }

    jd_send(state->service_number, JD_GET(JD_REG_READING), reports,
            (uint8_t *)report - (uint8_t *)reports);
}

static void ad_data(srv_t *state) {
    uint8_t addata[state->num_pins];
    uint8_t *dst = addata;
    for (int i = 0; i < state->num_pins; ++i) {
        if (state->button_pins[i] != 0xff) {
            *dst++ = i + 1;
        }
    }
    jd_send(state->service_number, JD_GET(JD_ARCADE_GAMEPAD_REG_AVAILABLE_BUTTONS), addata, (uint8_t *)dst - (uint8_t *)addata);
}

void gamepad_process(srv_t *state) {
    if (jd_should_sample(&state->nextSample, 9000)) {
        update(state);

        if (sensor_should_stream(state))
            send_report(state);
    }
}

void gamepad_handle_packet(srv_t *state, jd_packet_t *pkt) {
    sensor_handle_packet(state, pkt);

    if (pkt->service_command == JD_GET(JD_REG_READING))
        send_report(state);
    else if (pkt->service_command == JD_GET(JD_ARCADE_GAMEPAD_REG_AVAILABLE_BUTTONS))
        ad_data(state);
}

SRV_DEF(gamepad, JD_SERVICE_CLASS_ARCADE_GAMEPAD);

void gamepad_init(uint8_t num_pins, const uint8_t *pins, const uint8_t *ledPins) {
    SRV_ALLOC(gamepad);
    state->num_pins = num_pins;
    state->button_pins = pins;
    state->led_pins = ledPins;
    state->active = ledPins ? 1 : 0;
    update(state);
}
