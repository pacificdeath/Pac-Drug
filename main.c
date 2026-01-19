#include "raylib/include/raylib.h"

#include <stdlib.h>
#include <math.h>

#define LEVEL_MAX_CHANGE 10

#define PNG_DIMENSIONS 192
#define GRID_WIDTH 19
#define GRID_HEIGHT 22

typedef struct GridPosition {
    int x; int y;
} GridPosition;

#define GRID_TOP_RIGHT ((GridPosition){GRID_WIDTH-1,0})
#define GRID_TOP_LEFT ((GridPosition){0,0})
#define GRID_BOTTOM_LEFT ((GridPosition){0,GRID_HEIGHT-1})
#define GRID_BOTTOM_RIGHT ((GridPosition){GRID_WIDTH-1,GRID_HEIGHT-1})
#define GRID_WIDTH 19
#define GRID_HEIGHT 22

#define FLAG_NONE 0
#define FLAG_WALL (1 << 0)
#define FLAG_DOT (1 << 1)
#define FLAG_BIG_DOT (1 << 2)
#define FLAG_OUT_OF_BOUNDS (1 << 3)
#define FLAG_WALL_TO_RIGHT (1 << 4)
#define FLAG_WALL_ABOVE (1 << 5)
#define FLAG_WALL_TO_LEFT (1 << 6)
#define FLAG_WALL_BELOW (1 << 7)

#define SPEED_MULTIPLIER 6.0f
#define SPEED_PLAYER (1.0f * SPEED_MULTIPLIER)
#define SPEED_GHOST_INSIDE (0.5f * SPEED_MULTIPLIER)
#define SPEED_GHOST_LEAVING (0.5f * SPEED_MULTIPLIER)
#define SPEED_GHOST_OUTSIDE (1.0f * SPEED_MULTIPLIER)
#define SPEED_GHOST_FRIGHTENED (0.5f * SPEED_MULTIPLIER)
#define SPEED_GHOST_RETURNING (1.0f * SPEED_MULTIPLIER)

#define CELL_PLAYER_START ((GridPosition){9,16})
#define CELL_OUTSIDE_GHOST_HOUSE_DOOR ((GridPosition){9,8})
#define CELL_GHOST_HOUSE_DOOR ((GridPosition){9,8})
#define CELL_GHOST_HOUSE_CENTER ((GridPosition){9,10})
#define CELL_GHOST_HOUSE_CENTER ((GridPosition){9,10})
#define CELL_GHOST_HOUSE_RIGHT_SIDE ((GridPosition){10,10})
#define CELL_GHOST_HOUSE_LEFT_SIDE ((GridPosition){8,10})

#define COLOR_PLAYER ((Color){0xff,0xff,0x00,0xff})
#define COLOR_BLINKY ((Color){0xff,0x00,0x00,0xff})
#define COLOR_PINKY ((Color){0xff,0x80,0xff,0xff})
#define COLOR_INKY ((Color){0x00,0xff,0xff,0xff})
#define COLOR_CLYDE ((Color){0xff,0x80,0x00,0xff})
#define COLOR_DOT ((Color){0xff,0xff,0xff,0xff})
#define COLOR_FLOOR ((Color){ 0x00,0x00,0x00,255})
#define COLOR_WALL ((Color){ 0x19,0x19,0xa6,255})
#define COLOR_GHOST_HOUSE_DOOR ((Color){255,0,0,255})
#define COLOR_FRIGHTENED ((Color){0x21,0x21,0xde,255})

#define GAP_ANIMATION_SPEED 25
#define GAP_SIZE_MULTIPLIER 75
#define HALF_GAP_SIZE_MULTIPLIER (GAP_SIZE_MULTIPLIER / 2)

#define LEVEL_INTRO_LENGTH 1.0f

enum {
    DIRECTION_NONE,
    DIRECTION_RIGHT,
    DIRECTION_UP,
    DIRECTION_LEFT,
    DIRECTION_DOWN,
};

enum {
    GHOST_BLINKY,
    GHOST_PINKY,
    GHOST_INKY,
    GHOST_CLYDE,
    GHOST_COUNT,
};

enum {
    GHOST_SHAPE_TRAPEZOID,
    GHOST_SHAPE_TRIANGLE,
};

enum {
    GHOST_STATE_INSIDE,
    GHOST_STATE_LEAVING,
    GHOST_STATE_OUTSIDE,
    GHOST_STATE_FRIGHTENED,
    GHOST_STATE_RETURNING,
};

enum {
    PHASE_NONE,
    PHASE_SCATTER,
    PHASE_CHASE,
    PHASE_FRIGHTENED,
};

typedef struct {
    int count;
    GridPosition positions[4];
    int directions[4];
} Surroundings;

typedef struct {
    GridPosition position;
    int direction;
    int requested_direction;
    Vector2 fraction_position;
} Player;

typedef struct {
    int state;
    int shape;
    GridPosition (*get_target)(void);
    GridPosition target;
    GridPosition position;
    float fraction_position;
    int direction;
    Color color;
    Texture texture;
    int wait_amount;
} Ghost;

typedef struct {
    float global_sine;
    float global_sine_timer;
    float global_cosine;

    int level_idx;

    float level_scatter_min;
    float level_scatter_max;

    float level_chase_min;
    float level_chase_max;

    Player player;

    Ghost ghosts[GHOST_COUNT];
    Ghost *death_by_ghost;
    int ghost_phase;

    float red_ghost_speed_multiplier;

    float ghost_scatter_timer;
    float ghost_scatter_target_time;

    float ghost_chase_timer;
    float ghost_chase_target_time;

    float ghost_frightened_timer;
    float ghost_frightened_target_time;

    Texture ghost_frightened_texture;
    Texture ghost_returning_texture;

    int grid[GRID_WIDTH][GRID_HEIGHT];

    int dot_count;

    Texture texture;

    float render_x_offset;

    float level_intro;
} State;

State *state;

static inline float get_cell_size() {
    float w = GetScreenWidth();
    float h = GetScreenHeight();
    if ((w * GRID_HEIGHT) < (h * GRID_WIDTH)) {
        return (float)GetScreenWidth() / GRID_WIDTH;
    }
    return (float)GetScreenHeight() / GRID_HEIGHT;
}

static inline float get_half_cell_size() {
    return get_cell_size() / 2;
}

static inline float get_eighth_cell_size() {
    return get_cell_size() / 8;
}

