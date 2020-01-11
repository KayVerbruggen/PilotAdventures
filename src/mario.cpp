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
    Sprite ground, end, coin;
    Vector2f start_pos;
};

struct Player {
    Sprite sprite;
    f32 width, height;
    
    Vector2f position;
    Vector2f velocity;
    Vector2f acceleration;
    float max_speed;
};

enum {
    EMPTY_TILE,
    GROUND_TILE,
    START_TILE,
    END_TILE,
    COIN_TILE,
};

struct Engine {
    Input input;
    Audio audio;
    Window window;
    
    bool running;
    f32 delta_time;
    f32 target_time;
};

static Tile_Map load_tile_map(const char *filename) {
    Tile_Map result = {};
    
    Sprite level_design = load_bitmap(filename);
    result.height = level_design.height;
    result.width = level_design.width;
    result.tile_size = 32;
    result.ground = load_bitmap("assets\\grass.bmp");
    result.end = load_bitmap("assets\\door.bmp");
    result.coin = load_bitmap("assets\\coin.bmp");
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
                        Vector2f(f32(x * result.tile_size), f32(y * result.tile_size + 13));
                }
                
                if ((r == 255) && (g == 0) && (b == 255)) {
                    value = END_TILE;
                }
                
                if ((r == 255) && (g == 255) && (b == 0)) {
                    value = COIN_TILE;
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

struct Collision {
    i32 tile;
    bool on_ground;
};

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
    
    Vector2f old_tile =
        Vector2f(old_pos.x / (f32)tile_map->tile_size, old_pos.y / (f32)tile_map->tile_size);
    Vector2f new_tile =
        Vector2f(new_pos.x / (f32)tile_map->tile_size, new_pos.y / (f32)tile_map->tile_size);
    
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
                
                if ((tile_x >= 0) && (tile_y >= 0)) {
                    i32 tile = tile_map->tiles[tile_y * tile_map->width + tile_x];
                    if ((tile == GROUND_TILE) || (tile == END_TILE)) {
                        // Reken het midden van de tile uit, en de positie van de speler ten
                        // opzichte van dat midden.
                        Vector2f tile_center =
                            Vector2f((f32)tile_x, (f32)tile_y) * (f32)tile_map->tile_size;
                        Vector2f rel_pos = player->position - tile_center;
                        
                        // Verticale muren.
                        if (test_wall(&t_lowest, min_corner.x, min_corner.y, max_corner.y,
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            normal = Vector2f(-1.0f, 0.0f);
                            result.tile = tile;
                        }
                        
                        if (test_wall(&t_lowest, max_corner.x, min_corner.y, max_corner.y,
                                      rel_pos.x, rel_pos.y, delta_pos.x, delta_pos.y)) {
                            normal = Vector2f(1.0f, 0.0f);
                            result.tile = tile;
                        }
                        
                        // Horizontale muren.
                        if (test_wall(&t_lowest, min_corner.y, min_corner.x, max_corner.x,
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            normal = Vector2f(0.0f, -1.0f);
                            result.tile = tile;
                        }
                        if (test_wall(&t_lowest, max_corner.y, min_corner.x, max_corner.x,
                                      rel_pos.y, rel_pos.x, delta_pos.y, delta_pos.x)) {
                            normal = Vector2f(0.0f, 1.0f);
                            result.tile = tile;
                            result.on_ground = true;
                        }
                    }
                }
            }
        }
        
        player->position = player->position + delta_pos * t_lowest;
        player->velocity = player->velocity - normal * dot(player->velocity, normal);
        player->acceleration = player->acceleration - normal * dot(player->acceleration, normal);
        delta_pos = delta_pos - normal * dot(delta_pos, normal);
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
            if (!engine->input.use_gamepad)
                process_key_up(&engine->input, (u32)wparam);
            break;
        }
        
        default: { result = DefWindowProcA(window, msg, wparam, lparam); }
    }
    return result;
}

