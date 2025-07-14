#include "ui.h"
#include <ctype.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

// Input

bool input_write(input_buf *b, char c) {
    if (b->ptr == b->max_length) {
        return false;
    }
    b->buf[b->ptr] = c;
    b->ptr++;
    return true;
}

bool input_erase(input_buf *b) {
    if (b->ptr == 0) {
        return false;
    }
    b->ptr--;
    return true;
}

const char *input_to_text(input_buf *b) {
    return TextFormat("%.*s", b->ptr, b->buf);
}

void input_set_text(input_buf *b, const char *s) {
    int length = (int)strlen(s) <= b->max_length ? (int)strlen(s) : b->max_length;
    strncpy(b->buf, s, length);
    b->ptr = length;
}

bool input_is_blank(input_buf *b) {
    for (int i = 0; i < b->ptr; i++) {
        if (isblank(b->buf[i]) == false) {
            return false;
        }
    }
    return true;
}

void input_trim(input_buf *b) {
    char new[32] = {0};
    int new_ptr = 0;
    int i = 0;
    for (i = 0; i < b->ptr; i++) {
        if (isblank(b->buf[i]) == false) {
            break;
        }
    }
    for (; i < b->ptr; i++) {
        new[new_ptr] = b->buf[i];
        new_ptr++;
    }
    for (i = b->ptr; i > 0; i--) {
        if (isblank(b->buf[i])) {
            new_ptr--;
        } else {
            break;
        }
    }
    b->ptr = new_ptr;
    memcpy(b->buf, new, new_ptr);
}

bool input_clicked(input_buf *b) {
    return CheckCollisionPointRec(GetMousePosition(), b->rec) && IsMouseButtonPressed(0);
}

void input_render(input_buf *b, int active) {
    const char *text = TextFormat("%s: %s", b->prefix, input_to_text(b));
    DrawRectangleLinesEx(b->rec, 1, b->color);
    DrawText(text, b->rec.x + 2, b->rec.y + 2, b->font_size, b->color);
    if (active) {
        int text_width = MeasureText(text, 32);
        DrawRectangle(b->rec.x + text_width + 4, b->rec.y, 2, b->font_size, b->color);
    }
}

// Button

bool button_hover(button *b) {
    return CheckCollisionPointRec(GetMousePosition(), b->rec);
}

bool button_clicked(button *b) {
    return CheckCollisionPointRec(GetMousePosition(), b->rec) && IsMouseButtonPressed(0);
}

void button_render(button *b) {
    Color c = b->color;
    if (CheckCollisionPointRec(GetMousePosition(), b->rec)) {
        c = ColorTint(c, GRAY);
    }
    DrawRectangleRec(b->rec, c);

    int width = MeasureText(b->text, b->font_size);
    DrawText(b->text, b->rec.x + (b->rec.width - width) / 2, b->rec.y + (b->rec.height - b->font_size) / 2,
             b->font_size, WHITE);
}

// Slider

extern Texture2D life_bar_bg;

bool slider_hover(slider *s) {
    return CheckCollisionPointRec(GetMousePosition(), s->rec);
}

void slider_render(slider *s) {
    Rectangle source = {0, 0, life_bar_bg.width, life_bar_bg.height};
    DrawTexturePro(life_bar_bg, source, s->rec, (Vector2){0}, 0, WHITE);

    int percent = s->value / s->max * 100;

    //TODO: Maybe change offsets
    for (int i = 0; i < s->step; i++) {
        Rectangle life_cell = {s->rec.x + 22 + 14 * i, s->rec.y + 14, 8, 12};
        Color c = i * 10 <= percent ? RED : BLUE;
        DrawRectangleRec(life_cell, c);
    }
}