#if DEBUG
#include <stdio.h>
#define ASSERT(condition) do { are_you_a_horrible_person(condition, #condition, __FILE__, __LINE__); } while (0)
#define GET_FRAME_TIME() (GetFrameTime() * (slowmotion ? 0.2f : 1.0f))

bool slowmotion = false;
void toggle_slowmotion() {
    slowmotion = !slowmotion;
}

bool show_lines = false;
void toggle_lines() {
    show_lines = !show_lines;
}

__attribute__((unused))
void are_you_a_horrible_person(bool condition, char *condition_string, char *file_name, int line_number) {
    if (!(condition)) {
        printf("You are a horrible person\n");
        printf(" -> ");
        printf("%s", file_name);
        printf(":");
        printf("%i\n", line_number);
        printf(" -> (");
        printf("%s", condition_string);
        printf(")\n");
        exit(1);
    }
}

void debug_cell(GridPosition position, Color color) {
    if (!show_lines) {
        return;
    }

    DrawCircleLines(
        state->render_x_offset + (position.x * get_cell_size()) + get_half_cell_size(),
        (position.y * get_cell_size()) + get_half_cell_size(),
        get_half_cell_size(),
        color
    );
}

void debug_line(GridPosition a, GridPosition b, Color color) {
    if (!show_lines) {
        return;
    }

    DrawLine(
        state->render_x_offset + (a.x * get_cell_size()) + get_half_cell_size(),
        (a.y * get_cell_size()) + get_half_cell_size(),
        state->render_x_offset + (b.x * get_cell_size()) + get_half_cell_size(),
        (b.y * get_cell_size()) + get_half_cell_size(),
        color
    );
}

#else
#define ASSERT(condition) ((void)(condition))
#define GET_FRAME_TIME() GetFrameTime()
#endif

static inline bool grid_position_eq(GridPosition a, GridPosition b) {
    return a.x == b.x && a.y == b.y;
}

static inline Vector2 to_screen(GridPosition position) {
    return (Vector2) {
        state->render_x_offset + (position.x * get_cell_size()),
        position.y * get_cell_size(),
    };
}

static inline int get_grid_distance_squared(GridPosition a, GridPosition b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    return dx * dx + dy * dy;
}

static inline float get_screen_distance(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf((dx * dx) + (dy * dy));
}

static inline bool is_out_of_bounds(GridPosition position) {
    return
        position.x >= GRID_WIDTH ||
        position.y >= GRID_HEIGHT ||
        position.x < 0 ||
        position.y < 0;
}

static inline bool has_flag(GridPosition position, int flag) {
    if (is_out_of_bounds(position)) {
        return flag == FLAG_OUT_OF_BOUNDS;
    }
    return (state->grid[position.x][position.y] & flag) == flag;
}

static inline void add_flag(GridPosition position, int flag) {
    ASSERT(!is_out_of_bounds(position));
    state->grid[position.x][position.y] |= flag;
}

static inline void remove_flag(GridPosition position, int flag) {
    ASSERT(!is_out_of_bounds(position));
    state->grid[position.x][position.y] &= ~flag;
}

static inline float get_ghost_speed(Ghost *ghost) {
    switch (ghost->state) {
        default: ASSERT(false);
        case GHOST_STATE_INSIDE:
            return SPEED_GHOST_INSIDE;
        case GHOST_STATE_LEAVING:
            return SPEED_GHOST_LEAVING;
        case GHOST_STATE_OUTSIDE:
            if (ghost == &state->ghosts[0]) {
                return SPEED_GHOST_OUTSIDE * state->red_ghost_speed_multiplier;
            }
            return SPEED_GHOST_OUTSIDE;
        case GHOST_STATE_FRIGHTENED:
            return SPEED_GHOST_FRIGHTENED;
        case GHOST_STATE_RETURNING:
            return SPEED_GHOST_RETURNING;
    }
}

Vector2 get_player_screen_position() {
    Vector2 result = to_screen(state->player.position);

    result.x += (state->player.fraction_position.x * get_cell_size());
    result.y += (state->player.fraction_position.y * get_cell_size());

    result.x += get_half_cell_size();
    result.y += get_half_cell_size();

    return result;
}

Vector2 get_ghost_screen_position(Ghost *ghost) {
    Vector2 result = to_screen(ghost->position);

    switch (ghost->direction) {
        default: ASSERT(false);
        case DIRECTION_RIGHT:
            result.x += (ghost->fraction_position * get_cell_size());
            break;
        case DIRECTION_UP:
            result.y -= (ghost->fraction_position * get_cell_size());
            break;
        case DIRECTION_LEFT:
            result.x -= (ghost->fraction_position * get_cell_size());
            break;
        case DIRECTION_DOWN:
            result.y += (ghost->fraction_position* get_cell_size());
            break;
    }

    result.x += get_half_cell_size();
    result.y += get_half_cell_size();

    return result;
}

GridPosition get_position_in_direction(GridPosition from, int direction, int multiplier) {
    switch (direction) {
        default:                return (GridPosition) {from.x,                  from.y};
        case DIRECTION_RIGHT:   return (GridPosition) {from.x + multiplier,     from.y};
        case DIRECTION_UP:      return (GridPosition) {from.x,                  from.y - multiplier};
        case DIRECTION_LEFT:    return (GridPosition) {from.x - multiplier,     from.y};
        case DIRECTION_DOWN:    return (GridPosition) {from.x,                  from.y + multiplier};
    }
}

GridPosition wrap_teleport(GridPosition position) {
    if (position.x < 0) {
        position.x = GRID_WIDTH;
    } else if (position.x >= GRID_WIDTH) {
        position.x = (-1);
    }

    if (position.y < 0) {
        position.y = GRID_HEIGHT;
    } else if (position.y >= GRID_HEIGHT) {
        position.y = (-1);
    }

    return position;
}

GridPosition get_blinky_target(void) {
    if (state->ghost_phase == PHASE_SCATTER) {
        return GRID_TOP_RIGHT;
    }

    return state->player.position;
}

GridPosition get_pinky_target(void) {
    if (state->ghost_phase == PHASE_SCATTER) {
        return GRID_TOP_LEFT;
    }

    Ghost *pinky = &state->ghosts[GHOST_PINKY];
    float distance_to_player = get_screen_distance(
        get_ghost_screen_position(pinky),
        get_player_screen_position()
    );
    int cells_from_player = distance_to_player / get_cell_size();

    return get_position_in_direction(state->player.position, state->player.direction, cells_from_player);
}

