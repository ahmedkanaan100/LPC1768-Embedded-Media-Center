#include <LPC17xx.h>
#include "GLCD.h"
#include "LED.h"
#include "photo1.h"
#include "photo2.h"
#include "photo3.h"
#include "MediaAudio.h"

#define JOY_UP       (1UL << 23)
#define JOY_DOWN     (1UL << 25)
#define JOY_SELECT   (1UL << 20)
#define JOY_LEFT     (1UL << 26)
#define JOY_RIGHT    (1UL << 24)
#define JOY_MASK     (JOY_UP | JOY_DOWN | JOY_SELECT | JOY_LEFT | JOY_RIGHT)

#define SNAKE_COLS   20U
#define SNAKE_ROWS   12U
#define CELL_SIZE    12U
#define FIELD_X      40U
#define FIELD_Y      44U
#define SNAKE_LIMIT  (SNAKE_COLS * SNAKE_ROWS)

#define DIR_UP       0U
#define DIR_RIGHT    1U
#define DIR_DOWN     2U
#define DIR_LEFT     3U

static unsigned char snake_x[SNAKE_LIMIT];
static unsigned char snake_y[SNAKE_LIMIT];
static unsigned int snake_length;
static unsigned int snake_direction;
static unsigned int food_x;
static unsigned int food_y;
static unsigned int score;
static unsigned long random_state = 1234567UL;
static unsigned short cell_bg[CELL_SIZE * CELL_SIZE];
static unsigned short cell_body[CELL_SIZE * CELL_SIZE];
static unsigned short cell_head[CELL_SIZE * CELL_SIZE];
static unsigned short cell_food[CELL_SIZE * CELL_SIZE];

static void Joystick_Init(void)
{
    LPC_PINCON->PINSEL3 &= ~((3UL << 8) | (3UL << 14) |
                             (3UL << 16) | (3UL << 18) |
                             (3UL << 20));
    LPC_PINCON->PINMODE3 &= ~((3UL << 8) | (3UL << 14) |
                              (3UL << 16) | (3UL << 18) |
                              (3UL << 20));
    LPC_GPIO1->FIODIR &= ~JOY_MASK;
}

static unsigned int Joystick_Read(void)
{
    unsigned long pins;
    pins = LPC_GPIO1->FIOPIN;
    if ((pins & JOY_UP) == 0UL) return JOY_UP;
    if ((pins & JOY_DOWN) == 0UL) return JOY_DOWN;
    if ((pins & JOY_LEFT) == 0UL) return JOY_LEFT;
    if ((pins & JOY_RIGHT) == 0UL) return JOY_RIGHT;
    if ((pins & JOY_SELECT) == 0UL) return JOY_SELECT;
    return 0U;
}

static void Joystick_WaitRelease(unsigned int key)
{
    volatile unsigned int i;
    for (i = 0U; i < 100000U; ++i) { }
    while ((LPC_GPIO1->FIOPIN & key) == 0UL) { }
    for (i = 0U; i < 100000U; ++i) { }
}

static void Draw_Menu(unsigned int selected)
{
    GLCD_Clear(White);
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(0, 1, 1, (unsigned char *)"MEDIA CENTER");

    GLCD_SetTextColor(Black);
    GLCD_SetBackColor(selected == 0U ? Yellow : White);
    GLCD_DisplayString(3, 1, 1, (unsigned char *)" Photo Gallery ");
    GLCD_SetBackColor(selected == 1U ? Yellow : White);
    GLCD_DisplayString(5, 1, 1, (unsigned char *)" MP3 Player    ");
    GLCD_SetBackColor(selected == 2U ? Yellow : White);
    GLCD_DisplayString(7, 1, 1, (unsigned char *)" Snake Game    ");

    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(27, 1, 0, (unsigned char *)"UP/DOWN: MOVE   SELECT: OPEN");
}

static void Draw_Gallery(unsigned int photo)
{
    GLCD_Clear(White);
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(0, 1, 1, (unsigned char *)"PHOTO GALLERY");

    if (photo == 0U) {
        GLCD_Bitmap(80, 44, 160, 120, (unsigned char *)photo1_pixels);
        GLCD_DisplayString(24, 1, 0, (unsigned char *)"1/3 - Mountain Lake");
    } else if (photo == 1U) {
        GLCD_Bitmap(80, 44, 160, 120, (unsigned char *)photo2_pixels);
        GLCD_DisplayString(24, 1, 0, (unsigned char *)"2/3 - City Skyline");
    } else {
        GLCD_Bitmap(80, 44, 160, 120, (unsigned char *)photo3_pixels);
        GLCD_DisplayString(24, 1, 0, (unsigned char *)"3/3 - Flower Garden");
    }
    GLCD_DisplayString(27, 1, 0, (unsigned char *)"UP/DOWN: BROWSE  SELECT: BACK");
}

