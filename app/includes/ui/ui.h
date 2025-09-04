#ifndef __UI_H_H
#define __UI_H_H

#include <lvgl.h>

#define CANVAS_WIDTH (LV_HOR_RES - 2)
#define CANVAS_HEIGHT (LV_VER_RES / 2)

int ui_init(void);
void ui_set_time_txt(const char *txt);
void ui_canvas_set_px(int x, int y);
void ui_canvas_clear_px(int x, int y);
void ui_canvas_fill_noise(void);

#endif