GridPosition get_inky_target(void) {
    if (state->ghost_phase == PHASE_SCATTER) {
        return GRID_BOTTOM_RIGHT;
    }

    Player *player = &state->player;
    Ghost *blinky = &state->ghosts[GHOST_BLINKY];

    GridPosition player_look = get_position_in_direction(player->position, player->requested_direction, 2);

    GridPosition diff = {
        .x = player_look.x - blinky->position.x,
        .y = player_look.y - blinky->position.y,
    };

    return (GridPosition) {
        .x = blinky->position.x + diff.x * 2,
        .y = blinky->position.y + diff.y * 2
    };
}

GridPosition get_clyde_target(void) {
    if (state->ghost_phase == PHASE_SCATTER) {
        return GRID_BOTTOM_LEFT;
    }

    Player *player = &state->player;
    Ghost *clyde = &state->ghosts[GHOST_CLYDE];

    int dx = clyde->position.x - player->position.x;
    int dy = clyde->position.y - player->position.y;

    float distance = sqrtf(dx * dx + dy * dy);

    if (distance >= 8.0f) {
        return player->position;
    }

    return GRID_BOTTOM_LEFT;
}

float get_level_var_increasing(float start, float target) {
    if (state->level_idx >= LEVEL_MAX_CHANGE) {
        return target;
    }
    float diff = target - start;
    float multiplier = (float)state->level_idx / LEVEL_MAX_CHANGE;
    return start + (diff * multiplier);
}

float get_level_var_decreasing(float start, float target) {
    if (state->level_idx >= LEVEL_MAX_CHANGE) {
        return target;
    }
    float diff = start - target;
    float multiplier = (float)state->level_idx / (float)LEVEL_MAX_CHANGE;
    return start - (diff * multiplier);
}

void level_setup() {
    state->death_by_ghost = NULL;
    state->level_idx++;
    state->level_intro = 0;

    state->level_scatter_min = get_level_var_decreasing(5, 2);
    state->level_scatter_max = get_level_var_decreasing(10, 4);
    state->level_chase_min = get_level_var_increasing(10,50);
    state->level_chase_max = get_level_var_increasing(20,100);
    state->red_ghost_speed_multiplier = get_level_var_increasing(1.2f, 1.5f);

    state->ghost_scatter_timer = 0.0f;
    state->ghost_scatter_target_time = GetRandomValue(state->level_scatter_min, state->level_scatter_max);

    state->dot_count = 0;

    char byte_grid[GRID_WIDTH][GRID_HEIGHT] = {
        "########## ###########",
        "#.*....### ###..*#...#",
        "#.##.#.### ###.#...#.#",
        "#.##.#.### ###.###.#.#",
        "#..................#.#",
        "#.##.##### ###.#.###.#",
        "#.##...#      .#...#.#",
        "#.##.#.# ### #.#.#.#.#",
        "#....#.  # # #...#...#",
        "####.### # # ### ###.#",
        "#....#.  # # #...#...#",
        "#.##.#.# ### #.#.#.#.#",
        "#.##...#      .#...#.#",
        "#.##.#####.###.#.###.#",
        "#..................#.#",
        "#.##.#.### ###.###.#.#",
        "#.##.#.### ###.#...#.#",
        "#.*....### ###..*#...#",
        "########## ###########",
    };
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            switch (byte_grid[x][y]) {
                case '#':
                    state->grid[x][y] = FLAG_WALL;
                    break;
                case '.':
                    state->dot_count++;
                    state->grid[x][y] = FLAG_DOT;
                    break;
                case '*':
                    state->dot_count++;
                    state->grid[x][y] = FLAG_BIG_DOT;
                    break;
                default:
                    state->grid[x][y] = FLAG_NONE;
                    break;
            }
        }
    }

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            GridPosition g = { x, y };
            if (has_flag(get_position_in_direction(g, DIRECTION_RIGHT, 1), FLAG_WALL)) { add_flag(g, FLAG_WALL_TO_RIGHT); }
            if (has_flag(get_position_in_direction(g, DIRECTION_UP, 1), FLAG_WALL)) { add_flag(g, FLAG_WALL_ABOVE); }
            if (has_flag(get_position_in_direction(g, DIRECTION_LEFT, 1), FLAG_WALL)) { add_flag(g, FLAG_WALL_TO_LEFT); }
            if (has_flag(get_position_in_direction(g, DIRECTION_DOWN, 1), FLAG_WALL)) { add_flag(g, FLAG_WALL_BELOW); }
        }
    }

    state->ghost_phase = PHASE_SCATTER;

    state->ghost_frightened_target_time = (state->level_idx < 10) ? (10 - state->level_idx) : 0;

    state->player = (Player) {
        .position = CELL_PLAYER_START,
        .direction = DIRECTION_NONE,
        .requested_direction = DIRECTION_NONE,
        .fraction_position = (Vector2) {0},
    };

    state->ghosts[GHOST_BLINKY].state = GHOST_STATE_OUTSIDE;
    state->ghosts[GHOST_BLINKY].shape = GHOST_SHAPE_TRAPEZOID;
    state->ghosts[GHOST_BLINKY].position = CELL_OUTSIDE_GHOST_HOUSE_DOOR;
    state->ghosts[GHOST_BLINKY].direction = DIRECTION_LEFT;
    state->ghosts[GHOST_BLINKY].color = COLOR_BLINKY;
    state->ghosts[GHOST_BLINKY].get_target = get_blinky_target;

    state->ghosts[GHOST_PINKY].state = GHOST_STATE_INSIDE;
    state->ghosts[GHOST_PINKY].shape = GHOST_SHAPE_TRAPEZOID;
    state->ghosts[GHOST_PINKY].position = CELL_GHOST_HOUSE_LEFT_SIDE;
    state->ghosts[GHOST_PINKY].direction = DIRECTION_RIGHT;
    state->ghosts[GHOST_PINKY].color = COLOR_PINKY;
    state->ghosts[GHOST_PINKY].get_target = get_pinky_target;

    state->ghosts[GHOST_INKY].state = GHOST_STATE_INSIDE;
    state->ghosts[GHOST_INKY].shape = GHOST_SHAPE_TRAPEZOID;
    state->ghosts[GHOST_INKY].position = CELL_GHOST_HOUSE_CENTER;
    state->ghosts[GHOST_INKY].direction = DIRECTION_RIGHT;
    state->ghosts[GHOST_INKY].color = COLOR_INKY;
    state->ghosts[GHOST_INKY].get_target = get_inky_target;

    state->ghosts[GHOST_CLYDE].state = GHOST_STATE_INSIDE;
    state->ghosts[GHOST_CLYDE].shape = GHOST_SHAPE_TRAPEZOID;
    state->ghosts[GHOST_CLYDE].position = CELL_GHOST_HOUSE_RIGHT_SIDE;
    state->ghosts[GHOST_CLYDE].direction = DIRECTION_LEFT;
    state->ghosts[GHOST_CLYDE].color = COLOR_CLYDE;
    state->ghosts[GHOST_CLYDE].get_target = get_clyde_target;

    ASSERT(state->level_idx > 0);
    switch (state->level_idx) {
        case 1:
            state->ghosts[GHOST_BLINKY].wait_amount = 0;
            state->ghosts[GHOST_PINKY].wait_amount = 2;
            state->ghosts[GHOST_INKY].wait_amount = 22;
            state->ghosts[GHOST_CLYDE].wait_amount = 42;
            break;
        case 2:
            state->ghosts[GHOST_BLINKY].wait_amount = 0;
            state->ghosts[GHOST_PINKY].wait_amount = 2;
            state->ghosts[GHOST_INKY].wait_amount = 4;
            state->ghosts[GHOST_CLYDE].wait_amount = 24;
            break;
        case 3:
            state->ghosts[GHOST_BLINKY].wait_amount = 0;
            state->ghosts[GHOST_PINKY].wait_amount = 2;
            state->ghosts[GHOST_INKY].wait_amount = 4;
            state->ghosts[GHOST_CLYDE].wait_amount = 6;
            break;
    }
}

