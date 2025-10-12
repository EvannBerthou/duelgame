#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

typedef enum {
    TYPE_UINT8,
    TYPE_UINT8_PTR,
    TYPE_UINT8_ARRAY,
    TYPE_CHAR_ARRAY,
    TYPE_UINT64,
    TYPE_STRING,
    TYPE_CUSTOM,
} net_struct_field_type;

typedef struct {
    net_struct_field_type type;
    const char *custom_type;
    const char *name;
    int array_size;
    const char *size;
    const char *array_size_str;
} struct_field;

typedef struct {
    const char *name;
    struct_field fields[128];
    int field_count;
    int variable_size;
} net_struct;

net_struct structs[128] = {0};
int structs_count = 0;

int get_struct_size(net_struct *s);

int get_field_size(struct_field *f) {
    if (f->type == TYPE_CHAR_ARRAY) {
        return f->array_size;
    } else if (f->type == TYPE_UINT8) {
        return sizeof(uint8_t);
    } else if (f->type == TYPE_UINT8_PTR) {
        return 0;
    } else if (f->type == TYPE_UINT8_ARRAY) {
        return 0;
    } else if (f->type == TYPE_UINT64) {
        return sizeof(uint64_t);
    } else if (f->type == TYPE_CUSTOM) {
        for (int i = 0; i < structs_count; i++) {
            if (strcmp(f->name, structs[i].name) == 0) {
                return get_struct_size(&structs[i]);
            }
        }
    } else if (f->type == TYPE_STRING) {
        return 0;
    } else {
        printf("Unknown field\n");
        exit(1);
    }
    return 0;
}

int get_struct_size(net_struct *s) {
    int sum = 0;
    for (int i = 0; i < s->field_count; i++) {
        sum += get_field_size(&s->fields[i]);
    }
    return sum;
}

const char *struct_upper(net_struct *s) {
    int offset = strlen("net_packet_");
    static char upper[128] = {0};
    int len = strlen(s->name) - offset;
    for (int x = 0; x < len; x++) {
        upper[x] = toupper(s->name[offset + x]);
    }
    upper[len] = 0;
    return upper;
}

const char *struct_upper_str(const char *s) {
    int offset = strlen("net_packet_");
    static char upper[128] = {0};
    int len = strlen(s) - offset;
    for (int x = 0; x < len; x++) {
        upper[x] = toupper(s[offset + x]);
    }
    upper[len] = 0;
    return upper;
}

void expect_next_token(stb_lexer *l, long expect) {
    stb_c_lexer_get_token(l);
    if (l->token != expect) {
        stb_lex_location loc = {0};
        stb_c_lexer_get_location(l, l->where_firstchar, &loc);
        printf("%d:%d: ERROR: expected %ld, but got %ld\n", loc.line_number, loc.line_offset, expect, l->token);
        exit(1);
    }
}

void expect_next_token_string(stb_lexer *l, const char *s) {
    expect_next_token(l, CLEX_id);
    if (strcmp(l->string, s) != 0) {
        exit(1);
    }
}

void expect_next_token_char(stb_lexer *l, long c) {
    stb_c_lexer_get_token(l);
    if (l->token != c) {
        stb_lex_location loc = {0};
        stb_c_lexer_get_location(l, l->where_firstchar, &loc);
        printf("%d:%d: ERROR: expected char %c but got %ld\n", loc.line_number, loc.line_offset, (char)c, l->token);
        exit(1);
    }
}

int is_next_token_string(stb_lexer *l, const char *s) {
    int res = 0;
    char *saved_point = l->parse_point;
    stb_c_lexer_get_token(l);
    if (l->token == CLEX_id && strcmp(l->string, s) == 0) {
        res = 1;
    }
    l->parse_point = saved_point;
    return res;
}

int is_next_token_size(stb_lexer *l) {
    int res = 0;
    char *saved_point = l->parse_point;
    stb_c_lexer_get_token(l);
    if (l->token == CLEX_id && strncmp(l->string, "NET_SIZE", strlen("NET_SIZE")) == 0) {
        res = 1;
    }
    l->parse_point = saved_point;
    return res;
}

int is_next_token_char(stb_lexer *l, long c) {
    int res = 0;
    char *saved_point = l->parse_point;
    stb_c_lexer_get_token(l);
    if (l->token == c) {
        res = 1;
    }
    l->parse_point = saved_point;
    return res;
}

