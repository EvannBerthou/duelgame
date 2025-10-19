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

#define DISABLED_TINT DARKGRAY
#define CLICKED_TINT DARKGRAY
#define SELECTED_TINT DARKGRAY
#define HOVER_TINT LIGHTGRAY

// Utils

const char *ui_type_name[] = {"Empty", "Input", "Button", "Slider", "Button slider", "Card", "Icon", "Picker"};

const char *extract_raw_string(const char *s) {
    if (s == NULL) {
        return "";
    }
    static char raw[1024] = {0};
    int ptr = 0;
    while (*s != '\0') {
        if (*s == '$') {
            s += 2;
        } else {
            raw[ptr] = *s;
            s++;
            ptr++;
        }
    }
    raw[ptr] = '\0';
    return raw;
}

int get_width_center(Rectangle rec, const char *text, int font_size) {
    int text_width = MeasureText(extract_raw_string(text), font_size);
    return rec.x + (rec.width - text_width) / 2;
}

void DrawTextCenter(Rectangle rec, const char *text, int font_size, Color c) {
    int center = get_width_center(rec, text, font_size);
    int height = rec.y + (rec.height - font_size) / 2;
    DrawText(text, center, height, font_size, c);
}

// Based on : https://github.com/raysan5/raylib/blob/070c7894c6901eb206c76534627858e5913826f5/src/rtext.c#L1205
float GetTextWidth(const char *text, int len, float fontSize) {
    Font font = GetFontDefault();
    float scale = fontSize / font.baseSize;
    float width = 0.0f;
    int spacing = fontSize / 10;

    for (int i = 0; i < len && text[i] != '\0';) {
        int codepointByteCount = 0;
        int codepoint = GetCodepointNext(&text[i], &codepointByteCount);
        int index = GetGlyphIndex(font, codepoint);

        if (font.glyphs[index].advanceX == 0) {
            width += ((float)font.recs[index].width * scale + spacing);
        } else {
            width += ((float)font.glyphs[index].advanceX * scale + spacing);
        }
        i += codepointByteCount;
    }

    return width;
}

bool is_hover(Rectangle rec) {
    return is_console_closed() && CheckCollisionPointRec(get_mouse(), rec);
}

bool is_clicked(Rectangle rec) {
    return is_hover(rec) && IsMouseButtonPressed(0);
}

Color get_color(char c) {
    switch (c) {
        case 'r':
            return UI_RED;
        case 'g':
            return UI_GREEN;
        case 'b':
            return UI_BLUE;
        case 'y':
            return YELLOW;
        case 'd':
            return DARKGRAY;
        case 'n':
            return GRAY;
        case 'w':
            return WHITE;
        case 'l':
        default:
            return LIGHTGRAY;
    }
}

void DrawColoredText(const char *s, int x, int y, int font_size, Color c) {
    if (strlen(s) >= 2 && s[0] == '$') {
        c = get_color(s[1]);
        s += 2;
    }
    DrawText(s, x, y, font_size, c);
}

// Input

const char *input_selection(input_buf *b) {
    if (b->selection_length == 0) {
        return NULL;
    }
    return TextFormat("%.*s", b->selection_length, b->buf + b->selection_start);
}

void input_clear_selection(input_buf *b) {
    b->origin = 0;
    b->selection_start = b->ptr;
    b->selection_length = 0;
    b->down_position = (Vector2){0};
}

bool input_erase(input_buf *b) {
    if (b->ptr == 0) {
        return false;
    }

    if (b->selection_start == b->ptr) {
        if (b->selection_start == 0) {
            return false;
        }
        b->ptr--;
        b->selection_start = b->ptr;
    } else if (b->selection_length != 0) {
        memmove(b->buf + b->selection_start, b->buf + b->selection_start + b->selection_length, b->selection_length);
        b->ptr -= b->selection_length;
    } else {
        memmove(b->buf + b->selection_start - 1, b->buf + b->selection_start, b->ptr - b->selection_start);
        b->ptr--;
        b->selection_start--;
    }

    b->selection_length = 0;
    return true;
}

