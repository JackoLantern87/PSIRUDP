#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas.h"

Canvas *canvas_create(int width, int height) {
    Canvas *c = malloc(sizeof(Canvas));
    c->width = width;
    c->height = height;
    c->cells = malloc(height * sizeof(char *));

    for (int y = 0; y < height; y++) {
        c->cells[y] = malloc(width);
        for (int x = 0; x < width; x++)
            c->cells[y][x] = ' ';
    }
    return c;
}

void canvas_clear(Canvas *c) {
    for (int y = 0; y < c->height; y++)
        for (int x = 0; x < c->width; x++)
            c->cells[y][x] = ' ';
}

void canvas_set_cell(Canvas *c, int x, int y, char value) {
    if (x >= 0 && x < c->width && y >= 0 && y < c->height)
        c->cells[y][x] = value;
}

void canvas_print(Canvas *c) {
    for (int y = 0; y < c->height; y++) {
        for (int x = 0; x < c->width; x++)
            putchar(c->cells[y][x]);
        putchar('\n');
    }
}

void merge_2_canvases(Canvas* canvas_in, Canvas* canvas_out){
    int height = canvas_out->height;
    int width = canvas_out->width;
    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++){
            char c = canvas_in->cells[y][x];
            if(c != ' ')
                canvas_set_cell(canvas_out, x, y, c);
        }
    }
}

void canvas_destroy(Canvas *c) {
    for (int y = 0; y < c->height; y++)
        free(c->cells[y]);
    free(c->cells);
    free(c);
}

static void write_u16(uint8_t *buf, uint16_t v) {
    buf[0] = (v >> 8) & 0xff;
    buf[1] = v & 0xff;
}

static uint16_t read_u16(const uint8_t *buf) {
    return (buf[0] << 8) | buf[1];
}

int canvas_encoded_size(const Canvas *c) {
    return 4 + c->width * c->height;
}

int canvas_encode(const Canvas *c, uint8_t *buffer) {
    if (c->width <= 0 || c->width > 65535) return -1;
    if (c->height <= 0 || c->height > 65535) return -1;
    write_u16(buffer, c->width);
    write_u16(buffer + 2, c->height);

    int offset = 4;
    for (int y = 0; y < c->height; y++) {
        memcpy(buffer + offset, c->cells[y], c->width);
        offset += c->width;
    }
    return offset;
}

Canvas* canvas_decode(const uint8_t *buffer, int size) {
    if (size < 4)
        return NULL;

    int width = read_u16(buffer);
    int height = read_u16(buffer + 2);

    if (size < 4 + width * height)
        return NULL;

    Canvas *c = canvas_create(width, height);

    int offset = 4;
    for (int y = 0; y < height; y++) {
        memcpy(c->cells[y], buffer + offset, width);
        offset += width;
    }
    return c;
}