void init(void) {
    level_setup();

    state->ghost_frightened_texture = LoadTexture("frightened.png");
    state->ghost_returning_texture = LoadTexture("returning.png");

    state->ghosts[GHOST_BLINKY].texture = LoadTexture("blinky.png");
    state->ghosts[GHOST_PINKY].texture = LoadTexture("pinky.png");
    state->ghosts[GHOST_INKY].texture = LoadTexture("inky.png");
    state->ghosts[GHOST_CLYDE].texture = LoadTexture("clyde.png");
}

int get_opposite_direction(int direction) {
    switch (direction) {
        default: return DIRECTION_NONE;
        case DIRECTION_RIGHT: return DIRECTION_LEFT;
        case DIRECTION_UP: return DIRECTION_DOWN;
        case DIRECTION_LEFT: return DIRECTION_RIGHT;
        case DIRECTION_DOWN: return DIRECTION_UP;
    }
}

void player_on_position_new() {
    Player *player = &state->player;

    player->fraction_position = (Vector2){0};
    player->position = wrap_teleport(player->position);

    if (has_flag(player->position, FLAG_DOT)) {
        remove_flag(player->position, FLAG_DOT);
        state->dot_count--;
    } else if (has_flag(player->position, FLAG_BIG_DOT)) {
        remove_flag(player->position, FLAG_BIG_DOT);
        state->dot_count--;

        state->ghost_phase = PHASE_FRIGHTENED;
        state->ghost_frightened_timer = 0.0f;

        for (int i = 0; i < GHOST_COUNT; i++) {
            Ghost *ghost = &state->ghosts[i];
            switch (ghost->state) {
                default:
                    break;
                case GHOST_STATE_OUTSIDE:
                    ghost->state = GHOST_STATE_FRIGHTENED;
                    break;
            }
        }
    }

    if (state->dot_count == 0) {
        level_setup();
    }
}

void scan_surroundings(GridPosition from, int current_direction, Surroundings *surroundings) {
    ASSERT(surroundings->count == 0);

    for (int direction = DIRECTION_RIGHT; direction <= DIRECTION_DOWN; direction++) {
        int opposite_direction = get_opposite_direction(current_direction);
        if (direction == opposite_direction) {
            continue;
        }

        GridPosition position = get_position_in_direction(from, direction, 1);

        if (has_flag(position, FLAG_WALL)) {
            continue;
        }

        surroundings->positions[surroundings->count] = position;
        surroundings->directions[surroundings->count] = direction;
        surroundings->count++;
    }

    ASSERT(surroundings->count != 0);
}

int get_best_direction_towards_target(Surroundings *surroundings, GridPosition target) {
    int closest_distance = 999999;
    int best_direction = DIRECTION_NONE;

    for (int i = 0; i < surroundings->count; i++) {
        float distance = get_grid_distance_squared(surroundings->positions[i], target);

        if (distance <= closest_distance) {
            closest_distance = distance;
            best_direction = surroundings->directions[i];
        }
    }

    return best_direction;
}

