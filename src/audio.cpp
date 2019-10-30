struct Game_Audio {
    IXAudio2 *engine;
    IXAudio2MasteringVoice *master_voice;
    IXAudio2SourceVoice *source;
};

static void initialize_audio(Game_Audio *audio) {
    XAudio2Create(&audio->engine, 0, XAUDIO2_DEFAULT_PROCESSOR);

    audio->engine->CreateMasteringVoice(&audio->master_voice);

    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = 8000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
    format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;
    format.cbSize = 0;

    audio->engine->CreateSourceVoice(&audio->source, &format);

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes;
    buffer.pAudioData;
    buffer.PlayBegin = 0;
    buffer.PlayLength = 0;
    buffer.LoopBegin = 0;
    buffer.LoopLength = 0;
    buffer.LoopCount = 5;

    audio->source->SubmitSourceBuffer(&buffer);
    audio->source->Start();
}