bool input_write(input_buf *b, char c) {
    if (b->selection_start == b->ptr) {
        if (b->ptr == b->max_length) {
            return false;
        }
        b->buf[b->ptr] = c;
        b->ptr++;
        b->selection_start = b->ptr;
        return true;
    }

    if (b->selection_length != 0) {
        // Replaces selection
        memmove(b->buf + b->selection_start + 1, b->buf + b->selection_start + b->selection_length,
                b->selection_length);
    } else if (b->ptr != b->max_length) {
        // Insert in the middle
        memmove(b->buf + b->selection_start + 1, b->buf + b->selection_start, b->ptr - b->selection_start);
    } else {
        // No more space left to insert a new character
        return false;
    }

    b->buf[b->selection_start] = c;
    b->ptr -= b->selection_length - 1;
    b->selection_length = 0;
    b->selection_start = fmin(b->selection_start + 1, b->ptr);

    return true;
}

const char *input_to_text(input_buf *b) {
    return TextFormat("%.*s", b->ptr, b->buf);
}

void input_set_text(input_buf *b, const char *s) {
    int length = (int)strlen(s) <= b->max_length ? (int)strlen(s) : b->max_length;
    strncpy(b->buf, s, length);
    b->ptr = length;
    input_clear_selection(b);
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

static int input_last_click_time = 0;

void reset_blink_timer() {
    input_last_click_time = GetTime();
}

bool input_clicked(input_buf *b) {
    bool result = is_clicked(b->rec);
    if (result) {
        reset_blink_timer();
    }
    return result;
}

// Rounds to nearest pair for better rendering
int get_font_size(Rectangle rec) {
    int res = rec.height * 0.45f;
    return res % 2 == 1 ? res + 1 : res;
}

ui_input_result input_update(input_buf *b) {
    if (is_console_open()) {
        return UI_INPUT_NONE;
    }

    if (is_hover(b->rec) && IsMouseButtonDown(0)) {
        Vector2 mouse = get_mouse();
        int last_width = 0;
        int font_size = get_font_size(b->rec);

        int start_x = b->rec.x + simple_border_size;
        if (b->prefix) {
            start_x += MeasureText(TextFormat("%s: ", b->prefix), font_size);
        }

        int outside_text = true;
        for (int i = 0; i <= b->ptr; i++) {
            int current_width = start_x + MeasureText(TextFormat("%.*s", i, b->buf), font_size);
            if (current_width >= mouse.x && last_width <= mouse.x) {
                if (IsMouseButtonPressed(0)) {
                    b->origin = fmax(i - 1, 0);
                    b->selection_start = b->origin;
                    b->selection_length = 0;
                    b->down_position = get_mouse();
                } else if (b->down_position.x != mouse.x) {
                    if (i >= b->origin) {
                        b->selection_start = fmax(b->origin, 0);
                        b->selection_length = i - b->origin;
                    } else if (i < b->origin) {
                        b->selection_start = fmax(i, 0);
                        b->selection_length = b->origin - i;
                    }
                }
                outside_text = false;
                break;
            }
            last_width = current_width;
        }
        if (outside_text && IsMouseButtonPressed(0)) {
            b->selection_start = b->ptr;
            b->origin = b->ptr;
            b->selection_length = 0;
        }
    }

    if (IsKeyPressed(KEY_ENTER)) {
        reset_blink_timer();
        return UI_INPUT_ENTER;
    }

    if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && !IsKeyDown(KEY_LEFT_SHIFT)) {
        reset_blink_timer();
        return UI_INPUT_NEXT;
    }

    if ((IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB)) && IsKeyDown(KEY_LEFT_SHIFT)) {
        reset_blink_timer();
        return UI_INPUT_PREV;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) {
        const char *clipboard_text = GetClipboardText();
        while (*clipboard_text) {
            input_write(b, *clipboard_text);
            clipboard_text++;
        }
        return UI_INPUT_NONE;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) {
        const char *selection = input_selection(b);
        if (selection != NULL) {
            SetClipboardText(selection);
        }
        return UI_INPUT_NONE;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        input_erase(b);
        reset_blink_timer();
        return UI_INPUT_NONE;
    }

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        b->selection_length = 0;
        b->selection_start = fmax(b->selection_start - 1, 0);
        reset_blink_timer();
        return UI_INPUT_NONE;
    }

    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        b->selection_length = 0;
        b->selection_start = fmin(b->selection_start + 1, b->ptr);
        reset_blink_timer();
        return UI_INPUT_NONE;
    }

    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        if (b->selection_length == 0) {
            b->selection_start = fmin(b->selection_start + 1, b->ptr);
            input_erase(b);
            b->selection_start = fmax(b->selection_start, 0);
        } else {
            input_erase(b);
        }
        reset_blink_timer();
        return UI_INPUT_NONE;
    }

    int key;
    while ((key = GetCharPressed())) {
        input_write(b, key);
    }

    return UI_INPUT_NONE;
}