void update(void) {
    if (state->level_intro < LEVEL_INTRO_LENGTH) {
        return;
    }

    if (state->death_by_ghost) {
        return;
    }

    float delta_time = GET_FRAME_TIME();

    {
        state->global_sine_timer += delta_time;

        if (state->global_sine_timer > 1.0f) {
            state->global_sine_timer = 0.0f;
        }

        float x = state->global_sine_timer * PI * 2;
        state->global_sine = sinf(x);
        state->global_cosine = sinf(x);
    }

    switch (state->ghost_phase) {
        default: break;
        case PHASE_SCATTER:
            state->ghost_scatter_timer += delta_time;
            if (state->ghost_scatter_timer > state->ghost_scatter_target_time) {
                state->ghost_scatter_timer = 0.0f;
                state->ghost_phase = PHASE_CHASE;
                state->ghost_chase_target_time = GetRandomValue(
                    state->level_chase_min,
                    state->level_chase_max
                );
            }
            break;
        case PHASE_CHASE:
            state->ghost_chase_timer += delta_time;
            if (state->ghost_chase_timer > state->ghost_chase_target_time) {
                state->ghost_chase_timer = 0.0f;
                state->ghost_phase = PHASE_SCATTER;
                state->ghost_scatter_target_time = GetRandomValue(
                    state->level_scatter_min,
                    state->level_scatter_max
                );
            }
            break;
        case PHASE_FRIGHTENED:
            state->ghost_frightened_timer += delta_time;
            if (state->ghost_frightened_timer > state->ghost_frightened_target_time) {
                state->ghost_frightened_timer = 0.0f;
                for (int i = 0; i < GHOST_COUNT; i++) {
                    if (state->ghosts[i].state == GHOST_STATE_FRIGHTENED) {
                        state->ghosts[i].state = GHOST_STATE_OUTSIDE;
                    }
                }
                if (state->ghost_scatter_timer < state->ghost_scatter_target_time) {
                    state->ghost_phase = PHASE_SCATTER;
                }
                if (state->ghost_chase_timer < state->ghost_chase_target_time) {
                    state->ghost_phase = PHASE_CHASE;
                }
            }
            break;
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (IsKeyPressed(KEY_RIGHT)) {
            int monitor = GetCurrentMonitor();
            int monitor_width = GetMonitorWidth(monitor);
            int screen_width = GetScreenWidth();
            SetWindowPosition(monitor_width - screen_width, 0);
        }
        if (IsKeyPressed(KEY_LEFT)) {
            SetWindowPosition(0, 0);
        }
    }

    if (IsKeyPressed(KEY_RIGHT)) {
        state->player.requested_direction = DIRECTION_RIGHT;
    } else if (IsKeyPressed(KEY_UP)) {
        state->player.requested_direction = DIRECTION_UP;
    } else if (IsKeyPressed(KEY_LEFT)) {
        state->player.requested_direction = DIRECTION_LEFT;
    } else if (IsKeyPressed(KEY_DOWN)) {
        state->player.requested_direction = DIRECTION_DOWN;
    }

    {
        // player movement

        Player *player = &state->player;

        if (!is_out_of_bounds(state->player.position)) {
            if (player->direction != player->requested_direction) {
                int opposite_direction = get_opposite_direction(player->direction);
                if (player->requested_direction == opposite_direction) {
                    GridPosition requested_position = get_position_in_direction(state->player.position, opposite_direction, 1);
                    if (!has_flag(requested_position, FLAG_WALL)) {
                        player->direction = player->requested_direction;
                    }
                } else if ((player->position.x != 0) && (player->position.x != GRID_WIDTH - 1)) {
                    GridPosition intermediate_position = get_position_in_direction(player->position, player->direction, 1);
                    if (!has_flag(intermediate_position, FLAG_WALL)) {
                        GridPosition requested_position = get_position_in_direction(intermediate_position, player->requested_direction, 1);
                        if (!has_flag(requested_position, FLAG_WALL)) {
                            player->position = intermediate_position;
                            player->direction = player->requested_direction;
                            player_on_position_new();
                        }
                    } else {
                        GridPosition requested_position = get_position_in_direction(player->position, player->requested_direction, 1);
                        if (!has_flag(requested_position, FLAG_WALL)) {
                            player->direction = player->requested_direction;
                            player_on_position_new();
                        }
                    }
                }
                GridPosition requested_position = get_position_in_direction(state->player.position, state->player.requested_direction, 1);
                if (!has_flag(requested_position, FLAG_WALL)) {
                    state->player.direction = state->player.requested_direction;
                }
            }
        }

        GridPosition next_position = get_position_in_direction(player->position, player->direction, 1);

        if (!has_flag(next_position, FLAG_WALL)) {
            switch (player->direction) {
                case DIRECTION_RIGHT:
                    player->fraction_position.x += SPEED_PLAYER * delta_time;
                    player->fraction_position.y = 0.0f;
                    if (player->fraction_position.x > 1.0f) {
                        player->position.x++;
                        player_on_position_new();
                    }
                    break;
                case DIRECTION_UP:
                    player->fraction_position.x = 0.0f;
                    player->fraction_position.y -= SPEED_PLAYER * delta_time;
                    if (player->fraction_position.y < (-1.0f)) {
                        player->position.y--;
                        player_on_position_new();
                    }
                    break;
                case DIRECTION_LEFT:
                    player->fraction_position.x -= SPEED_PLAYER * delta_time;
                    player->fraction_position.y = 0.0f;
                    if (player->fraction_position.x < (-1.0f)) {
                        player->position.x--;
                        player_on_position_new();
                    }
                    break;
                case DIRECTION_DOWN:
                    player->fraction_position.x = 0.0f;
                    player->fraction_position.y += SPEED_PLAYER * delta_time;
                    if (player->fraction_position.y > 1.0f) {
                        player->position.y++;
                        player_on_position_new();
                    }
                    break;
            }
        } else {
            player->fraction_position = (Vector2){0};
        }
    }

    for (int i = 0; i < GHOST_COUNT; i++) {
        Ghost *ghost = &state->ghosts[i];

        float distance = get_screen_distance(
            get_player_screen_position(),
            get_ghost_screen_position(ghost)
        );

        if (distance < get_half_cell_size()) {
            switch (ghost->state) {
                case GHOST_STATE_RETURNING:
                    break;
                case GHOST_STATE_FRIGHTENED:
                    ghost->state = GHOST_STATE_RETURNING;
                    break;
                default:
                    state->death_by_ghost = ghost;
                    break;
            }
        }

        ghost->fraction_position += delta_time * get_ghost_speed(ghost);
        if (ghost->fraction_position < 1.0f) {
            continue;
        }
        ghost->fraction_position = 0.0f;

        ghost->position = get_position_in_direction(ghost->position, ghost->direction, 1);
        ghost->position = wrap_teleport(ghost->position);

        if (is_out_of_bounds(ghost->position)) {
            // do not allow changes to direction
            break;
        }

        switch (ghost->state) {
            case GHOST_STATE_INSIDE: {
                ASSERT(ghost->position.y == 10);
                switch (ghost->position.x) {
                    default: ASSERT(false);
                    case 8:
                        ghost->direction = DIRECTION_RIGHT;
                        break;
                    case 9:
                        ASSERT(ghost->wait_amount >= 0);
                        if (ghost->wait_amount > 0) {
                            ghost->wait_amount--;
                        } else {
                            ghost->state = GHOST_STATE_LEAVING;
                            ghost->direction = DIRECTION_UP;
                        }
                        break;
                    case 10:
                        ghost->direction = DIRECTION_LEFT;
                        break;
                }
            } break;
            case GHOST_STATE_LEAVING: {
                if (ghost->position.y == 10) {
                    switch (ghost->position.x) {
                        default: ASSERT(false);
                        case 8: ghost->direction = DIRECTION_RIGHT; break;
                        case 9:
                            ghost->direction = DIRECTION_UP;
                            break;
                        case 10: ghost->direction = DIRECTION_LEFT; break;
                    }
                } else if (ghost->position.y == 9) {
                    // keep direction
                    ghost->state = GHOST_STATE_OUTSIDE;
                }
            } break;
            case GHOST_STATE_OUTSIDE: {
                ghost->target = ghost->get_target();

                Surroundings surroundings = {0};
                scan_surroundings(ghost->position, ghost->direction, &surroundings);

                if ((ghost->position.x == ghost->target.x) &&
                    (ghost->position.y == ghost->target.y) &&
                    (ghost->position.x == state->player.position.x) &&
                    (ghost->position.y == state->player.position.y)
                ) {

                    // so close that the player and the ghost are in the same cell, but not close enough for death
                    // "get_best_direction_towards_target" returns weird results in this case
                    // so at this distance we simply take the player direction
                    // this can be exploited if someone is extremely frame-perfect good

                    bool direction_available = false;
                    for (int i = 0; i < surroundings.count; i++) {
                        if (state->player.direction == surroundings.directions[i]) {
                            direction_available = true;
                        }
                    }

                    if (direction_available) {
                        ghost->direction = state->player.direction;
                    } else {
                        ghost->direction = get_best_direction_towards_target(&surroundings, ghost->target);
                    }
                } else {
                    ghost->direction = get_best_direction_towards_target(&surroundings, ghost->target);
                }
                ASSERT(ghost->direction != DIRECTION_NONE);
            } break;
            case GHOST_STATE_FRIGHTENED: {
                Surroundings surroundings = {0};
                scan_surroundings(ghost->position, ghost->direction, &surroundings);

                int random_direction_idx = GetRandomValue(0, surroundings.count - 1);
                ghost->direction = surroundings.directions[random_direction_idx];
                ASSERT(ghost->direction != DIRECTION_NONE);
            } break;
            case GHOST_STATE_RETURNING: {
                if (grid_position_eq(ghost->position, CELL_OUTSIDE_GHOST_HOUSE_DOOR)) {
                    ghost->direction = DIRECTION_DOWN;
                } else if (grid_position_eq(ghost->position, CELL_GHOST_HOUSE_DOOR)) {
                    // keep direction
                } else if (grid_position_eq(ghost->position, CELL_GHOST_HOUSE_CENTER)) {
                    ghost->direction = DIRECTION_LEFT;
                    ghost->state = GHOST_STATE_LEAVING;
                } else {
                    Surroundings surroundings = {0};
                    scan_surroundings(ghost->position, ghost->direction, &surroundings);
                    ghost->direction = get_best_direction_towards_target(&surroundings, CELL_OUTSIDE_GHOST_HOUSE_DOOR);
                    ASSERT(ghost->direction != DIRECTION_NONE);
                }
            } break;
        }
    }

#if DEBUG
    if (IsKeyPressed(KEY_S)) {
        toggle_slowmotion();
    }
    if (IsKeyPressed(KEY_L)) {
        toggle_lines();
    }
#endif

    float level_width = get_cell_size() * GRID_WIDTH;
    state->render_x_offset = (GetScreenWidth() - level_width) / 2;
}

