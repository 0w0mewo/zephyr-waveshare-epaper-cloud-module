#include <game_of_life.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

LOG_MODULE_REGISTER(game_of_life, LOG_LEVEL_INF);

// how many cells can be encoded in one byte
#define CELLS_PER_BYTE (BITS_PER_BYTE / CELL_STATE_MAX)

// how many bits to encode a cell state
#define BITS_PER_CELL (LOG2CEIL(CELL_STATE_MAX))

int game_of_life_init(struct game_of_life *w, size_t cols, size_t rows) {
	w->size = ROUND_UP(rows * cols, CELLS_PER_BYTE) / CELLS_PER_BYTE;
	w->cells = (uint8_t *)k_malloc(w->size);
	if (w->cells == NULL) {
		return -ENOMEM;
	}
	w->col_max = cols;
	w->row_max = rows;
	memset(w->cells, CELL_STATE_DEAD_PRESENT, w->size);

	LOG_INF("allocated %d byte for the world\n", w->size);
	LOG_INF("world size: %dx%d", w->row_max, w->col_max);
	return 0;
}

void game_of_life_reset(struct game_of_life *w) {
	for (size_t y = 0; y < w->row_max; y++) {
		for (size_t x = 0; x < w->col_max; x++) {
			uint8_t random_cell_state = sys_rand8_get() > 128 ? CELL_STATE_ALIVE_PRESENT : CELL_STATE_DEAD_PRESENT;
			game_of_life_set_cell(w, x, y, random_cell_state);
		}
	}
}

void game_of_life_deinit(struct game_of_life *w) {
	k_free(w->cells);
}

static int game_of_life_get_cell_index(struct game_of_life *w, size_t x, size_t y, size_t *bit_idx, size_t *byte_idx) {
	size_t cell_idx = (w->col_max * y + x);
	*byte_idx = (cell_idx * BITS_PER_CELL) / BITS_PER_BYTE;
	*bit_idx = (cell_idx * BITS_PER_CELL) % BITS_PER_BYTE;

	return ((*byte_idx < w->size) ? 0 : -EINVAL);
}

uint8_t game_of_life_get_cell(struct game_of_life *w, size_t x, size_t y) {
	if (x >= w->col_max || y >= w->row_max) {
		return CELL_STATE_MAX;
	}

	size_t byte_idx, bit_idx;
	if (game_of_life_get_cell_index(w, x, y, &bit_idx, &byte_idx) < 0) {
		LOG_DBG("get cell out of range");

		return CELL_STATE_MAX;
	}

	uint8_t byte = w->cells[byte_idx];
	uint8_t cell_state = (IS_BIT_SET(byte, bit_idx) << 1) + IS_BIT_SET(byte, bit_idx + 1);
	return cell_state;
}

void game_of_life_set_cell(struct game_of_life *w, size_t x, size_t y, uint8_t cell_state) {
	if (x >= w->col_max || y >= w->row_max || cell_state >= CELL_STATE_MAX) {
		return;
	}

	size_t byte_idx, bit_idx;
	if (game_of_life_get_cell_index(w, x, y, &bit_idx, &byte_idx) < 0) {
		LOG_DBG("set cell out of range");

		return;
	}

	WRITE_BIT(w->cells[byte_idx], bit_idx, (cell_state & 0b10) >> 1);
	WRITE_BIT(w->cells[byte_idx], bit_idx + 1, cell_state & 0b01);
}

static size_t game_of_life_cell_neigbors_count(struct game_of_life *w, size_t x, size_t y) {
	size_t live = 0;
	/*
		x: neigbors, o: self
		x x x
		x o x
		x x x
	*/
	for (int8_t dy = -1; dy < 2; dy++) {
		for (int8_t dx = -1; dx < 2; dx++) {
			// ignore itself
			if (dx == 0 && dy == 0) {
				continue;
			}

			uint8_t cell_state = game_of_life_get_cell(w, x + dx, y + dy);
			if (cell_state != CELL_STATE_MAX) {
				if (cell_state == CELL_STATE_ALIVE_PRESENT || cell_state == CELL_STATE_DEAD_NEXT) {
					live++;
				}
			}
		}
	}

	return live++;
}

static uint8_t cell_state_rule(uint8_t state, size_t neigbors_count) {
	if (state == CELL_STATE_ALIVE_PRESENT) {
		if (neigbors_count < 2 || neigbors_count > 3) {
			return CELL_STATE_DEAD_NEXT;
		}
	} else if (state == CELL_STATE_DEAD_PRESENT) {
		if (neigbors_count == 3) {
			return CELL_STATE_ALIVE_NEXT;
		}
	}

	return state;
}

void game_of_life_update(struct game_of_life *w) {
	for (size_t y = 0; y < w->row_max; y++) {
		for (size_t x = 0; x < w->col_max; x++) {
			size_t alives = game_of_life_cell_neigbors_count(w, x, y);
			uint8_t next_state = cell_state_rule(game_of_life_get_cell(w, x, y), alives);
			game_of_life_set_cell(w, x, y, next_state);
		}
	}

	for (size_t y = 0; y < w->row_max; y++) {
		for (size_t x = 0; x < w->col_max; x++) {
			uint8_t cell_state = game_of_life_get_cell(w, x, y);
			if (cell_state == CELL_STATE_ALIVE_NEXT) {
				cell_state = CELL_STATE_ALIVE_PRESENT;
			}
			if (cell_state == CELL_STATE_DEAD_NEXT) {
				cell_state = CELL_STATE_DEAD_PRESENT;
			}

			game_of_life_set_cell(w, x, y, cell_state);

			if (w->cell_update_cb != NULL) {
				w->cell_update_cb(x, y, cell_state);
			}
		}
	}
}
void game_of_life_set_cell_update_callback(struct game_of_life *w, game_of_life_cb_t cb) {
	w->cell_update_cb = cb;
}