#pragma once
#include "reset_app.h"
/* Caller owns LVGL lock and calls from one UI task. */
void reset_ui_render(reset_nav_t *nav,const reset_snapshot_t *snapshot,int64_t now,int battery);