// Dit is de main functie zoals Windows die gebruikt, dit is nodig om een venster te kunnen openen.
int WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd) {
    // Maak de initiële game state.
    Engine engine = {};
    
    engine.window.width = 640;
    engine.window.height = 360;
    engine.window.stretch_on_resize = true;
    engine.window.resized = false;
    engine.running = true;
    engine.input.use_gamepad = false;
    engine.target_time = 1.0f / 120.0f;
    
    // Hier maken we het venster waarin we vervolgens onze engine in kunnen laten zien.
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
                                  CW_USEDEFAULT, CW_USEDEFAULT, engine.window.width,
                                  engine.window.height, 0, 0, instance, &engine);
    
    // Als we hier nog steeds geen venster hebben, dan is er iets mis en stoppen we het programma.
    if (!window) {
        OutputDebugStringA("Failed to create window handle!");
        return 1;
    }
    ShowWindow(window, SW_SHOWDEFAULT);
    
    engine.window.device_context = GetDC(window);
    resize_buffer(&engine.window);
    
    // Audio.
    initialize_audio(&engine.audio);
    Sound jump_sound = load_sound(&engine.audio, "assets\\jump.wav");
    Sound coin_sound = load_sound(&engine.audio, "assets\\coin.wav");
    
    // Laad de plaatjes.
    Sprite background = load_bitmap("assets\\background.bmp");
    
    // Laad de levels.
    // TODO(Kay Verbruggen): Laad alle levels uit een mapje met FindFirstFile en FindNextFile.
#define NUM_LEVELS 3
    i32 level = 0;
    i32 coin_count = 0;
    Tile_Map tile_maps[NUM_LEVELS];
    tile_maps[0] = load_tile_map("levels\\1.bmp");
    tile_maps[1] = load_tile_map("levels\\2.bmp");
    tile_maps[2] = load_tile_map("levels\\3.bmp");
    
    // Maak de speler aan.
    Sprite player_right = load_bitmap("assets\\player_right.bmp");
    Sprite player_left = load_bitmap("assets\\player_left.bmp");
    
    Player player = {};
    player.position = tile_maps[level].start_pos;
    player.sprite = player_right;
    player.max_speed = 250.0f;
    player.width = (f32)player_left.width;
    player.height = (f32)player_left.height;
    
    Vector2f camera = Vector2f();
    Collision col = {};
    
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    
    LARGE_INTEGER start_count, end_count;
    QueryPerformanceCounter(&start_count);
    
#if PROFILE
    i64 start_cycles = __rdtsc();
