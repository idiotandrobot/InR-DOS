#include <pebble.h>

#define TIME_FONT RESOURCE_ID_FONT_PERFECT_DOS_48
#define TIME_TOP 52
#define TIME_TOP_ROUND 58
#define TIME_HEIGHT 50

#define DAY_FONT RESOURCE_ID_FONT_PERFECT_DOS_20
#define DAY_TOP 24
#define DAY_TOP_ROUND 30
#define DAY_HEIGHT 25

#define MONTH_FONT RESOURCE_ID_FONT_PERFECT_DOS_20
#define MONTH_TOP 0
#define MONTH_TOP_ROUND 6
#define MONTH_HEIGHT 25

#define WEATHER_FONT RESOURCE_ID_FONT_PERFECT_DOS_20
#define WEATHER_TOP 116
#define WEATHER_TOP_ROUND 122
#define WEATHER_HEIGHT 25

#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1

#define BATTERY_LEFT 16
#define BATTERY_LEFT_ROUND 34
#define BATTERY_TOP 53
#define BATTERY_TOP_ROUND 59
#define BATTERY_WIDTH 112
#define BATTERY_HEIGHT 2

static Window *s_main_window;

static BitmapLayer *s_background_layer;
static GBitmap *s_background_bitmap;

static GFont s_time_font;
static TextLayer *s_time_layer;

static GFont s_day_font;
static TextLayer *s_day_layer;

static GFont s_month_font;
static TextLayer *s_month_layer;

static GFont s_weather_font;
static TextLayer *s_weather_layer;

static int s_battery_level;
static Layer *s_battery_layer;

static BitmapLayer *s_background_layer, *s_bt_icon_layer;
static GBitmap *s_background_bitmap, *s_bt_icon_bitmap;

static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);

  if(!connected) {
    // Issue a vibrating alert
    vibes_double_pulse();
  }
}

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  
  layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int width = (int)(float)(((float)s_battery_level / 100.0F) * 114.0F);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 0, GCornerNone);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];

  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);

  // If all data is available, use it
  if(temp_tuple && conditions_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%dC", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
    
    // Assemble full string and display
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
    text_layer_set_text(s_weather_layer, weather_layer_buffer);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void update_weather(struct tm *tick_time) {
  if(tick_time->tm_min % 30 == 0) {    
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    dict_write_uint8(iter, 0, 0);

    app_message_outbox_send();
  } 
}

static void update_time(struct tm *tick_time) {
  static char s_time_buffer[6];
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ?
                                       "%H:%M" : "%I:%M", tick_time);
    
  text_layer_set_text(s_time_layer, s_time_buffer); 
}

static void update_day(struct tm *tick_time) {
  static char s_day_buffer[7];
  strftime(s_day_buffer, sizeof(s_day_buffer), "%a %d", tick_time);
  
  text_layer_set_text(s_day_layer, s_day_buffer);
}

static void update_month(struct tm *tick_time) {
  static char s_month_buffer[9];
  strftime(s_month_buffer, sizeof(s_month_buffer), "%b %Y", tick_time);
  
  text_layer_set_text(s_month_layer, s_month_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  update_weather(tick_time);
  update_day(tick_time);
  update_month(tick_time);
}

static void configure_background(Layer *window_layer, GRect bounds) {
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  
  s_background_layer = bitmap_layer_create(bounds);
  
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));  
}

static void configure_time(Layer *window_layer, GRect bounds) {
  s_time_font = fonts_load_custom_font(resource_get_handle(TIME_FONT));
  
  s_time_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(TIME_TOP_ROUND, TIME_TOP), bounds.size.w, 50));
  
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));  
}

static void configure_day(Layer *window_layer, GRect bounds) {
  s_day_font = fonts_load_custom_font(resource_get_handle(DAY_FONT));
  
  s_day_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(DAY_TOP_ROUND, DAY_TOP), bounds.size.w, DAY_HEIGHT));
    
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_color(s_day_layer, GColorWhite);
  text_layer_set_text(s_day_layer, "Sun 01");
  text_layer_set_font(s_day_layer, s_day_font);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));  
}

