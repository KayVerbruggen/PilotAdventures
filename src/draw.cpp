struct Offscreen_Buffer {
    BITMAPINFO info;
    void *memory;
    u32 width, height;
    i8 bytes_per_pixel;
    i32 pitch;
};

struct Game_Window {
    u32 width, height;
    bool stretch_on_resize;
    bool resized;
    HDC device_context;
    Offscreen_Buffer buffer;
};

// NOTE: Uitleg pragma pack.
// We gebruiken pragma pack om te voorkomen dat er padding tussen deze variabele komt. Padding
// betekent dat de compiler een paar bytes tussen twee variabele open laat zodat het beter uitkomt
// in het geheugen. We willen dit voorkomen zodat we in een keer de inhoud van het bitmap bestand om
// kunnen zetten naar deze struct. Op het einde moeten we weer pop gebruiken om aan te geven dat
// alles wat daarna komt gewoon weer padding mag gebruiken
#pragma pack(push, 1)
struct Bitmap_Header {
    u16 file_type;
    u32 file_size;
    u16 reserved1;
    u16 reserved2;
    u32 bitmap_offset;

    u32 size;           /* Size of this header in bytes */
    u32 width;          /* Image width in pixels */
    i32 height;         /* Image height in pixels */
    u16 planes;         /* Number of color planes */
    u16 bits_per_pixel; /* Number of bits per pixel */
};
#pragma pack(pop)

struct Sprite {
    u32 *pixels;
    u32 width;
    u32 height;
    u16 bits_per_pixel;
};

static Sprite load_bitmap(const char *filename) {
    Sprite sprite = {};

    HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxA(0, "Kon afbeelding niet laden!", "Bitmap laden", MB_OK);
        return sprite;
    }
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        MessageBoxA(0, "Kon de grootte niet opvragen!", "Bitmap laden", MB_OK);
        return sprite;
    }
    void *memory = VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ReadFile(file, memory, (u32)file_size.QuadPart, 0, 0)) {
        MessageBoxA(0, "Kon afbeelding niet laden!", "Bitmap laden", MB_OK);
        return sprite;
    }

    Bitmap_Header *header = (Bitmap_Header *)memory;

    if (header->bits_per_pixel != 32) {
        MessageBoxA(0, "We ondersteunen alleen bitmaps met 32 bits per pixel!", "Bitmap", MB_OK);
        return sprite;
    }

    sprite.width = header->width;
    sprite.height = header->height;
    sprite.bits_per_pixel = header->bits_per_pixel;
    sprite.pixels = (u32 *)((u8 *)memory + header->bitmap_offset);
    return sprite;
}

static void draw_sprite(Game_Window *window, Sprite *sprite, Vector2f pos = Vector2f(0.0f, 0.0f)) {
    Vector2i min = Vector2i((i32)pos.x, (i32)pos.y);
    Vector2i max = Vector2i((i32)pos.x + sprite->width, (i32)pos.y + sprite->height);

    Vector2i offset = Vector2i();
    if (min.x < 0) {
        offset.x = -min.x;
        min.x = 0;
    }
    if (min.y < 0) {
        offset.y = -min.y;
        min.y = 0;
    }

    if (max.x > (i32)window->buffer.width) {
        max.x = window->buffer.width;
    }
    if (max.y > (i32)window->buffer.height) {
        max.y = window->buffer.height;
    }

    u8 *dest_row = (u8 *)window->buffer.memory + (u32)min.x * window->buffer.bytes_per_pixel +
                   (u32)min.y * window->buffer.pitch;
    u32 *source_row = sprite->pixels;
    source_row += offset.y * sprite->width + offset.x;

    for (i32 y = min.y; y < max.y; y++) {
        // Pak de pixel.
        u32 *dest = (u32 *)dest_row;
        u32 *source = source_row;
        for (i32 x = min.x; x < max.x; x++) {
            // NOTE: Uitleg alpha kanaal.
            // Als het alpha kanaal 0 is, betekent dit dat de pixel transparant hoort te zijn.
            // Daarom slaan we deze pixel over en gaan we door naar de volgende. Om het alpha kanaal
            // te lezen, moeten we de source pixel 24 bits naar rechts schuiven, zodat we de RGB
            // waardes als het waren uit de variabele hebben geschoven. Dan hebben we dus alleen nog
            // maar de alpha waarde.
            // AA RR GG BB (elk kleurkanaal 8 bits) 24 bits naar rechts -> 00 00 00 AA. Dus alleen
            // de alpha waarde blijft over.
            if (*source >> 24 == 0) {
                dest++;
                source++;
            } else {
                *dest++ = *source++;
            }
        }

        // We gaan naar de volgende rij in het geheugen.
        dest_row += window->buffer.pitch;
        source_row += sprite->width;
    }
}