#endif
    
    MSG msg;
    while (engine.running) {
        // Kijk of er nog berichten zijn van Windows, zoja dan moeten we deze eerst afhandelen.
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // Controller input.
        if (engine.input.use_gamepad) {
            process_gamepad_input(&engine.input);
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
        if (engine.window.resized) {
            RECT window_rect;
            GetWindowRect(window, &window_rect);
            engine.window.width = window_rect.right - window_rect.left;
            engine.window.height = window_rect.bottom - window_rect.top;
            
            if (!engine.window.stretch_on_resize) {
                resize_buffer(&engine.window);
            }
        }
        
        static f32 gravity = 500.0f;
        
        // Zwaartekracht en de horizontale beweging door de speler.
        player.acceleration.x = engine.input.movement * 1000;
        player.acceleration.y = -gravity;
        
        // Spring systeem: https://www.youtube.com/watch?v=7KiK0Aqtmzc
        if (engine.input.jump) {
            engine.input.jump = false;
            
            if (col.on_ground) {
                player.acceleration.y = 50000.0f;
                play_sound(&jump_sound);
            }
        }
        
        if (player.velocity.y > 0.0f) {
            player.acceleration.y -= gravity * 2.0f;
        } else if (player.velocity.y < 0.0f && !engine.input.space) {
            player.acceleration.y -= gravity;
        }
        
        if (player.acceleration.x > 0.0f) {
            player.sprite = player_right;
        } else if (player.acceleration.x < 0.0f) {
            player.sprite = player_left;
        }
        
        // Wrijving
        player.acceleration.x -= player.velocity.x * 5.0f;
        
        // Zorg dat we niet boven de maximale snelheid gaan.
        if (player.velocity.x > player.max_speed) {
            player.velocity.x = player.max_speed;
        } else if (player.velocity.x < -player.max_speed) {
            player.velocity.x = -player.max_speed;
        }
        
        Tile_Map current_tile_map = tile_maps[level];
        col = update_player_position(&current_tile_map, &player, engine.delta_time);
        camera.x = player.position.x - (f32)engine.window.buffer.width / 2.0f;
        
        // Ga naar het volgende level als we het level hebben gehaald.
        // TODO(Kay Verbruggen): Als we het level halen en menu of knop er tussen hebben om verder
        // te gaan, blijft de delta tijd voorlopig oplopen, misschien moeten we hier een oplossing
        // voor bedenken. Het kan ook zijn dat het probleem sowieso al niet meer bestaat als we een
        // in-engine menu hebben.
        if (col.tile == END_TILE) {
            if (level < NUM_LEVELS - 1) {
                level++;
                player.velocity = Vector2f();
                player.position = tile_maps[level].start_pos;
            } else {
                MessageBoxA(0, "Je hebt de game uitgespeeld!", "Sucess", MB_OK);
                engine.running = false;
            }
        }
        
        i32 player_tile_pos =
            (i32)(player.position.y / current_tile_map.tile_size) * current_tile_map.width +
            (i32)(player.position.x / current_tile_map.tile_size);
        if ((player_tile_pos <= current_tile_map.width * current_tile_map.height) &&
            (player_tile_pos > 0)) {
            if (current_tile_map.tiles[player_tile_pos] == COIN_TILE) {
                play_sound(&coin_sound);
                coin_count++;
                char buffer[256];
                StringCbPrintfA(buffer, 256, "Coins: %d\n", coin_count);
                OutputDebugStringA(buffer);
                current_tile_map.tiles[player_tile_pos] = 0;
            }
        }
        
        draw_sprite(&engine.window, camera, &background);
        
        // De tilemap op het scherm zetten.
        for (i32 y = 0; y < current_tile_map.height; y++) {
            for (i32 x = 0; x < current_tile_map.width; x++) {
                i32 tile = current_tile_map.tiles[y * tile_maps[level].width + x];
                if (tile == GROUND_TILE) {
                    draw_sprite(&engine.window, camera, &current_tile_map.ground,
                                Vector2f(f32(x * current_tile_map.tile_size),
                                         f32(y * current_tile_map.tile_size)));
                } else if (tile == END_TILE) {
                    // TODO: Fix hardcoden van de deur offset op de y-as.
                    draw_sprite(&engine.window, camera, &current_tile_map.end,
                                Vector2f(f32(x * current_tile_map.tile_size),
                                         f32(y * current_tile_map.tile_size + 20)));
                } else if (tile == COIN_TILE) {
                    draw_sprite(&engine.window, camera, &current_tile_map.coin,
                                Vector2f(f32(x * current_tile_map.tile_size),
                                         f32(y * current_tile_map.tile_size)));
                }
            }
        }
        
        draw_sprite(&engine.window, camera, &player.sprite, player.position);
        update_window(&engine.window);
        
        QueryPerformanceCounter(&end_count);
        i64 delta_counter = end_count.QuadPart - start_count.QuadPart;
        engine.delta_time = (f32)(delta_counter) / (f32)frequency.QuadPart;
        
        // Profile performance hier, de sleep hoort niet bij de daadwerkelijke performance.
#if PROFILE
        i64 end_cycles = __rdtsc();
        i64 fps = frequency.QuadPart / delta_counter;
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
        
        start_count = end_count;
    }
    
    ReleaseDC(window, engine.window.device_context);
    close_audio(&engine.audio);
    return 0;
}