static void configure_month(Layer *window_layer,GRect bounds) {    
  s_month_font = fonts_load_custom_font(resource_get_handle(MONTH_FONT));
  
  s_month_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(MONTH_TOP_ROUND, MONTH_TOP), bounds.size.w, MONTH_HEIGHT));
    
  text_layer_set_background_color(s_month_layer, GColorClear);
  text_layer_set_text_color(s_month_layer, GColorWhite);
  text_layer_set_text(s_month_layer, "Jan 01");
  text_layer_set_font(s_month_layer, s_month_font);
  text_layer_set_text_alignment(s_month_layer, GTextAlignmentCenter);
  
  layer_add_child(window_layer, text_layer_get_layer(s_month_layer));  
}

static void configure_weather(Layer *window_layer, GRect bounds) {
  
  s_weather_layer = text_layer_create(
    GRect(0, PBL_IF_ROUND_ELSE(WEATHER_TOP_ROUND, WEATHER_TOP), bounds.size.w, 25));
  
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorWhite);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "Config Needed");
  
  s_weather_font = fonts_load_custom_font(resource_get_handle(WEATHER_FONT));
  text_layer_set_font(s_weather_layer, s_weather_font);
  
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));  
}

static void configure_battery(Layer *window_layer, GRect bounds) {
  s_battery_layer = layer_create(
    GRect(PBL_IF_ROUND_ELSE(BATTERY_LEFT_ROUND, BATTERY_LEFT), 
          PBL_IF_ROUND_ELSE(BATTERY_TOP_ROUND, BATTERY_TOP),
          BATTERY_WIDTH, 
          BATTERY_HEIGHT));
  
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  
  layer_add_child(window_layer, s_battery_layer);
}

static void configure_bluetooth(Layer *window_layer) {
  // Create the Bluetooth icon GBitmap
  s_bt_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_ICON);

  // Create the BitmapLayer to display the GBitmap
  s_bt_icon_layer = bitmap_layer_create(GRect(59, 138, 30, 30));
  bitmap_layer_set_bitmap(s_bt_icon_layer, s_bt_icon_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bt_icon_layer));
}
  
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  configure_background(window_layer, bounds);
  configure_time(window_layer, bounds);  
  configure_day(window_layer, bounds);
  configure_month(window_layer, bounds);
  configure_weather(window_layer, bounds);
  configure_battery(window_layer, bounds);
  configure_bluetooth(window_layer);
}

static void deconfigure_background() {
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);
}

static void deconfigure_time() {
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
}

static void deconfigure_day() {
  text_layer_destroy(s_day_layer);
  fonts_unload_custom_font(s_day_font);
}

static void deconfigure_month() {
  text_layer_destroy(s_month_layer);
  fonts_unload_custom_font(s_month_font); 
}

static void deconfigure_weather() {
  text_layer_destroy(s_weather_layer);
  fonts_unload_custom_font(s_weather_font);
}

static void deconfigure_battery() {
  layer_destroy(s_battery_layer);
}

static void deconfigure_bluetooth() {
  gbitmap_destroy(s_bt_icon_bitmap);
  bitmap_layer_destroy(s_bt_icon_layer);
}

static void main_window_unload(Window *window) {
  deconfigure_background();
  deconfigure_time();
  deconfigure_day();
  deconfigure_month();
  deconfigure_weather();
  deconfigure_battery();
  deconfigure_bluetooth();
}

static void init() {
  s_main_window = window_create();
  
  window_set_background_color(s_main_window, GColorBlack);
  
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_stack_push(s_main_window, true);
  
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  update_time(tick_time);
  update_day(tick_time);
  update_month(tick_time);
  
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  battery_callback(battery_state_service_peek());
  
  battery_state_service_subscribe(battery_callback);
  
  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  // Show the correct state of the BT connection from the start
  bluetooth_callback(connection_service_peek_pebble_app_connection());
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}