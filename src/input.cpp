struct Input {
    float movement;
    bool space;
    bool jump;
    bool use_gamepad;
    bool next;
};

// NOTE(Kay Verbruggen): += bij key_down en key_up, om te voorkomen dat je stil staat,
// bijvoorbeeld als je D loslaat maar A nog in hebt gehouden.
static void process_key_down(Input *input, u32 key) {
    if (key == VK_LEFT || key == 'A') {
        input->movement += -1.0f;
    }
    
    if (key == VK_RIGHT || key == 'D') {
        input->movement += 1.0f;
    }
    
    if (key == VK_SPACE || key == 'W' || key == VK_UP) {
        input->space = true;
        input->jump = true;
    }
}

static void process_key_up(Input *input, u32 key) {
    if (key == VK_LEFT || key == 'A') {
        input->movement += 1.0f;
    }
    
    if (key == VK_RIGHT || key == 'D') {
        input->movement += -1.0f;
    }
    
    if (key == VK_SPACE || key == 'W' || key == VK_UP) {
        input->space = false;
    }
}

static void process_gamepad_input(Input *input) {
    // Loop door alle mogelijke controllers, er kunnen er vier zijn aangesloten.
    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
        XINPUT_STATE controller_state = {};
        
        // Als dit lukt dan is de controller verbonden.
        if (XInputGetState(i, &controller_state) == ERROR_SUCCESS) {
            // Knoppen.
            if (!input->space && (controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_A)) {
                input->jump = true;
            }
            
            input->space = controller_state.Gamepad.wButtons & XINPUT_GAMEPAD_A;
            
            input->movement = 0.0f;
            
            // NOTE(Kay Verbruggen): Uitleg deadzone.
            // Check de stickjes van de controller. Hiervoor moet je gebruik maken van een
            // deadzone. Dat wil zeggen dat pas wanneer de stickjes meer dan een bepaalde
            // hoeveelheid zijn bewogen, je ook daadwerkelijk iets moet doen. Dit komt doordat
            // de stickjes anders te gevoelig zijn en misschien als input herkennen als je met
            // de controller rammelt en hierdoor de stickjes bewegen.
            if (controller_state.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement = (f32)controller_state.Gamepad.sThumbLX / 32767.0f;
            }
            if (controller_state.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input->movement = (f32)controller_state.Gamepad.sThumbLX / 32767.0f;
            }
            
        } else {
            // TODO(Kay Verbruggen): Moeten we de speler een waarschuwing geven als controllers niet meer
            // verbonden zijn?
        }
    }
}