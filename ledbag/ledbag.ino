typedef unsigned short led_mask_t;
typedef unsigned long time_millis;

#ifdef ARDUINO_AVR_UNO
const unsigned short LG3 = 12,
                     LG2 = 11,
                     LG1 = 8,
                     LW = 9,
                     LR = 7,
                     LED_COUNT = 5,
                     BTN1 = 2,
                     BTN2 = 3;
#elif ARDUINO_ARCH_ESP32
const unsigned short LG3 = 32,
                     LG2 = 31,
                     LG1 = 25,
                     LW = 26,
                     LW_PWM_CHAN = 0,
                     LR = 27,
                     LED_COUNT = 5,
                     BTN1 = 34,
                     BTN2 = 35;
#endif

const unsigned short LEDS[] = {LG3, LG2, LG1, LW, LR};

const unsigned short POT = 1;
const unsigned int POT_MAX = 1024;

const unsigned int DEBOUNCE_INTERVAL = 80;

time_millis DELAY_GAME_START = 1000,
            DELAY_GAME_OVER = 2000,
            DELAY_WELCOME = 200;

const unsigned short PULSE_DELAY = 1,
                     PULSE_DELTA = 1;

const unsigned short MASK_OFF = 1,
                     MASK_BAG = 1 << 4,
                     MASK_FAIL = 1 << 5;

enum game_states {
  WELCOME,
  GAME_START,
  GAME,
  GAME_OVER
};

const time_millis LEVEL_TIMEOUT_DEFAULT = 3000,
                  LEVEL_MULTIPLIER = 250;
                  
const unsigned short LEVEL_TIMEOUT_DIV = 8,
                     LEVEL_NUM = 8;

led_mask_t curr_mask;

unsigned short game_state,
               welcome_state,
               bag_count;

time_millis level_start_time,
            level_timeout = LEVEL_TIMEOUT_DEFAULT;

char msg_buff[70];

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

  #ifdef ARDUINO_ARCH_ESP32
  // provides 3.3V power for buttons
  digitalWrite(13, HIGH);

  ledcAttachPin(LW, LW_PWM_CHAN);
  ledcSetup(LW_PWM_CHAN, 5000, 8);
  #endif
  
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
  welcome_state = 0;
  reset_pulse_bag();
  Serial.println("Welcome to Led to Bag. Press Key TS to Start");
}

void state_welcome() {
  static time_millis last_time = 0;
  
  if (async_wait(&last_time, DELAY_WELCOME)) {
    return;
  }

  unsigned int pot_value = analogRead(POT);
  unsigned short level = pot_value * LEVEL_NUM / POT_MAX;
  time_millis new_timeout = LEVEL_TIMEOUT_DEFAULT - (LEVEL_MULTIPLIER * level);
  if (level_timeout != new_timeout) {
    level_timeout = new_timeout;
    sprintf(msg_buff, "Selected level %d. Timeout is now %lu ms", level+1, level_timeout);
    Serial.println(msg_buff);
  }
  
  if (welcome_state == 3) {
    welcome_state = 0;
    curr_mask = MASK_OFF;
  }
  curr_mask = curr_mask << 1;
  welcome_state++;
  update_display();
}

void state_game_start() {
  curr_mask = MASK_OFF;
  update_display();
  
  delay(DELAY_GAME_START);

  game_start();
}

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

void state_game_over() {
  curr_mask = MASK_FAIL;
  update_display();
  
  delay(DELAY_GAME_OVER);

  game_reset();
}

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

