struct Button {
    i32 half_width;
    i32 half_height;
    Sprite sprite;
    Vector2f position;
    bool is_pressed;
    bool is_hovered;
    Sound select_sound;
};

void update_button(Engine *engine, Button *button) {
    button->is_pressed = false;

    POINT cursor;
    RECT window_dim;
    GetWindowRect(engine->window.handle, &window_dim);
    GetCursorPos(&cursor);
    cursor.y = window_dim.bottom - cursor.y;
    cursor.x = cursor.x - window_dim.left;

    HCURSOR normal_cursor = LoadCursorA(0, IDC_ARROW);
    HCURSOR click_cursor = LoadCursorA(0, IDC_HAND);

    if ((cursor.x > button->position.x - button->half_width) &&
        (cursor.x < button->position.x + button->half_width) &&
        (cursor.y > button->position.y - button->half_height) &&
        (cursor.y < button->position.y + button->half_height)) {

        button->is_hovered = true;
        SetCursor(click_cursor);

        if (engine->input.click) {
            engine->input.click = false;
            button->is_pressed = true;

            play_sound(&button->select_sound);
            SetCursor(normal_cursor);
        }
    } else if (button->is_hovered) {
        button->is_hovered = false;
        SetCursor(normal_cursor);
    }

    draw_sprite(&engine->window, Vector2f(), &button->sprite, button->position);
}