#include <sounds/SoundManager.h>

#include "spdlog/sinks/stdout_color_sinks.h"

#include <iostream>

int main(int argc, char** argv)
{
    spdlog::stdout_color_mt("sounds");
    ad::sounds::SoundManager manager;

    //auto data = ad::sounds::CreateOggData("ahouais.ogg");
    //manager.storeDataInLoadedSound(data);
    //manager.playSound(data.soundId, {.gain = 2.f});

    auto data = ad::sounds::CreateStreamedOggData("big.ogg");
    stb_vorbis_close(data.vorbisData);
}