// TODO: Some broken stuff with the rendering going outside of the given rec
void input_render(input_buf *b) {
    Rectangle inner_rec = b->rec;
    Color bg_color = UI_BEIGE;
    if (is_hover(b->rec)) {
        bg_color = ColorTint(bg_color, HOVER_TINT);
    }
    DrawRectangleRec(inner_rec, bg_color);
    NPatchInfo patch = {.source = {0, 0, simple_border.width, simple_border.height},
                        .left = simple_border_size,
                        .top = simple_border_size,
                        .right = simple_border_size,
                        .bottom = simple_border_size,
                        .layout = NPATCH_NINE_PATCH};
    DrawTextureNPatch(simple_border, patch, inner_rec, (Vector2){0, 0}, 0.0f, WHITE);

    int font_size = get_font_size(inner_rec);
    int inner_x = inner_rec.x + simple_border_size;
    int inner_y = inner_rec.y + (inner_rec.height - font_size) / 2.f;

    int prefix_offset = 0;
    if (b->prefix) {
        const char *prefix = TextFormat("%s: ", b->prefix);
        prefix_offset = MeasureText(prefix, font_size);
        DrawText(prefix, inner_x, inner_y, font_size, BLACK);
    }

    DrawText(input_to_text(b), inner_x + prefix_offset, inner_y, font_size, BLACK);

    if (b->selection_length != 0) {
        float non_selection_offset = GetTextWidth(b->buf, b->selection_start, font_size);
        float selection_width = GetTextWidth(b->buf + b->selection_start, b->selection_length, font_size);
        DrawRectangle(inner_x + prefix_offset + non_selection_offset, inner_y, selection_width, font_size,
                      ColorAlpha(HOVER_TINT, 0.4f));
    }

    int elapsed_time = GetTime() - input_last_click_time;
    if (b->active && (elapsed_time % 2 == 0 || elapsed_time < 1)) {
        int ptr_offset = GetTextWidth(b->buf, b->selection_start, font_size);
        DrawRectangle(inner_x + prefix_offset + ptr_offset, inner_y, 3, font_size, BLACK);
    }
}

// Button

