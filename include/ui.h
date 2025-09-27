#ifndef UI_H
#define UI_H

#include <raylib.h>

// Textures

extern Texture2D simple_border;
#define simple_border_size 14
extern Texture2D life_bar_bg;
extern Texture2D box;
extern Texture2D spell_box_select;
extern Sound ui_button_clicked;
extern Sound ui_tab_switch;

// Colors

#define UI_GREEN ((Color){0x4B, 0xA7, 0x47, 0xFF})
#define UI_RED ((Color){0xDA, 0x4E, 0x38, 0xFF})
#define UI_BLUE ((Color){0x36, 0x74, 0xB5, 0xFF})
#define UI_NORD ((Color){0x3B, 0x42, 0x52, 0xFF})
#define UI_BLACK ((Color){0x18, 0x18, 0x18, 0xFF})
#define UI_BEIGE ((Color){0xD3, 0xBF, 0xA9, 0xFF})

// Utils

int get_width_center(Rectangle rec, const char *text, int font_size);
void DrawTextCenter(Rectangle rec, const char *text, int font_size, Color c);

// Input

typedef struct {
    Rectangle rec;
    const char *prefix;
    char buf[32];
    int ptr;
    int max_length;

    Vector2 down_position;
    int origin;
    int selection_start;
    int selection_length;
} input_buf;

typedef enum {
    UI_INPUT_NONE,
    UI_INPUT_ENTER,
    UI_INPUT_NEXT,
    UI_INPUT_PREV,
} ui_input_result;

ui_input_result input_update(input_buf *b);
void input_clear_selection(input_buf *b);
bool input_write(input_buf *b, char c);
bool input_erase(input_buf *b);
const char *input_to_text(input_buf *b);
void input_set_text(input_buf *b, const char *s);
bool input_is_blank(input_buf *b);
void input_trim(input_buf *b);
bool input_clicked(input_buf *b);
void input_render(input_buf *b, int active);

// Button

typedef enum {
    BT_COLOR,
    BT_TEXTURE,
} button_type;

typedef struct {
    Rectangle rec;
    button_type type;
    Color color;
    Texture2D texture;
    Rectangle texture_sprite;
    const char *text;
    int font_size;
    bool disabled;
    bool was_down;
    bool muted;
} button;

#define BUTTON(x, y, w, h, btn_type, btn_color, btn_texture, btn_text, btn_font_size) \
    (button){                                               \
        .rec = (Rectangle){x,y,w,h},                        \
        .type = (btn_type),                                 \
        .color = (btn_color),                               \
        .texture = (Texture2D)btn_texture,                  \
        .text = (btn_text),                                 \
        .font_size = (btn_font_size),                       \
        .disabled = false,                                  \
        .was_down = false                                   \
    }

#define BUTTON_COLOR(x, y, w, h, color, text, font_size) BUTTON(x, y, w, h, BT_COLOR, color, {0}, text, font_size)

#define BUTTON_TEXTURE(x, y, w, h, texture, text, font_size) BUTTON(x, y, w, h, BT_TEXTURE, WHITE, texture, text, font_size)


bool button_hover(button *b);
bool button_clicked(button *b);
void button_render(button *b);

// Slider

typedef struct {
    Rectangle rec;
    Color color;
    float max;
    float min;
    int value;
} slider;

bool slider_decrement(slider *s);
bool slider_increment(slider *s);
bool slider_hover(slider *s);
void slider_render(slider *s);

// Butonned slider

typedef struct {
    Rectangle rec;
    button minus;
    button plus;
    slider slider;
    int step;
} buttoned_slider;

bool buttoned_slider_decrement(buttoned_slider *s);
bool buttoned_slider_increment(buttoned_slider *s);
void buttoned_slider_init(buttoned_slider *bs, Rectangle rec, int max, int step);
void buttoned_slider_render(buttoned_slider *bs);

// Tooltip

Rectangle render_box(int x, int y, int w, int h);
void set_tooltip(Vector2 rec, const char *title, const char *description);
void clear_tooltip();
void render_tooltip();

// Card

typedef struct {
    Rectangle rec;
    Color color;
    //Max 4 tabs for now
    const char *tabs[4];
    int tab_count;
    int selected_tab;
} card;

bool card_tab_clicked(card *c, int tab);
void card_update_tabs(card *c);
void card_render(card *c);

#define CARD(x, y, w, h, c, ...) ((card){ \
        .rec = (Rectangle){x, y, w, h}, \
        .color = (c), \
        .tabs = {__VA_ARGS__}, \
        .tab_count = ((int)(sizeof ((char*[]){__VA_ARGS__}) / sizeof ((char*[]){__VA_ARGS__})[0])), \
        .selected_tab = 0 \
        })

// Icon

bool icon_hover(Rectangle rec);
void icon_render(Rectangle icon, Rectangle rec);

// Picker

typedef struct {
    Rectangle rec;
    const char **options;
    int option_count;
    int option_size;
    int selected_option;
    int option_frame;
    int option_offset;
    bool opened;
} picker;

void picker_add_option(picker *p, const char *option);
int picker_option_clicked(picker *p);
bool picker_clicked(picker *p);
void picker_update_scroll(picker *p);
void picker_render(picker *p);
void clear_picker(picker *p);

#endif