void render_noise(const char *text) {
    int amount = 100;
    float w = GetScreenWidth() / (float)amount;
    float h = GetScreenHeight() / (float)amount;
    for (int x = 0; x < amount; x++) {
        for (int y = 0; y < amount; y++) {
            int p = GetRandomValue(0, 1) * 127;
            Rectangle rec;
            rec.x = x * w;
            rec.y = y * h;
            rec.width = w;
            rec.height = h;
            DrawRectangleRec(rec, (Color){p,p,p,255});
        }
    }

    if (text == NULL) {
        return;
    }

    float font_size = GetScreenWidth() / 10.0f;
    float spacing = 10;

    Vector2 dimensions = MeasureTextEx(GetFontDefault(), text, font_size, spacing);

    Rectangle rec;
    rec.x = (GetScreenWidth() / 2) - (dimensions.x / 2) - spacing;
    rec.y = (GetScreenHeight() / 2) - (dimensions.y / 2) - spacing;
    rec.width = dimensions.x + (spacing * 2);
    rec.height = dimensions.y + (spacing * 2);

    DrawRectangleRec(rec, (Color){0,0,0,255});

    Vector2 text_position;
    text_position.x = GetScreenWidth() / 2;
    text_position.y = GetScreenHeight() / 2;

    Vector2 text_origin;
    text_origin.x = (dimensions.x / 2);
    text_origin.y = (dimensions.y / 2);

    DrawTextPro(GetFontDefault(), text, text_position, text_origin, 0, font_size, spacing, (Color){255,0,0,255});
}

void render_player(void) {
    Player *player = &state->player;

    static float gap = 0.0f;

    float start_angle = 0.0f;
    float end_angle = 0.0f;

    gap += GET_FRAME_TIME() * GAP_ANIMATION_SPEED;
    if (gap > PI * 2) {
        gap = 0.0f;
    }

    float base_angle = 0.0f;
    switch (player->direction) {
        case DIRECTION_RIGHT:
            base_angle = 0.0f;
            break;
        case DIRECTION_UP:
            base_angle = 270.0f;
            break;
        case DIRECTION_LEFT:
            base_angle = 180.0f;
            break;
        case DIRECTION_DOWN:
            base_angle = 90.0f;
            break;
    }

    start_angle = (base_angle + 360.0f)
        - ((cosf(gap) + 1.0f) * HALF_GAP_SIZE_MULTIPLIER);

    end_angle = base_angle
        + ((cosf(gap) + 1.0f) * HALF_GAP_SIZE_MULTIPLIER);

    DrawCircleSector(get_player_screen_position(), (get_half_cell_size() * 0.9f), start_angle, end_angle, 16, COLOR_PLAYER);
}

