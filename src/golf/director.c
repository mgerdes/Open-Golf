#include "golf/director.h"

void golf_director_init(golf_director_t *director) {
	director->state = GD_IN_GAME;

	director->ui = malloc(sizeof(golf_ui_t));
	golf_ui_init(director->ui);

	director->game = malloc(sizeof(golf_game_t));
	golf_game_init(director->game);

	director->renderer = malloc(sizeof(golf_renderer_t));
	golf_renderer_init(director->renderer);
}

void golf_director_update(golf_director_t *director, float dt, struct button_inputs inputs) {
	if (director->state == GD_IN_GAME) {
		golf_ui_update(director->ui, dt, inputs);
		golf_game_update(director->game, dt, inputs);
	}

	renderer_new_frame(director->renderer);
	renderer_end_frame(director->renderer);
}
