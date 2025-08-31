#include <utils/wifi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_l2.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/wifi_mgmt.h>

LOG_MODULE_REGISTER(wifi_utils, LOG_LEVEL_INF);

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define WIFI_SCAN_EVENT_MASK (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE)

#define MAX_WIFI_SCAN_RESULTS 10
#define MAX_WIFI_SCAN_TIME_SEC 20
#define MAX_WIFI_INET_WAIT_SEC 200

#define FMT_MAC_ADDR "%x:%x:%x:%x:%x:%x"
#define MAC_ADDR(x) x[0], x[1], x[2], x[3], x[4], x[5]

enum {
	WIFI_SIMPLE_CONNECTED,
	WIFI_SIMPLE_SNTP_RESYNCED,
};

struct wifi_simple_scan_results {
	struct wifi_scan_result *res;
	uint8_t count;
	struct k_mutex mu;
};

static int wifi_simple_scan_results_init(struct wifi_simple_scan_results *scan_results) {
	scan_results->res = k_malloc(MAX_WIFI_SCAN_RESULTS * sizeof(struct wifi_scan_result));
	if (scan_results->res == NULL) {
		LOG_ERR("fail to allocate memory for wifi scan results");
		return -ENOMEM;
	}

	k_mutex_init(&scan_results->mu);
	scan_results->count = 0;

	return 0;
}

static void wifi_simple_scan_results_deinit(struct wifi_simple_scan_results *scan_results) {
	k_free(scan_results->res);
}

static void wifi_simple_scan_results_reset(struct wifi_simple_scan_results *scan_results) {
	memset(scan_results->res, 0, MAX_WIFI_SCAN_RESULTS * sizeof(struct wifi_scan_result));
	scan_results->count = 0;
}

K_SEM_DEFINE(inet_ready, 0, 1);
K_SEM_DEFINE(scan_done, 0, 1);

static atomic_t flags;
static struct wifi_simple_scan_results scan_results;

static void sysclock_resync_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(sysclock_resync_work, sysclock_resync_work_handler);

void wifi_simple_connect(void) {
	struct net_if *iface = net_if_get_first_wifi();
	if (iface == NULL) {
		LOG_ERR("no wifi intf");
		return;
	}

	struct wifi_connect_req_params wifi_params = {
		.ssid = CONFIG_SSID,
		.psk = CONFIG_PSK,
		.ssid_length = strlen(CONFIG_SSID),
		.psk_length = strlen(CONFIG_PSK),
		.channel = WIFI_CHANNEL_ANY,
		.security = WIFI_SECURITY_TYPE_PSK,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
		.mfp = WIFI_MFP_OPTIONAL,
	};

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &wifi_params, sizeof(struct wifi_connect_req_params))) {
		LOG_ERR("NET_REQUEST_WIFI_CONNECT failed");
	}
}

void wifi_simple_disconnect(void) {
	struct net_if *iface = net_if_get_first_wifi();
	if (iface == NULL) {
		LOG_ERR("no wifi intf");
		return;
	}

	if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)) {
		LOG_ERR("NET_REQUEST_WIFI_DISCONNECT failed");
	}
}

static void handle_connect(void) {
	LOG_INF("network connected");

	// sync systime without wait
	k_work_reschedule(&sysclock_resync_work, K_NO_WAIT);
}

static void handle_disconnect(void) {
	LOG_INF("network disconnected");

	// cancel further systime resync task
	k_work_cancel_delayable(&sysclock_resync_work);
}

static struct net_mgmt_event_callback inet_connectivity_cb;
static void inet_connectivity_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
	if ((mgmt_event & L4_EVENT_MASK) != mgmt_event) {
		return;
	}

	switch (mgmt_event) {
		case NET_EVENT_L4_CONNECTED: {
			atomic_set_bit(&flags, WIFI_SIMPLE_CONNECTED);
			k_sem_give(&inet_ready);

			handle_connect();
		} break;

		case NET_EVENT_L4_DISCONNECTED: {
			atomic_clear_bit(&flags, WIFI_SIMPLE_CONNECTED);
			k_sem_reset(&inet_ready);

			handle_disconnect();
		} break;

		default:
			break;
	}
}