void render_ghost(Ghost *ghost) {
    // Color color = ghost->color;
    // switch (ghost->state) {
    //     default:
    //         break;
    //     case GHOST_STATE_FRIGHTENED:
    //     case GHOST_STATE_RETURNING:
    //         color.r /= 2;
    //         color.g /= 2;
    //         color.b /= 2;
    //         break;
    // }
    //
    // float eye_size = get_cell_size() / 6;
    // Color eye_color = (Color){255,255,255,255};
    //
    // float pupille_size = get_cell_size() / 12;
    // float pupille_x = 0;
    // float pupille_y = 0;
    // Color pupille_color = (Color){0,0,0,255};
    //
    // float pupille_offset = eye_size / 2;
    //
    // if (ghost->state == GHOST_STATE_FRIGHTENED) {
    //     float t = ghost->fraction_position * PI * 2;
    //     pupille_x = pupille_offset * sinf(t);
    //     pupille_y = pupille_offset * cosf(t);
    // } else {
    //     switch (ghost->direction) {
    //         case DIRECTION_RIGHT: pupille_x = pupille_offset; break;
    //         case DIRECTION_UP: pupille_y = -pupille_offset; break;
    //         case DIRECTION_LEFT: pupille_x = -pupille_offset; break;
    //         case DIRECTION_DOWN: pupille_y = pupille_offset; break;
    //     }
    // }
    //
    // switch (ghost->shape) {
    //     case GHOST_SHAPE_TRAPEZOID: {
    //         if (ghost->state != GHOST_STATE_RETURNING) {
    //             const float o = get_eighth_cell_size();
    //             Vector2 tl = { center.x - (o*3), center.y - (o*3) };
    //             Vector2 bl = { center.x - (o*4), center.y + (o*3) };
    //             Vector2 br = { center.x + (o*4), bl.y };
    //             Vector2 tr = { center.x + (o*3), tl.y };
    //
    //             DrawTriangle(tl, bl, br, color);
    //             DrawTriangle(tl, br, tr, color);
    //         }
    //
    //         float eye_left = center.x - eye_size;
    //         float eye_right = center.x + eye_size;
    //
    //         DrawCircle(eye_left, center.y, eye_size, eye_color);
    //         DrawCircle(eye_right, center.y, eye_size, eye_color);
    //
    //         DrawCircle(eye_left + pupille_x, center.y + pupille_y, pupille_size, pupille_color);
    //         DrawCircle(eye_right + pupille_x, center.y + pupille_y, pupille_size, pupille_color);
    //     } break;
    //     case GHOST_SHAPE_TRIANGLE: {
    //         if (ghost->state != GHOST_STATE_RETURNING) {
    //             float d = (2.0f * PI) / 3.0f;
    //             Vector2 v[3];
    //             for (int i = 0; i < 3; i++) {
    //                 float value = (d * i) + (state->global_sine_timer * d * 4);
    //                 v[i] = (Vector2) {
    //                     .x = center.x + (sinf(value) * get_half_cell_size()),
    //                     .y = center.y + (cosf(value) * get_half_cell_size()),
    //                 };
    //             }
    //             DrawTriangle(v[0], v[1], v[2], color);
    //         }
    //         DrawCircle(center.x, center.y, eye_size, eye_color);
    //         DrawCircle(center.x + pupille_x, center.y + pupille_y, pupille_size, pupille_color);
    //     } break;
    // }

    Vector2 center = get_ghost_screen_position(ghost);

    float scale = (get_cell_size() / (float)PNG_DIMENSIONS);

    if (ghost == &state->ghosts[0]) {
        scale *= 1.5f;
    }

    Rectangle src;
    src.x = 0;
    src.y = 0;
    src.width = PNG_DIMENSIONS;
    src.height = PNG_DIMENSIONS;

    if (ghost->direction == DIRECTION_LEFT) {
        src.x = PNG_DIMENSIONS;
        src.width = -PNG_DIMENSIONS;
    }

    Rectangle dst;
    dst.width = PNG_DIMENSIONS * scale;
    dst.height = PNG_DIMENSIONS * scale;
    dst.x = center.x;
    dst.y = center.y;

    Vector2 origin;
    origin.x = dst.width / 2;
    origin.y = dst.height / 2;

    float rotation;
    Color color = { 255, 255, 255, 255 };
    Texture texture;

    switch (ghost->direction) {
        case DIRECTION_RIGHT:
        case DIRECTION_LEFT: rotation = 0; break;
        case DIRECTION_UP: rotation = 270; break;
        case DIRECTION_DOWN: rotation = 90; break;
        default: ASSERT(false);
    }

    switch (ghost->state) {
        default:
            texture = ghost->texture;
            break;
        case GHOST_STATE_FRIGHTENED: {
            float flicker_speed = 0.0f;
            if ((state->ghost_frightened_timer + 1) > state->ghost_frightened_target_time) {
                flicker_speed = 0.05f;
            } else if ((state->ghost_frightened_timer + 3) > state->ghost_frightened_target_time) {
                flicker_speed = 0.1f;
            }

            if (flicker_speed) {
                float x = state->ghost_frightened_target_time - state->ghost_frightened_timer;
                if ((int)floorf(x / flicker_speed) % 2 == 0) {
                    texture = state->ghost_frightened_texture;
                } else {
                    texture = ghost->texture;
                }
            } else {
                texture = state->ghost_frightened_texture;
            }
        } break;
        case GHOST_STATE_RETURNING:
            texture = state->ghost_returning_texture;
            break;
    }

    DrawTexturePro(texture, src, dst, origin, rotation, color);
}

Color blend_influences(Vector2 position, Color base_color) {
    float r = base_color.r * 0.5f; // base color is dim
    float g = base_color.g * 0.5f;
    float b = base_color.b * 0.5f;

    for (int i = 0; i < GHOST_COUNT; i++) {
        Ghost *ghost = &state->ghosts[i];

        switch (ghost->state) {
            default:
                break;
            case GHOST_STATE_FRIGHTENED:
            case GHOST_STATE_RETURNING:
                continue;
        }

        Vector2 ghost_screen_position = get_ghost_screen_position(ghost);
        float dx = ghost_screen_position.x - position.x;
        float dy = ghost_screen_position.y - position.y;
        float dist = sqrtf(dx*dx + dy*dy);

        float sigma = 70.0f; // controls spread
        float weight = expf(-(dist*dist) / (2 * sigma*sigma));

        // additive light effect
        r += ghost->color.r * weight;
        g += ghost->color.g * weight;
        b += ghost->color.b * weight;
    }

    // clamp to [0, 255]
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

Color hsv(float t) {
    t = fmodf(t, 1.0f);
    if (t < 0) t += 1.0f;

    float h = t * 360.0f;   // hue (0–360)
    float s = 1.0f;         // full saturation
    float v = 1.0f;         // full brightness

    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2) - 1));
    float m = v - c;

    float r = 0, g = 0, b = 0;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    return (Color){
        (unsigned char)((r + m) * 255),
        (unsigned char)((g + m) * 255),
        (unsigned char)((b + m) * 255),
        255
    };
}

