/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "hardware/flash.h"

#include "hardware/uart.h"

// totp header file
#include "totp.h"

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 16
#define UART_RX_PIN 17

#include "usb_descriptors.h"
#define FLASH_TARGET_OFFSET (512 * 1024) // choosing to start at 512K

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 60,
  BLINK_MOUNTED = 250,
  BLINK_SUSPENDED = 2500,
};

int numButtons = 8; // Number of GPIO Inputs
uint32_t userChosen = 0;
bool usePass = false;

// Password Initial Variables
char password[] = {0, 6, 7,  3};
bool authorizedPass = true;

char myData[4096];
char myData1[4096];

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Time variables
uint32_t unix_time;
uint32_t start_time_ms = 0;


// --- TOTP Variables ---
// Maximum secret length for TOTP configuration
#define SECRET_MAX 32
// Global Base32 secret for TOTP generation (default value)
char base32_secret[SECRET_MAX] = "JBSWY3DPEHPK3PXP";
volatile bool new_secret_programmed = false;

void led_blinking_task(void);
void lock_check_task(void);
void gpio_task(void);
void storeString();
void readString();

// TOTP task: generates and sends a TOTP code periodically.
void totp_task(void) {
  uint32_t elapsed_time_ms = board_millis() - start_time_ms;
  uint32_t current_time = unix_time + (elapsed_time_ms / 1000);

  char otp[10] = {0};
  int time_remaining = 0;
  if (totp(current_time, base32_secret, 30, 6, otp, sizeof(otp), &time_remaining) == 0) {
      printf("TOTP: %s, valid for %d seconds\n", otp, time_remaining);
      // Optionally, send the OTP via USB HID:
      // (We call a function similar to send_string_via_hid below.)
      // send_string_via_hid(otp);
  } else {
      printf("Error generating TOTP\n");
  }
}

/*------------- MAIN -------------*/
int main(void)
{
 //added for init
 stdio_init_all();

 uart_init(UART_ID, BAUD_RATE);
 gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
 gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

  // init device stack on configured roothub port
 tud_init(BOARD_TUD_RHPORT);

 board_init();

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  stdio_init_all();

  //Initiaizing all the GPIO pins as inputs for the buttons
  for (int i = 0; i < numButtons ; i++) {
      gpio_init(i);
      gpio_set_dir(i, GPIO_IN);
  }

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_LED_PIN, 0);

  //Check initial button presses for password
  char passwordInput[] = {0, 0, 0, 0};
  int numInputs = 0;
  bool wrongPassword = false;

  readString();


  while (1)
  {
    tud_task(); // Process USB tasks
    
    while (!authorizedPass) {
        //Cycles through to check for button press, blinks if there is input
        for (int i = 0; i < numButtons; i++) {
            if(gpio_get(i)) {
                passwordInput[numInputs] = i;
                numInputs++;
                gpio_put(PICO_DEFAULT_LED_PIN, 1);
                sleep_ms(500);
                gpio_put(PICO_DEFAULT_LED_PIN, 0);
                sleep_ms(500);
            }
            
            //On 4th input will check to see if passcode is correct
            if (numInputs == 4) {
                numInputs = 0;
                for (int j = 0; j < 4 && !wrongPassword; j++) {
                    if (password[j] != passwordInput[j]) {
                        wrongPassword = true;
                    }
                }

                if (!wrongPassword) {
                    authorizedPass = true;
                } else {
          	  wrongPassword = false;
          	  for(int i=0; i<3; i++) {
                	    gpio_put(PICO_DEFAULT_LED_PIN, 1);
                	    sleep_ms(100);
                	    gpio_put(PICO_DEFAULT_LED_PIN, 0);
                	    sleep_ms(100);
          	  }
                }
            }
        }
    }
    while (authorizedPass) {
      tud_task(); // tinyusb device task
      led_blinking_task();
      gpio_task();
      lock_check_task();
    }
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}


