#include <lvgl.h>
#include <ui/ui.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(myui, LOG_LEVEL_INF);

struct main_window {
	lv_obj_t *root;
	lv_obj_t *time_txt;
	lv_obj_t *canvas;
} main_win;

static void lvgl_update_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(lvgl_update_work, lvgl_update_work_handler);
K_MUTEX_DEFINE(lvgl_mu);
lv_draw_buf_t *canvas_buf;

static const lv_color32_t black = {
	.alpha = 0,
	.blue = 0,
	.green = 0,
	.red = 0,
};

static const lv_color32_t white = {
	.alpha = 255,
	.blue = 0,
	.green = 0,
	.red = 0,
};

static int ui_create(lv_obj_t *screen_handle) {
	main_win.root = screen_handle;

	main_win.canvas = lv_canvas_create(main_win.root);
	if (main_win.canvas == NULL) {
		LOG_ERR("fail to create canvas");
		return -ENOMEM;
	}
	canvas_buf = lv_draw_buf_create(CANVAS_WIDTH, CANVAS_HEIGHT, LV_COLOR_FORMAT_I1, 0);
	if (canvas_buf == NULL) {
		LOG_ERR("fail to create canvas buffer");
		return -ENOMEM;
	}
	lv_canvas_set_draw_buf(main_win.canvas, canvas_buf);
	lv_obj_align(main_win.canvas, LV_ALIGN_DEFAULT, 0, 0);
	lv_canvas_set_palette(main_win.canvas, 0, black);
	lv_canvas_set_palette(main_win.canvas, 1, white);
	lv_canvas_fill_bg(main_win.canvas, lv_color_black(), LV_OPA_COVER);

	main_win.time_txt = lv_label_create(main_win.root);
	if (main_win.time_txt == NULL) {
		LOG_ERR("fail to create time label");
		return -ENOMEM;
	}
	lv_obj_align_to(main_win.time_txt, main_win.canvas, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_obj_set_y(main_win.time_txt, CANVAS_HEIGHT + 2);

	return 0;
}

int ui_init(void) {
	int ret = 0;
	ret = ui_create(lv_screen_active());
	if (ret < 0) {
		return ret;
	}

	k_work_schedule(&lvgl_update_work, K_NO_WAIT);

	return 0;
}

void ui_set_time_txt(const char *txt) {
	if (main_win.time_txt == NULL) {
		return;
	}

	lv_label_set_text(main_win.time_txt, txt);
}

void ui_canvas_set_px(int x, int y) {
	if (main_win.canvas == NULL) {
		return;
	}

	lv_canvas_set_px(main_win.canvas, x, y, lv_color_white(), LV_OPA_100);
}

void ui_canvas_clear_px(int x, int y) {
	if (main_win.canvas == NULL) {
		return;
	}

	lv_canvas_set_px(main_win.canvas, x, y, lv_color_black(), LV_OPA_100);
}

void ui_canvas_fill_noise(void) {
	for (int y = 0; y < CANVAS_HEIGHT; y++) {
		for (int x = 0; x < CANVAS_WIDTH; x++) {
			if (sys_rand8_get() > 127) {
				ui_canvas_set_px(x, y);
			} else {
				ui_canvas_clear_px(x, y);
			}
		}
	}
}

static void lvgl_update_work_handler(struct k_work *work) {
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);

	k_mutex_lock(&lvgl_mu, K_FOREVER);
	uint32_t sleep_ms = lv_timer_handler();
	k_mutex_unlock(&lvgl_mu);

	if (sleep_ms == LV_NO_TIMER_READY) {
		sleep_ms = CONFIG_LV_DEF_REFR_PERIOD;
	}

	k_work_schedule(dwork, K_MSEC(sleep_ms));
}
