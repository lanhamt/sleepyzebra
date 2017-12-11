#include <stdbool.h>
#include <stdlib.h>

#include "gpio.h"
#include "led.h"
#include "ieee802154.h"
#include "timer.h"
#include "tock.h"
#include "rng.h"
#include "alarm.h"
#include "button.h"

// IEEE 802.15.4 sample packet transmission app.
// Continually transmits frames at the specified short address to the specified
// destination address.

#define BUF_SIZE 60
char packet[BUF_SIZE];
char packet_rx[IEEE802154_FRAME_LEN];

// We change SRC_ADDR to be unique for each node, allowing us to identify
// them in packet captures
#define SRC_ADDR 0x150c
#define SRC_PAN 0xABCD
#define INIT_DELAY 1000
#define BUTTON_NUM 0
#define GPIO_NUM 0
#define LED_NUM 0
// True if the multi-hop functionality is enabled; this means that a node
// drops all packets whose MAC addresses are > SRC_ADDR + HOP_SIZE or < SRC_ADDR - HOP_SIZE
#define MULTI_HOP_SIM true
// Distance function for who we can hear packets sent from; defined as
// we drop all packets who MAC addresses are > SRC_ADDR + HOP_SIZE or < SRC_ADDR - HOP_SIZE
#define HOP_SIZE 1

// Trickle constants
#define I_MIN 10000 // In ms
#define I_MAX 7    // Doublings of interval size
#define K 4        // Redundancy constant

typedef struct {
  uint32_t i;    // Current interval size
  uint32_t t;    // Time in current interval
  uint32_t c;    // Counter
  int val;       // Our current value
  tock_timer_t trickle_i_timer;
  tock_timer_t trickle_t_timer;
} trickle_state;

static uint32_t I_MAX_VAL = 0;
// This is only necessary as one of the callbacks doesn't pass a reference
// to our state struct back - thus, anything not global is lost
static trickle_state* global_state = NULL;

void interval_t(trickle_state* state);
void interval_end(trickle_state* state);
void initialize_state(trickle_state* state);
void interval_start(trickle_state* state);
void set_timer(trickle_state* state, int ms, bool set_interval_timer);
void transmit(int payload);
void consistent_transmission(trickle_state* state);
void inconsistent_transmission(trickle_state* state, int val);
void updated_value(int new_val);
bool check_addrs(void);

static void button_pressed(__attribute__ ((unused)) int btn_num,
                           int val, // 1 if pressed, 0 if not pressed
                           __attribute__ ((unused)) int arg2,
                           void *ud) {
  trickle_state* state = (trickle_state*)ud;
  // If button pressed, update value
  if (val == 1) {
    // We simulate an "inconsistent transmission" here, as if we received
    // the update from another node
    inconsistent_transmission(state, state->val + 1);
  }
}

static void t_timer_fired(__attribute__ ((unused)) int unused1,
                        __attribute__ ((unused)) int unused2,
                        __attribute__ ((unused)) int unused3,
                        __attribute__ ((unused)) void* arg) {
  printf("t fired\n");
  interval_t(((trickle_state*)arg));
}

static void interval_timer_fired(__attribute__ ((unused)) int unused1,
                        __attribute__ ((unused)) int unused2,
                        __attribute__ ((unused)) int unused3,
                        __attribute__ ((unused)) void* arg) {
  printf("i fired\n");
  interval_end(((trickle_state*)arg));
}


void initialize_state(trickle_state* state) {
  state->i = I_MIN;
  state->t = 0;
  state->c = 0;
  state->val = 0;

  I_MAX_VAL = I_MIN;
  int i;
  for (i = 0; i < I_MAX; i++) {
    I_MAX_VAL *= 2;
  }
  global_state = state;

  button_subscribe(button_pressed, state);
  button_enable_interrupt(BUTTON_NUM);
}

void interval_start(trickle_state* state) {
  state->c = 0;
  uint32_t t = 0;
  int ret_val = rng_sync(((uint8_t*)(&t)), sizeof(uint32_t), sizeof(uint32_t));
  if (ret_val < 0) {
    printf("Error with TRNG module: %d\n", ret_val);
  }
  state->t = (t % (state->i/2)) + state->i/2;
  // Set a timer for time t ahead of us
  set_timer(state, state->t, false);
  set_timer(state, state->i, true);
}

void set_timer(trickle_state* state, int ms, bool set_interval_timer) {
  if (set_interval_timer) {
    timer_in(ms, interval_timer_fired, state, &state->trickle_i_timer);
  } else {
    timer_in(ms, t_timer_fired, state, &state->trickle_t_timer);
  }
}

void interval_t(trickle_state* state) {
  if (state->c < K) {
    transmit(state->val); 
  } 
}

