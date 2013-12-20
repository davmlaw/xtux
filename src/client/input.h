enum {
    MOUSEMODE_OFF,
    MOUSEMODE_ROTATE,
    MOUSEMODE_POINT
};

enum {
    K_ACLOCKWISE,
    K_CLOCKWISE,
    K_FORWARD, /* 1<<0 */
    K_BACK,    /* 1<<1 */
    K_LEFT,    /* 1<<2 */
    K_RIGHT,   /* 1<<3 */
    K_B1,      /* 1<<4 */
    K_B2,      /* 1<<5 */
    K_WEAP_UP, /* 1<<6 */
    K_WEAP_DN, /* 1<<7 */
    K_SNIPER,
    K_SCREENSHOT,
    K_NS_TOGGLE,
    K_RAD_DEC, /* Crosshair radius modifiers (DEC/INC - rement) */
    K_RAD_INC, /* = Script-kiddie corporation? */
    K_TEXTENTRY,
    K_SUICIDE,
    K_FRAGS,
    NUM_KEYS
};

#define NUM_MOUSE_BUTTONS 3

#define RELEASED -1
#define UNPRESSED 0
#define PRESSED 1

byte get_input(void);
void input_clear(void);
int get_key(void);