void parse_struct(stb_lexer *l) {
    net_struct *s = &structs[structs_count];
    expect_next_token_string(l, "struct");
    expect_next_token_char(l, '{');
    while (1) {
        char *saved_point = l->parse_point;
        stb_c_lexer_get_token(l);
        if (l->token == '}') {
            break;
        }
        l->parse_point = saved_point;

        struct_field *f = &s->fields[s->field_count];
        expect_next_token(l, CLEX_id);
        if (strcmp(l->string, "char") == 0) {
            f->type = TYPE_CHAR_ARRAY;
            expect_next_token(l, CLEX_id);
            f->name = strdup(l->string);
            expect_next_token_char(l, '[');
            expect_next_token(l, CLEX_intlit);
            f->array_size = l->int_number;
            expect_next_token_char(l, ']');
        } else if (strcmp(l->string, "uint8_t") == 0) {
            f->type = TYPE_UINT8;
            if (is_next_token_char(l, '*')) {
                expect_next_token_char(l, '*');
                f->type = TYPE_UINT8_PTR;
            }

            expect_next_token(l, CLEX_id);
            f->name = strdup(l->string);

            if (is_next_token_char(l, '[')) {
                f->type = TYPE_UINT8_ARRAY;
                expect_next_token_char(l, '[');
                expect_next_token(l, CLEX_id);
                expect_next_token_char(l, ']');
            }
        } else if (strcmp(l->string, "uint64_t") == 0) {
            f->type = TYPE_UINT64;
            expect_next_token(l, CLEX_id);
            f->name = strdup(l->string);
        } else {
            int exists = 0;
            for (int i = 0; i < structs_count; i++) {
                if (strcmp(l->string, structs[i].name) == 0) {
                    exists = 1;
                    break;
                }
            }
            if (exists) {
                f->type = TYPE_CUSTOM;
                f->custom_type = strdup(l->string);
                expect_next_token(l, CLEX_id);
                f->name = strdup(l->string);
                if (is_next_token_char(l, '[')) {
                    expect_next_token_char(l, '[');
                    expect_next_token(l, CLEX_id);
                    f->array_size_str = strdup(l->string);
                    expect_next_token_char(l, ']');
                }
            } else {
                if (strcmp(l->string, "network_string") == 0) {
                    f->type = TYPE_STRING;
                    expect_next_token(l, CLEX_id);
                    f->name = strdup(l->string);
                    s->variable_size = 1;
                    f->size = calloc(100, 1);
                    strcat((char *)f->size, "s->");
                    strcat((char *)f->size, f->name);
                    strcat((char *)f->size, ".len + 1");
                } else {
                    fprintf(stderr, "Structure %s does not exists\n", l->string);
                    exit(1);
                }
            }
        }

        if (is_next_token_size(l)) {
            s->variable_size = 1;
            expect_next_token(l, CLEX_id);
            expect_next_token_char(l, '(');
            expect_next_token(l, CLEX_dqstring);
            f->size = strdup(l->string);
            expect_next_token_char(l, ')');
        }
        expect_next_token_char(l, ';');
        s->field_count++;
    }
    expect_next_token(l, CLEX_id);
    if (strcmp(l->string, "net_packet") == 0 || strcmp(l->string, "network_string") == 0) {
        fprintf(stderr, "skipping %s\n", l->string);
        s->field_count = 0;
        return;
    }
    s->name = strdup(l->string);
    expect_next_token_char(l, ';');
    structs_count++;
}