bool button_hover(button *b) {
    return is_hover(b->rec);
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
        c = ColorTint(c, DISABLED_TINT);
    } else if (is_console_closed()) {
        if (CheckCollisionPointRec(get_mouse(), b->rec)) {
            if (IsMouseButtonDown(0)) {
                c = ColorTint(c, CLICKED_TINT);
            } else {
                c = ColorTint(c, HOVER_TINT);
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

    int width = MeasureText(extract_raw_string(b->text), b->font_size);
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

// TODO: Clicking on a slider should set its value (can be disabled)
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
    return is_hover(s->rec);
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

    float range = fabsf(s->max - s->min);
    float inner_width = s->rec.width - slider_border * 2;
    float single_cell_width = inner_width / range;
    float offset = 0;
    if (s->min < 0) {
        offset = single_cell_width * fabs(s->min);
    }

    float inner_x = s->rec.x + slider_border + offset;
    float inner_y = s->rec.y + slider_border / 2.f;

    float width = (s->value - s->min) * single_cell_width - offset;
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
    bs->minus.disabled = bs->minus.disabled || bs->slider.value == bs->slider.min;
    bs->plus.disabled = bs->plus.disabled || bs->slider.value == bs->slider.max;

    button_render(&bs->minus);
    slider_render(&bs->slider);
    button_render(&bs->plus);

    bs->minus.disabled = false;
    bs->plus.disabled = false;
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

    int tooltip_height = TOOLTIP_TITLE_FONT_SIZE + 24 * line_count + borderSize / 2.f;
    int tooltip_width = MeasureText(extract_raw_string(tooltip_title), TOOLTIP_TITLE_FONT_SIZE) + borderSize * 2;

    for (int i = 0; i < line_count; i++) {
        tooltip_width = fmax(tooltip_width, MeasureText(extract_raw_string(lines[i]), 24) + borderSize);
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
    DrawColoredText(tooltip_title, text_center, inner.y, TOOLTIP_TITLE_FONT_SIZE, WHITE);

    for (int i = 0; i < line_count; i++) {
        DrawColoredText(lines[i], inner.x, inner.y + TOOLTIP_TITLE_FONT_SIZE + 24 * i, 24, LIGHTGRAY);
    }
}

// Card

bool card_tab_clicked(card *c, int tab) {
    float tab_width = c->rec.width / c->tab_count;
    Rectangle tab_rec = {c->rec.x + tab_width * tab, c->rec.y, tab_width, 40};
    return is_clicked(tab_rec);
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
            if (is_hover(tab_rec)) {
                color = ColorTint(color, HOVER_TINT);
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

    // Render tab layout
    layout_render(&c->layouts[c->selected_tab]);
}

void card_layout_set_specs(card *c, int tab, layout specs) {
    c->node = (layout_node){.rec = c->rec};
    specs.parent = &c->node;
    specs.padding[0] = 50;
    specs.padding[1] = 15;
    specs.padding[2] = 15;
    specs.padding[3] = 15;
    c->layouts[tab] = specs;
    layout_refresh(&c->layouts[tab]);
}

// Icon

bool icon_hover(Rectangle rec) {
    return is_hover(rec);
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
    return is_clicked(p->rec);
}

int picker_option_clicked(picker *p) {
    if (p->opened == false) {
        return -1;
    }

    int len = fmin(p->option_count, p->option_offset + p->option_frame);
    for (int i = p->option_offset; i < len; i++) {
        int y = p->rec.y + p->rec.height + 40 * (i - p->option_offset);
        Rectangle option_rec = {p->rec.x, y, p->rec.width, 40};
        if (is_clicked(option_rec)) {
            p->selected_option = i;
            p->opened = false;
            return i;
        }
    }
    // We clicked outside of the picker so we close it
    if (!CheckCollisionPointRec(get_mouse(), p->rec) && IsMouseButtonPressed(0)) {
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
    if (is_hover(p->rec)) {
        picker_color = ColorTint(picker_color, HOVER_TINT);
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
                c = ColorTint(c, SELECTED_TINT);
            }
            if (is_hover(option_rec)) {
                c = ColorTint(c, HOVER_TINT);
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

// Layout

static int layout_get_node_progress(layout *l, ui_spec_value spec) {
    int base = l->type == LT_VERTICAL ? l->layout_rec.height : l->layout_rec.width;
    base -= l->spacing * (l->node_count - 1);
    switch (spec.unit) {
        case UNIT_FIT:
            return base;
        case UNIT_PERCENT:
            return base * (spec.value / 100.f);
        case UNIT_PX:
            return spec.value;
    }
    return 0;
}

static int layout_compute_node_rec(layout *l, int node_index, int free_progress) {
    Rectangle *rec = (Rectangle *)l->nodes[node_index].data;

    if (l->type == LT_HORIZONTAL) {
        rec->y = l->layout_rec.y;
        rec->height = l->layout_rec.height;

        if (l->width != LAYOUT_FREE) {
            float node_width = (l->layout_rec.width - (l->spacing * (l->node_count - 1))) / l->node_count;
            if (l->node_count == 1) {
                node_width = l->layout_rec.width;
            }
            rec->x = l->layout_rec.x + (node_width + l->spacing) * node_index;
            rec->width = node_width;
        } else {
            rec->x = l->layout_rec.x + free_progress;
            int width = layout_get_node_progress(l, l->nodes[node_index].specs.width);
            rec->width = width;
            if (free_progress + rec->width > l->layout_rec.width) {
                LOGL(LL_DEBUG, "UI node ends=%f but width=%f", free_progress + rec->width, l->layout_rec.width);
            }
            free_progress += width + l->spacing;
        }
    } else if (l->type == LT_VERTICAL) {
        rec->x = l->layout_rec.x;
        rec->width = l->layout_rec.width;

        if (l->height != LAYOUT_FREE) {
            float node_height = (l->layout_rec.height - (l->spacing * (l->node_count - 1))) / l->node_count;
            if (l->node_count == 1) {
                node_height = layout_get_node_progress(l, l->nodes[0].specs.height);
            }

            rec->y = l->layout_rec.y + (node_height + l->spacing) * node_index;
            rec->height = node_height;
        } else {
            rec->y = l->layout_rec.y + free_progress;
            int height = layout_get_node_progress(l, l->nodes[node_index].specs.height);
            rec->height = height;

            if (free_progress + rec->height > l->layout_rec.height) {
                LOGL(LL_DEBUG, "UI node ends=%f but layout height=%f", free_progress + rec->y, l->layout_rec.height);
            }

            free_progress += height + l->spacing;
        }
    }
    l->nodes[node_index].rec = *rec;
    return free_progress;
}

void layout_refresh(layout *l) {
    if (l->parent != NULL) {
        l->layout_rec.x = l->parent->rec.x;
        l->layout_rec.y = l->parent->rec.y;
        l->layout_rec.width = l->parent->rec.width;
        l->layout_rec.height = l->parent->rec.height;
    } else {
        l->layout_rec.x = 0;
        l->layout_rec.y = 0;
        l->layout_rec.width = WIDTH;
        l->layout_rec.height = HEIGHT;
    }

    // int max_height = 0;
    // if (l->height == LAYOUT_CONTENT_FIT) {
    //     for (int i = 0; i < l->node_count; i++) {
    //         Rectangle *rec = (Rectangle *)l->nodes[i].data;
    //         max_height = fmax(rec->height, max_height);
    //     }
    //     l->layout_rec.height = max_height;
    // }

    if (l->height == LAYOUT_FIT_CONTAINER || l->height == LAYOUT_FREE) {
        l->layout_rec.height = l->parent == NULL ? HEIGHT : l->parent->rec.height;
    }

    if (l->width == LAYOUT_FIT_CONTAINER || l->width == LAYOUT_FREE) {
        l->layout_rec.width = l->parent == NULL ? WIDTH : l->parent->rec.width;
    }

    l->layout_rec.x += l->padding[3];
    l->layout_rec.width -= l->padding[3] + l->padding[1];

    l->layout_rec.y += l->padding[0];
    l->layout_rec.height -= l->padding[0] + l->padding[2];

    int free_progress = 0;
    for (int i = 0; i < l->node_count; i++) {
        free_progress = layout_compute_node_rec(l, i, free_progress);
    }

    for (int i = 0; i < l->children_count; i++) {
        layout_refresh(l->children[i]);
    }
}

void layout_push(layout *l, ui_type type, void *data, ui_node_specs specs) {
    l->nodes[l->node_count].node_type = type;
    l->nodes[l->node_count].data = data;
    l->nodes[l->node_count].specs = specs;
    l->node_count++;
    layout_refresh(l);
}

Rectangle deepest_rectangle = {0};

static void layout_node_debug(layout_node *node) {
    if (is_hover(node->rec)) {
        deepest_rectangle = node->rec;
        set_tooltip(get_mouse(), "Layout Node Debug",
                    TextFormat("Type=%s\nx=%0.f\ny=%0.f\nwidth=%0.f\nheight=%0.f\n", ui_type_name[node->node_type],
                               node->rec.x, node->rec.y, node->rec.width, node->rec.height));
    }
}

bool layout_debug = true;

static void render_node(layout_node *node) {
    void *data = node->data;
    switch (node->node_type) {
        case UI_INPUT:
            input_render(data);
            break;
        case UI_BUTTON:
            button_render(data);
            break;
        case UI_SLIDER:
            slider_render(data);
            break;
        case UI_BUTON_SLIDER:
            buttoned_slider_render(data);
            break;
        case UI_CARD:
            card_render(data);
            break;
        case UI_ICON:
            LOGL(LL_ERROR, "UI_ICON render call not implemetend in layout");
            // icon_render(data);
            break;
        case UI_PICKER:
            picker_render(data);
            break;
        case UI_EMPTY:
            if (layout_debug) {
                DrawRectangleLinesEx(node->rec, 5, BLUE);
            }
            break;
    }
}

static void layout_render_no_debug(layout *l) {
    for (int i = 0; i < l->node_count; i++) {
        render_node(&l->nodes[i]);
    }

    for (int i = 0; i < l->children_count; i++) {
        layout_render_no_debug(l->children[i]);
    }
}

static void layout_render_debug(layout *l) {
    if (is_hover(l->layout_rec)) {
        deepest_rectangle = l->layout_rec;
        set_tooltip(get_mouse(), "Layout Node Debug",
                    TextFormat("Type=Layout\nx=%0.f\ny=%0.f\nwidth=%0.f\nheight=%0.f\n", l->layout_rec.x,
                               l->layout_rec.y, l->layout_rec.width, l->layout_rec.height));
    }

    for (int i = 0; i < l->node_count; i++) {
        layout_node_debug(&l->nodes[i]);
        render_node(&l->nodes[i]);
    }
    DrawRectangleLinesEx(l->layout_rec, 1, GREEN);
    DrawRectangleLinesEx(l->layout_rec, 1, RED);

    for (int i = 0; i < l->children_count; i++) {
        layout_render_debug(l->children[i]);
    }
}

int nesting = 0;

void layout_render(layout *l) {
    if (nesting == 1) {
        deepest_rectangle = (Rectangle){0};
    }
    nesting++;
    if (layout_debug) {
        layout_render_debug(l);
    } else {
        layout_render_no_debug(l);
    }
    nesting--;
    if (nesting == 0) {
        DrawRectangleRec(deepest_rectangle, GetColor(0x18181868));
    }
}

layout *layout_push_layout(layout *parent, int node_index, layout base) {
    if (parent->children_count == 8) {
        LOGL(LL_ERROR, "Too much children layout");
        exit(1);
    }
    if (parent->children == NULL) {
        parent->children = malloc(sizeof(layout *) * 8);
    }

    layout *child = malloc(sizeof(layout));
    *child = base;
    child->parent = &parent->nodes[node_index];
    parent->children[parent->children_count] = child;
    parent->children_count++;
    layout_refresh(child);
    return child;
}

void toggle_layout_debug_render() {
    layout_debug = !layout_debug;
}
