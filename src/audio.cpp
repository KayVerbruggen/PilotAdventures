struct Audio {
    IXAudio2 *engine;
    IXAudio2MasteringVoice *master_voice;
};

static void initialize_audio(Audio *audio) {
    if (FAILED(XAudio2Create(&audio->engine, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        OutputDebugStringA("[ERROR]: XAudio 2 engine aanmaken is mislukt!");
        return;
    }
    
    audio->engine->CreateMasteringVoice(&audio->master_voice);
}

static void close_audio(Audio *audio) {
    audio->master_voice->DestroyVoice();
    audio->engine->Release();
}

struct Sound {
    IXAudio2SourceVoice *source;
    XAUDIO2_BUFFER buffer;
};

struct Wave_Header {
    // RIFF
    u8 RIFF[4];
    u32 chunk_size;
    u8 WAVE[4];
    
    // FMT chunk.
    u8 fmt[4];
    u32 subchunk1_size;
    u16 audio_format;
    u16 number_channels;
    u32 samples_per_sec;
    u32 bytes_per_sec;
    u16 block_align;
    u16 bits_per_sample;
    
    // Data chunk.
    u8 subchunk2_id[4];
    u32 subchunk2_size;
};

static Sound load_sound(Audio *audio, const char* filename) {
    Sound sound = {};
    
    HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        MessageBoxA(0, "[ERROR]: Kan geluidsbestand niet laden!", "Audio laden", MB_OK);
        return sound;
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        MessageBoxA(0, "Kon de grootte niet opvragen!", "Audio laden", MB_OK);
        return sound;
    }
    void *memory = VirtualAlloc(0, file_size.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ReadFile(file, memory, (u32)file_size.QuadPart, 0, 0)) {
        MessageBoxA(0, "Kon afbeelding niet laden!", "Audio laden", MB_OK);
        return sound;
    }
    
    Wave_Header *header = (Wave_Header *)memory;
    
    WAVEFORMATEX format = {0};
    format.wFormatTag  = WAVE_FORMAT_PCM;
    format.nChannels = header->number_channels;
    format.nSamplesPerSec = header->samples_per_sec;
    format.nAvgBytesPerSec = header->bytes_per_sec;;
    format.nBlockAlign = header->block_align;
    format.wBitsPerSample = header->bits_per_sample;
    
    XAUDIO2_BUFFER buffer = {0};
    buffer.AudioBytes = header->subchunk2_size;
    buffer.pAudioData = (BYTE *)((u8 *)memory + sizeof(Wave_Header));
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    
    IXAudio2SourceVoice *source;
    if (S_OK != audio->engine->CreateSourceVoice(&source, &format))
        MessageBoxA(0, "[ERROR]: Kan geen source voice maken!", "Audio laden", MB_OK);
    
    sound.source = source;
    sound.buffer = buffer;
    
    return sound;
}

static void play_sound(Sound *sound) {
    if (S_OK != sound->source->SubmitSourceBuffer(&sound->buffer))
        MessageBoxA(0, "[ERROR]: Kan de buffer niet doorgeven!", "Audio laden", MB_OK);
    
    sound->source->Start();
}