static void Run_Gallery(void)
{
    unsigned int photo;
    unsigned int key;
    photo = 0U;

    Draw_Gallery(photo);
    for (;;) {
        key = Joystick_Read();
        if (key == JOY_UP) {
            Joystick_WaitRelease(key);
            photo = (photo + 2U) % 3U;
            Draw_Gallery(photo);
        } else if (key == JOY_DOWN) {
            Joystick_WaitRelease(key);
            photo = (photo + 1U) % 3U;
            Draw_Gallery(photo);
        } else if (key == JOY_SELECT) {
            Joystick_WaitRelease(key);
            return;
        }
    }
}

/* MP3 menu: USB audio is active ONLY while this page is open. */
static void Run_MP3_Player(void)
{
    GLCD_Clear(White);
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(0, 1, 1, (unsigned char *)"USB AUDIO PLAYER");
    GLCD_SetTextColor(Black);
    GLCD_DisplayString(4, 1, 0, (unsigned char *)"PLAY AUDIO ON WINDOWS PC");
    GLCD_DisplayString(8, 1, 0, (unsigned char *)"SELECT NXP LPC17xx SPEAKER");
    GLCD_DisplayString(12, 1, 0, (unsigned char *)"BLUE KNOB: CHANGE VOLUME");
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(27, 1, 0, (unsigned char *)"PRESS SELECT: DISCONNECT");

    MediaAudio_Start();

    for (;;) {
        if (Joystick_Read() == JOY_SELECT) {
            Joystick_WaitRelease(JOY_SELECT);
            MediaAudio_Stop();
            return;
        }
    }
}

static void Snake_InitTiles(void)
{
    unsigned int i;
    for (i = 0U; i < CELL_SIZE * CELL_SIZE; ++i) {
        cell_bg[i] = 0xEF7DU;
        cell_body[i] = 0x05E0U;
        cell_head[i] = 0x0320U;
        cell_food[i] = 0xF800U;
    }
}

static void Snake_DrawCell(unsigned int x, unsigned int y, unsigned short *pixels)
{
    GLCD_Bitmap(FIELD_X + x * CELL_SIZE,
                FIELD_Y + y * CELL_SIZE,
                CELL_SIZE, CELL_SIZE,
                (unsigned char *)pixels);
}

static void Snake_DrawScore(void)
{
    unsigned int tens;
    unsigned int ones;
    unsigned char label[] = "SCORE: 000";
    tens = (score / 10U) % 10U;
    ones = score % 10U;
    label[7] = (unsigned char)('0' + ((score / 100U) % 10U));
    label[8] = (unsigned char)('0' + tens);
    label[9] = (unsigned char)('0' + ones);
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(25, 2, 0, label);
}

static unsigned int Snake_Occupies(unsigned int x, unsigned int y)
{
    unsigned int i;
    for (i = 0U; i < snake_length; ++i) {
        if ((unsigned int)snake_x[i] == x && (unsigned int)snake_y[i] == y)
            return 1U;
    }
    return 0U;
}

static void Snake_NewFood(void)
{
    unsigned int attempt;
    unsigned int x;
    unsigned int y;

    for (attempt = 0U; attempt < SNAKE_LIMIT * 3U; ++attempt) {
        random_state = random_state * 1664525UL + 1013904223UL;
        x = (unsigned int)((random_state >> 8) % SNAKE_COLS);
        random_state = random_state * 1664525UL + 1013904223UL;
        y = (unsigned int)((random_state >> 8) % SNAKE_ROWS);
        if (!Snake_Occupies(x, y)) {
            food_x = x;
            food_y = y;
            Snake_DrawCell(food_x, food_y, cell_food);
            return;
        }
    }
    for (y = 0U; y < SNAKE_ROWS; ++y) {
        for (x = 0U; x < SNAKE_COLS; ++x) {
            if (!Snake_Occupies(x, y)) {
                food_x = x;
                food_y = y;
                Snake_DrawCell(food_x, food_y, cell_food);
                return;
            }
        }
    }
}

static void Snake_Pause(void)
{
    volatile unsigned long i;
    for (i = 0UL; i < 2800000UL; ++i) { __NOP(); }
}

static void Snake_GameOver(void)
{
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Red);
    GLCD_DisplayString(0, 1, 1, (unsigned char *)"GAME OVER");
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(27, 1, 0, (unsigned char *)"SELECT: BACK TO MAIN MENU");
    while (Joystick_Read() == JOY_SELECT) { }
    for (;;) {
        if (Joystick_Read() == JOY_SELECT) {
            Joystick_WaitRelease(JOY_SELECT);
            return;
        }
    }
}

