#include "ui.h"
#include <ctype.h>
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern bool is_console_open();
extern bool is_console_closed();
extern Vector2 get_mouse();

#define WIDTH 1280
#define HEIGHT 720

// Utils

int get_width_center(Rectangle rec, const char *text, int font_size) {
    int text_width = MeasureText(text, font_size);
    return rec.x + (rec.width - text_width) / 2;
}

void DrawTextCenter(Rectangle rec, const char *text, int font_size, Color c) {
    int center = get_width_center(rec, text, font_size);
    int height = rec.y + (rec.height - font_size) / 2;
    DrawText(text, center, height, font_size, c);
}

// Input

bool input_write(input_buf *b, char c) {
    if (b->ptr == b->max_length || is_console_open()) {
        return false;
    }
    b->buf[b->ptr] = c;
    b->ptr++;
    return true;
}

bool input_erase(input_buf *b) {
    if (b->ptr == 0 || is_console_open()) {
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
    return CheckCollisionPointRec(get_mouse(), b->rec) && IsMouseButtonPressed(0) && is_console_closed();
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

    const char *text = NULL;
    if (b->prefix) {
        text = TextFormat("%s: %s", b->prefix, input_to_text(b));
    } else {
        text = TextFormat("%s", input_to_text(b));
    }
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
    return CheckCollisionPointRec(get_mouse(), b->rec) && is_console_closed();
}

bool button_clicked(button *b) {
    if (b->disabled || is_console_open()) {
        return false;
    }
    if (CheckCollisionPointRec(get_mouse(), b->rec)) {
        if (IsMouseButtonPressed(0)) {
            b->was_down = true;
            return false;
        }
        if (b->was_down && IsMouseButtonReleased(0)) {
            if (b->muted == false) {
                PlaySound(ui_button_clicked);
            }
            return true;
        }
    } else {
        b->was_down = false;
    }
    return false;
}

void button_render(button *b) {
    Color c = b->color;
    if (b->disabled) {
        c = ColorTint(c, DARKGRAY);
    } else if (is_console_closed()) {
        if (CheckCollisionPointRec(get_mouse(), b->rec)) {
            if (IsMouseButtonDown(0)) {
                c = ColorTint(c, DARKGRAY);
            } else {
                c = ColorTint(c, GRAY);
            }
        }
    }
    if (b->type == BT_COLOR) {
        DrawRectangleRec(b->rec, c);
    } else {
        Rectangle src;
        if (b->texture_sprite.width == 0) {
            src = (Rectangle){0, 0, b->texture.width, b->texture.height};
        } else {
            src = b->texture_sprite;
        }
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
    if (s->value == 0 || is_console_open()) {
        return false;
    }
    s->value--;
    return true;
}

bool slider_increment(slider *s) {
    if (s->value == s->max || is_console_open()) {
        return false;
    }
    s->value++;
    return true;
}

bool slider_hover(slider *s) {
    return CheckCollisionPointRec(get_mouse(), s->rec) && is_console_closed();
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

    int cell_height = s->rec.height - slider_border;

    float range = fabsf(s->max) + fabs(s->min);
    float inner_width = s->rec.width - slider_border * 2;
    float single_cell_width = inner_width / range;
    float offset = single_cell_width * fabs(s->min);

    float inner_x = s->rec.x + slider_border + offset;
    float inner_y = s->rec.y + slider_border / 2.f;

    float width = s->value * single_cell_width;
    if (width < 0) {
        width = -width;
        inner_x -= width;
    }
    Rectangle final_rect = {inner_x, inner_y, width, cell_height};
    DrawRectangleRec(final_rect, UI_RED);
    const char *txt = TextFormat("%d", (int)s->value);
    int center = get_width_center(s->rec, txt, cell_height);
    DrawText(txt, center, inner_y, cell_height, WHITE);
}

// Buttonned slider

bool buttoned_slider_decrement(buttoned_slider *bs) {
    if (bs->slider.value == bs->slider.min || is_console_open()) {
        return false;
    }
    bs->slider.value = (bs->slider.value - bs->step > bs->slider.min) ? bs->slider.value - bs->step : bs->slider.min;
    return true;
}

bool buttoned_slider_increment(buttoned_slider *bs) {
    if (bs->slider.value == bs->slider.max || is_console_open()) {
        return false;
    }
    bs->slider.value = (bs->slider.value + bs->step < bs->slider.max) ? bs->slider.value + bs->step : bs->slider.max;
    return true;
}

void buttoned_slider_init(buttoned_slider *bs, Rectangle rec, int max, int step) {
    bs->rec = rec;

    int height = rec.height;
    Rectangle slider = {rec.x + height, rec.y, rec.width - height * 2, height};

    bs->minus = BUTTON_COLOR(rec.x, rec.y, height, height, UI_RED, "-", 28);
    bs->plus = BUTTON_COLOR(rec.x + height + slider.width, rec.y, height, height, UI_GREEN, "+", 28);

    bs->slider.rec = slider;
    bs->slider.color = WHITE;
    bs->slider.max = max;
    bs->slider.value = 0;
    bs->slider.min = 0;
    bs->slider.color = UI_RED;
    bs->step = step;
}

void buttoned_slider_render(buttoned_slider *bs) {
    button_render(&bs->minus);
    slider_render(&bs->slider);
    button_render(&bs->plus);
}

// Tooltip

int box_border = 15;

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

static Vector2 tooltip_pos = {0};
static char tooltip_title[128] = {0};
static char tooltip_description[256] = {0};
static bool tooltip_enabled = false;

void set_tooltip(Vector2 pos, const char *title, const char *description) {
    tooltip_pos = pos;
    strncpy(tooltip_title, title == NULL ? "(null)" : title, 128);
    strncpy(tooltip_description, description == NULL ? "(null)" : description, 256);
    tooltip_enabled = true;
}

void clear_tooltip() {
    tooltip_enabled = false;
}

void render_tooltip() {
    if (is_console_open() || tooltip_enabled == false) {
        return;
    }

    int borderSize = 32;

    int line_count = 0;
    const char **lines = TextSplit(tooltip_description, '\n', &line_count);

    int tooltip_height = TOOLTIP_TITLE_FONT_SIZE + 24 * line_count + borderSize;
    int tooltip_width = 0;

    for (int i = 0; i < line_count; i++) {
        tooltip_width = fmax(tooltip_width, MeasureText(lines[i], 24) + borderSize);
    }

    tooltip_pos.x += borderSize / 2.f;
    tooltip_pos.y += borderSize / 2.f;

    if (tooltip_pos.y + tooltip_height >= HEIGHT) {
        tooltip_pos.y -= tooltip_height;
        tooltip_pos.y -= borderSize;
    }

    if (tooltip_pos.x + tooltip_width >= WIDTH) {
        tooltip_pos.x -= tooltip_width;
        tooltip_pos.x -= borderSize;
    }

    Rectangle tooltip_rec = {tooltip_pos.x, tooltip_pos.y, tooltip_width, tooltip_height};
    DrawRectangleRec(tooltip_rec, UI_NORD);

    NPatchInfo patch = {.source = {0, 0, spell_box_select.width, spell_box_select.height},
                        .left = borderSize,
                        .top = borderSize,
                        .right = borderSize,
                        .bottom = borderSize,
                        .layout = NPATCH_NINE_PATCH};
    Rectangle dest = {tooltip_rec.x - borderSize / 2.f, tooltip_rec.y - borderSize / 2.f,
                      tooltip_rec.width + borderSize, tooltip_rec.height + borderSize};
    DrawTextureNPatch(spell_box_select, patch, dest, (Vector2){0, 0}, 0.0f, WHITE);

    Rectangle inner = {tooltip_rec.x + 16, tooltip_rec.y + 8, tooltip_rec.width - 16, tooltip_rec.height - 8};
    int text_center = get_width_center(tooltip_rec, tooltip_title, TOOLTIP_TITLE_FONT_SIZE);
    DrawText(tooltip_title, text_center, inner.y, TOOLTIP_TITLE_FONT_SIZE, WHITE);

    for (int i = 0; i < line_count; i++) {
        DrawText(lines[i], inner.x, inner.y + TOOLTIP_TITLE_FONT_SIZE * (i + 1), 24, LIGHTGRAY);
    }
}

// Card

bool card_tab_clicked(card *c, int tab) {
    if (IsMouseButtonPressed(0) == false || is_console_open()) {
        return false;
    }

    float tab_width = c->rec.width / c->tab_count;
    Rectangle tab_rec = {c->rec.x + tab_width * tab, c->rec.y, tab_width, 40};
    return CheckCollisionPointRec(get_mouse(), tab_rec);
}

void card_update_tabs(card *c) {
    for (int i = 0; i < c->tab_count; i++) {
        if (card_tab_clicked(c, i)) {
            if (i != c->selected_tab) {
                PlaySound(ui_tab_switch);
            }
            c->selected_tab = i;
        }
    }
}

float card_roundness = 0.05f;
int shadow_offset = 12;

void card_render(card *c) {
    // Shadow
    Rectangle card = {c->rec.x, c->rec.y + 40, c->rec.width, c->rec.height - 40};
    Rectangle card_shadow = {card.x + shadow_offset, c->rec.y + shadow_offset, card.width, c->rec.height};
    DrawRectangleRec(card_shadow, Fade(DARKGRAY, 0.3f));

    // Tabs
    float tab_width = c->rec.width / c->tab_count;
    for (int i = 0; i < c->tab_count; i++) {
        Rectangle tab_rec = {c->rec.x + tab_width * i, c->rec.y, tab_width, 40};
        Color color = c->color;
        if (i != c->selected_tab) {
            if (CheckCollisionPointRec(get_mouse(), tab_rec) && is_console_closed()) {
                color = ColorTint(color, LIGHTGRAY);
            } else {
                color = ColorTint(color, GRAY);
            }
        }
        DrawRectangleRec(tab_rec, color);
    }

    // Card
    DrawRectangleRec(card, c->color);

    for (int i = 0; i < c->tab_count; i++) {
        Rectangle tab_rec = {c->rec.x + tab_width * i, c->rec.y, tab_width, 40};
        DrawTextCenter(tab_rec, c->tabs[i], 24, WHITE);
    }
}

// Icon

bool icon_hover(Rectangle rec) {
    return CheckCollisionPointRec(get_mouse(), rec) && is_console_closed();
}

extern Texture2D icons_sheet;
void icon_render(Rectangle icon, Rectangle rec) {
    DrawTexturePro(icons_sheet, icon, rec, (Vector2){0}, 0, WHITE);
}

// Picker

void picker_add_option(picker *p, const char *option) {
    if (p->option_count == p->option_size) {
        p->option_size = p->option_size == 0 ? 16 : p->option_size * 2;
        p->options = realloc(p->options, sizeof(const char *) * p->option_size);
    }
    p->options[p->option_count] = strdup(option);
    p->option_count++;
}

bool picker_clicked(picker *p) {
    if (IsMouseButtonPressed(0) == false || is_console_open()) {
        return false;
    }

    return CheckCollisionPointRec(get_mouse(), p->rec);
}

int picker_option_clicked(picker *p) {
    if (p->opened == false) {
        return -1;
    }
    if (IsMouseButtonPressed(0) == false || is_console_open()) {
        return -1;
    }

    int len = fmin(p->option_count, p->option_offset + p->option_frame);
    for (int i = p->option_offset; i < len; i++) {
        int y = p->rec.y + p->rec.height + 40 * (i - p->option_offset);
        Rectangle option_rec = {p->rec.x, y, p->rec.width, 40};
        if (CheckCollisionPointRec(get_mouse(), option_rec)) {
            p->selected_option = i;
            p->opened = false;
            return i;
        }
    }
    // We clicked outside of the picker so we close it
    if (!CheckCollisionPointRec(get_mouse(), p->rec)) {
        p->opened = false;
    }
    return -1;
}

void picker_update_scroll(picker *p) {
    float mouse_wheel = GetMouseWheelMove();
    if (mouse_wheel == 1) {
        p->option_offset = fmax(p->option_offset - 1, 0);
    } else if (mouse_wheel == -1) {
        p->option_offset = fmin(p->option_offset + 1, p->option_count - p->option_frame);
    }
}

void picker_render(picker *p) {
    Color picker_color = UI_BEIGE;
    if (CheckCollisionPointRec(get_mouse(), p->rec)) {
        picker_color = ColorTint(picker_color, LIGHTGRAY);
    }
    DrawRectangleRec(p->rec, picker_color);
    NPatchInfo patch = {.source = {0, 0, simple_border.width, simple_border.height},
                        .left = simple_border_size,
                        .top = simple_border_size,
                        .right = simple_border_size,
                        .bottom = simple_border_size,
                        .layout = NPATCH_NINE_PATCH};
    DrawTextureNPatch(simple_border, patch, p->rec, (Vector2){0, 0}, 0.0f, WHITE);
    int inner_x = p->rec.x + simple_border_size;
    int inner_y = p->rec.y + (p->rec.height - 32) / 2.f;
    if (p->options) {
        DrawText(p->options[p->selected_option], inner_x, inner_y, 32, BLACK);
    }

    if (p->opened) {
        int len = fmin(p->option_count, p->option_offset + p->option_frame);
        for (int i = p->option_offset; i < len; i++) {
            int y = p->rec.y + p->rec.height + 40 * (i - p->option_offset);
            Rectangle option_rec = {p->rec.x, y, p->rec.width, 40};
            inner_x = option_rec.x + simple_border_size;
            inner_y = option_rec.y + (option_rec.height - 32) / 2.f;
            Color c = UI_BEIGE;
            if (i == p->selected_option) {
                c = ColorTint(c, DARKGRAY);
            }
            if (CheckCollisionPointRec(get_mouse(), option_rec)) {
                c = ColorTint(c, LIGHTGRAY);
            }
            DrawRectangleRec(option_rec, c);
            DrawText(p->options[i], inner_x, inner_y, 32, BLACK);
        }
        Rectangle frame_rec = {p->rec.x, p->rec.y + p->rec.height, p->rec.width, 40 * len};
        NPatchInfo patch = {.source = {0, 0, simple_border.width, simple_border.height},
                            .left = simple_border_size,
                            .top = simple_border_size,
                            .right = simple_border_size,
                            .bottom = simple_border_size,
                            .layout = NPATCH_NINE_PATCH};
        DrawTextureNPatch(simple_border, patch, frame_rec, (Vector2){0, 0}, 0.0f, WHITE);
    }
}

void clear_picker(picker *p) {
    for (int i = 0; i < p->option_count; i++) {
        free((void *)p->options[i]);
    }
    p->option_count = 0;
}
