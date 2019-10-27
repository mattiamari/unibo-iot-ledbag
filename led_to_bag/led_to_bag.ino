/**
 * Led To Bag - Progetto 1 - Sistemi embedded e Internet of Things
 * 
 * 27/10/2019
 * 
 * Autori: Mattia Mari, Daniele Manfredonia, Matteo Vavusotto, Matteo Manzi
 */

typedef unsigned short led_mask_t;
typedef unsigned long time_millis;

// Led pins
const unsigned short LG3 = 12,
                     LG2 = 11,
                     LG1 = 8,
                     LW = 9,
                     LR = 7,
                     LED_COUNT = 5,
                     BTN1 = 2,
                     BTN2 = 3;

const unsigned short LEDS[] = {LG3, LG2, LG1, LW, LR};

// Potentiometer pin
const unsigned short POT = 1;

// Max potentiometer value used to map the analog value to the corresponding game level
const unsigned int POT_MAX = 1024;

// Debounce delay for buttons
const unsigned int DEBOUNCE_INTERVAL = 80;

time_millis DELAY_GAME_START = 1000, // delay between TS pressed and game start
            DELAY_GAME_OVER = 2000,  // red led stays on for this amount of time on game-over
            DELAY_WELCOME = 200;     // delay for the blinking leds

const unsigned short PULSE_DELAY = 1,
                     PULSE_DELTA = 1;

const unsigned short MASK_OFF = 1,        // all LEDS are off. Least significant bit is set to 1 to allow shifting.
                     MASK_BAG = 1 << 4,   // white led ON
                     MASK_FAIL = 1 << 5;  // red led ON

enum game_states {
  WELCOME,
  GAME_START,
  GAME,
  GAME_OVER
};

/**
 * Default game timeouts
 * 
 * First item of the first level has a timeout of LEVEL_TIMEOUT_DEFAULT ms.
 * From there, each item brought to the bag reduces the timeout of 1/LEVEL_TIMEOUT_DIV.
 * 
 * Each level has a timeout of LEVEL_TIMEOUT_DEFAULT - (level_number * LEVEL_MULTIPLIER)
 * where level_number is in (0,...,LEVEL_NUM - 1)
 */
const time_millis LEVEL_TIMEOUT_DEFAULT = 3000,
                  LEVEL_MULTIPLIER = 250;
                  
const unsigned short LEVEL_TIMEOUT_DIV = 8,
                     LEVEL_NUM = 8;

// Current bit mask, corresponding to the LEDs currently ON
led_mask_t curr_mask;

unsigned short game_state,
               bag_count;

time_millis level_start_time,
            level_timeout = LEVEL_TIMEOUT_DEFAULT;

// Buffer to be used by sprintf()
char msg_buff[70];

// State of the white led fade
boolean pulse_end;
unsigned short pulse_direction;
int pulse_intensity;

void setup() {
  for (unsigned short i = 0; i < LED_COUNT; i++) {
    pinMode(LEDS[i], OUTPUT);
  }
  
  pinMode(BTN1, INPUT);
  pinMode(BTN2, INPUT);

  randomSeed(analogRead(0));
  
  attachInterrupt(digitalPinToInterrupt(BTN1), btn1_pressed, RISING);
  attachInterrupt(digitalPinToInterrupt(BTN2), btn2_pressed, RISING);
  
  Serial.begin(9600);

  game_reset();
}

void loop() {
  switch (game_state) {
    case WELCOME:
      state_welcome();
      break;
    case GAME_START:
      state_game_start();
      break;
    case GAME:
      state_game();
      break;
    case GAME_OVER:
      state_game_over();
      break;
  }
}

void game_reset() {
  curr_mask = MASK_OFF;
  game_state = WELCOME;
  reset_pulse_bag();
  Serial.println("Welcome to Led to Bag. Press Key TS to Start");
}

/**
 * Blinks the 3 green leds while reading the potentiometer value.
 */
void state_welcome() {
  unsigned int pot_value = analogRead(POT);
  unsigned short level = pot_value * LEVEL_NUM / POT_MAX;
  
  time_millis new_timeout = LEVEL_TIMEOUT_DEFAULT - (LEVEL_MULTIPLIER * level);
  if (level_timeout != new_timeout) {
    level_timeout = new_timeout;
    sprintf(msg_buff, "Selected level %d. Timeout is now %lu ms", level+1, level_timeout);
    Serial.println(msg_buff);
  }
  
  curr_mask = curr_mask << 1;
  if (curr_mask == MASK_BAG) {
    curr_mask = MASK_OFF << 1;
  }
  update_display();
  delay(DELAY_WELCOME);
}

/**
 * On game start, all leds will stay off for some time.
 */
void state_game_start() {
  curr_mask = MASK_OFF;
  update_display();
  
  delay(DELAY_GAME_START);

  game_start();
}

/**
 * This state runs in a loop and it is never blocked by other code, this means
 * we can rely on millis() to check for the level timeout.
 */
void state_game() {
  boolean timed_out = !(curr_mask & MASK_BAG) && millis() - level_start_time > level_timeout;
  boolean bag_failed = curr_mask & MASK_FAIL;
  
  if (timed_out || bag_failed) {
    bag_fail();
  } else if (pulse_end && curr_mask & MASK_BAG) {
    bag_success();
  }
  
  update_display();
}

/**
 * On game-over, the red led will stay on for some time.
 */
void state_game_over() {
  curr_mask = MASK_FAIL;
  pulse_end = true;
  update_display();
  
  delay(DELAY_GAME_OVER);

  game_reset();
}

/*
 * What to do when the item is placed in the bag
 */
void bag_success() {
  bag_count += 1;
  
  sprintf(msg_buff, "Another object in the bag! Count: %d objects", bag_count);
  Serial.println(msg_buff);
  
  curr_mask = random_mask();
  reset_pulse_bag();

  // difficuly increase
  level_timeout = level_timeout - level_timeout / LEVEL_TIMEOUT_DIV;

  sprintf(msg_buff, "Hurry up! Timeout is in %lu ms", level_timeout);
  Serial.println(msg_buff);
  
  reset_timer();
}

/*
 * What to do when the time runs out or the item is moved outside of the bag
 */
void bag_fail() {
  sprintf(msg_buff, "Game Over - Score: %d", bag_count);
  Serial.println(msg_buff);
  game_state = GAME_OVER;
}

void game_start() {
  Serial.println("Go!");
  level_timeout = LEVEL_TIMEOUT_DEFAULT;
  bag_count = 0;
  curr_mask = random_mask();
  game_state = GAME;
  reset_timer();
}

void btn1_pressed() {
  static time_millis last_time = 0;
 
  if (async_wait(&last_time, DEBOUNCE_INTERVAL)) {
    return;
  }
  
  if (game_state != GAME) {
    return;
  }

  curr_mask = curr_mask << 1;
}

void btn2_pressed() {
  static time_millis last_time = 0;
 
  if (async_wait(&last_time, DEBOUNCE_INTERVAL)) {
    return;
  }
  
  if (game_state != WELCOME) {
    return;
  }
  
  game_state = GAME_START;
}
