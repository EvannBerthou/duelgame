#ifndef UI_H
#define UI_H

#include <raylib.h>

// Textures

extern Texture2D simple_border;
#define simple_border_size 14
extern Texture2D life_bar_bg;
extern Texture2D box;
extern Texture2D spell_box_select;

// Colors

#define UI_GREEN ((Color){0x4B, 0xA7, 0x47, 0xFF})
#define UI_RED ((Color){0xDA, 0x4E, 0x38, 0xFF})
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
} input_buf;

bool input_write(input_buf *b, char c);
bool input_erase(input_buf *b);
const char *input_to_text(input_buf *b);
void input_set_text(input_buf *b, const char *s);
bool input_is_blank(input_buf *b);
void input_trim(input_buf *b);
bool input_clicked(input_buf *b);
void input_render(input_buf *b, int active);

// Button

typedef struct {
    Rectangle rec;
    Color color;
    const char *text;
    int font_size;
} button;

bool button_hover(button *b);
bool button_clicked(button *b);
void button_render(button *b);

// Slider

typedef struct {
    Rectangle rec;
    Color color;
    float max;
    float value;
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
void render_tooltip(Rectangle rec, const char *title, const char *description);

// Card

typedef struct {
    Rectangle rec;
    Color color;
} card;

void card_render(card *c);

#endif