//--------------------------------------------------------------------+
// Lock Check Task
//--------------------------------------------------------------------+
void lock_check_task(void) {
  uint32_t const btn = board_button_read();
  if(btn) {
	blink_interval_ms = BLINK_MOUNTED;
	userChosen = 0;
  }
}

//--------------------------------------------------------------------+
// DATA STORAGE
//--------------------------------------------------------------------+
void storeString () {
    uint32_t userMult = 2 * (userChosen - 1) + usePass;
    uint8_t* myDataAsBytes = (uint8_t*) &myData1;
    int myDataSize = sizeof(myData1);
    
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET + (4096 * userMult), 1);
    flash_range_program(FLASH_TARGET_OFFSET + (4096 * userMult), myDataAsBytes, 1);
    restore_interrupts(interrupts);
}

void readString() {
    uint32_t userMult = 2 * (userChosen - 1) + usePass;
    const uint8_t* flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET + (4096 * userMult));
    memcpy(&myData, flash_target_contents, sizeof(myData1));
}

void programmer(uint32_t btn) {

  uint32_t btn2 = (gpio_get(0));
  for(int i=1; i<numButtons; i++) {
      btn2 = btn2 | (gpio_get(i) << i);
  }

  char s[4096];
  int i=0;

  while(1) {
    s[i] = uart_getc(UART_ID);
    i++;
    if(i == 4096){
      break;
    }
    if(s[i-1] == ';'){
      i--;
      break;
    }
  }

  if(btn == 2) {
    start_time_ms = board_millis();
    unix_time = atoi(s);
  } else {
    s[i] = '\0';
    strcpy(myData1, s);
    storeString(); 
  }
}
//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void send_key(uint8_t hid_send_key) {
    waiter1:
    if(!tud_hid_ready()) goto waiter1;

    uint8_t mod = 0;
    if((hid_send_key >= 65) && (hid_send_key <= 90)) {
      mod = KEYBOARD_MODIFIER_LEFTSHIFT;
    }

    switch(hid_send_key) {
      // Numbers 0-9
      case '1': hid_send_key = HID_KEY_1; break;
      case '2': hid_send_key = HID_KEY_2; break;
      case '3': hid_send_key = HID_KEY_3; break;
      case '4': hid_send_key = HID_KEY_4; break;
      case '5': hid_send_key = HID_KEY_5; break;
      case '6': hid_send_key = HID_KEY_6; break;
      case '7': hid_send_key = HID_KEY_7; break;
      case '8': hid_send_key = HID_KEY_8; break;
      case '9': hid_send_key = HID_KEY_9; break;
      case '0': hid_send_key = HID_KEY_0; break;

      // Lowercase letters a-z
      case 'a': hid_send_key = HID_KEY_A; break;
      case 'b': hid_send_key = HID_KEY_B; break;
      case 'c': hid_send_key = HID_KEY_C; break;
      case 'd': hid_send_key = HID_KEY_D; break;
      case 'e': hid_send_key = HID_KEY_E; break;
      case 'f': hid_send_key = HID_KEY_F; break;
      case 'g': hid_send_key = HID_KEY_G; break;
      case 'h': hid_send_key = HID_KEY_H; break;
      case 'i': hid_send_key = HID_KEY_I; break;
      case 'j': hid_send_key = HID_KEY_J; break;
      case 'k': hid_send_key = HID_KEY_K; break;
      case 'l': hid_send_key = HID_KEY_L; break;
      case 'm': hid_send_key = HID_KEY_M; break;
      case 'n': hid_send_key = HID_KEY_N; break;
      case 'o': hid_send_key = HID_KEY_O; break;
      case 'p': hid_send_key = HID_KEY_P; break;
      case 'q': hid_send_key = HID_KEY_Q; break;
      case 'r': hid_send_key = HID_KEY_R; break;
      case 's': hid_send_key = HID_KEY_S; break;
      case 't': hid_send_key = HID_KEY_T; break;
      case 'u': hid_send_key = HID_KEY_U; break;
      case 'v': hid_send_key = HID_KEY_V; break;
      case 'w': hid_send_key = HID_KEY_W; break;
      case 'x': hid_send_key = HID_KEY_X; break;
      case 'y': hid_send_key = HID_KEY_Y; break;
      case 'z': hid_send_key = HID_KEY_Z; break;

      // Lowercase letters a-z
      case 'A': hid_send_key = HID_KEY_A; break;
      case 'B': hid_send_key = HID_KEY_B; break;
      case 'C': hid_send_key = HID_KEY_C; break;
      case 'D': hid_send_key = HID_KEY_D; break;
      case 'E': hid_send_key = HID_KEY_E; break;
      case 'F': hid_send_key = HID_KEY_F; break;
      case 'G': hid_send_key = HID_KEY_G; break;
      case 'H': hid_send_key = HID_KEY_H; break;
      case 'I': hid_send_key = HID_KEY_I; break;
      case 'J': hid_send_key = HID_KEY_J; break;
      case 'K': hid_send_key = HID_KEY_K; break;
      case 'L': hid_send_key = HID_KEY_L; break;
      case 'M': hid_send_key = HID_KEY_M; break;
      case 'N': hid_send_key = HID_KEY_N; break;
      case 'O': hid_send_key = HID_KEY_O; break;
      case 'P': hid_send_key = HID_KEY_P; break;
      case 'Q': hid_send_key = HID_KEY_Q; break;
      case 'R': hid_send_key = HID_KEY_R; break;
      case 'S': hid_send_key = HID_KEY_S; break;
      case 'T': hid_send_key = HID_KEY_T; break;
      case 'U': hid_send_key = HID_KEY_U; break;
      case 'V': hid_send_key = HID_KEY_V; break;
      case 'W': hid_send_key = HID_KEY_W; break;
      case 'X': hid_send_key = HID_KEY_X; break;
      case 'Y': hid_send_key = HID_KEY_Y; break;
      case 'Z': hid_send_key = HID_KEY_Z; break;
    }

    // Prepare the key report
    uint8_t keycode[6] = {0};
    keycode[0] = hid_send_key;

    // Send key press
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, mod, keycode);
    sleep_ms(10); // Wait for key to be recognized
    tud_task();
    waiter2:
    if(!tud_hid_ready()) goto waiter2;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    sleep_ms(10);
}