void render(void) {
    if (state->level_intro < LEVEL_INTRO_LENGTH) {
        const char *text = TextFormat("LEVEL %i", state->level_idx);
        render_noise(text);
        state->level_intro += GetFrameTime();
        return;
    }

    float thickness_multiplier = get_eighth_cell_size();
    float thickness = thickness_multiplier + (thickness_multiplier + (state->global_sine * thickness_multiplier * 0.9f));

    ClearBackground(COLOR_FLOOR);
    for (int x = 0; x < GRID_WIDTH; x++) {
        float sin_offset = sinf((state->global_sine_timer * PI * 2) + x) * get_eighth_cell_size();
        Color column_color = hsv((float)x/GRID_WIDTH);
        for (int y = 0; y < GRID_HEIGHT; y++) {
            float cos_offset = cosf((state->global_sine_timer * PI * 2) + y) * get_eighth_cell_size();
            GridPosition cell = {x,y};
            bool is_wall = has_flag(cell, FLAG_WALL);
            Color wall_color = blend_influences(to_screen((GridPosition){x,y}), COLOR_WALL);
            if (x == 9 && y == 9) {
                // colored like floor but it is really a wall
            } else if (is_wall) {
                Vector2 s = to_screen(cell);

                if (!has_flag(cell, FLAG_WALL_TO_RIGHT)) {
                    Rectangle rec = {
                        s.x + get_cell_size() - thickness,
                        s.y + thickness,
                        thickness,
                        get_cell_size() - (thickness * 2),
                    };
                    DrawRectangleRec(rec, wall_color);
                }
                if (!has_flag(cell, FLAG_WALL_ABOVE)) {
                    Rectangle rec = {
                        s.x + thickness,
                        s.y,
                        get_cell_size() - (thickness * 2),
                        thickness,
                    };
                    DrawRectangleRec(rec, wall_color);
                }
                if (!has_flag(cell, FLAG_WALL_TO_LEFT)) {
                    Rectangle rec = {
                        s.x,
                        s.y + thickness,
                        thickness,
                        get_cell_size() - (thickness * 2),
                    };
                    DrawRectangleRec(rec, wall_color);
                }
                if (!has_flag(cell, FLAG_WALL_BELOW)) {
                    Rectangle rec = {
                        s.x + thickness,
                        s.y + get_cell_size() - thickness,
                        get_cell_size() - (thickness * 2),
                        thickness,
                    };
                    DrawRectangleRec(rec, wall_color);
                }
            }
            if (!is_wall) {
                if (has_flag(cell, FLAG_DOT)) {
                    const float dot_radius = get_cell_size() / 10;
                    DrawCircle(
                        state->render_x_offset + (x * get_cell_size()) + get_half_cell_size() + sin_offset,
                        (y * get_cell_size()) + get_half_cell_size() + cos_offset,
                        dot_radius,
                        column_color
                    );
                } else if (has_flag(cell, FLAG_BIG_DOT)) {
                    const float dot_radius = get_cell_size() / 4;
                    DrawCircle(
                        state->render_x_offset + (x * get_cell_size()) + get_half_cell_size() + sin_offset,
                        (y * get_cell_size()) + get_half_cell_size() + cos_offset,
                        dot_radius,
                        column_color
                    );
                }
            }
        }
    }

    {
        Vector2 line_start = {
            state->render_x_offset + 9 * get_cell_size(),
            (9 * get_cell_size()) + get_half_cell_size()
        };

        Vector2 line_end = {
            line_start.x + get_cell_size(),
            line_start.y
        };

        const float line_thickness = get_cell_size() * 0.25;
        DrawLineEx(line_start, line_end, line_thickness, COLOR_GHOST_HOUSE_DOOR);
    }

    for (int i = 0; i < GHOST_COUNT; i++) {
        render_ghost(&state->ghosts[i]);
    }

    render_player();

    if (state->death_by_ghost) {
        static float death_timer = 0.0f;
        death_timer += GetFrameTime();

        if (death_timer < 1) {
            float the_bigger_side = (GetScreenWidth() < GetScreenHeight()) ? GetScreenHeight() : GetScreenWidth();

            float scale = the_bigger_side * death_timer * 2;

            Rectangle src;
            src.x = 0;
            src.y = 0;
            src.width = PNG_DIMENSIONS;
            src.height = PNG_DIMENSIONS;

            if (state->death_by_ghost->direction == DIRECTION_LEFT) {
                src.x = PNG_DIMENSIONS;
                src.width = -PNG_DIMENSIONS;
            }

            Rectangle dst;
            dst.width = scale;
            dst.height = scale;
            dst.x = GetScreenWidth() / 2;
            dst.y = GetScreenHeight() / 2;

            Vector2 origin;
            origin.x = dst.width / 2;
            origin.y = dst.height / 2;

            float rotation;
            Color color = { 255, 255, 255, 255 };
            switch (state->death_by_ghost->state) {
                default:
                    switch (state->death_by_ghost->direction) {
                        case DIRECTION_RIGHT:
                        case DIRECTION_LEFT: rotation = 0; break;
                        case DIRECTION_UP: rotation = 270; break;
                        case DIRECTION_DOWN: rotation = 90; break;
                        default: ASSERT(false);
                    }
                    break;
                case GHOST_STATE_FRIGHTENED:
                    rotation = state->global_sine_timer * 360.0f;
                    color.a = 128;
                    break;
                case GHOST_STATE_RETURNING:
                    rotation = state->global_sine_timer * 360.0f * 4;
                    color.a = 64;
                    break;
            }

            DrawTexturePro(state->death_by_ghost->texture, src, dst, origin, rotation, color);
        } else {
            death_timer = 0;
            state->level_idx = 0;
            level_setup();
        }
    }

#if DEBUG
    for (int i = 0; i < GHOST_COUNT; i++) {
        Ghost *g = &state->ghosts[i];
        debug_cell(g->target, g->color);
        debug_line(g->position, g->target, g->color);
    }
#endif
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(40 * GRID_WIDTH, 40 * GRID_HEIGHT, "Mats Pac");
    SetWindowMinSize(GRID_WIDTH * 10, GRID_HEIGHT * 10);
    SetTargetFPS(60);
    state = (State *)calloc(sizeof(State), 1);
    init();
    while (!WindowShouldClose()) {
        update();
        BeginDrawing();
        render();
        EndDrawing();
    }
    CloseWindow();
    free(state);
    return 0;
}

