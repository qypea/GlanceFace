#include <pebble.h>

Window *window;
TextLayer *text_time_layer;
TextLayer *text_date_layer;
TextLayer *text_watchbatt_layer;
TextLayer *text_phonebatt_layer;
TextLayer *text_calendar_layer;
Layer *line_layer;

static AppSync sync;
static uint8_t sync_buffer[256];
enum qpebble_keys {
   BATTERY_LEVEL = 1,
   CALENDAR = 2,
};

void line_layer_update_callback(Layer *layer, GContext* ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
   // Need to be static because they're used by the system later.
   static char time_text[] = "00:00";
   static char date_text[] = "00/00";
   char *time_format;

   // TODO: Only update the date when it's changed.
   strftime(date_text, sizeof(date_text), "%m/%d", tick_time);
   text_layer_set_text(text_date_layer, date_text);

   if (clock_is_24h_style()) {
      time_format = "%R";
   } else {
      time_format = "%I:%M";
   }

   strftime(time_text, sizeof(time_text), time_format, tick_time);

   // Kludge to handle lack of non-padded hour format string
   // for twelve hour clock.
   if (!clock_is_24h_style() && (time_text[0] == '0')) {
      memmove(time_text, &time_text[1], sizeof(time_text) - 1);
   }

   text_layer_set_text(text_time_layer, time_text);
}

void handle_watchbatt_change(BatteryChargeState state) {
   // Need to be static because they're used by the system later.
   static char watchbatt_text[] = "100%";

   if (state.is_charging) {
      snprintf(watchbatt_text, 4, "CHR");
   } else if (state.charge_percent > 30) {
      snprintf(watchbatt_text, 4, " ");
   } else {
      snprintf(watchbatt_text, 4, "%d%%", state.charge_percent);
   }
   text_layer_set_text(text_watchbatt_layer, watchbatt_text);
}

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {
   switch (key) {
      case BATTERY_LEVEL: {
         static char phonebatt_text[] = "100%";
         uint8_t level = new_tuple->value->uint8;
           if (level == 255) {
              snprintf(phonebatt_text, 4, "Unk");
         } else if (level == 254) {
            snprintf(phonebatt_text, 4, "CHR");
         } else if (level > 60) {
            snprintf(phonebatt_text, 4, " ");
         } else {
            snprintf(phonebatt_text, 4, "%d%%", level);
         }
         text_layer_set_text(text_phonebatt_layer, phonebatt_text);
         break;
      }

    case CALENDAR:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(text_calendar_layer, new_tuple->value->cstring);
      break;
  }
}

void handle_init(void) {
   window = window_create();
   window_stack_push(window, true /* Animated */);
   window_set_background_color(window, GColorBlack);

   Layer *window_layer = window_get_root_layer(window);

   text_date_layer = text_layer_create(GRect(0, 0, 60, 22));
   text_layer_set_text_color(text_date_layer, GColorWhite);
   text_layer_set_background_color(text_date_layer, GColorClear);
   text_layer_set_font(text_date_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

   text_watchbatt_layer = text_layer_create(GRect(62, 0, 40, 22));
   text_layer_set_text_color(text_watchbatt_layer, GColorWhite);
   text_layer_set_background_color(text_watchbatt_layer, GColorClear);
   text_layer_set_font(text_watchbatt_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   text_layer_set_text_alignment(text_watchbatt_layer, GTextAlignmentCenter);
   layer_add_child(window_layer, text_layer_get_layer(text_watchbatt_layer));

   text_phonebatt_layer = text_layer_create(GRect(104, 0, 40, 22));
   text_layer_set_text_color(text_phonebatt_layer, GColorWhite);
   text_layer_set_background_color(text_phonebatt_layer, GColorClear);
   text_layer_set_font(text_phonebatt_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   text_layer_set_text_alignment(text_phonebatt_layer, GTextAlignmentRight);
   layer_add_child(window_layer, text_layer_get_layer(text_phonebatt_layer));

   text_time_layer = text_layer_create(GRect(8, 22, 144-16, 50));
   text_layer_set_text_color(text_time_layer, GColorWhite);
   text_layer_set_background_color(text_time_layer, GColorClear);
   text_layer_set_text_alignment(text_watchbatt_layer, GTextAlignmentCenter);
   text_layer_set_font(text_time_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
   layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

   GRect line_frame = GRect(8, 75, 144-16, 2);
   line_layer = layer_create(line_frame);
   layer_set_update_proc(line_layer, line_layer_update_callback);
   layer_add_child(window_layer, line_layer);

   text_calendar_layer = text_layer_create(GRect(0, 78, 144, 168 - 76));
   text_layer_set_text_color(text_calendar_layer, GColorWhite);
   text_layer_set_background_color(text_calendar_layer, GColorClear);
   text_layer_set_overflow_mode(text_calendar_layer, GTextOverflowModeFill);
   text_layer_set_font(text_calendar_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   layer_add_child(window_layer, text_layer_get_layer(text_calendar_layer));

   // Subscribe to date/time
   tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

   // Subscribe to watchbatt
   battery_state_service_subscribe(handle_watchbatt_change);
   handle_watchbatt_change(battery_state_service_peek());

   // TODO: Subscribe to phone data
   const int inbound_size = 256;
   const int outbound_size = 16;
   app_message_open(inbound_size, outbound_size);
   Tuplet initial_values[] = {
       TupletInteger(BATTERY_LEVEL, 255),
       TupletCString(CALENDAR, "None!!"),
   };
   app_sync_init(&sync, sync_buffer, sizeof(sync_buffer),
                 initial_values, ARRAY_LENGTH(initial_values),
                 sync_tuple_changed_callback,
                 NULL /*sync_error_callback*/, NULL);
}

void handle_deinit(void) {
   tick_timer_service_unsubscribe();
   layer_destroy(line_layer);
   text_layer_destroy(text_time_layer);
   text_layer_destroy(text_date_layer);
   text_layer_destroy(text_watchbatt_layer);
   text_layer_destroy(text_phonebatt_layer);
   text_layer_destroy(text_calendar_layer);
   window_destroy(window);
}

int main(void) {
   handle_init();
   app_event_loop();
   handle_deinit();
}
