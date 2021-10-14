#ifndef _GOLF_UI_H
#define _GOLF_UI_H

typedef struct golf_ui {
    int temp;
} golf_ui_t;

golf_ui_t *golf_ui_get(void);
void golf_ui_init(void);
void golf_ui_update(float dt);

#endif
