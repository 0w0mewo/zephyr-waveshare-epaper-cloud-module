#include <time.h>
#include <utils/wifi.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
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
	int ret;

	char *strbuf = k_malloc(SMALL_STRBUF_SIZE);
	if (strbuf == NULL) {
		LOG_ERR("no memory on heap");
		return -ENOMEM;
	}

	wifi_simple_connect();

	ret = wifi_simple_wait_online();
	if (ret < 0) {
		LOG_ERR("fail to connect to internet");
		cfb_print(disp_dev, "OFFLINE", 0, 0);
		cfb_framebuffer_finalize(disp_dev);
		goto exit;
	}
	LOG_INF("connected to internet");

	// print time on display every minute
	for (;;) {
		sysclock_time_txt(strbuf, SMALL_STRBUF_SIZE);
		ret = cfb_print(disp_dev, strbuf, 0, 0);
		if (ret) {
			LOG_ERR("Failed to print a string\n");
			continue;
		}

		// commit changes of the Char FB and update changes to display device
		cfb_framebuffer_finalize(disp_dev);

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
	strftime(strbuf, size, "%Y-%m-%d %H:%M:%S", t);

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

	display_blanking_off(disp_dev);

	// attach the display device to cfb instant and initialise char framebuffer
	if (cfb_framebuffer_init(disp_dev)) {
		return -EIO;
	}

	// clear char framebuffer
	cfb_framebuffer_clear(disp_dev, true);
	cfb_framebuffer_invert(disp_dev);
	cfb_set_kerning(disp_dev, 3);
	cfb_framebuffer_set_font(disp_dev, 1);

	// clear Char FB but do not clear FB of the display device
	cfb_framebuffer_clear(disp_dev, false);

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