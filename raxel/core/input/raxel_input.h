#ifndef __RAXEL_INPUT_H__
#define __RAXEL_INPUT_H__

#include <raxel/core/util.h>
typedef struct raxel_surface raxel_surface_t;


typedef enum raxel_keys {
    /* Printable keys */
    RAXEL_KEY_SPACE = 32,
    RAXEL_KEY_APOSTROPHE = 39, /* ' */
    RAXEL_KEY_COMMA = 44,      /* , */
    RAXEL_KEY_MINUS = 45,      /* - */
    RAXEL_KEY_PERIOD = 46,     /* . */
    RAXEL_KEY_SLASH = 47,      /* / */
    RAXEL_KEY_0 = 48,
    RAXEL_KEY_1 = 49,
    RAXEL_KEY_2 = 50,
    RAXEL_KEY_3 = 51,
    RAXEL_KEY_4 = 52,
    RAXEL_KEY_5 = 53,
    RAXEL_KEY_6 = 54,
    RAXEL_KEY_7 = 55,
    RAXEL_KEY_8 = 56,
    RAXEL_KEY_9 = 57,
    RAXEL_KEY_SEMICOLON = 59, /* ; */
    RAXEL_KEY_EQUAL = 61,     /* = */
    RAXEL_KEY_A = 65,
    RAXEL_KEY_B = 66,
    RAXEL_KEY_C = 67,
    RAXEL_KEY_D = 68,
    RAXEL_KEY_E = 69,
    RAXEL_KEY_F = 70,
    RAXEL_KEY_G = 71,
    RAXEL_KEY_H = 72,
    RAXEL_KEY_I = 73,
    RAXEL_KEY_J = 74,
    RAXEL_KEY_K = 75,
    RAXEL_KEY_L = 76,
    RAXEL_KEY_M = 77,
    RAXEL_KEY_N = 78,
    RAXEL_KEY_O = 79,
    RAXEL_KEY_P = 80,
    RAXEL_KEY_Q = 81,
    RAXEL_KEY_R = 82,
    RAXEL_KEY_S = 83,
    RAXEL_KEY_T = 84,
    RAXEL_KEY_U = 85,
    RAXEL_KEY_V = 86,
    RAXEL_KEY_W = 87,
    RAXEL_KEY_X = 88,
    RAXEL_KEY_Y = 89,
    RAXEL_KEY_Z = 90,
    RAXEL_KEY_LEFT_BRACKET = 91,  /* [ */
    RAXEL_KEY_BACKSLASH = 92,     /* \ */
    RAXEL_KEY_RIGHT_BRACKET = 93, /* ] */
    RAXEL_KEY_GRAVE_ACCENT = 96,  /* ` */
    RAXEL_KEY_WORLD_1 = 161,      /* non-US #1 */
    RAXEL_KEY_WORLD_2 = 162,      /* non-keys */
    RAXEL_KEY_ESCAPE = 256,
    RAXEL_KEY_ENTER = 257,
    RAXEL_KEY_TAB = 258,
    RAXEL_KEY_BACKSPACE = 259,
    RAXEL_KEY_INSERT = 260,
    RAXEL_KEY_DELETE = 261,
    RAXEL_KEY_RIGHT = 262,
    RAXEL_KEY_LEFT = 263,
    RAXEL_KEY_DOWN = 264,
    RAXEL_KEY_UP = 265,
    RAXEL_KEY_PAGE_UP = 266,
    RAXEL_KEY_PAGE_DOWN = 267,
    RAXEL_KEY_HOME = 268,
    RAXEL_KEY_END = 269,
    RAXEL_KEY_CAPS_LOCK = 280,
    RAXEL_KEY_SCROLL_LOCK = 281,
    RAXEL_KEY_NUM_LOCK = 282,
    RAXEL_KEY_PRINT_SCREEN = 283,
    RAXEL_KEY_PAUSE = 284,
    RAXEL_KEY_F1 = 290,
    RAXEL_KEY_F2 = 291,
    RAXEL_KEY_F3 = 292,
    RAXEL_KEY_F4 = 293,
    RAXEL_KEY_F5 = 294,
    RAXEL_KEY_F6 = 295,
    RAXEL_KEY_F7 = 296,
    RAXEL_KEY_F8 = 297,
    RAXEL_KEY_F9 = 298,
    RAXEL_KEY_F10 = 299,
    RAXEL_KEY_F11 = 300,
    RAXEL_KEY_F12 = 301,
    RAXEL_KEY_F13 = 302,
    RAXEL_KEY_F14 = 303,
    RAXEL_KEY_F15 = 304,
    RAXEL_KEY_F16 = 305,
    RAXEL_KEY_F17 = 306,
    RAXEL_KEY_F18 = 307,
    RAXEL_KEY_F19 = 308,
    RAXEL_KEY_F20 = 309,
    RAXEL_KEY_F21 = 310,
    RAXEL_KEY_F22 = 311,
    RAXEL_KEY_F23 = 312,
    RAXEL_KEY_F24 = 313,
    RAXEL_KEY_F25 = 314,
    RAXEL_KEY_KP_0 = 320,
    RAXEL_KEY_KP_1 = 321,
    RAXEL_KEY_KP_2 = 322,
    RAXEL_KEY_KP_3 = 323,
    RAXEL_KEY_KP_4 = 324,
    RAXEL_KEY_KP_5 = 325,
    RAXEL_KEY_KP_6 = 326,
    RAXEL_KEY_KP_7 = 327,
    RAXEL_KEY_KP_8 = 328,
    RAXEL_KEY_KP_9 = 329,
    RAXEL_KEY_KP_DECIMAL = 330,
    RAXEL_KEY_KP_DIVIDE = 331,
    RAXEL_KEY_KP_MULTIPLY = 332,
    RAXEL_KEY_KP_SUBTRACT = 333,
    RAXEL_KEY_KP_ADD = 334,
    RAXEL_KEY_KP_ENTER = 335,
    RAXEL_KEY_KP_EQUAL = 336,
    RAXEL_KEY_LEFT_SHIFT = 340,
    RAXEL_KEY_LEFT_CONTROL = 341,
    RAXEL_KEY_LEFT_ALT = 342,
    RAXEL_KEY_LEFT_SUPER = 343,
    RAXEL_KEY_RIGHT_SHIFT = 344,
    RAXEL_KEY_RIGHT_CONTROL = 345,
    RAXEL_KEY_RIGHT_ALT = 346,
    RAXEL_KEY_RIGHT_SUPER = 347,
    RAXEL_KEY_MENU = 348,
    RAXEL_KEY_COUNT = 349,
} raxel_keys_t;

