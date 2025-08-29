#include <lvgl.h>
#include <ui/ui.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(myui, LOG_LEVEL_INF);

lv_obj_t *label;

static void lvgl_update_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(lvgl_update_work, lvgl_update_work_handler);
K_MUTEX_DEFINE(lvgl_mu);

static int ui_create(void) {
	label = lv_label_create(lv_scr_act());
	if (label == NULL) {
		LOG_ERR("fail to create label");
		return -ENOMEM;
	}
	lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

	return 0;
}

int ui_init(void) {
	int ret = 0;
	
	ret = ui_create();
	if (ret < 0) {
		return ret;
	}

	k_work_schedule(&lvgl_update_work, K_NO_WAIT);

	return 0;
}

void ui_update_txt(const char *txt) {
	lv_label_set_text(label, txt);
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