static struct net_mgmt_event_callback wifi_scan_cb;
static void wifi_scan_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
	if ((mgmt_event & WIFI_SCAN_EVENT_MASK) != mgmt_event) {
		return;
	}

	switch (mgmt_event) {
		case NET_EVENT_WIFI_SCAN_DONE: {
			k_sem_give(&scan_done);
		} break;

		case NET_EVENT_WIFI_SCAN_RESULT: {
			k_mutex_lock(&scan_results.mu, K_FOREVER);
			if (scan_results.count < MAX_WIFI_SCAN_RESULTS) {
				const struct wifi_scan_result *res = (const struct wifi_scan_result *)cb->info;

				memcpy(&scan_results.res[scan_results.count], res, MIN(cb->info_length, sizeof(struct wifi_scan_result)));
				scan_results.count++;
			} else {
				LOG_WRN("dropping scan results");
			}
			k_mutex_unlock(&scan_results.mu);
		} break;

		default:
			break;
	}
}

int wifi_simple_print_scan(void) {
	int ret;
	const struct wifi_scan_params scan_params = {0};
	struct net_if *iface = net_if_get_first_wifi();

	// it's an one shot operation, so we register and unregister the event callback
	// while scan starts and scan finishes
	net_mgmt_add_event_callback(&wifi_scan_cb);

	ret = wifi_simple_scan_results_init(&scan_results);
	if (ret < 0) {
		goto exit_nomem;
	}

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, (void *)(&scan_params), sizeof(scan_params)) < 0) {
		ret = -EIO;
		goto exit;
	}

	LOG_INF("scan requested, waitting for results");
	if (k_sem_take(&scan_done, K_SECONDS(MAX_WIFI_SCAN_TIME_SEC)) < 0) {
		ret = -ETIMEDOUT;
		goto exit;
	}

	LOG_INF("found %d APs", scan_results.count);

	struct wifi_scan_result *res = scan_results.res;
	for (uint8_t i = 0; i < scan_results.count; i++) {
		printk("%s(" FMT_MAC_ADDR ") | %-6s(%u) | %-4d | %-20s\n",
			   res[i].ssid, MAC_ADDR(res[i].mac),
			   wifi_band_txt(res[i].band), res[i].channel,
			   res[i].rssi, wifi_security_txt(res[i].security));
	}

exit:
	wifi_simple_scan_results_reset(&scan_results);
	wifi_simple_scan_results_deinit(&scan_results);
exit_nomem:
	net_mgmt_del_event_callback(&wifi_scan_cb);
	return ret;
}

void wifi_simple_init(void) {
	net_mgmt_init_event_callback(&inet_connectivity_cb, inet_connectivity_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&inet_connectivity_cb);

	net_mgmt_init_event_callback(&wifi_scan_cb, wifi_scan_event_handler, WIFI_SCAN_EVENT_MASK);

	flags = ATOMIC_INIT(0);
}

int wifi_simple_wait_online(void) {
	if (wifi_simple_is_connected()) {
		return 0;
	}

	return k_sem_take(&inet_ready, K_SECONDS(MAX_WIFI_INET_WAIT_SEC));
}

bool wifi_simple_is_connected(void) {
	return atomic_test_bit(&flags, WIFI_SIMPLE_CONNECTED);
}

/* sync SYS_CLOCK_REALTIME with sntp */
int sysclock_sync_sntp(void) {
	struct sntp_time ts;
	struct timespec tspec;

	int res = sntp_simple(CONFIG_MY_SNTP_SERVER, 3000, &ts);
	if (res < 0) {
		LOG_ERR("Cannot set time using SNTP");
		return res;
	}

	tspec.tv_sec = ts.seconds;
	tspec.tv_nsec = ((uint64_t)ts.fraction * NSEC_PER_SEC) >> 32;
	res = sys_clock_settime(SYS_CLOCK_REALTIME, &tspec);
	if (res < 0) {
		LOG_ERR("fail to set sysclock: %d", res);
	}

	return res;
}

bool sysclock_is_synced(void) {
	return atomic_test_bit(&flags, WIFI_SIMPLE_SNTP_RESYNCED);
}

static void sysclock_resync_work_handler(struct k_work *work) {
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	int ret = sysclock_sync_sntp();
	if (ret < 0) {
		LOG_ERR("fail to resync clock: %d", ret);
		atomic_clear_bit(&flags, WIFI_SIMPLE_SNTP_RESYNCED);
	} else {
		LOG_INF("time resync");
		atomic_set_bit(&flags, WIFI_SIMPLE_SNTP_RESYNCED);
	}

	k_work_reschedule(dwork, ret < 0 ? K_SECONDS(5) : K_SECONDS(3600));
}
