#ifndef L_SYSTEM_SAMODZIELNIE_CANVAS_H
#define L_SYSTEM_SAMODZIELNIE_CANVAS_H
#include "stdint.h"

typedef struct {
    int width;
    int height;
    char** cells;
} Canvas;

Canvas *canvas_create(int width, int height);
void canvas_clear(Canvas *c);
void canvas_set_cell(Canvas *c, int x, int y, char value);
void canvas_print(Canvas *c);
void merge_2_canvases(Canvas* canvas_in, Canvas* canvas_out);
void canvas_destroy(Canvas *c);
int canvas_encoded_size(const Canvas *c);
int canvas_encode(const Canvas *c, uint8_t *buffer);
Canvas* canvas_decode(const uint8_t *buffer, int size);
#endif //L_SYSTEM_SAMODZIELNIE_CANVAS_H
