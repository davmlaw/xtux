typedef struct {
    int pos;
    int width;
    int items;
    char **item_names;
} menu_t;

void menu_init(void);
void menu_driver(void);


