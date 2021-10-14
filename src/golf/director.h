#ifndef _GOLF_DIRECTOR_H
#define _GOLF_DIRECTOR_H

typedef enum golf_director_state {
	GD_IN_GAME,
} golf_director_state_t;

typedef struct golf_director {
	golf_director_state_t state;

	golf_ui_t *ui;
	golf_game_t *game;
	golf_renderer_t *renderer;
} golf_director_t;

void golf_director_init(golf_director_t *director);
void golf_director_update(golf_director_t *director, float dt, struct button_inputs inputs);

#endif