typedef struct raxel_mouse_event {
    double x;
    double y;
    double dx;
    double dy;
    int button;
    int action;
    int mods;
} raxel_mouse_event_t;

typedef struct raxel_key_event {
    raxel_keys_t key;
    int scancode;
    int action;
    int mods;
} raxel_key_event_t;

typedef enum raxel_key_state {
    RAXEL_KEY_STATE_UP = 0,
    RAXEL_KEY_STATE_DOWN_THIS_FRAME = 1,
    RAXEL_KEY_STATE_DOWN = 2,
    RAXEL_KEY_STATE_UP_THIS_FRAME = 3,
} raxel_key_state_t;

typedef struct raxel_key_callback {
    raxel_keys_t key;
    void (*on_button)(raxel_key_event_t event);
} raxel_key_callback_t;

typedef struct raxel_mouse_callback {
    void (*on_mouse)(raxel_mouse_event_t event);
} raxel_mouse_callback_t;

typedef struct raxel_surface raxel_surface_t;

typedef struct raxel_input_manager {
    raxel_surface_t *__surface;
    raxel_allocator_t *__allocator;
    raxel_key_state_t __key_state[RAXEL_KEY_COUNT];
    raxel_list(raxel_key_callback_t) key_callbacks;
    raxel_list(raxel_mouse_callback_t) mouse_callbacks;
} raxel_input_manager_t;

raxel_input_manager_t *raxel_input_manager_create(raxel_allocator_t *allocator, raxel_surface_t *surface);
void raxel_input_manager_destroy(raxel_input_manager_t *manager);

void raxel_input_manager_update(raxel_input_manager_t *manager);
void raxel_input_manager_add_button_callback(raxel_input_manager_t *manager, raxel_key_callback_t callback);
void raxel_input_manager_add_mouse_callback(raxel_input_manager_t *manager, raxel_mouse_callback_t callback);

int raxel_input_manager_is_key_down(raxel_input_manager_t *manager, raxel_keys_t key);
int raxel_input_manager_is_key_up(raxel_input_manager_t *manager, raxel_keys_t key);
int raxel_input_manager_is_key_pressed(raxel_input_manager_t *manager, raxel_keys_t key);


#endif  // __RAXEL_INPUT_H__