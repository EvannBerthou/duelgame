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

bool slider_hover(slider *s);
void slider_render(slider *s);

#endif
