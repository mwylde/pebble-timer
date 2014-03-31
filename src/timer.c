#include "pebble.h"
#include "time.h"
#include "resource_ids.auto.h"

#define WHITE_ON_BLACK

#ifndef WHITE_ON_BLACK
#define COLOR_FOREGROUND GColorBlack
#define COLOR_BACKGROUND GColorWhite
#else
#define COLOR_FOREGROUND GColorWhite
#define COLOR_BACKGROUND GColorBlack
#endif

#define LONG_CLICK_MS 700
#define MULTI_CLICK_INTERVAL 10

#define TOTAL_SECONDS_KEY 1
#define TOTAL_SECONDS_DEFAULT 5 * 60

enum State {
  DONE,
  SETTING,
  PAUSED,
  COUNTING_DOWN
};


// Main window stuff
Window *window;

TextLayer *title;
TextLayer *count_down;
TextLayer *time_text;

Layer *unit_marker;

enum State current_state = DONE;

int total_seconds;
int current_seconds = 5 * 60;
int last_set_time = -1;

const VibePattern alarm_finished = {
  .durations = (uint32_t []) {300, 150, 150, 150,  300, 150, 300},
  .num_segments = 7
};

// Setting state

enum SettingUnit {
  SETTING_SECOND = 2,
  SETTING_MINUTE = 1,
  SETTING_HOUR = 0,
};

enum SettingUnit setting_unit = SETTING_MINUTE;

void update_countdown() {
  if (current_seconds == last_set_time) {
    return;
  }

  static char time_text[] = " 00:00:00";

  time_t now = time(NULL);
  struct tm *time = localtime(&now);
  
  time->tm_hour = current_seconds / (60 * 60);
  time->tm_min  = (current_seconds - time->tm_hour * 60 * 60) / 60;
  time->tm_sec  = current_seconds - time->tm_hour * 60 * 60 - time->tm_min * 60;

  strftime(time_text, sizeof(time_text), " %T", time);

  text_layer_set_text(count_down, time_text);

  last_set_time = current_seconds;
}


bool setting_blink_state = true;

void draw_setting_unit() {
  layer_mark_dirty(unit_marker);  
}

void toggle_setting_mode(ClickRecognizerRef recognizer, void *window) {
  if (current_state == SETTING) {
    current_state = DONE;
  }
  else {
    current_seconds = total_seconds;
    update_countdown();
    current_state = SETTING;
    setting_unit = SETTING_MINUTE;
    draw_setting_unit();
  }
}

void unit_marker_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  int width = 32;
  int start = 8 + (width + 14) * setting_unit;

  if (current_state == SETTING && setting_blink_state) {
    graphics_context_set_stroke_color(ctx, COLOR_BACKGROUND);

    for (int i = 0; i < 4; i++) {
      graphics_draw_line(ctx, GPoint(start, 95 + i), GPoint(start + width, 95 + i));
    }
  }
}


void select_pressed(ClickRecognizerRef recognizer, void *window) {
  if (current_state == SETTING) {
    setting_unit = (setting_unit + 1) % 3;
    setting_blink_state = true;
    draw_setting_unit();
  }
  else if (current_state == PAUSED || current_state == DONE) {
    current_state = COUNTING_DOWN;
  }
  else {
    current_state = PAUSED;
  }
}

void select_long_release_handler(ClickRecognizerRef recognizer, void *window) {
  // This is needed to avoid missing clicks. Seems to be a bug in the SDK.
}

void increment_time(int direction) {
  if (current_state == SETTING) {
    switch (setting_unit) {
    case SETTING_HOUR: direction *= 60;
    case SETTING_MINUTE: direction *= 60;
    default: break;
    }

    int new_seconds = total_seconds + direction;
    if (new_seconds >= 0 && new_seconds < 100 * 60 * 60) {
      total_seconds = new_seconds;
      current_seconds = total_seconds;
      update_countdown();
    }
  }
}

void button_pressed_up(ClickRecognizerRef recognizer, void *window) {
  increment_time(1);
}

