void reset_timer() {
  level_start_time = millis();
}

void reset_pulse_bag() {
  pulse_intensity = 0;
  pulse_direction = 1;
  pulse_end = false;
}

void update_display() {
  static led_mask_t prev_mask = 0;
  
  if (prev_mask != curr_mask) {
    for (unsigned short i = 0; i < LED_COUNT; i++) {
      if (LEDS[i] == LW) {
        continue;
      }
      digitalWrite(LEDS[i], curr_mask & (1 << (i+1)));
    }

    prev_mask = curr_mask;
  }

  pulse_bag();
}

void pulse_bag() {
  static time_millis last_time = 0;

  if (!(curr_mask & MASK_BAG)) {
    digitalWrite(LW, LOW);
    return;
  }
  
  if (async_wait(&last_time, PULSE_DELAY)) {
    return;
  }
  
  pulse_intensity += pulse_direction * PULSE_DELTA;
  
  if (pulse_intensity >= 255) {
    pulse_direction = -1;
  }

  if (pulse_intensity <= 0) {
    pulse_direction = 1;
    pulse_intensity = 0;
    pulse_end = true;
  }

  #ifdef ARDUINO_AVR_UNO
  analogWrite(LW, pulse_intensity);
  #elif ARDUINO_ARCH_ESP32
  ledcWrite(LW_PWM_CHAN, pulse_intensity);
  #endif
}

boolean async_wait(time_millis *last_time, time_millis wait_time) {
  time_millis curr_time = millis();
  if (curr_time - *last_time < wait_time) {
    return true;
  }
  *last_time = millis();
  return false;
}

led_mask_t random_mask() {
  return 1 << random(1, 4);
}