int main(void) {
    stb_lexer l = {0};
    char store[2048] = {0};
    // TODO: Should be an argument
    FILE *f = fopen("include/net_protocol_base.h", "r");
    if (f == NULL) {
        printf("Error reading file\n");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    stb_c_lexer_init(&l, string, string + fsize, store, 2048);
    stb_c_lexer_get_token(&l);
    while (l.token != CLEX_eof) {
        if (l.token == CLEX_id) {
            parse_struct(&l);
        }
        stb_c_lexer_get_token(&l);
    }

    // Headers
    printf("#ifndef NET_PROTOCOL_H\n");
    printf("#define NET_PROTOCOL_H\n");
    printf("// This file was auto-generated by %s\n", __FILE__);

    printf("%s\n", string);

    // Enum
    printf("typedef enum {\n");
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        printf("    PKT_%s,\n", struct_upper(s));
    }
    printf("} net_packet_type_enum;\n");

    // Signatures
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        const char *suffix = s->name + strlen("net_packet_");
        printf("net_packet pkt_%s(", suffix);
        for (int j = 0; j < s->field_count; j++) {
            struct_field *f = &s->fields[j];
            if (f->type == TYPE_UINT8) {
                printf("uint8_t %s", f->name);
            } else if (f->type == TYPE_UINT8_PTR) {
                printf("uint8_t *%s", f->name);
            } else if (f->type == TYPE_UINT8_ARRAY) {
                printf("uint8_t *%s", f->name);
            } else if (f->type == TYPE_CHAR_ARRAY) {
                printf("const char *%s", f->name);
            } else if (f->type == TYPE_UINT64) {
                printf("uint64_t %s", f->name);
            } else if (f->type == TYPE_CUSTOM) {
                printf("%s *%s", f->custom_type, f->name);
            } else if (f->type == TYPE_STRING) {
                printf("const char *%s", f->name);
            } else {
                fprintf(stderr, "Unknown type for signature\n");
                exit(1);
            }

            if (j != s->field_count - 1) {
                printf(", ");
            }
        }
        printf(");\n");
    }

    printf("#ifdef NET_PROTOCOL_IMPLEMENTATION\n");

    // Size
    printf("uint64_t get_packet_length(net_packet_type_enum type, void *p) {\n");
    printf("    switch (type) {\n");
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        if (s->variable_size) {
            printf("        case PKT_%s: {\n", struct_upper(s));
            printf("            %s *s = (%s*)p;\n", s->name, s->name);
            printf("            (void)s;\n");
            printf("            return %d", get_struct_size(s));
            for (int x = 0; x < s->field_count; x++) {
                if (s->fields[x].size != NULL) {
                    printf(" + (%s)", s->fields[x].size);
                }
            }
            printf(";\n        }\n");
        } else {
            printf("        case PKT_%s: return %d;\n", struct_upper(s), get_struct_size(s));
        }
    }
    printf("        default: { fprintf(stderr, \"Unknown type\\n\"); exit(1); }\n");
    printf("    }\n");

    printf("}\n");
    //
    // Builders
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        const char *suffix = s->name + strlen("net_packet_");
        printf("net_packet pkt_%s(", suffix);
        for (int j = 0; j < s->field_count; j++) {
            struct_field *f = &s->fields[j];
            if (f->type == TYPE_UINT8) {
                printf("uint8_t %s", f->name);
            } else if (f->type == TYPE_UINT8_PTR) {
                printf("uint8_t *%s", f->name);
            } else if (f->type == TYPE_UINT8_ARRAY) {
                printf("uint8_t *%s", f->name);
            } else if (f->type == TYPE_CHAR_ARRAY) {
                printf("const char *%s", f->name);
            } else if (f->type == TYPE_UINT64) {
                printf("uint64_t %s", f->name);
            } else if (f->type == TYPE_CUSTOM) {
                printf("%s *%s", f->custom_type, f->name);
            } else if (f->type == TYPE_STRING) {
                printf("const char *%s", f->name);
            } else {
                fprintf(stderr, "Unkown type for builder %s\n", f->name);
                exit(1);
            }
            if (j != s->field_count - 1) {
                printf(", ");
            }
        }
        printf(") {\n");
        printf("    %s s = {};\n", s->name);
        for (int j = 0; j < s->field_count; j++) {
            struct_field *f = &s->fields[j];
            if (f->type == TYPE_UINT8) {
                printf("    s.%s = %s;\n", f->name, f->name);
            } else if (f->type == TYPE_UINT8_PTR) {
                printf("    s.%s = %s;\n", f->name, f->name);
            } else if (f->type == TYPE_UINT8_ARRAY) {
                printf("    memcpy(s.%s, %s, %s);\n", f->name, f->name, f->size);
            } else if (f->type == TYPE_CHAR_ARRAY) {
                // TODO: Use strcpy ?
                printf("    memcpy(s.%s, %s, %d);\n", f->name, f->name, f->array_size);
            } else if (f->type == TYPE_UINT64) {
                printf("    s.%s = %s;\n", f->name, f->name);
            } else if (f->type == TYPE_CUSTOM) {
                printf("    for (int i = 0; i < %s; i++) {\n", f->array_size_str);
                printf("        s.%s[i] = %s[i];\n", f->name, f->name);
                printf("    }\n");
            } else if (f->type == TYPE_STRING) {
                printf("    s.%s.len = strlen(%s);\n", f->name, f->name);
                printf("    memcpy(s.%s.str, %s, s.%s.len);\n", f->name, f->name, f->name);
            } else {
                fprintf(stderr, "Unknown type for %s\n", f->name);
                exit(1);
            }
        }
        printf("    net_packet result = {0};\n");
        printf("    result.type = PKT_%s;\n", struct_upper(s));
        printf("    result.len = get_packet_length(PKT_%s, &s);\n", struct_upper(s));
        printf("    memcpy(result.content, &s, sizeof(%s));\n", s->name);
        printf("    return result;\n");
        printf("}\n");
    }

    printf("char *packu8(char *buf, uint8_t u);\n");
    printf("char *packu64(char *buf, uint64_t u);\n");
    printf("char *packsv(char *buf, char *str, int len);\n");

    printf("char *packstruct(char *buf, void *content, net_packet_type_enum type) {\n");
    printf("    switch (type) {\n");
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        printf("        case PKT_%s: {\n", struct_upper(s));
        if (s->field_count != 0) {
            printf("            %s *s = (%s*)content;\n", s->name, s->name);
            for (int j = 0; j < s->field_count; j++) {
                struct_field *f = &s->fields[j];
                if (f->type == TYPE_UINT8) {
                    printf("            buf = packu8(buf, s->%s);\n", f->name);
                } else if (f->type == TYPE_CHAR_ARRAY) {
                    printf("            buf = packsv(buf, s->%s, %d);\n", f->name, f->array_size);
                } else if (f->type == TYPE_UINT8_PTR) {
                    printf("            for (int i = 0; i < %s; i++) {\n", f->size);
                    printf("                buf = packu8(buf, s->%s[i]);\n", f->name);
                    printf("            }\n");
                } else if (f->type == TYPE_UINT8_ARRAY) {
                    printf("            for (int i = 0; i < %s; i++) {\n", f->size);
                    printf("                buf = packu8(buf, s->%s[i]);\n", f->name);
                    printf("            }\n");
                } else if (f->type == TYPE_UINT64) {
                    printf("            buf = packu64(buf, s->%s);\n", f->name);
                } else if (f->type == TYPE_CUSTOM) {
                    if (f->array_size_str != NULL) {
                        printf("            for (int i = 0; i < %s; i++) {\n", f->array_size_str);
                        printf("                buf = packstruct(buf, &s->%s[i], PKT_%s);\n", f->name,
                               struct_upper_str(f->custom_type));
                        printf("            }\n");
                    } else {
                        printf("TODO %d", __LINE__);
                        exit(1);
                    }
                } else if (f->type == TYPE_STRING) {
                    printf("            buf = packu8(buf, s->%s.len);\n", f->name);
                    printf("            buf = packsv(buf, s->%s.str, s->%s.len);\n", f->name, f->name);
                } else {
                    fprintf(stderr, "Unknown type to pack %s\n", f->name);
                    exit(1);
                }
            }
        }
        printf("            return buf;\n");
        printf("        } break;\n");
    }
    printf("    }\n");
    printf("    return buf;\n");
    printf("}\n");

    printf("uint8_t unpacku8(uint8_t **buf);\n");
    printf("uint64_t unpacku64(uint8_t **buf);\n");
    printf("void unpacksv(uint8_t **buf, char *dest, uint8_t len);\n");

    printf("void *unpackstruct(net_packet_type_enum type, uint8_t *buf, uint8_t *out) {\n");
    printf("    uint8_t **base = &buf;\n");
    printf("    switch (type) {\n");
    for (int i = 0; i < structs_count; i++) {
        net_struct *s = &structs[i];
        if (s->field_count == 0) {
            printf("        case PKT_%s: return NULL;\n", struct_upper(s));
        } else {
            printf("        case PKT_%s: {\n", struct_upper(s));
            printf("            %s *s = (%s*)out;\n", s->name, s->name);
            printf("            if (s == NULL) exit(1);\n");
            for (int j = 0; j < s->field_count; j++) {
                struct_field *f = &s->fields[j];
                if (f->type == TYPE_UINT8) {
                    printf("            s->%s = unpacku8(base);\n", f->name);
                } else if (f->type == TYPE_CHAR_ARRAY) {
                    printf("            unpacksv(base, s->%s, %d);\n", f->name, f->array_size);
                } else if (f->type == TYPE_UINT8_PTR) {
                    printf("            s->%s = malloc(%s);\n", f->name, f->size);
                    printf("            for (int i = 0; i < %s; i++) {\n", f->size);
                    printf("                s->%s[i] = unpacku8(base);\n", f->name);
                    printf("            }\n");
                } else if (f->type == TYPE_UINT8_ARRAY) {
                    printf("            for (int i = 0; i < %s; i++) {\n", f->size);
                    printf("                s->%s[i] = unpacku8(base);\n", f->name);
                    printf("            }\n");
                } else if (f->type == TYPE_UINT64) {
                    printf("            s->%s = unpacku64(base);\n", f->name);
                } else if (f->type == TYPE_CUSTOM) {
                    if (f->array_size_str != NULL) {
                        printf("            for (int i = 0; i < %s; i++) {\n", f->array_size_str);
                        printf("                base = unpackstruct(PKT_%s, *base, (void*)&s->%s[i]);\n",
                               struct_upper_str(f->custom_type), f->name);
                        printf("            }\n");
                    } else {
                        printf("TODO %d", __LINE__);
                        exit(1);
                    }
                } else if (f->type == TYPE_STRING) {
                    printf("            s->%s.len = unpacku8(base);\n", f->name);
                    printf("            unpacksv(base, s->%s.str, s->%s.len);\n", f->name, f->name);
                } else {
                    fprintf(stderr, "Unknown type to unpack %s\n", f->name);
                    exit(1);
                }
            }
            printf("            return base;\n");
            printf("        } break;\n");
        }
    }
    printf("    }\n");
    printf("    return NULL;\n");
    printf("}\n");

    printf("#endif\n");
    printf("#endif\n");

    return 0;
}
