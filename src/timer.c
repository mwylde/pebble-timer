#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"


#define MY_UUID { 0x61, 0x8C, 0xA5, 0x8D, 0xC0, 0xEB, 0x49, 0xDB, 0x98, 0x56, 0x03, 0x40, 0x36, 0xAE, 0xBC, 0x45 }
PBL_APP_INFO(MY_UUID,
             "Timer", "Micah Wylde",
             0, 2, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

enum State {
  DONE,
  SETTING,
  PAUSED,
  COUNTING_DOWN
};


// Main window stuff
Window window;

TextLayer count_down;

enum State current_state = DONE;

int total_seconds = 5 * 60;
int current_seconds = 5 * 60;

// Setting state

enum SettingUnit {
  SETTING_SECOND = 2,
  SETTING_MINUTE = 1,
  SETTING_HOUR = 0,
};

enum SettingUnit setting_unit = SETTING_MINUTE;

void update_time() {
  static char time_text[] = "00:00:00";

  PblTm time;
  time.tm_hour = current_seconds / (60 * 60);
  time.tm_min  = (current_seconds - time.tm_hour * 60 * 60) / 60;
  time.tm_sec  = current_seconds - time.tm_hour * 60 * 60 - time.tm_min * 60;

  string_format_time(time_text, sizeof(time_text), "%T", &time);

  text_layer_set_text(&count_down, time_text);
}


Layer unit_marker;

void draw_setting_unit() {
  layer_mark_dirty(&unit_marker);  
}

void toggle_setting_mode(ClickRecognizerRef recognizer, Window *window) {
  if (current_state == SETTING) {
    current_state = DONE;
  }
  else {
    current_seconds = total_seconds;
    update_time();
    current_state = SETTING;
    setting_unit = SETTING_MINUTE;
    draw_setting_unit();
  }
}

void unit_marker_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  int width = 32;
  int start = 8 + (width + 14) * setting_unit;

  if (current_state == SETTING) {
    graphics_context_set_stroke_color(ctx, GColorBlack);

    graphics_draw_line(ctx, GPoint(start, 96), GPoint(start + width, 96));
    graphics_draw_line(ctx, GPoint(start, 97), GPoint(start + width, 97));
    graphics_draw_line(ctx, GPoint(start, 98), GPoint(start + width, 98));
  }
}


void select_pressed(ClickRecognizerRef recognizer, Window *window) {
  if (current_state == SETTING) {
    setting_unit = (setting_unit + 1) % 3;
    draw_setting_unit();
  }
  else if (current_state == PAUSED || current_state == DONE) {
    current_state = COUNTING_DOWN;
  }
  else {
    current_state = PAUSED;
  }
}

void select_long_release_handler(ClickRecognizerRef recognizer, Window *window) {
  // This is needed to avoid missing clicks. Seems to be a bug in the SDK.
}

void increment_time(int direction) {
  if (current_state == SETTING) {
    switch (setting_unit) {
    case SETTING_HOUR: direction *= 60;
    case SETTING_MINUTE: direction *= 60;
    default: break;
    }

    if (total_seconds + direction >= 0) {
      total_seconds += direction;
      current_seconds = total_seconds;
      update_time();
    }
  }
}


void button_pressed_up(ClickRecognizerRef recognizer, Window *window) {
  increment_time(1);
}

void button_pressed_down(ClickRecognizerRef recognizer, Window *window) {
  increment_time(-1);
}

void reset_timer(ClickRecognizerRef recognizer, Window *window) {
  if (current_state != SETTING) {
    current_state = DONE;
    current_seconds = total_seconds;
    update_time();
  }
}


void main_click_provider(ClickConfig **config, Window *window) {
  // See ui/click.h for more information and default values.

  config[BUTTON_ID_SELECT]->click.handler = 
    (ClickHandler) select_pressed;

  config[BUTTON_ID_SELECT]->long_click.handler = 
    (ClickHandler) toggle_setting_mode;

  config[BUTTON_ID_SELECT]->multi_click.handler = (ClickHandler) reset_timer;
  config[BUTTON_ID_SELECT]->multi_click.min = 2;
  config[BUTTON_ID_SELECT]->multi_click.max = 2;

  config[BUTTON_ID_SELECT]->long_click.release_handler = 
    (ClickHandler) select_long_release_handler;

  config[BUTTON_ID_SELECT]->long_click.delay_ms = 700;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) button_pressed_up;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 300;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) button_pressed_down;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 300;


  (void)window;
}


void handle_second_counting_down() {
  current_seconds--;

  update_time();

  if (current_seconds == 0) {
    vibes_double_pulse();
    current_state = DONE;
  }
}

void handle_second_waiting() {
  current_seconds = total_seconds;
  update_time();
}

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
  switch(current_state) {
  case DONE:
    handle_second_waiting();
    break;
  case COUNTING_DOWN:
    handle_second_counting_down();
    break;
  default:
    break;
  }
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

  resource_init_current_app(&TIMER_RESOURCES);

  window_init(&window, "Main Window");
  window_stack_push(&window, true /* Animated */);

  window_set_click_config_provider(&window, (ClickConfigProvider) main_click_provider);

  GFont custom_font = \
    fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_32));

  text_layer_init(&count_down, GRect(7, 54, 144, 168-54));
  text_layer_set_font(&count_down, custom_font);
  text_layer_set_text(&count_down, "00:00:00");
  layer_add_child(&window.layer, &count_down.layer);

  layer_init(&unit_marker, window.layer.frame);
  unit_marker.update_proc = unit_marker_update_callback;
  layer_add_child(&window.layer, &unit_marker);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);
}
