#include <windows.h>
#include <Xinput.h>
#include <xaudio2.h>
#include <stdio.h>

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

#include "math.h"
// Include alle cpp bestanden hier.
#include "audio.cpp"
#include "input.cpp"
#include "draw.cpp"
#include "math.cpp"

struct Tile_Map {
    i32 width, height, tile_size;
    i32 *tiles;
    Sprite ground;
    Sprite end;
};

struct Player {
    Sprite sprite;
    Vector2f position;
    Vector2f velocity;
    f32 speed;
};

struct Game_State {
    Game_Input input;
    Game_Audio audio;
    Game_Window window;
    bool running;
    f32 delta_time;

    // Gameplay.
    Player player;
    Tile_Map tile_map;
};

#define GROUND_TILE 1
#define START_TILE 2
#define END_TILE 3

static Tile_Map load_tile_map(const char *filename, Player *player) {
    Tile_Map result = {};

    Sprite level_design = load_bitmap(filename);
    result.height = level_design.height;
    result.width = level_design.width;
    result.tile_size = 32;
    result.ground = load_bitmap("assets\\grass.bmp");
    result.end = load_bitmap("assets\\door.bmp");
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
                    player->position =
                        Vector2f(f32(x * result.tile_size), f32(y * result.tile_size));
                }
                if ((r == 255) && (g == 0) && (b == 255)) {
                    value = END_TILE;
                }
            }

            *tile++ = value;
        }
    }

    return result;
}

static void draw_tile_map(Game_Window *window, Tile_Map *tile_map) {
    for (i32 y = 0; y < tile_map->height; y++) {
        for (i32 x = 0; x < tile_map->width; x++) {
            i32 tile = tile_map->tiles[y * tile_map->width + x];
            if (tile == GROUND_TILE) {
                draw_sprite(window, &tile_map->ground,
                            Vector2f(f32(x * tile_map->tile_size), f32(y * tile_map->tile_size)));
            } else if (tile == END_TILE) {
                draw_sprite(window, &tile_map->end,
                            Vector2f(f32(x * tile_map->tile_size), f32(y * tile_map->tile_size)));
            }
        }
    }
}

// Dit is de functie die berichten van Windows afhandeld. Op dit moment doen we alleen iets met QUIT
// en DESTROY berichten. Als we een van deze twee aantreffen stoppen we het programma.
LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;

    // NOTE: Uitleg wegwerken globale variabele.
    // In eerste instantie hebben we globbale variabale gebruikt. Dit hebben we gedaan
    // om deze functie per se deze parameters moest hebben volgens Windows, niet meer en niet
    // minder. Globale variabele waren dus de enige manier om iets te doen met de berichten dit
    // Windows ons in deze functie geeft. Na wat google werk kwam ik er achter dat je ook een Struct
    // aan Windows kon geven bij het maken van je venster. Vervolgens kun je dan met je HWND, waar
    // we hier wel toegang tot hebben, die struct weer opvragen. Toen hebben we dus een Game_State
    // struct gemaakt met alle belangrijke variabele en kunnen we deze hiervoor gebruiken.
    Game_State *state;
    if (msg == WM_CREATE) {
        CREATESTRUCT *pCreate = (CREATESTRUCT *)lparam;
        state = (Game_State *)pCreate->lpCreateParams;
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)state);
    } else {
        state = (Game_State *)GetWindowLongPtrA(window, GWLP_USERDATA);
    }

    switch (msg) {
    case WM_DESTROY: {
        state->running = false;
        break;
    }

    case WM_CLOSE: {
        state->running = false;
        break;
    }

    case WM_SIZE: {
        state->window.resized = true;
        break;
    }

    case WM_KEYDOWN: {
        if ((lparam >> 30) == 0) {
            process_key_down(&state->input, (u32)wparam);
        }
        break;
    }

    case WM_KEYUP: {
        process_key_up(&state->input, (u32)wparam);
        break;
    }

    default: { result = DefWindowProcA(window, msg, wparam, lparam); }
    }

    return result;
}