void button_pressed_down(ClickRecognizerRef recognizer, void *window) {
  increment_time(-1);
}

void reset_timer(ClickRecognizerRef recognizer, void *window) {
  if (current_state != SETTING) {
    current_state = DONE;
    current_seconds = total_seconds;
    update_countdown();
  }
}

void main_click_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_pressed);

  window_long_click_subscribe(BUTTON_ID_SELECT, LONG_CLICK_MS,
                              toggle_setting_mode, select_long_release_handler);

  window_multi_click_subscribe(BUTTON_ID_SELECT, 2, 2, 10, true, reset_timer);

  window_single_repeating_click_subscribe(BUTTON_ID_UP, 300, button_pressed_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 300, button_pressed_down);  

}

void handle_second_counting_down() {
  current_seconds--;

  update_countdown();

  if (current_seconds == 0) {
    vibes_enqueue_custom_pattern(alarm_finished);
    current_state = DONE;
  }
}

void handle_second_waiting() {
  current_seconds = total_seconds;
  update_countdown();
}

void handle_second_setting() {
  setting_blink_state = !setting_blink_state;
  layer_mark_dirty(unit_marker);
}

void update_time(struct tm *tick_time) {
  static char time[] = "Xxxxxxxxx - 00 00:00";

  char *time_format;

  if (clock_is_24h_style()) {
    time_format = "%B %e   %R";
  } else {
    time_format = "%B %e   %I:%M";
  }

  strftime(time, sizeof(time), time_format, tick_time);

  text_layer_set_text(time_text, time);
}

void handle_second_tick(struct tm* tick_time, TimeUnits units_changed) {
  switch(current_state) {
  case DONE:
    handle_second_waiting();
    break;
  case COUNTING_DOWN:
    handle_second_counting_down();
    break;
  case SETTING:
    handle_second_setting();
    break;
  default:
    break;
  }

  if (units_changed & MINUTE_UNIT) {
    update_time(tick_time);
  }
}

void handle_init(void) {
  total_seconds = persist_exists(TOTAL_SECONDS_KEY) ?
    persist_read_int(TOTAL_SECONDS_KEY) : TOTAL_SECONDS_DEFAULT;

  window = window_create();
  window_set_fullscreen(window, true);
  window_set_background_color(window, COLOR_BACKGROUND);
  window_stack_push(window, true /* Animated */);

  Layer *window_layer = window_get_root_layer(window);
  
  window_set_click_config_provider(window, (ClickConfigProvider) main_click_provider);

  GFont custom_font = \
    fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_32));

  count_down = text_layer_create(GRect(0, 62, 144, 40));
  text_layer_set_font(count_down, custom_font);
  text_layer_set_text_color(count_down, COLOR_BACKGROUND);
  text_layer_set_background_color(count_down, COLOR_FOREGROUND);
  update_countdown();
  layer_add_child(window_layer, text_layer_get_layer(count_down));

  unit_marker = layer_create(
    layer_get_frame(window_layer));
  layer_set_update_proc(unit_marker, unit_marker_update_callback);
  layer_add_child(window_layer, unit_marker);

  title = text_layer_create(GRect(30, 5, 100, 24));
  text_layer_set_font(title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(title, COLOR_FOREGROUND);
  text_layer_set_background_color(title, GColorClear);
  text_layer_set_text(title, "pebble timer");
  layer_add_child(window_layer, text_layer_get_layer(title));

  time_text = text_layer_create(GRect(20, 130, 110, 24));
  text_layer_set_font(time_text,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(time_text, COLOR_FOREGROUND);
  text_layer_set_background_color(time_text, GColorClear);
  
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  update_time(current_time);
  layer_add_child(window_layer, text_layer_get_layer(time_text));

  tick_timer_service_subscribe(SECOND_UNIT, &handle_second_tick);
}


void handle_deinit() {
  persist_write_int(TOTAL_SECONDS_KEY, total_seconds);
  text_layer_destroy(title);
  text_layer_destroy(count_down);
  text_layer_destroy(time_text);
  layer_destroy(unit_marker);
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

