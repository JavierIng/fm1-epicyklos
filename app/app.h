/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Javier Sánchez (EPICYKLOS changes) */
/* EPICYKLOS's half of the contract with the platform (device/unity.c, host/plat_host.c), from fm1-base's template
 * (MPL-2.0: this file keeps that licence; the rest of app/ is GPL-3.0-only). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "EPICYKLOS"
#define APP_SPLASH_RGB RGB(79, 209, 197)
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_TOUCH                        /* velocity and pressure from the keys (lib/touch.h) */
#define APP_BLE_AT_BOOT (!epk_ble_off())   /* Bluetooth MIDI at power-on (settings: phase 6) */
int epk_ble_off(void);

/* the store: the app's objects, the panel calibration (the platform's) and Bluetooth's address and pairing among
 * them, and their flash offsets */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}

/* the contract: the platform calls these */
void app_init(void);                     /* boot: the engine, then the saved settings (or defaults) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: n stereo frames, 24-bit */
void app_panic(void);                    /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                  /* the first screen, after the splash */
void app_frame(void);                    /* the UI: input and a redraw, about 60 times a second */
void app_input_only(void);               /* between frames: input only */
void app_say(const char *a, const char *b);   /* a short message on the screen and to a screen reader */

/* the host simulator's checks (host/plat_host.c "expect"): track, view, page, octave, playing, tempo,
 * muteN, soloN, stepN, levelN, trigN (N: track 1..6) */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
