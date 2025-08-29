#include <ui/ui.h>
#include <time.h>
#include <utils/wifi.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static int display_init(const struct device *disp_dev);

/* get SYS_CLOCK_REALTIME time as human readable string*/
static int sysclock_time_txt(char *strbuf, size_t size);

#define SMALL_STRBUF_SIZE 64
static const struct device *const disp_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

int main(void) {
	int ret = 0;

	char *strbuf = k_malloc(SMALL_STRBUF_SIZE);
	if (strbuf == NULL) {
		LOG_ERR("no memory on heap");
		return -ENOMEM;
	}

	ui_init();
	k_msleep(100); // wait for ui initialised

	ui_update_txt("connecting...");
	wifi_simple_connect();

	ret = wifi_simple_wait_online();
	if (ret < 0) {
		LOG_ERR("fail to connect to internet");
		ui_update_txt("offline");

		goto exit;
	}
	LOG_INF("connected to internet");

	// print time on display every minute
	for (;;) {
		sysclock_time_txt(strbuf, SMALL_STRBUF_SIZE);
		ui_update_txt(strbuf);

		k_sleep(K_SECONDS(60));
	}

exit:
	k_free(strbuf);
	return ret;
}

static int sysclock_time_txt(char *strbuf, size_t size) {
	struct timespec ts;

	int ret = sys_clock_gettime(SYS_CLOCK_REALTIME, &ts);
	if (ret < 0) {
		LOG_ERR("fail to get sysclock: %d", ret);
		return ret;
	}

	struct tm *t = localtime(&ts.tv_sec);
	strftime(strbuf, size, "%Y-%m-%d\n%H:%M:%S", t);

	return 0;
}

static int display_fill_white(const struct device *disp_dev) {
	struct display_capabilities cap;
	display_get_capabilities(disp_dev, &cap);

	uint32_t fb_size = cap.x_resolution * cap.y_resolution / 8;
	uint8_t *zeros = k_malloc(fb_size);
	if (zeros == NULL) {
		return -ENOMEM;
	}
	memset(zeros, 0xff, fb_size);

	struct display_buffer_descriptor desc = {
		.buf_size = fb_size,
		.frame_incomplete = false,
		.height = cap.y_resolution,
		.width = cap.x_resolution,
		.pitch = cap.x_resolution,
	};
	display_write(disp_dev, 0, 0, &desc, zeros);

	k_free(zeros);
	return 0;
}

static int display_init(const struct device *disp_dev) {
	if (!device_is_ready(disp_dev)) {
		return -ENODEV;
	}

	// set pixel format to Monochrome
	if (display_set_pixel_format(disp_dev, PIXEL_FORMAT_MONO10) != 0) {
		if (display_set_pixel_format(disp_dev, PIXEL_FORMAT_MONO01) != 0) {
			return -ENOTSUP;
		}
	}

	// clear the screen
	display_fill_white(disp_dev);

	return 0;
}

static int app_perh_init(void) {
	int ret;

	ret = display_init(disp_dev);
	if (ret < 0) {
		return ret;
	}

	wifi_simple_init();

	return 0;
}

SYS_INIT(app_perh_init, APPLICATION, 42);