// If the interval ended without hearing an inconsistent
// frame, we double our I val and restart the interval
void interval_end(trickle_state* state) {
  printf("Interval end: node_id: %04x\t i: %lu\t t: %lu\t c: %lu\n", SRC_ADDR, state->i, state->t, state->c);
  state->i = 2*state->i;
  if (state->i > I_MAX_VAL) {
    state->i = I_MAX_VAL;
  }
  interval_start(state);
}


static void receive_frame(__attribute__ ((unused)) int pans,
                          __attribute__ ((unused)) int dst_addr,
                          __attribute__ ((unused)) int src_addr,
                          __attribute__ ((unused)) void* ud) {
  printf("Packet received\n");
  // Re-subscribe to the callback, so that we again receive any frames
  ieee802154_receive(receive_frame, packet_rx, IEEE802154_FRAME_LEN);
  
  unsigned offset = ieee802154_frame_get_payload_offset(packet_rx);
  unsigned length = ieee802154_frame_get_payload_length(packet_rx);

  if (!check_addrs()) {
    return;
  }

  if (length < sizeof(int)) {
    // Payload too short
    return;
  }
  int received_val = (int)packet_rx[offset];
  if (global_state->val == received_val) {
    consistent_transmission(global_state);
  } else {
    inconsistent_transmission(global_state, received_val);
  }
}

bool check_addrs(void) {
  // Destination address checks
  unsigned short dst_short_addr;
  unsigned char dst_long_addr[8];
  addr_mode_t dst_addr_mode;
  dst_addr_mode = ieee802154_frame_get_dst_addr(packet_rx, &dst_short_addr, dst_long_addr);
  if (dst_addr_mode == ADDR_SHORT) {
    if (dst_short_addr != 0xffff) {
      // Not for us - only listen to broadcast messages
      return false;
    }
  } else if (dst_addr_mode == ADDR_LONG) {
    int i;
    for (i = 0; i < 8; i++) {
      if (dst_long_addr[i] != 0xff) {
        return false;
      }
    }
  } else {
    // Error: No address
    return false;
  }

  // Only do source address checks if simulating multi-hop network
  if (MULTI_HOP_SIM) {
    // Source address checks
    unsigned short src_short_addr;
    unsigned char src_long_addr[8];
    addr_mode_t src_addr_mode;
    src_addr_mode = ieee802154_frame_get_src_addr(packet_rx, &src_short_addr, src_long_addr);
    if (src_addr_mode == ADDR_SHORT) {
      // Return false if src addr is greater than += HOP_SIZE away from our addr
      if (src_short_addr > SRC_ADDR + HOP_SIZE || src_short_addr < SRC_ADDR - HOP_SIZE) {
        return false;
      }
    } else {
      // If address is long or non-existent, then we return false - the
      // source address should *always* be short
      return false;
    }
  }

  return true;
}

void consistent_transmission(trickle_state* state) {
  // Increment the "heard" counter
  state->c += 1;
}

void inconsistent_transmission(trickle_state* state, int val) {
  // Lets us detect when we need to update our value
  if (state->val < val) {
    state->val = val;
    updated_value(val);
  }
  printf("Inconsistent transmission\n");
  if (state->i > I_MIN) {
    state->i = I_MIN;
    interval_start(state);
  }
}

void updated_value(int new_val) {
  // Toggle the gpio pin when we update our value - we use the
  // timing from this to measure propogation delay
  gpio_set(GPIO_NUM);
  if ((new_val % 2) == 0) {
    led_off(LED_NUM);
  } else {
    led_on(LED_NUM);
  }
  printf("New val: %d\n", new_val);
}

void transmit(int payload) {
  *((int*)packet) = payload;
  int err = ieee802154_send(0xFFFF,         // Destination short MAC addr
                            SEC_LEVEL_NONE, // Security level
                            0,              // key_id_mode
                            NULL,           // key_id
                            packet,
                            sizeof(int));
  if (err < 0) {
    printf("Error in transmit: %d\n", err);
  } else {
    printf("Packet sent\n");
  }
}

int main(void) {
  // Initialize GPIO, LED
  gpio_enable_output(0);
  gpio_clear(GPIO_NUM);
  led_off(LED_NUM);

  // Initialize radio
  ieee802154_set_address(SRC_ADDR);
  ieee802154_set_pan(SRC_PAN);
  ieee802154_config_commit();
  ieee802154_up();
  // This delay is necessary as if we receive a callback too early, we will
  // panic/crash
  delay_ms(INIT_DELAY);
  // Set our callback function as the callback
  ieee802154_receive(receive_frame, packet_rx, IEEE802154_FRAME_LEN);

  trickle_state* state = (trickle_state*)malloc(sizeof(trickle_state));
  initialize_state(state);
  interval_start(state);
}
