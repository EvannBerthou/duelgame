#ifndef UI_H
#define UI_H

#include <raylib.h>

// Input

typedef struct {
    Rectangle rec;
    Color color;
    int font_size;
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
    int step;
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

#endif