static void resize_buffer(Game_Window *window) {
    // Eerst moeten we het geheugen van de buffer legen als hier al iets in staat.
    if (window->buffer.memory) {
        VirtualFree(window->buffer.memory, 0, MEM_RELEASE);
    }

    // Vul de buffer met de nieuwe informatie, voornamelijk de breedte en hoogte.
    window->buffer.width = window->width;
    window->buffer.height = window->height;

    window->buffer.info.bmiHeader.biSize = sizeof(window->buffer.info.bmiHeader);
    window->buffer.info.bmiHeader.biWidth = window->buffer.width;
    window->buffer.info.bmiHeader.biHeight = window->buffer.height;
    window->buffer.info.bmiHeader.biPlanes = 1;
    window->buffer.info.bmiHeader.biBitCount = 32;
    window->buffer.info.bmiHeader.biCompression = BI_RGB;

    // 4 bytes per pixel aangezien de bitcount ook op 32 bits staat.
    window->buffer.bytes_per_pixel = 4;
    // Dit is de totale buffer grootte.
    i32 bitmap_memory_size =
        window->buffer.bytes_per_pixel * window->buffer.width * window->buffer.height;

    // Alloc het geheugen zodat we het kunnen gaan gebruiken.
    window->buffer.memory =
        VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // Dit is een rij aan pixels, dit kunnen we gebruiken om makelijker naar een bepaalde rij te
    // gaan.
    window->buffer.pitch = window->buffer.width * window->buffer.bytes_per_pixel;
}

static void draw_gradient(Game_Input *input, Game_Window *window) {
    // TODO: Haal dit weg wanneer we bitmaps kunnen renderen.
    static u32 x_offset = 0;
    static u32 y_offset = 0;

    u8 *row = (u8 *)window->buffer.memory;
    for (u32 y = window->buffer.height; y > 0; y--) {
        // Pak de pixel.
        u32 *pixel = (u32 *)row;
        for (u32 x = 0; x < window->buffer.width; x++) {

            // Set the RGB values.
            u8 red = 0;
            u8 green = (u8)(x + x_offset);
            u8 blue = (u8)(y + y_offset);

            // NOTE: Uitleg pixels.
            // Geheugen:    xx RR GG BB
            // Dus de rode en groene pixels moeten iets naar links worden geschoven.
            // Elke kleur is 8 bits, rood moet twee plaatsen naar links, dus 16 bits.
            // Groen moet een plaats naar links dus 8 bits. Kleuren zijn 8 bits omdat ze van
            // 0-255 lopen. Dit past precies in 1 byte/8 bits.
            *pixel++ = (red << 16) | (green << 8) | blue;
        }

        // We gaan naar de volgende rij in het geheugen.
        row += window->buffer.pitch;
    }
}

static void update_window(Game_Window *window) {
    StretchDIBits(window->device_context, 0, 0, window->width, window->height, 0, 0,
                  window->buffer.width, window->buffer.height, window->buffer.memory,
                  &window->buffer.info, DIB_RGB_COLORS, SRCCOPY);
}