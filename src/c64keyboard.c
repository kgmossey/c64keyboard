/**
 * C64 USB Keyboard Adapter - By Kevin Mossey 2022
 *
 * This code is released under the GPL Version 3 license.
 *
 * It is heavily derived from the TinyUSB example hid_composite
 */

// Since the Pico can't act as an HID and seriol connection over USB at the same
// time, you need to uncomment the following line to view debug data in the
// terminal.  Edit the cmake file as well.
//#define DEBUG

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "us_keycodes.h"

#ifndef DEBUG
  #include "bsp/board.h"
  #include "tusb.h"
  #include "usb_descriptors.h"
#else
  #include <string.h>
#endif

#define HIGH 1
#define LOW 0

uint8_t keycode[6] = { 0 };
static const uint8_t keycode_defaults[6] = { 0, 0, 0, 0, 0, 0 };
const uint LED_PIN = PICO_DEFAULT_LED_PIN;  // Pin 25
const uint LED_PIN_MASK = 1<<PICO_DEFAULT_LED_PIN;
int ColPinsMapping[9] = {9,   6,  7,  8,  5, 10, 11, 12, 4};
int RowPinsMapping[9] = {20, 14, 15, 16, 17, 18, 19, 13, 21};
uint32_t ColPinMap = 511<<4;   // mask pins 5-12
uint32_t RowPinMap = 511<<13;  // mask pins 13-20
int numRows = sizeof(RowPinsMapping)/sizeof(RowPinsMapping[0]);
int numCols = sizeof(ColPinsMapping)/sizeof(ColPinsMapping[0]);
bool key_cached = false;

char keymap[81] = {KC_RUN_STOP, KC_Q,       KC_COMMODORE, KC_SPACE,   KC_2,        KC_CTRL,      KC_L_ARROW,  KC_1,        0,
                   KC_SLASH,    KC_U_ARROW, KC_EQUALS,    KC_R_SHIFT, KC_CLR_HOME, KC_SEMICOLON, KC_ASTERISK, KC_POUND,    0,
                   KC_COMMA,    KC_AT,      KC_COLON,     KC_PERIOD,  KC_MINUS,    KC_L,         KC_P,        KC_PLUS,     0,
                   KC_N,        KC_O,       KC_K,         KC_M,       KC_0,        KC_J,         KC_I,        KC_9,        0,
                   KC_V,        KC_U,       KC_H,         KC_B,       KC_8,        KC_G,         KC_Y,        KC_7,        0,
                   KC_X,        KC_T,       KC_F,         KC_C,       KC_6,        KC_D,         KC_R,        KC_5,        0,
                   KC_L_SHIFT,  KC_E,       KC_S,         KC_Z,       KC_4,        KC_A,         KC_W,        KC_3,        0,
                   KC_DOWN,     KC_F5,      KC_F3,        KC_F1,      KC_F7,       KC_RIGHT,     KC_RETURN,   KC_INST_DEL, 0,
                   0,           0,          0,            0,          0,           0,            0,           0,           KC_RESTORE};

void detect_keypress();
void hid_task(void);

int main() {
  // USB Inititalization
  #ifndef DEBUG
    board_init();
    tusb_init();
  #endif

  // STDIO Inititalization
  stdio_init_all();

  gpio_init_mask(LED_PIN_MASK+RowPinMap+ColPinMap); //gpio init all pins
  gpio_set_dir_out_masked(LED_PIN_MASK+RowPinMap);  // set row for output
  gpio_put_all(LED_PIN_MASK);   //set LED pin high, everything else low

  char buffer[33];
  while (1)
  {
    #ifndef DEBUG
      tud_task(); // TinyUsb Device task
    #endif
    detect_keypress();
    #ifndef DEBUG
      hid_task();
    #endif
  }
}

void keycode_override(uint8_t new_keycode) {
  memcpy (keycode, keycode_defaults, sizeof(keycode_defaults));
  keycode[0] = new_keycode;
}

void detect_keypress() {

  static bool was_key_pressed = false;
  static uint index = 0;
  static int right_shift_index = -1;
  // reset keycode array to 0
  memcpy (keycode, keycode_defaults, sizeof(keycode_defaults));

  for (uint row=0; row<numRows; row++) {
    // set one row high. then test columns
    gpio_put(RowPinsMapping[row], HIGH);
    busy_wait_us(5);  // time delay because the old keyboard wiring seems to
                      // have a lot of resistonce
      for (uint col=0; col<numCols; col++) {
        if (gpio_get(ColPinsMapping[col]) == HIGH) {
          if (index < 5) {  // limit of six keys can be sent at once
            was_key_pressed = true;
            // Special keys combos:
            // - three finger salute ctrl+commodore+inst_del for Mister FPGA menu
            //   - this combo is handled below
            // - shift for up, left, f2, f4, f6, f8
            if (right_shift_index >= 0) {
              switch (keymap[row*numCols+col]) {
                case KC_DOWN:
                  keycode[right_shift_index] = KC_UP;
                  break;
                case KC_RIGHT:
                  keycode[right_shift_index] = KC_LEFT;
                  break;
                case KC_F1:
                  keycode[right_shift_index] = KC_F2;
                  break;
                case KC_F3:
                  keycode[right_shift_index] = KC_F4;
                  break;
                case KC_F5:
                  keycode[right_shift_index] = KC_F6;
                  break;
                case KC_F7:
                  keycode[right_shift_index] = KC_F8;
                  break;
                default:
                  keycode[index] = keymap[row*numCols+col];
              }
            } else {
              keycode[index] = keymap[row*numCols+col];
            }
            if (keymap[row*numCols+col] == KC_R_SHIFT)
            right_shift_index = index;
            index++;
          }
          /*printf("row %i col %i - 0x%x\n", RowPinsMapping[row]-13,
                                             ColPinsMapping[col]-5,
                                             keymap[row*numCols+col] & 0xff); */
        }
      }
      gpio_put(RowPinsMapping[row], LOW);
    }

    if (keycode[0]==KC_COMMODORE && keycode[1]==KC_CTRL && keycode[2]==KC_INST_DEL) {
    keycode_override(KC_MENU);
  }

  if (was_key_pressed) {
    #ifdef DEBUG
      printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
          keycode[0] & 0xff, keycode[1] & 0xff,
          keycode[2] & 0xff, keycode[3] & 0xff,
          keycode[4] & 0xff, keycode[5] & 0xff);
    #endif
    key_cached = true;
    was_key_pressed = false;
  } else {
    key_cached = false;
  }
  index = 0;
  right_shift_index = -1;
  #ifdef DEBUG
    busy_wait_us(250000);  // slow down the output during debugging
  #endif
}

/* The following functions need to be suppressed if doing serial output */
#ifndef DEBUG
// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
  //send_hid_report(REPORT_ID_KEYBOARD, btn);

  static bool has_keyboard_key = false;
  if ( key_cached )
  {
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
    has_keyboard_key = true;
  }else
  {
    // send empty key report if previously has key pressed
    if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    has_keyboard_key = false;
  }
}


// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len)
{
  (void) instance;
  (void) len;
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}


// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
/*
  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
*/
}
#endif
