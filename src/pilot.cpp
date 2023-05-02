#include <windows.h>
#include <Xinput.h>
#include <xaudio2.h>
#include <strsafe.h>

#define i8 char
#define i16 short
#define i32 int
#define i64 long long

#define u8 unsigned char
#define u16 unsigned short
#define u32 unsigned int
#define u64 unsigned long long

#define f32 float
#define f64 double

enum {
    WALK_LEFT,
    WALK_RIGHT,
    IDLE_LEFT,
    IDLE_RIGHT,
};

#define shift(x) 1 << (x)

enum {
    EMPTY_TILE = shift(0),
    GROUND_TILE = shift(1),
    START_TILE = shift(2),
    END_TILE = shift(3),
    COIN_TILE = shift(4),
    DEATH_TILE = shift(5),
    SPIKES_TILE = shift(6),
};

enum State {
    MAIN_MENU,
    IN_LEVEL,
    LEVEL_COMPLETE,
    LEVEL_FAILED,
    END,
};

// Include alle cpp bestanden hier.
#include "math.cpp"
#include "audio.cpp"
#include "input.cpp"
#include "draw.cpp"

struct Engine {
    Input input;
    Audio audio;
    Window window;

    bool running;
    f32 delta_time;
    f32 target_time;
};

#include "ui.cpp"

#define minimum(A, B) ((A < B) ? (A) : (B))
#define maximum(A, B) ((A > B) ? (A) : (B))

struct Tile_Map {
    i32 width, height, tile_size;
    i32 *tiles;
    Sprite ground, end, coin, spikes;
    Vector2f start_pos;
};

struct Player {
    Animation walk_right;
    Animation walk_left;
    Animation idle_right;
    Animation idle_left;
    Animation current_anim;

    float frame;

    f32 width, height;

    Vector2f position;
    Vector2f velocity;
    Vector2f acceleration;
    float max_speed;
};

struct Collision {
    i32 coin_index;
    i32 tile;
    bool on_ground;
};

#define NUM_LEVELS 10
struct Game {
    f32 gravity;
    Vector2f camera;

    Player *player;

    Tile_Map tile_maps[NUM_LEVELS];
    u32 level;
    u32 coin_count;

    Sprite background;
    Sprite main_menu;
    Sprite level_complete;
    Sprite level_failed;
    Sprite end_game;

    Sprite tips_pc[3];
    Sprite tips_console[3];

    Button quit_button;
    Button restart_button;
    Button next_button;
    Button play_button;

    Sound jump_sound;
    Sound coin_sound;
    Sound hit_sound;
    Sound completed_sound;
    Sound test_sound;
    Sound select_sound;
    Sound failed_sound;

    bool coin_collected;
    int coin_index;
    bool dead;
    float death_timer;

    State state;
    Collision collision;
};

