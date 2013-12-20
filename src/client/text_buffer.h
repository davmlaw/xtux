typedef enum {
    ALIGN_LEFT,
    ALIGN_CENTER
} align_t;

void text_buf_push(char *line, int color, align_t align);
void text_buf_update(void);
void text_buf_clear(void);
