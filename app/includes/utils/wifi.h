#ifndef __WIFI_UTILS_H_H
#define __WIFI_UTILS_H_H

#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>

/* connect to wifi with preconfig SSID and PSK */
void wifi_simple_connect(void);

/* disconnect wifi */
void wifi_simple_disconnect(void);

/* block and wait for wifi connected and IP address assigned */
int wifi_simple_wait_online(void);

/* initialise wifi stack */
void wifi_simple_init(void);

/* check if wifi is connected to an AP */
bool wifi_simple_is_connected(void);

/* one shoot scan */
int wifi_simple_print_scan(void);

/* sync SYS_CLOCK_REALTIME with sntp */
int sysclock_sync_sntp(void);

/* is SYS_CLOCK_REALTIME last synced successfully*/
bool sysclock_is_synced(void);

#endif