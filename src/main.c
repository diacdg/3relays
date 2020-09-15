/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stdio.h"
#include "string.h"
#include "mgos.h"
#include "mgos_http_server.h"
#include "common/json_utils.h"
#include "common/str_util.h"

#define LED_PIN 16
#define HIGH 1
#define LOW 0
#define RELAYS_COUNT 3
#define MGOS_F_RELOAD_CONFIG MG_F_USER_5
#define JSON_HEADERS "Connection: close\r\nContent-Type: application/json"
#define LISTENER_SPEC "80"

int relaysPinMap[RELAYS_COUNT] = {12,13,14};

void sendRelayStatusResponse(struct mg_connection *c, int relay_index, int status)
{
	mg_send_response_line(c, 200,"Content-Type: application/json\r\n");
  	mg_printf(c, "{\"relay\":%d,\"status\":%d}\r\n", relay_index+1, status);
  	c->flags |= MG_F_SEND_AND_CLOSE;
}

static void set_on_relay_handler(struct mg_connection *c, int ev, void *p, void *relay_index) {
	if (ev != MG_EV_HTTP_REQUEST) return;
	
	mgos_gpio_write(relaysPinMap[*((int*)relay_index)], HIGH);
  
  	sendRelayStatusResponse(c, *((int*)relay_index), mgos_gpio_read(relaysPinMap[*((int*)relay_index)]));
}

static void set_off_relay_handler(struct mg_connection *c, int ev, void *p, void *relay_index) {
	if (ev != MG_EV_HTTP_REQUEST) return;
	
	mgos_gpio_write(relaysPinMap[*((int*)relay_index)], LOW);
  
  	sendRelayStatusResponse(c, *((int*)relay_index), mgos_gpio_read(relaysPinMap[*((int*)relay_index)]));
}

static void relay_status_handler(struct mg_connection *c, int ev, void *p, void *relay_index) {
	if (ev != MG_EV_HTTP_REQUEST) return;
	  
  	sendRelayStatusResponse(c, *((int*)relay_index), mgos_gpio_read(relaysPinMap[*((int*)relay_index)]));
}

static void device_status_handler(struct mg_connection *c, int ev, void *p, void *user_data) {
	if (ev != MG_EV_HTTP_REQUEST) return;
	
	int i;
	struct mbuf jsmb;
  	struct json_out jsout = JSON_OUT_MBUF(&jsmb);
  	mbuf_init(&jsmb, 0);
	
	json_printf(&jsout, "{uptime: %.2lf,ram: %lu, free_ram: %lu, relays: [", mgos_uptime(), (unsigned long) mgos_get_heap_size(), (unsigned long) mgos_get_free_heap_size());
	
	for (i=0; i<RELAYS_COUNT; i++) {
		json_printf(&jsout, "{relay: %d, status: %d}", i+1, mgos_gpio_read(relaysPinMap[i]));
		
		if (RELAYS_COUNT-1 != i) {
			json_printf(&jsout, ",");
		}
	}
	
	json_printf(&jsout, "]}");
	  
  	if (jsmb.len > 0) {
    		mg_send_head(c, 200, jsmb.len, JSON_HEADERS);
    		mg_send(c, jsmb.buf, jsmb.len);
  	}
  	c->flags |= MG_F_SEND_AND_CLOSE;
  	mbuf_free(&jsmb);
  	
  	(void) user_data;
}


static void timer_cb(void *arg) {
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
#ifdef LED_PIN
  mgos_gpio_toggle(LED_PIN);
#endif
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {

struct mg_connection *mgos_get_sys_http_server();

int i;
int *relayIndex;
char path [100];

for (i=0; i<RELAYS_COUNT; i++) {
	relayIndex = malloc(sizeof(int));
	*relayIndex = i;
	LOG(LL_INFO, ("Setup pin %d, pin index %d", relaysPinMap[i], i));
	mgos_gpio_setup_output(relaysPinMap[i], 0);
	
	sprintf(path, "/relay/%d/on", i+1);
	mgos_register_http_endpoint(path, set_on_relay_handler, relayIndex);
	
	sprintf(path, "/relay/%d/off", i+1);
	mgos_register_http_endpoint(path, set_off_relay_handler, relayIndex);
	
	sprintf(path, "/relay/%d/status", i+1);
	mgos_register_http_endpoint(path, relay_status_handler, relayIndex);
}

mgos_register_http_endpoint("/status", device_status_handler, NULL);

#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif
  mgos_set_timer(10000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
