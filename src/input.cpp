struct Game_Input {
    Vector2f movement;
    bool use_gamepad;
};

static void process_key_down(Game_Input *input, u32 key) {
    if (key == VK_LEFT || key == 'A') {
        input->movement.x = -1.0f;
    }
    if (key == VK_RIGHT || key == 'D') {
        input->movement.x = 1.0f;
    }
    if (key == VK_UP || key == 'W') {
        input->movement.y = 1.0f;
    }
    if (key == VK_DOWN || key == 'S') {
        input->movement.y = -1.0f;
    }
}

static void process_key_up(Game_Input *input, u32 key) {
    if (key == VK_LEFT || key == 'A') {
        input->movement.x = 0.0f;
    }
    if (key == VK_RIGHT || key == 'D') {
        input->movement.x = 0.0f;
    }
    if (key == VK_UP || key == 'W') {
        input->movement.y = 0.0f;
    }
    if (key == VK_DOWN || key == 'S') {
        input->movement.y = 0.0f;
    }
}

static void process_gamepad_input(Game_Input *input) {
    // Loop door alle mogelijke controllers, er kunnen er vier zijn aangesloten.
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE controller_state = {};

        // Als dit lukt dan is de controller verbonden.
        if (XInputGetState(i, &controller_state) == ERROR_SUCCESS) {
            input->movement = Vector2f();

            // NOTE: Uitleg deadzone.
            // Check de stickjes van de controller. Hiervoor moet je gebruik maken van een
            // deadzone. Dat wil zeggen dat pas wanneer de stickjes meer dan een bepaalde
            // hoeveelheid zijn bewogen, je ook daadwerkelijk iets moet doen. Dit komt doordat
            // de stickjes anders te gevoelig zijn en misschien als input herkennen als je met
            // de controller rammelt en hierdoor de stickjes bewegen.
            if (controller_state.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement.x = (f32)controller_state.Gamepad.sThumbLX / 32767.0f;
            }
            if (controller_state.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement.x = (f32)controller_state.Gamepad.sThumbLX / 32767.0f;
            }

            if (controller_state.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement.y = (f32)controller_state.Gamepad.sThumbLY / 32767.0f;
            }
            if (controller_state.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement.y = (f32)controller_state.Gamepad.sThumbLY / 32767.0f;
            }
        } else {
            // TODO: Moeten we de speler een waarschuwing geven als controllers niet meer
            // verbonden zijn?
        }
    }
}