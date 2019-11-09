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

// #include "math.h"
// Include alle cpp bestanden hier.
#include "math.cpp"
#include "audio.cpp"
#include "input.cpp"
#include "draw.cpp"

#define minimum(A, B) ((A < B) ? (A) : (B))
#define maximum(A, B) ((A > B) ? (A) : (B))

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
    f32 width, height;
    f32 speed;
};

struct Game_State {
    Game_Input input;
    Game_Audio audio;
    Game_Window window;
    bool running;
    f32 delta_time;
    f32 target_time;
    
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

static bool test_wall(f32 *t_lowest, f32 wall_coord, f32 wall_min, f32 wall_max, 
                      f32 rel_x, f32 rel_y, f32 delta_x, f32 delta_y) {
    f32 t_epsilon = 0.001f;
    
    if (delta_x != 0.0f) {
        // Reken uit wanneer de speler de muur raakt coordinaat.
        f32 t_result = (wall_coord - rel_x) / delta_x;
        
        // We willen alleen de dichstbijzijnde botsing, dus slaan we telkens de laagste tijd op.
        if ((t_result < *t_lowest) && (t_result >= 0)) {
            // Check of de y coordinaat ook op de muur ligt.
            f32 y = rel_y + t_result*delta_y;
            if ((y >= wall_min) && (y <= wall_max)) {
                *t_lowest = maximum(0.0f, t_result - t_epsilon);
                return true;
            }
        }
    }
    
    return false;
}

static i32 move_player(Tile_Map *tile_map, Player *player, f32 delta_time) {
    Vector2f old_pos = player->position;
    Vector2f new_pos = player->position + player->velocity*delta_time;
    
    Vector2f delta_pos = new_pos - old_pos;
    
    Vector2f old_tile = Vector2f(old_pos.x / (f32)tile_map->tile_size, 
                                 old_pos.y / (f32)tile_map->tile_size);
    Vector2f new_tile = Vector2f(new_pos.x / (f32)tile_map->tile_size, 
                                 new_pos.y / (f32)tile_map->tile_size);
    
    Vector2i min_tile = Vector2i((i32)minimum(old_tile.x, new_tile.x), 
                                 (i32)minimum(old_tile.y, new_tile.y));
    Vector2i max_tile = Vector2i((i32)maximum(old_tile.x, new_tile.x), 
                                 (i32)maximum(old_tile.y, new_tile.y));
    Vector2i player_tile_size = Vector2i((i32)(player->width / (f32)tile_map->tile_size) + 1, 
                                         (i32)(player->height / (f32)tile_map->tile_size) + 1);
    
    min_tile = min_tile - player_tile_size;
    max_tile = max_tile + player_tile_size;
    
    i32 final_tile = 0;
    
    
    // Dit is zijn de hoeken linksonder en rechtsboven ten opzichte van het midden van de tile.
    Vector2f diameter = Vector2f((f32)tile_map->tile_size + player->width,
                                 (f32)tile_map->tile_size + player->height);
    Vector2f min_corner = diameter * -0.5f;
    Vector2f max_corner = diameter * 0.5f;
    
    f32 t_remaining = 1.0f;
    
    // // TODO(Kay Verbruggen): Hoe vaak moeten we deze loop uitvoeren.
    for (i32 i = 0; (i < 4) && (t_remaining > 0.0f); i++) {
        
        f32 t_lowest = 1.0f;
        Vector2f wall_normal = Vector2f();
        
        // Loop door alle mogelijke tiles heen. Dit zijn er eigenlijk te veel, dit dus kan nog sneller.
        for (i32 tile_y = min_tile.y; tile_y <= max_tile.y; tile_y++) {
            for (i32 tile_x = min_tile.x; tile_x <= max_tile.x; tile_x++) {
                
                if ((tile_x >= 0) && (tile_y >= 0)) {
                    i32 tile = tile_map->tiles[tile_y * tile_map->width + tile_x];
                    if ((tile == GROUND_TILE) || (tile == END_TILE)) {
                        // Reken het midden van de tile uit, en de positie van de speler ten opzichte van dat midden.
                        Vector2f tile_center = Vector2f((f32)tile_x, (f32)tile_y)*(f32)tile_map->tile_size;
                        Vector2f rel_pos = player->position - tile_center;
                        
                        // Verticale muren.
                        if (test_wall(&t_lowest, min_corner.x, min_corner.y, max_corner.y, 
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            wall_normal = Vector2f(-1.0f, 0.0f);
                            final_tile = tile;
                        }
                        
                        if (test_wall(&t_lowest, max_corner.x, min_corner.y, max_corner.y, 
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            wall_normal = Vector2f(1.0f, 0.0f);
                            final_tile = tile;
                        }
                        
                        // Horizontale muren.
                        if (test_wall(&t_lowest, min_corner.y, min_corner.x, max_corner.x, 
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            wall_normal = Vector2f(0.0f, -1.0f);
                            final_tile = tile;
                        }
                        if (test_wall(&t_lowest, max_corner.y, min_corner.x, max_corner.x, 
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            wall_normal = Vector2f(0.0f, 1.0f);
                            final_tile = tile;
                        }
                    }
                }
            }
        }
        
        player->position = player->position + delta_pos*t_lowest;
        player->velocity = player->velocity - wall_normal*dot(player->position, wall_normal);
        delta_pos = delta_pos - wall_normal*dot(delta_pos, wall_normal);
        t_remaining -= t_lowest*t_remaining;
    }
    
    return final_tile;
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
            if ((lparam >> 30) == 0 && (!state->input.use_gamepad)) {
                process_key_down(&state->input, (u32)wparam);
            }
            
            break;
        }
        
        
        case WM_KEYUP: {
            if (!state->input.use_gamepad)
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
    game.target_time = 1.0f/120.0f;
    
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
    game.player.speed = 200.0f;
    game.player.width = (f32)player_left.width;
    game.player.height = (f32)player_left.height;
    
    game.tile_map = load_tile_map("assets\\level.bmp", &game.player);
    
    // Tekst.
    RECT text_rect = {};
    text_rect.left = 0;
    text_rect.top = 0;
    text_rect.bottom = 300;
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
        // NOTE(Kay Verbruggen): Uitleg resizen van het venster.
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
        
        static f32 gravity = 10000.0f;
        
        game.player.velocity =
            game.input.movement * game.player.speed;
        game.player.velocity.y -= gravity * game.delta_time;
        
        // Spring systeem: https://www.youtube.com/watch?v=7KiK0Aqtmzc
        if (game.input.jump) {
            game.player.velocity.y = 5000.0f;
            game.input.jump = false;
        }
        
        if (game.player.velocity.y > 0.0f) {
            game.player.velocity.y -= gravity * 1.0f * game.delta_time;
        } else if  (game.player.velocity.y < 0.0f && !game.input.space) {
            game.player.velocity.y -= gravity *  game.delta_time;
        }
        
        i32 col_tile = move_player(&game.tile_map, &game.player, game.delta_time);
        if (col_tile == END_TILE) {
            if (MessageBoxA(0, "Je hebt het level gehaald!", "Sucess", MB_OK) == IDOK)
                game.running = false;
            
        }
        
        
        if (game.player.velocity.x > 0.0f) {
            game.player.sprite = player_right;
        } else if (game.player.velocity.x < 0.0f) {
            game.player.sprite = player_left;
        }
        
        draw_sprite(&game.window, &background);
        
        // De tilemap op het scherm zetten.
        for (i32 y = 0; y < game.tile_map.height; y++) {
            for (i32 x = 0; x < game.tile_map.width; x++) {
                i32 tile = game.tile_map.tiles[y * game.tile_map.width + x];
                if (tile == GROUND_TILE) {
                    draw_sprite(&game.window, &game.tile_map.ground,
                                Vector2f(f32(x * game.tile_map.tile_size), f32(y * game.tile_map.tile_size)));
                } else if (tile == END_TILE) {
                    // TODO: Fix hardcoden van de deur offset op de y-as.
                    draw_sprite(&game.window, &game.tile_map.end,
                                Vector2f(f32(x * game.tile_map.tile_size), f32(y * game.tile_map.tile_size + 20)));
                }
            }
        }
        
        draw_sprite(&game.window, &game.player.sprite, game.player.position);
        update_window(&game.window);
        
        QueryPerformanceCounter(&end_count);
        i64 delta_counter = end_count.QuadPart - start_count.QuadPart;
        game.delta_time = (f32)(delta_counter) / (f32)frequency.QuadPart;
        if (game.delta_time < game.target_time) {
            Sleep((DWORD)((game.target_time - game.delta_time) * 1000.0f));
        }
        
        QueryPerformanceCounter(&end_count);
        delta_counter = end_count.QuadPart - start_count.QuadPart;
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