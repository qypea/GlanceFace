#include <pebble.h>

Window *window;
TextLayer *text_time_layer;
TextLayer *text_date_layer;
TextLayer *text_watchbatt_layer;
TextLayer *text_event_layer;
TextLayer *text_location_layer;
Layer *line_layer;

static AppSync sync;
static uint8_t sync_buffer[512];
enum glanceface_keys {
   EVENT = 1,
   LOCATION = 2,
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
   static char watchbatt_text[] = "UNK";

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
    case EVENT:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(text_event_layer, new_tuple->value->cstring);
      break;

    case LOCATION:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(text_location_layer, new_tuple->value->cstring);
      break;
  }
}

static void sync_error_callback(DictionaryResult dict_error,
                                AppMessageResult app_message_error,
                                void *context) {
   // Error!
   static char error_str[] = "Sync Error";
   static char blank_str[] = "";
   text_layer_set_text(text_event_layer, error_str);
   text_layer_set_text(text_location_layer, blank_str);
}

void handle_init(void) {
   window = window_create();
   window_stack_push(window, true /* Animated */);
   window_set_background_color(window, GColorBlack);

   Layer *window_layer = window_get_root_layer(window);
   GRect bounds = layer_get_bounds(window_layer);

   int smallh = 21;
   int largeh = 51;
   int halfw = bounds.size.w / 2;
   int margin = 8;
   int offy = bounds.origin.y;

   // Date in upper-left
   text_date_layer = text_layer_create(
      GRect(bounds.origin.x,
            offy,
            halfw,
            smallh));
   text_layer_set_text_color(text_date_layer, GColorWhite);
   text_layer_set_background_color(text_date_layer, GColorClear);
   text_layer_set_font(text_date_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   layer_add_child(window_layer, text_layer_get_layer(text_date_layer));

   // Watch battery in upper-right
   text_watchbatt_layer = text_layer_create(
      GRect(bounds.origin.x + halfw,
            offy,
            halfw,
            smallh));
   text_layer_set_text_color(text_watchbatt_layer, GColorWhite);
   text_layer_set_background_color(text_watchbatt_layer, GColorClear);
   text_layer_set_font(text_watchbatt_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   text_layer_set_text_alignment(text_watchbatt_layer, GTextAlignmentRight);
   layer_add_child(window_layer, text_layer_get_layer(text_watchbatt_layer));

   offy = offy + smallh;

   // Time next
   text_time_layer = text_layer_create(
      GRect(bounds.origin.x + margin,
            offy,
            bounds.size.w - margin*2,
            largeh));
   text_layer_set_text_color(text_time_layer, GColorWhite);
   text_layer_set_background_color(text_time_layer, GColorClear);
   text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
   text_layer_set_font(text_time_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
   layer_add_child(window_layer, text_layer_get_layer(text_time_layer));

   offy = offy + largeh + 2;

   // Separator
   GRect line_frame = GRect(bounds.origin.x + margin,
                            offy,
                            bounds.size.w - margin*2,
                            2);
   line_layer = layer_create(line_frame);
   layer_set_update_proc(line_layer, line_layer_update_callback);
   layer_add_child(window_layer, line_layer);

   offy = offy + 2;

   // Event in bottom half -1
   text_event_layer = text_layer_create(
      GRect(bounds.origin.x,
            offy,
            bounds.size.w,
            bounds.size.h - offy - smallh));
   text_layer_set_text_color(text_event_layer, GColorWhite);
   text_layer_set_background_color(text_event_layer, GColorClear);
   text_layer_set_overflow_mode(text_event_layer, GTextOverflowModeFill);
   text_layer_set_font(text_event_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   layer_add_child(window_layer, text_layer_get_layer(text_event_layer));


   // Location in bottom row
   text_location_layer = text_layer_create(
      GRect(bounds.origin.x,
            bounds.size.h - smallh,
            bounds.size.w,
            smallh));
   text_layer_set_text_color(text_location_layer, GColorWhite);
   text_layer_set_background_color(text_location_layer, GColorClear);
   text_layer_set_overflow_mode(text_location_layer, GTextOverflowModeFill);
   text_layer_set_font(text_location_layer,
                       fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
   layer_add_child(window_layer, text_layer_get_layer(text_location_layer));



   // Subscribe to date/time
   tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

   // Subscribe to watchbatt
   battery_state_service_subscribe(handle_watchbatt_change);
   handle_watchbatt_change(battery_state_service_peek());

   // Subscribe to phone data
   const int inbound_size = 256;
   const int outbound_size = 16;
   app_message_open(inbound_size, outbound_size);
   Tuplet initial_values[] = {
       TupletCString(EVENT, "No event synced"),
       //TupletCString(EVENT, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea"),
       TupletCString(LOCATION, ""),
   };
   app_sync_init(&sync, sync_buffer, sizeof(sync_buffer),
                 initial_values, ARRAY_LENGTH(initial_values),
                 sync_tuple_changed_callback,
                 sync_error_callback, NULL);
}

void handle_deinit(void) {
   app_sync_deinit(&sync);
   battery_state_service_unsubscribe();
   tick_timer_service_unsubscribe();
   layer_destroy(line_layer);
   text_layer_destroy(text_time_layer);
   text_layer_destroy(text_date_layer);
   text_layer_destroy(text_watchbatt_layer);
   text_layer_destroy(text_event_layer);
   text_layer_destroy(text_location_layer);
   window_destroy(window);
}

int main(void) {
   handle_init();
   app_event_loop();
   handle_deinit();
}
