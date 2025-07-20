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

void DrawTextCenter(Rectangle rec, const char *text, int font_size, Color c) {
    int center = get_width_center(rec, text, font_size);
    DrawText(text, center, rec.y + font_size / 2.f, font_size, c);
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
    DrawRectangleRec(b->rec, UI_BEIGE);
    NPatchInfo patch = {.source = {0, 0, simple_border.width, simple_border.height},
                        .left = simple_border_size,
                        .top = simple_border_size,
                        .right = simple_border_size,
                        .bottom = simple_border_size,
                        .layout = NPATCH_NINE_PATCH};
    DrawTextureNPatch(simple_border, patch, b->rec, (Vector2){0, 0}, 0.0f, WHITE);

    const char *text = TextFormat("%s: %s", b->prefix, input_to_text(b));
    int font_size = b->rec.height * 0.5f;
    int inner_x = b->rec.x + simple_border_size;
    int inner_y = b->rec.y + (b->rec.height - font_size) / 2.f;
    DrawText(text, inner_x, inner_y, font_size, BLACK);
    if (active) {
        int text_width = MeasureText(text, font_size);
        DrawRectangle(inner_x + text_width + 2, inner_y, 2, font_size, BLACK);
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
        if (IsMouseButtonDown(0)) {
            c = ColorTint(c, DARKGRAY);
        } else {
            c = ColorTint(c, GRAY);
        }
    }
    if (b->type == BT_COLOR) {
        DrawRectangleRec(b->rec, c);
    } else {
        Rectangle src = {0, 0, b->texture.width, b->texture.height};
        DrawTexturePro(b->texture, src, b->rec, (Vector2){0}, 0, c);
    }

    int width = MeasureText(b->text, b->font_size);
    DrawText(b->text, b->rec.x + (b->rec.width - width) / 2, b->rec.y + (b->rec.height - b->font_size) / 2,
             b->font_size, WHITE);

    NPatchInfo patch = {.source = {0, 0, simple_border.width, simple_border.height},
                        .left = simple_border_size,
                        .top = simple_border_size,
                        .right = simple_border_size,
                        .bottom = simple_border_size,
                        .layout = NPATCH_NINE_PATCH};
    DrawTextureNPatch(simple_border, patch, b->rec, (Vector2){0, 0}, 0.0f, WHITE);
}

// Slider

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

int slider_border = 16;
void slider_render(slider *s) {
    NPatchInfo patch = {.source = {0, 0, life_bar_bg.width, life_bar_bg.height},
                        .left = slider_border,
                        .top = slider_border,
                        .right = slider_border,
                        .bottom = slider_border,
                        .layout = NPATCH_NINE_PATCH};
    DrawTextureNPatch(life_bar_bg, patch, s->rec, (Vector2){0, 0}, 0.0f, WHITE);

    float percent = s->value / s->max;
    int cell_height = s->rec.height - slider_border;

    int inner_x = s->rec.x + slider_border;
    int inner_y = s->rec.y + slider_border / 2.f;

    int width = percent * (s->rec.width - slider_border * 2);
    Rectangle final_rect = {inner_x, inner_y, width, cell_height};
    DrawRectangleRec(final_rect, UI_RED);
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

    bs->minus.font_size = 28;
    bs->minus.text = "-";
    bs->minus.color = UI_RED;

    bs->slider.color = WHITE;
    bs->slider.max = max;
    bs->slider.value = 0;
    bs->slider.color = UI_RED;

    bs->plus.font_size = 28;
    bs->plus.text = "+";
    bs->plus.color = UI_GREEN;

    bs->step = step;
}

void buttoned_slider_render(buttoned_slider *bs) {
    button_render(&bs->minus);
    slider_render(&bs->slider);
    button_render(&bs->plus);
}

// Tooltip

int box_border = 15;

// TODO: Border should have a fixed size and not scale with the content
Rectangle render_box(int x, int y, int w, int h) {
    NPatchInfo patch = {.source = {0, 0, box.width, box.height},
                        .left = box_border,
                        .top = box_border,
                        .right = box_border,
                        .bottom = box_border,
                        .layout = NPATCH_NINE_PATCH};
    Rectangle dest = {x, y, w, h};
    DrawTextureNPatch(box, patch, dest, (Vector2){0, 0}, 0.0f, WHITE);
    return (Rectangle){x + box_border, y + box_border, w - box_border * 2, h - box_border * 2};
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

    DrawRectangleRec(rec, UI_NORD);

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

// Card

float card_roundness = 0.05f;
void card_render(card *c) {
    int shadow_offset = 12;
    Rectangle card = c->rec;
    Rectangle card_shadow = {card.x + shadow_offset, card.y + shadow_offset, card.width, card.height};

    DrawRectangleRounded(card_shadow, card_roundness, 12, Fade(DARKGRAY, 0.3f));
    DrawRectangleRounded(card, card_roundness, 12, c->color);
    //
    // DrawLine(card.x, card.y, card.x + card.width, card.y, Fade(LIGHTGRAY, 0.6f));
    // DrawLine(card.x, card.y, card.x, card.y + card.height, Fade(LIGHTGRAY, 0.6f));
    //
    // DrawLine(card.x, card.y + card.height, card.x + card.width, card.y + card.height, Fade(GRAY, 0.6f));
    // DrawLine(card.x + card.width, card.y, card.x + card.width, card.y + card.height, Fade(GRAY, 0.6f));
}