static void Run_Snake(void)
{
    unsigned int x;
    unsigned int y;
    unsigned int i;
    unsigned int key;
    unsigned int new_direction;
    unsigned int grow;
    unsigned int collided;
    int new_x;
    int new_y;

    Snake_InitTiles();
    snake_length = 3U;
    snake_direction = DIR_RIGHT;
    score = 0U;
    snake_x[0] = 10U; snake_y[0] = 6U;
    snake_x[1] = 9U;  snake_y[1] = 6U;
    snake_x[2] = 8U;  snake_y[2] = 6U;

    GLCD_Clear(White);
    GLCD_SetBackColor(White);
    GLCD_SetTextColor(Blue);
    GLCD_DisplayString(0, 1, 1, (unsigned char *)"SNAKE GAME");
    GLCD_DisplayString(27, 1, 0, (unsigned char *)"JOYSTICK: MOVE  SELECT: EXIT");
    Snake_DrawScore();

    for (y = 0U; y < SNAKE_ROWS; ++y) {
        for (x = 0U; x < SNAKE_COLS; ++x) {
            Snake_DrawCell(x, y, cell_bg);
        }
    }
    Snake_DrawCell(8U, 6U, cell_body);
    Snake_DrawCell(9U, 6U, cell_body);
    Snake_DrawCell(10U, 6U, cell_head);
    Snake_NewFood();

    for (;;) {
        key = Joystick_Read();
        if (key == JOY_SELECT) {
            Joystick_WaitRelease(key);
            return;
        }

        new_direction = snake_direction;
        if (key == JOY_UP) new_direction = DIR_UP;
        else if (key == JOY_DOWN) new_direction = DIR_DOWN;
        else if (key == JOY_LEFT) new_direction = DIR_LEFT;
        else if (key == JOY_RIGHT) new_direction = DIR_RIGHT;

        if (((snake_direction + 2U) % 4U) != new_direction) {
            snake_direction = new_direction;
        }

        Snake_Pause();

        new_x = (int)snake_x[0];
        new_y = (int)snake_y[0];
        if (snake_direction == DIR_UP) --new_y;
        else if (snake_direction == DIR_DOWN) ++new_y;
        else if (snake_direction == DIR_LEFT) --new_x;
        else ++new_x;

        if (new_x < 0 || new_y < 0 ||
            new_x >= (int)SNAKE_COLS || new_y >= (int)SNAKE_ROWS) {
            Snake_GameOver();
            return;
        }

        grow = ((unsigned int)new_x == food_x &&
                (unsigned int)new_y == food_y) ? 1U : 0U;
        collided = 0U;
        for (i = 0U; i < snake_length - (grow ? 0U : 1U); ++i) {
            if ((unsigned int)snake_x[i] == (unsigned int)new_x &&
                (unsigned int)snake_y[i] == (unsigned int)new_y) {
                collided = 1U;
                break;
            }
        }
        if (collided) {
            Snake_GameOver();
            return;
        }

        Snake_DrawCell(snake_x[0], snake_y[0], cell_body);
        if (grow) {
            if (snake_length < SNAKE_LIMIT) ++snake_length;
        } else {
            Snake_DrawCell(snake_x[snake_length - 1U],
                           snake_y[snake_length - 1U], cell_bg);
        }
        for (i = snake_length - 1U; i > 0U; --i) {
            snake_x[i] = snake_x[i - 1U];
            snake_y[i] = snake_y[i - 1U];
        }
        snake_x[0] = (unsigned char)new_x;
        snake_y[0] = (unsigned char)new_y;
        Snake_DrawCell(snake_x[0], snake_y[0], cell_head);
        if (grow) {
            ++score;
            Snake_DrawScore();
            if (snake_length >= SNAKE_LIMIT) {
                Snake_GameOver();
                return;
            }
            Snake_NewFood();
        }
    }
}

int main(void)
{
    unsigned int selected;
    unsigned int key;

    selected = 0U;
    LED_Init();
    GLCD_Init();
    Joystick_Init();
    Draw_Menu(selected);

    for (;;) {
        key = Joystick_Read();
        if (key == JOY_UP) {
            Joystick_WaitRelease(key);
            selected = (selected + 2U) % 3U;
            Draw_Menu(selected);
        } else if (key == JOY_DOWN) {
            Joystick_WaitRelease(key);
            selected = (selected + 1U) % 3U;
            Draw_Menu(selected);
        } else if (key == JOY_SELECT) {
            Joystick_WaitRelease(key);
            if (selected == 0U) Run_Gallery();
            else if (selected == 1U) Run_MP3_Player();
            else Run_Snake();
            Draw_Menu(selected);
        }
    }
}