static Tile_Map load_tile_map(const char *filename) {
    Tile_Map result = {};

    Sprite level_design = load_bitmap(filename);
    result.height = level_design.height;
    result.width = level_design.width;
    result.tile_size = 96;
    result.ground = load_bitmap("assets\\grass.bmp");
    result.end = load_bitmap("assets\\door.bmp");
    result.coin = load_bitmap("assets\\coin.bmp");
    result.spikes = load_bitmap("assets\\spikes.bmp");
    result.tiles = (i32 *)VirtualAlloc(0, sizeof(i32) * result.width * result.height,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    i32 *tile = result.tiles;
    for (i32 y = 0; y < result.height; y++) {
        for (i32 x = 0; x < result.width; x++) {
            i32 value = 0;

            u32 color = level_design.pixels[level_design.width * y + x];
            u8 a = (u8)(color >> 24);
            u8 r = (u8)(color >> 16);
            u8 g = (u8)(color >> 8);
            u8 b = (u8)color;
            if (a == 255) {
                if ((r == 0) && (g == 255) && (b == 0)) {
                    value = GROUND_TILE;
                }

                if ((r == 0) && (g == 0) && (b == 255)) {
                    value = START_TILE;
                    result.start_pos =
                        Vector2f(f32(x * result.tile_size), f32(y * result.tile_size + 40));
                }

                if ((r == 255) && (g == 0) && (b == 255)) {
                    value = END_TILE;
                }

                if ((r == 255) && (g == 255) && (b == 0)) {
                    value = COIN_TILE;
                }

                if ((r == 255) && (g == 0) && (b == 0)) {
                    value = DEATH_TILE;
                }

                if ((r == 127) && (g == 127) && (b == 127)) {
                    value = SPIKES_TILE;
                }
            }

            *tile++ = value;
        }
    }

    return result;
}

static bool test_wall(f32 *t_lowest, f32 wall_coord, f32 wall_min, f32 wall_max, f32 rel_x,
                      f32 rel_y, f32 delta_x, f32 delta_y) {
    f32 t_epsilon = 0.01f;

    if (delta_x != 0.0f) {
        // Reken uit wanneer de speler de muur raakt coordinaat.
        f32 t_result = (wall_coord - rel_x) / delta_x;

        // We willen alleen de dichstbijzijnde botsing, dus slaan we telkens de laagste tijd op.
        if ((t_result < *t_lowest) && (t_result >= 0)) {
            // Check of de y coordinaat ook op de muur ligt.
            f32 y = rel_y + t_result * delta_y;
            if ((y >= wall_min) && (y <= wall_max)) {
                *t_lowest = maximum(0.0f, t_result - t_epsilon);
                return true;
            }
        }
    }

    return false;
}

static Collision update_player_position(Tile_Map *tile_map, Player *player, f32 delta_time) {
    Collision result = {};

    Vector2f old_pos = player->position;

    // Maak gebruik van de formules uit Binas Tabel 35.
    // s = 0.5*a*t^2 + v*t + s
    Vector2f new_pos = player->acceleration * delta_time * delta_time * .5f +
                       player->velocity * delta_time + player->position;
    // v = a*t + v
    player->velocity = player->acceleration * delta_time + player->velocity;

    Vector2f delta_pos = new_pos - old_pos;

    Vector2f old_tile = Vector2f(old_pos.x, old_pos.y) / (f32)tile_map->tile_size;
    Vector2f new_tile = Vector2f(new_pos.x, new_pos.y) / (f32)tile_map->tile_size;

    Vector2i min_tile =
        Vector2i((i32)minimum(old_tile.x, new_tile.x), (i32)minimum(old_tile.y, new_tile.y));
    Vector2i max_tile =
        Vector2i((i32)maximum(old_tile.x, new_tile.x), (i32)maximum(old_tile.y, new_tile.y));

    Vector2i player_tile_size = Vector2i((i32)(player->width / (f32)tile_map->tile_size) + 1,
                                         (i32)(player->height / (f32)tile_map->tile_size) + 1);

    min_tile = min_tile - player_tile_size;
    max_tile = max_tile + player_tile_size;

    // Dit is zijn de hoeken linksonder en rechtsboven ten opzichte van het midden van de tile.
    Vector2f diameter = Vector2f((f32)tile_map->tile_size + player->width,
                                 (f32)tile_map->tile_size + player->height);
    Vector2f min_corner = diameter * -0.5f;
    Vector2f max_corner = diameter * 0.5f;

    f32 t_remaining = 1.0f;

    // TODO(Kay Verbruggen): Hoe vaak moeten we deze loop uitvoeren.
    for (i32 i = 0; (i < 4) && (t_remaining > 0.0f); i++) {
        f32 t_lowest = 1.0f;
        Vector2f normal = Vector2f();

        // Loop door alle mogelijke tiles heen. Dit zijn er eigenlijk te veel, dit dus kan nog
        // sneller.
        for (i32 tile_y = min_tile.y; tile_y <= max_tile.y; tile_y++) {
            for (i32 tile_x = min_tile.x; tile_x <= max_tile.x; tile_x++) {
                if ((tile_x >= 0) && (tile_y >= 0) && (tile_x < tile_map->width) &&
                    tile_y < tile_map->height) {
                    i32 tile_index = tile_y * tile_map->width + tile_x;
                    i32 tile = tile_map->tiles[tile_index];

                    // Reken het midden van de tile uit, en de positie van de speler ten
                    // opzichte van dat midden.
                    Vector2f tile_center =
                        Vector2f((f32)tile_x, (f32)tile_y) * (f32)tile_map->tile_size;
                    Vector2f rel_pos = player->position - tile_center;

                    if ((tile == COIN_TILE) || (tile == DEATH_TILE) || (tile == SPIKES_TILE) ||
                        (tile == END_TILE)) {
                        Vector2f temp_max_corner = max_corner;
                        Vector2f temp_min_corner = min_corner;
                        if (tile == SPIKES_TILE) {
                            temp_max_corner.y -= 40;

                            temp_max_corner.x -= 15;
                            temp_min_corner.x += 15;
                        }

                        if (test_wall(&t_lowest, temp_min_corner.x, temp_min_corner.y,
                                      temp_max_corner.y, rel_pos.x, rel_pos.y, delta_pos.x,
                                      delta_pos.y) ||
                            test_wall(&t_lowest, temp_max_corner.x, temp_min_corner.y,
                                      temp_max_corner.y, rel_pos.x, rel_pos.y, delta_pos.x,
                                      delta_pos.y) ||
                            test_wall(&t_lowest, temp_min_corner.y, temp_min_corner.x,
                                      temp_max_corner.x, rel_pos.y, rel_pos.x, delta_pos.y,
                                      delta_pos.x) ||
                            test_wall(&t_lowest, temp_max_corner.y, temp_min_corner.x,
                                      temp_max_corner.x, rel_pos.y, rel_pos.x, delta_pos.y,
                                      delta_pos.x)) {
                            result.tile |= tile;

                            if (tile == COIN_TILE) {
                                tile_map->tiles[tile_index] = EMPTY_TILE;
                                result.coin_index = tile_index;
                            }
                        }
                    } else if ((tile == GROUND_TILE)) {
                        // Verticale muren.
                        if (test_wall(&t_lowest, min_corner.x, min_corner.y, max_corner.y,
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            normal = Vector2f(-1.0f, 0.0f);
                            result.tile |= tile;
                        }

                        if (test_wall(&t_lowest, max_corner.x, min_corner.y, max_corner.y,
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            normal = Vector2f(1.0f, 0.0f);
                            result.tile |= tile;
                        }

                        // Horizontale muren.
                        if (test_wall(&t_lowest, min_corner.y, min_corner.x, max_corner.x,
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            normal = Vector2f(0.0f, -1.0f);
                            result.tile |= tile;
                        }

                        if (test_wall(&t_lowest, max_corner.y, min_corner.x, max_corner.x,
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            normal = Vector2f(0.0f, 1.0f);
                            result.on_ground = true;
                            result.tile |= tile;
                        }
                    }
                }
            }
        }

        if (normal == Vector2f()) {
            player->position = player->position + delta_pos;
            // t_remaining = 0;
        } else {
            player->position = player->position + delta_pos * t_lowest;
            player->velocity = player->velocity - normal * dot(player->velocity, normal);
            player->acceleration =
                player->acceleration - normal * dot(player->acceleration, normal);
            delta_pos = delta_pos - normal * dot(delta_pos, normal);
        }
        t_remaining -= t_lowest * t_remaining;
    }

    return result;
}

// Dit is de functie die berichten van Windows afhandeld. Op dit moment doen we alleen iets met QUIT
// en DESTROY berichten. Als we een van deze twee aantreffen stoppen we het programma.
LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    // NOTE(Kay Verbruggen):  Uitleg wegwerken globale variabele.
    // In eerste instantie hebben we globbale variabale gebruikt. Dit hebben we gedaan
    // om deze functie per se deze parameters moest hebben volgens Windows, niet meer en niet
    // minder. Globale variabele waren dus de enige manier om iets te doen met de berichten dit
    // Windows ons in deze functie geeft. Na wat google werk kwam ik er achter dat je ook een Struct
    // aan Windows kon geven bij het maken van je venster. Vervolgens kun je dan met je HWND, waar
    // we hier wel toegang tot hebben, die struct weer opvragen. Toen hebben we dus een Game_State
    // struct gemaakt met alle belangrijke variabele en kunnen we deze hiervoor gebruiken.
    Engine *engine;
    if (msg == WM_CREATE) {
        CREATESTRUCT *pCreate = (CREATESTRUCT *)lparam;
        engine = (Engine *)pCreate->lpCreateParams;
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)engine);
    } else {
        engine = (Engine *)GetWindowLongPtrA(window, GWLP_USERDATA);
    }

    switch (msg) {
        case WM_DESTROY: {
            engine->running = false;
            break;
        }

        case WM_CLOSE: {
            engine->running = false;
            break;
        }

        case WM_SIZE: {
            engine->window.resized = true;
            break;
        }

        case WM_KEYDOWN: {
            if ((lparam >> 30) == 0 && (!engine->input.use_gamepad)) {
                process_key_down(&engine->input, (u32)wparam);
            }

            break;
        }

        case WM_KEYUP: {
            if (!engine->input.use_gamepad) process_key_up(&engine->input, (u32)wparam);
            break;
        }

        case WM_LBUTTONDOWN: {
            engine->input.click = true;
            break;
        }

        case WM_LBUTTONUP: {
            engine->input.click = false;
            break;
        }

        default: {
            result = DefWindowProcA(window, msg, wparam, lparam);
        }
    }
    return result;
}

void in_level(Engine *engine, Game *game) {
    if (game->dead)
        game->death_timer += engine->delta_time;
    else
        game->death_timer = 0;

    Player *player = game->player;

    // Zwaartekracht
    player->acceleration.y = -game->gravity;

    // De horizontale beweging door de speler.
    player->acceleration.x = engine->input.movement * 3000;
    if (game->dead) player->acceleration.x = 0;

    // Spring systeem: https://www.youtube.com/watch?v=7KiK0Aqtmzc
    if (engine->input.jump) {
        engine->input.jump = false;

        if (game->collision.on_ground) {
            player->acceleration.y = 1200.0f / engine->delta_time;
            play_sound(&game->jump_sound);  // Speel het geluidje af!
        }
    }

    if (player->velocity.y > 0.0f) {
        player->acceleration.y -= game->gravity * 2.0f;
    } else if (player->velocity.y < 0.0f && !engine->input.space) {
        player->acceleration.y -= game->gravity;
    }

    // Wrijving
    player->acceleration.x -= player->velocity.x * 5.0f;

    // Zorg dat we niet boven de maximale snelheid gaan.
    if (player->velocity.x > player->max_speed) {
        player->velocity.x = player->max_speed;
    } else if (player->velocity.x < -player->max_speed) {
        player->velocity.x = -player->max_speed;
    }

    Tile_Map cur_map = game->tile_maps[game->level];
    game->collision = update_player_position(&cur_map, player, engine->delta_time);

    // game->camera.x = player->position.x - 0.5f*engine->window.buffer.width;

    float follow_speed = (7.0f * engine->delta_time);
    Vector2f target = player->position - Vector2f(engine->window.buffer.width / 2.0f,
                                                  engine->window.buffer.height / 2.0f);
    Vector2f delta_camera = (target - game->camera) * follow_speed;
    game->camera = game->camera + delta_camera;

    // Ga naar het volgende level als we het level hebben gehaald.
    // TODO(Kay Verbruggen): Als we het level halen en menu of knop er tussen hebben om verder
    // te gaan, blijft de delta tijd voorlopig oplopen, misschien moeten we hier een oplossing
    // voor bedenken. Het kan ook zijn dat het probleem sowieso al niet meer bestaat als we een
    // in-engine menu hebben.
    if ((game->collision.tile & END_TILE) && (game->coin_collected)) {
        player->velocity = Vector2f();

        engine->window.stretch_on_resize = true;
        free_sprite(&game->background);
        play_sound(&game->completed_sound);

        if (game->level < NUM_LEVELS - 1) {
            game->state = LEVEL_COMPLETE;
            game->level_complete = load_bitmap("assets\\level complete.bmp");
        } else {
            game->state = END;
            game->end_game = load_bitmap("assets\\end game.bmp");
        }

        return;
    }

    if ((game->collision.tile & DEATH_TILE) || (game->collision.tile & SPIKES_TILE)) {
        if (game->coin_collected) {
            cur_map.tiles[game->coin_index] = COIN_TILE;
        }
        play_sound(&game->failed_sound);
        engine->window.stretch_on_resize = true;
        game->state = LEVEL_FAILED;
        game->level_failed = load_bitmap("assets\\level failed.bmp");
        free_sprite(&game->background);
        return;
    }

    if (game->collision.tile & COIN_TILE) {
        game->coin_index = game->collision.coin_index;
        play_sound(&game->coin_sound);
        game->coin_collected = true;
        char buffer[256];
        StringCbPrintfA(buffer, 256, "Coins: %d\n", game->coin_count);
        OutputDebugStringA(buffer);
    }

    draw_sprite(&engine->window, Vector2f(), &game->background,
                Vector2f(1920.0f / 2.0f, 1080.0f / 2.0f));

    // De tilemap op het scherm zetten.
    for (i32 y = cur_map.height - 1; y >= 0; y--) {
        for (i32 x = 0; x < cur_map.width; x++) {
            i32 tile = cur_map.tiles[y * cur_map.width + x];
            f32 real_x = (f32)(x * cur_map.tile_size);
            f32 real_y = (f32)(y * cur_map.tile_size);
            if ((real_x > game->camera.x - 100) &&
                (real_x < game->camera.x + engine->window.buffer.width + 100) &&
                (real_y > game->camera.y - 100) &&
                (real_y < game->camera.y + engine->window.buffer.height + 100)) {
                if (tile == GROUND_TILE) {
                    draw_sprite(&engine->window, game->camera, &cur_map.ground,
                                Vector2f(real_x, real_y));
                } else if (tile == END_TILE) {
                    // TODO: Fix hardcoden van de deur offset op de y-as.
                    draw_sprite(&engine->window, game->camera, &cur_map.end,
                                Vector2f(real_x, real_y + 60));
                } else if (tile == COIN_TILE) {
                    draw_sprite(&engine->window, game->camera, &cur_map.coin,
                                Vector2f(real_x, real_y));
                } else if (tile == SPIKES_TILE) {
                    draw_sprite(&engine->window, game->camera, &cur_map.spikes,
                                Vector2f(real_x, real_y - 10));
                }
            }
        }
    }

    player->frame += engine->delta_time * player->current_anim.fps;
    if (player->frame >= 8.0f) player->frame = 0.0f;

    // Wissel van animatie als de speler van kant wisselt of stopt met lopen.
    // TODO(Kay Verbruggen): Hardcoded!
    if (player->velocity.x > 7.0f) {
        player->current_anim = player->walk_right;
    } else if (player->velocity.x < -7.0f) {
        player->current_anim = player->walk_left;
    } else {
        if (player->current_anim.id == WALK_RIGHT)
            player->current_anim = player->idle_right;
        else if (player->current_anim.id == WALK_LEFT)
            player->current_anim = player->idle_left;
    }

    if (engine->input.use_gamepad && game->level < 3) {
        draw_sprite(&engine->window, game->camera, &game->tips_console[game->level],
                    Vector2f(1400.0f, 800.0f));
    } else if (game->level < 3) {
        draw_sprite(&engine->window, game->camera, &game->tips_pc[game->level],
                    Vector2f(1400.0f, 800.0f));
    }

    draw_sprite(&engine->window, game->camera, &player->current_anim.sprites[(u8)player->frame],
                player->position);
}

i32 read_progress() {
    FILE *in_file;
    int number;

    in_file = fopen("progress.txt", "r");

    if (in_file == NULL) {
        printf("Can't open file for reading.\n");
    } else {
        fscanf(in_file, "%d", &number);
        fclose(in_file);
    }

    return number;
}

void save_progress(i32 level) {
    FILE *out_file;
    out_file = fopen("progress.txt", "w");

    if (out_file == NULL) {
        printf("Can't open file for reading.\n");
    } else {
        fprintf(out_file, "%d", level);
        fclose(out_file);
    }
}

// Dit is de main functie zoals Windows die gebruikt, dit is nodig om een venster te kunnen openen.
int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    // Maak de initiële game state.
    Engine engine = {};

    engine.window.stretch_on_resize = false;
    engine.window.resized = false;
    engine.running = true;
    engine.input.use_gamepad = false;
    engine.target_time = 1.0f / 60.0f;

    // Hier maken we het venster waarin we vervolgens onze engine in kunnen laten zien.
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = "class_name";
    wc.hIcon = LoadIcon(instance, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(instance, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        OutputDebugStringA("Failed to register window class!");
        return 1;
    }

    HWND window = CreateWindowExA(0, wc.lpszClassName, "Pilot Adventures", WS_POPUP | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, engine.window.width,
                                  engine.window.height, 0, 0, instance, &engine);

    HDC hdc = GetDC(window);

    engine.window.width = GetDeviceCaps(hdc, HORZRES);
    engine.window.height = GetDeviceCaps(hdc, VERTRES);

    SetWindowPos(window, NULL, 0, 0, engine.window.width, engine.window.height, SWP_FRAMECHANGED);

    // Als we hier nog steeds geen venster hebben, dan is er iets mis en stoppen we het programma.
    if (!window) {
        OutputDebugStringA("Failed to create window handle!");
        return 1;
    }
    ShowWindow(window, SW_SHOWDEFAULT);

    engine.window.handle = window;
    engine.window.device_context = hdc;
    resize_buffer(&engine.window.buffer, Vector2i(1920, 1080));

    // Audio.
    initialize_audio(&engine.audio);

    Player player = {};

    // Right animation
    player.walk_right.sprites[0] = load_bitmap("assets\\walk_right\\0.bmp");
    player.walk_right.sprites[1] = load_bitmap("assets\\walk_right\\1.bmp");
    player.walk_right.sprites[2] = load_bitmap("assets\\walk_right\\2.bmp");
    player.walk_right.sprites[3] = load_bitmap("assets\\walk_right\\3.bmp");
    player.walk_right.sprites[4] = load_bitmap("assets\\walk_right\\4.bmp");
    player.walk_right.sprites[5] = load_bitmap("assets\\walk_right\\5.bmp");
    player.walk_right.sprites[6] = load_bitmap("assets\\walk_right\\6.bmp");
    player.walk_right.sprites[7] = load_bitmap("assets\\walk_right\\7.bmp");
    player.walk_right.fps = 8;
    player.walk_right.id = WALK_RIGHT;

    // Left animation
    player.walk_left.sprites[0] = load_bitmap("assets\\walk_left\\0.bmp");
    player.walk_left.sprites[1] = load_bitmap("assets\\walk_left\\1.bmp");
    player.walk_left.sprites[2] = load_bitmap("assets\\walk_left\\2.bmp");
    player.walk_left.sprites[3] = load_bitmap("assets\\walk_left\\3.bmp");
    player.walk_left.sprites[4] = load_bitmap("assets\\walk_left\\4.bmp");
    player.walk_left.sprites[5] = load_bitmap("assets\\walk_left\\5.bmp");
    player.walk_left.sprites[6] = load_bitmap("assets\\walk_left\\6.bmp");
    player.walk_left.sprites[7] = load_bitmap("assets\\walk_left\\7.bmp");
    player.walk_left.fps = 8;
    player.walk_left.id = WALK_LEFT;

    // Idle right animation.
    player.idle_right.sprites[0] = load_bitmap("assets\\idle_right\\0.bmp");
    player.idle_right.sprites[1] = load_bitmap("assets\\idle_right\\1.bmp");
    player.idle_right.sprites[2] = load_bitmap("assets\\idle_right\\2.bmp");
    player.idle_right.sprites[3] = load_bitmap("assets\\idle_right\\3.bmp");
    player.idle_right.sprites[4] = load_bitmap("assets\\idle_right\\4.bmp");
    player.idle_right.sprites[5] = load_bitmap("assets\\idle_right\\5.bmp");
    player.idle_right.sprites[6] = load_bitmap("assets\\idle_right\\6.bmp");
    player.idle_right.sprites[7] = load_bitmap("assets\\idle_right\\7.bmp");
    player.idle_right.fps = 4;
    player.idle_right.id = IDLE_RIGHT;

    // Idle left animation.
    player.idle_left.sprites[0] = load_bitmap("assets\\idle_left\\0.bmp");
    player.idle_left.sprites[1] = load_bitmap("assets\\idle_left\\1.bmp");
    player.idle_left.sprites[2] = load_bitmap("assets\\idle_left\\2.bmp");
    player.idle_left.sprites[3] = load_bitmap("assets\\idle_left\\3.bmp");
    player.idle_left.sprites[4] = load_bitmap("assets\\idle_left\\4.bmp");
    player.idle_left.sprites[5] = load_bitmap("assets\\idle_left\\5.bmp");
    player.idle_left.sprites[6] = load_bitmap("assets\\idle_left\\6.bmp");
    player.idle_left.sprites[7] = load_bitmap("assets\\idle_left\\7.bmp");
    player.idle_left.fps = 4;
    player.idle_left.id = IDLE_LEFT;

    player.current_anim = player.idle_right;

    player.max_speed = 750.0f;
    player.width = 31 * 3;
    player.height = 56 * 3;

    // Fill out the game struct.
    Game game = {};
    game.gravity = 1500.0f;
    game.player = &player;
    game.camera = Vector2f();
    game.collision = {};
    game.state = MAIN_MENU;

    game.hit_sound = load_sound(&engine.audio, "assets\\hit.wav");
    game.completed_sound = load_sound(&engine.audio, "assets\\completed.wav");
    game.failed_sound = load_sound(&engine.audio, "assets\\failed.wav");
    game.jump_sound = load_sound(&engine.audio, "assets\\jump 1.wav");
    game.coin_sound = load_sound(&engine.audio, "assets\\coin.wav");

    // Laad de plaatjes.
    game.main_menu = load_bitmap("assets\\main menu.bmp");
    // game.background = load_bitmap("assets\\background.bmp");
    // game.level_complete = load_bitmap("assets\\level complete.bmp");
    // game.end_game = load_bitmap("assets\\end game.bmp");
    // game.level_failed = load_bitmap("assets\\level failed.bmp");

    // Maak de UI.
    game.quit_button.half_width = 225;
    game.quit_button.half_height = 90;
    game.quit_button.sprite = load_bitmap("assets\\quit button.bmp");
    game.quit_button.position.x = 1920 / 2;
    game.quit_button.position.y = 250;
    game.quit_button.select_sound = load_sound(&engine.audio, "assets\\select.wav");

    Button center_button = {};
    center_button.half_width = 225;
    center_button.half_height = 90;
    center_button.position.x = engine.window.width / 2.0f;
    center_button.position.y = engine.window.height / 2.0f;
    center_button.select_sound = load_sound(&engine.audio, "assets\\select.wav");

    game.next_button = center_button;
    game.next_button.sprite = load_bitmap("assets\\next button.bmp");

    game.play_button = center_button;
    game.play_button.sprite = load_bitmap("assets\\play button.bmp");

    game.restart_button = center_button;
    game.restart_button.sprite = load_bitmap("assets\\restart button.bmp");

    // Laad de levels.
    // TODO(Kay Verbruggen): Laad alle levels uit een mapje met FindFirstFile en FindNextFile.
    game.tile_maps[0] = load_tile_map("levels\\1.bmp");
    game.tile_maps[1] = load_tile_map("levels\\2.bmp");
    game.tile_maps[2] = load_tile_map("levels\\3.bmp");
    game.tile_maps[3] = load_tile_map("levels\\4.bmp");
    game.tile_maps[4] = load_tile_map("levels\\5.bmp");
    game.tile_maps[5] = load_tile_map("levels\\6.bmp");
    game.tile_maps[6] = load_tile_map("levels\\7.bmp");
    game.tile_maps[7] = load_tile_map("levels\\8.bmp");
    game.tile_maps[8] = load_tile_map("levels\\9.bmp");
    game.tile_maps[9] = load_tile_map("levels\\10.bmp");

    game.level = read_progress();
    game.player->position = game.tile_maps[game.level].start_pos;

    game.tips_console[0] = load_bitmap("assets\\console tip 1.bmp");
    game.tips_console[1] = load_bitmap("assets\\console tip 2.bmp");
    game.tips_console[2] = load_bitmap("assets\\console tip 3.bmp");

    game.tips_pc[0] = load_bitmap("assets\\pc tip 1.bmp");
    game.tips_pc[1] = load_bitmap("assets\\pc tip 2.bmp");
    game.tips_pc[2] = load_bitmap("assets\\pc tip 3.bmp");

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER start_count, end_count;
    QueryPerformanceCounter(&start_count);

#if PROFILE
    i64 start_cycles = __rdtsc();
#endif
    MSG msg;

    Sound theme_song = load_sound(&engine.audio, "assets\\song.wav", true);
    play_sound(&theme_song);

    while (engine.running) {
        // Kijk of er nog berichten zijn van Windows, zoja dan moeten we deze eerst afhandelen.
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Controller input.
        // if (engine.input.use_gamepad) {
        process_gamepad_input(&engine.input);
        //}

        // NOTE(Kay Verbruggen): Uitleg resizen van het venster.
        // Als we van Windows het bericht WM_SIZE hebben gekregen, weten we dat de afmetingen
        // van het venster zijn veranderd. Daarom vragen we de nieuwe breedte en hoogte,
        // vervolgens kunnen we twee dingen doen:
        // - We kunnen het spel uitrekken zodat het hele venster wordt gevuld.
        // - We kunnen meer van het spel laten zien zodat de verhoudingen niet veranderen
        // Op dit moment hebben we een variabele die dit controleerd, misschien dat we later
        // een keuze tussen de twee maken. Of de speler laten kiezen.
        if (engine.window.resized) {
            RECT window_rect;
            GetWindowRect(window, &window_rect);
            engine.window.width = window_rect.right - window_rect.left;
            engine.window.height = window_rect.bottom - window_rect.top;

            if (!engine.window.stretch_on_resize) {
                engine.window.resized = false;
                resize_buffer(&engine.window.buffer,
                              Vector2i(engine.window.width, engine.window.height));
            }
        }

        switch (game.state) {
            case MAIN_MENU: {
                game.camera = Vector2f();
                Vector2f center_screen =
                    Vector2f(engine.window.width / 2.0f, engine.window.height / 2.0f);
                draw_sprite(&engine.window, game.camera, &game.main_menu, center_screen);

                update_button(&engine, &game.play_button);
                if (game.play_button.is_pressed || engine.input.next) {
                    game.state = IN_LEVEL;
                    update_window(&engine.window);

                    game.coin_collected = false;

                    game.background = load_bitmap("assets\\background.bmp");
                    free_sprite(&game.main_menu);

                    break;
                }

                update_button(&engine, &game.quit_button);
                if (game.quit_button.is_pressed || engine.input.quit) {
                    engine.running = false;
                }

                update_window(&engine.window);
                break;
            }

            case IN_LEVEL: {
                in_level(&engine, &game);
                update_window(&engine.window);
                break;
            }

            case LEVEL_COMPLETE: {
                game.camera = Vector2f();
                Vector2f center_screen =
                    Vector2f(engine.window.width / 2.0f, engine.window.height / 2.0f);
                draw_sprite(&engine.window, game.camera, &game.level_complete, center_screen);
                save_progress(game.level + 1);

                update_button(&engine, &game.next_button);
                if (game.next_button.is_pressed || engine.input.next) {
                    game.state = IN_LEVEL;

                    game.level++;
                    game.coin_collected = false;
                    game.player->position = game.tile_maps[game.level].start_pos;

                    update_window(&engine.window);

                    game.background = load_bitmap("assets\\background.bmp");
                    free_sprite(&game.level_complete);

                    break;
                }

                update_button(&engine, &game.quit_button);
                if (game.quit_button.is_pressed || engine.input.quit) {
                    engine.running = false;
                }

                update_window(&engine.window);
                break;
            }

            case LEVEL_FAILED: {
                game.camera = Vector2f();
                Vector2f center_screen =
                    Vector2f(engine.window.width / 2.0f, engine.window.height / 2.0f);
                draw_sprite(&engine.window, game.camera, &game.level_failed, center_screen);

                update_button(&engine, &game.restart_button);
                if (game.restart_button.is_pressed || engine.input.next) {
                    game.state = IN_LEVEL;

                    game.coin_collected = false;
                    game.player->position = game.tile_maps[game.level].start_pos;
                    game.player->velocity = Vector2f();

                    update_window(&engine.window);

                    game.background = load_bitmap("assets\\background.bmp");
                    free_sprite(&game.level_failed);

                    break;
                }

                update_button(&engine, &game.quit_button);
                if (game.quit_button.is_pressed || engine.input.quit) {
                    engine.running = false;
                }

                update_window(&engine.window);
                break;
            }

            case END: {
                game.camera = Vector2f();
                Vector2f center_screen =
                    Vector2f(engine.window.width / 2.0f, engine.window.height / 2.0f);
                draw_sprite(&engine.window, game.camera, &game.end_game, center_screen);
                save_progress(0);

                update_button(&engine, &game.restart_button);
                if (game.restart_button.is_pressed || engine.input.next) {
                    game.state = MAIN_MENU;

                    game.tile_maps[0] = load_tile_map("levels\\1.bmp");
                    game.tile_maps[1] = load_tile_map("levels\\2.bmp");
                    game.tile_maps[2] = load_tile_map("levels\\3.bmp");
                    game.tile_maps[3] = load_tile_map("levels\\4.bmp");
                    game.tile_maps[4] = load_tile_map("levels\\5.bmp");
                    game.tile_maps[5] = load_tile_map("levels\\6.bmp");
                    game.tile_maps[6] = load_tile_map("levels\\7.bmp");
                    game.tile_maps[7] = load_tile_map("levels\\8.bmp");
                    game.tile_maps[8] = load_tile_map("levels\\9.bmp");
                    game.tile_maps[9] = load_tile_map("levels\\10.bmp");

                    game.level = 0;
                    game.coin_collected = false;
                    game.player->position = game.tile_maps[game.level].start_pos;

                    game.main_menu = load_bitmap("assets\\main menu.bmp");
                    free_sprite(&game.end_game);
                    break;
                }

                update_button(&engine, &game.quit_button);
                if (game.quit_button.is_pressed || engine.input.quit) {
                    engine.running = false;
                }

                update_window(&engine.window);
                break;
            }
        }

        QueryPerformanceCounter(&end_count);
        i64 delta_counter = end_count.QuadPart - start_count.QuadPart;
        i64 fps = frequency.QuadPart / delta_counter;
        engine.delta_time = (f32)(delta_counter) / (f32)frequency.QuadPart;

        // Profile performance hier, de sleep hoort niet bij de daadwerkelijke performance.
#if PROFILE
        i64 end_cycles = __rdtsc();
        i64 delta_cycles = end_cycles - start_cycles;
        char buffer[256];
        StringCbPrintfA(buffer, 256, "Delta Time: %fms\tFPS: %lld\tCycles: %lld\n",
                        engine.delta_time * 1000.0f, fps, delta_cycles);
        OutputDebugStringA(buffer);
        start_cycles = end_cycles;
#endif
        // Sleep zodat de engine op een bepaald aantal fps runt,
        // anders zou de engine gewoon een hele core gebruiken.
        if (engine.delta_time < engine.target_time) {
            Sleep((DWORD)((engine.target_time - engine.delta_time) * 1000.0f));
        }

        QueryPerformanceCounter(&end_count);
        delta_counter = end_count.QuadPart - start_count.QuadPart;
        engine.delta_time = (f32)(delta_counter) / (f32)frequency.QuadPart;

#if PROFILE
        char window_title[256];
        StringCbPrintfA(window_title, 256, "Pilot Adventures\t\t FPS: %d\n",
                        (int)(1.0f / engine.delta_time));
        SetWindowTextA(window, window_title);
#endif

        start_count = end_count;
    }

    ReleaseDC(window, engine.window.device_context);
    close_audio(&engine.audio);
    return 0;
}
