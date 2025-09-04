#include <game_of_life.h>
#include <time.h>
#include <ui/ui.h>
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
static struct game_of_life world;
static uint32_t world_checksum = 0;

static void cell_update_callback(size_t x, size_t y, uint8_t cell_state) {
	if (cell_state == CELL_STATE_ALIVE_PRESENT) {
		ui_canvas_set_px(x, y);
	} else {
		ui_canvas_clear_px(x, y);
	}

	world_checksum += cell_state * (x + y);
}

int main(void) {
	int ret = 0;
	uint32_t last_world_checksum = 0;
	uint8_t steady_count = 0;

	char *strbuf = k_malloc(SMALL_STRBUF_SIZE);
	if (strbuf == NULL) {
		LOG_ERR("no memory on heap");
		return -ENOMEM;
	}

	ui_init();
	k_msleep(100);	// wait for ui initialised

	game_of_life_init(&world, CANVAS_WIDTH, CANVAS_HEIGHT);
	game_of_life_set_cell_update_callback(&world, cell_update_callback);
	game_of_life_reset(&world);

	ui_set_time_txt("connecting...");
	ui_canvas_fill_noise();
	wifi_simple_connect();

	ret = wifi_simple_wait_online();
	if (ret < 0) {
		LOG_ERR("fail to connect to internet");
		ui_set_time_txt("offline");

	} else {
		LOG_INF("connected to internet");
	}

	for (;;) {
		// print time on display every minute
		if (sysclock_is_synced()) {
			sysclock_time_txt(strbuf, SMALL_STRBUF_SIZE);
		} else {
			strncpy(strbuf, "SNTP error", SMALL_STRBUF_SIZE);
		}
		ui_set_time_txt(strbuf);

		// update game of life world state
		game_of_life_update(&world);
		if (world_checksum == last_world_checksum) {
			steady_count++;
		}
		if (steady_count > 3) {
			game_of_life_reset(&world);
			steady_count = 0;
		}
		last_world_checksum = world_checksum;
		world_checksum = 0;

		k_sleep(K_SECONDS(60));
	}

	k_free(strbuf);
	game_of_life_deinit(&world);

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