// Dit is de main functie zoals Windows die gebruikt, dit is nodig om een venster te kunnen openen.
int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    // Maak de initiële game state.
    Game_State game = {};

    game.window.width = 640;
    game.window.height = 360;
    game.window.stretch_on_resize = true;
    game.window.resized = false;
    game.running = true;
    game.input.use_gamepad = false;

    // Hier maken we het venster waarin we vervolgens onze game in kunnen laten zien.
    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = "class_name";

    if (!RegisterClassA(&wc)) {
        OutputDebugStringA("Failed to register window class!");
        return 1;
    }

    HWND window = CreateWindowExA(0, wc.lpszClassName, "Mario", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, game.window.width,
                                  game.window.height, 0, 0, instance, &game);

    // Als we hier nog steeds geen venster hebben, dan is er iets mis en stoppen we het programma.
    if (!window) {
        OutputDebugStringA("Failed to create window handle!");
        return 1;
    }
    ShowWindow(window, SW_SHOWDEFAULT);

    game.window.device_context = GetDC(window);
    resize_buffer(&game.window);

    // Audio.
    initialize_audio(&game.audio);

    // Laad de plaatjes.
    Sprite background = load_bitmap("assets\\background.bmp");

    // Zet de speler.
    Sprite player_right = load_bitmap("assets\\player_right.bmp");
    Sprite player_left = load_bitmap("assets\\player_left.bmp");
    game.player.sprite = player_right;
    game.player.speed = 300.0f;

    game.tile_map = load_tile_map("assets\\level.bmp", &game.player);

    // Tekst.
    RECT text_rect = {};
    text_rect.left = 300;
    text_rect.top = 300;
    text_rect.bottom = 400;
    text_rect.right = 400;
    if (!AddFontResourceA("assets\\Kenney Future.ttf")) {
        MessageBoxA(0, "Failed to load fonts!", "Fonts", MB_OK);
    }
    HFONT font =
        CreateFontA(24, 0, 0, 0, 500, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, "Kenney Future");
    SelectObject(game.window.device_context, font);
    SetBkMode(game.window.device_context, TRANSPARENT);

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER start_count, end_count;
    QueryPerformanceCounter(&start_count);

#if PROFILE
    i64 start_cycles = __rdtsc();
#endif

    MSG msg;
    while (game.running) {
        // Kijk of er nog berichten zijn van Windows, zoja dan moeten we deze eerst afhandelen.
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Controller input.
        if (game.input.use_gamepad) {
            process_gamepad_input(&game.input);
        }

        // Game.
        // NOTE: Uitleg resizen van het venster.
        // Als we van Windows het bericht WM_SIZE hebben gekregen, weten we dat de afmetingen
        // van het venster zijn veranderd. Daarom vragen we de nieuwe breedte en hoogte,
        // vervolgens kunnen we twee dingen doen:
        // - We kunnen het spel uitrekken zodat het hele venster wordt gevuld.
        // - We kunnen meer van het spel laten zien zodat de verhoudingen niet veranderen
        // Op dit moment hebben we een variabele die dit controleerd, misschien dat we later
        // een keuze tussen de twee maken. Of de speler laten kiezen.
        if (game.window.resized) {
            RECT window_rect;
            GetWindowRect(window, &window_rect);
            game.window.width = window_rect.right - window_rect.left;
            game.window.height = window_rect.bottom - window_rect.top;

            if (!game.window.stretch_on_resize) {
                resize_buffer(&game.window);
            }
        }

        game.player.velocity =
            game.input.movement.normalize() * game.delta_time * game.player.speed;
        game.player.position = game.player.position + game.player.velocity;
        if (game.player.velocity.x > 0.0f) {
            game.player.sprite = player_right;
        } else if (game.player.velocity.x < 0.0f) {
            game.player.sprite = player_left;
        }

        draw_sprite(&game.window, &background);
        draw_tile_map(&game.window, &game.tile_map);
        draw_sprite(&game.window, &game.player.sprite, game.player.position);
        update_window(&game.window);

        QueryPerformanceCounter(&end_count);
        i64 delta_counter = end_count.QuadPart - start_count.QuadPart;
        game.delta_time = (f32)(delta_counter) / (f32)frequency.QuadPart;

#if PROFILE
        i64 end_cycles = __rdtsc();
        i64 fps = frequency.QuadPart / delta_counter;
        i64 delta_cycles = end_cycles - start_cycles;
        char buffer[256];
        _snprintf_s(buffer, 256, 256, "Delta Time: %fms\tFPS: %lld\tCycles: %lld\n",
                    game.delta_time * 1000.0f, fps, delta_cycles);
        OutputDebugStringA(buffer);
        start_cycles = end_cycles;
#endif
        start_count = end_count;
    }

    ReleaseDC(window, game.window.device_context);
    game.audio.engine->Release();
    return 0;
}