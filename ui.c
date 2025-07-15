#include "ui.h"
#include <ctype.h>
#include <raylib.h>
#include <stdbool.h>
#include <string.h>

// Utils

int get_width_center(Rectangle rec, const char *text, int font_size) {
    int text_width = MeasureText(text, font_size);
    return rec.x + (rec.width - text_width) / 2;
}

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

bool slider_decrement(slider *s) {
    if (s->value == 0) {
        return false;
    }
    s->value--;
    return true;
}

bool slider_increment(slider *s) {
    if (s->value == s->max) {
        return false;
    }
    s->value++;
    return true;
}

bool slider_hover(slider *s) {
    return CheckCollisionPointRec(GetMousePosition(), s->rec);
}

void slider_render(slider *s) {
    Rectangle source = {0, 0, life_bar_bg.width, life_bar_bg.height};
    DrawTexturePro(life_bar_bg, source, s->rec, (Vector2){0}, 0, WHITE);

    int percent = s->value / s->max * 100;

    int cell_width = (s->rec.width - s->step * 11) / (s->step + 2);
    int cell_height = s->rec.height / 3;
    int y = s->rec.y + cell_height;

    for (int i = 0; i < s->step; i++) {
        int x = s->rec.x + (cell_width + 8) * (i + 2);
        Rectangle life_cell = {x, y, cell_width, cell_height};
        Color c = i * 10 < percent ? RED : BLUE;
        DrawRectangleRec(life_cell, c);
    }
}

// Buttonned slider

bool buttoned_slider_decrement(buttoned_slider *bs) {
    if (bs->slider.value == 0) {
        return false;
    }
    bs->slider.value = (bs->slider.value - bs->step > 0) ? bs->slider.value - bs->step : 0;
    return true;
}

bool buttoned_slider_increment(buttoned_slider *bs) {
    if (bs->slider.value == bs->slider.max) {
        return false;
    }
    bs->slider.value = (bs->slider.value + bs->step < bs->slider.max) ? bs->slider.value + bs->step : bs->slider.max;
    return true;
}

void buttoned_slider_init(buttoned_slider *bs, Rectangle rec, int max, int step) {
    bs->rec = rec;

    int height = rec.height;
    Rectangle minus = {rec.x, rec.y, height, height};
    Rectangle slider = {rec.x + height, rec.y, rec.width - height * 2, height};
    Rectangle plus = {rec.x + minus.width + slider.width, rec.y, height, height};

    bs->minus.rec = minus;
    bs->slider.rec = slider;
    bs->plus.rec = plus;

    bs->minus.font_size = 32;
    bs->minus.text = "-";
    bs->minus.color = RED;

    bs->slider.color = WHITE;
    bs->slider.max = max;
    bs->slider.value = 0;
    bs->slider.step = step;
    bs->slider.color = RED;

    bs->plus.font_size = 32;
    bs->plus.text = "+";
    bs->plus.color = GREEN;

    bs->step = step;
}

void buttoned_slider_render(buttoned_slider *bs) {
    button_render(&bs->minus);
    slider_render(&bs->slider);
    button_render(&bs->plus);
}

// Tooltip

extern Texture2D box_side;
extern Texture2D box_mid;
extern Texture2D spell_box_select;

// TODO: Border should have a fixed size and not scale with the content
Rectangle render_box(int x, int y, int w, int h) {
    // Left side
    Rectangle source_left = {0, 0, box_side.width, box_side.height};
    float scale_factor_h = (float)h / (float)box_side.height;
    Rectangle dest_left = {x, y, box_side.width * scale_factor_h, h};
    DrawTexturePro(box_side, source_left, dest_left, (Vector2){0}, 0, WHITE);

    // Right side
    Rectangle source_right = {0, 0, -box_side.width, box_side.height};
    Rectangle dest_right = {x + w - box_side.width * scale_factor_h, y, box_side.width * scale_factor_h, h};
    DrawTexturePro(box_side, source_right, dest_right, (Vector2){0}, 0, WHITE);

    // Mid
    Rectangle source_mid = {0, 0, box_mid.width, box_mid.height};
    Rectangle dest_mid = {x + dest_left.width, y, dest_right.x - dest_left.x - dest_left.width, h};
    DrawTexturePro(box_mid, source_mid, dest_mid, (Vector2){0}, 0, WHITE);

    Rectangle res = {0};
    res.x = dest_mid.x;
    res.y = dest_mid.y + 7 * scale_factor_h;
    res.width = dest_right.x - dest_mid.x;
    res.height = (dest_mid.height - res.y) - 5 * scale_factor_h;
    return res;
}

#define TOOLTIP_TITLE_FONT_SIZE 32

void render_tooltip(Rectangle rec, const char *title, const char *description) {
    int borderSize = 32;
    rec.x += borderSize / 2.f;
    rec.y += borderSize / 2.f;

    if (rec.y + rec.height >= GetRenderHeight()) {
        rec.y -= rec.height;
        rec.y -= borderSize;
    }

    if (rec.x + rec.width >= GetRenderWidth()) {
        rec.x -= rec.width;
        rec.x -= borderSize;
    }

    DrawRectangleRec(rec, GetColor(0x3B4252FF));

    NPatchInfo patch = {.source = {0, 0, spell_box_select.width, spell_box_select.height},
                        .left = borderSize,
                        .top = borderSize,
                        .right = borderSize,
                        .bottom = borderSize,
                        .layout = NPATCH_NINE_PATCH};
    Rectangle dest = {rec.x - borderSize / 2.f, rec.y - borderSize / 2.f, rec.width + borderSize,
                      rec.height + borderSize};
    DrawTextureNPatch(spell_box_select, patch, dest, (Vector2){0, 0}, 0.0f, WHITE);

    Rectangle inner = {rec.x + 16, rec.y + 8, rec.width - 16, rec.height - 8};
    int text_center = get_width_center(rec, title, TOOLTIP_TITLE_FONT_SIZE);
    DrawText(title, text_center, inner.y, TOOLTIP_TITLE_FONT_SIZE, WHITE);

    char line[1024] = {0};
    int ptr = 0;
    int line_index = 1;
    if (description != NULL) {
        while (*description != '\0') {
            if (*description == '\n') {
                line[ptr] = '\0';
                DrawText(line, inner.x, inner.y + TOOLTIP_TITLE_FONT_SIZE * line_index, 24, LIGHTGRAY);
                ptr = 0;
                line_index++;
            } else {
                line[ptr] = *description;
                ptr++;
            }
            description++;
        }
        line[ptr] = '\0';
        DrawText(line, inner.x, inner.y + TOOLTIP_TITLE_FONT_SIZE * line_index, 24, LIGHTGRAY);
    }
}