void send_multiple_keys(char* string) {
    for (size_t i = 0; i < strlen(string); i++) {
        send_key(string[i]);
	tud_task();
    }
}

void gpio_task(void) {
  // Poll every 10ms
  const uint32_t interval_ms = 100;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  uint32_t btn = (gpio_get(0));
  for(int i=1; i<numButtons; i++) {
  	btn = btn | (gpio_get(i) << i);
  }

  // Remote wakeup
  if ( tud_suspended() && btn ) {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  } else if (btn && (btn<64) && (userChosen == 0)) {
        blink_interval_ms = 0;
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
  	userChosen = btn;
  } else if (btn==128 && (userChosen != 0)) {
	usePass = false;
	readString();
        send_multiple_keys(myData);
  } else if (btn==64 && (userChosen != 0)) {
	usePass = true;
	readString();
        send_multiple_keys(myData);
  } else if (btn==32 && (userChosen != 0)) {
	totp_task();
  } else if (btn==16 && (userChosen != 0)) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
	usePass = false;
        programmer(btn);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
  } else if (btn==8 && (userChosen != 0)) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
	usePass = true;
        programmer(btn);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
  } else if (btn==2 && (userChosen != 0)) {
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
	programmer(btn);
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
  }
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

  if (report_type == HID_REPORT_TYPE_INPUT)
  {
blink_interval_ms = 0;
board_led_write(true);
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];
    }
  }
}



//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}
