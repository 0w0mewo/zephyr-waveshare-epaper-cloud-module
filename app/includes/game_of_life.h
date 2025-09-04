#ifndef __GAME_OF_LIFE_H_H
#define __GAME_OF_LIFE_H_H
#include <stddef.h>
#include <stdint.h>

enum {
	CELL_STATE_DEAD_PRESENT,
	CELL_STATE_ALIVE_PRESENT,
	CELL_STATE_DEAD_NEXT,
	CELL_STATE_ALIVE_NEXT,
	CELL_STATE_MAX
};

typedef void (*game_of_life_cb_t) (size_t x, size_t y, uint8_t cell_state);

struct game_of_life {
	uint8_t *cells;
	size_t size;
	size_t col_max;
	size_t row_max;
    game_of_life_cb_t cell_update_cb;
};


int game_of_life_init(struct game_of_life *w, size_t cols, size_t rows);
void game_of_life_deinit(struct game_of_life *w);
void game_of_life_update(struct game_of_life *w);
void game_of_life_set_cell_update_callback(struct game_of_life *w, game_of_life_cb_t cb);
uint8_t game_of_life_get_cell(struct game_of_life *w, size_t x, size_t y);
void game_of_life_set_cell(struct game_of_life *w, size_t x, size_t y, uint8_t cell_state);
void game_of_life_reset(struct game_of_life *w);

#endif