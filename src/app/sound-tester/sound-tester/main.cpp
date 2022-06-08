#include <sounds/SoundManager.h>

#include "spdlog/sinks/stdout_color_sinks.h"

#include <iostream>

int main(int argc, char** argv)
{
    spdlog::stdout_color_mt("sounds");
    spdlog::get("sounds")->set_level(spdlog::level::warn);
    ad::sounds::SoundManager manager;

    //auto data = ad::sounds::CreateOggData("test.ogg");
    //manager.storeDataInLoadedSound(data);
    //manager.playSound({{data.soundId, {.gain = 2.f}}});

    auto data = ad::sounds::CreateStreamedOggData("test.ogg");
    manager.storeDataInLoadedSound(data);
    manager.playSound({{data.soundId, {.gain = 1.f, .position = {2.f, 0.f, 0.f}}}});
    manager.playSound({{data.soundId, {.gain = 1.f, .position = {2.f, 2.f, 0.f}}}});
    manager.playSound({{data.soundId, {.gain = 1.f}}});
    manager.playSound({{data.soundId, {.gain = 1.f}}});
    //manager.playSound({{data.soundId, {.gain = 1.f, .position = {2.f, 0.f, 0.f}}}});
    //manager.playSound({{data.soundId, {.gain = 1.f, .position = {2.f, 2.f, 0.f}}}});
    //manager.playSound({{data.soundId, {.gain = 1.f}}});
    //manager.playSound({{data.soundId, {.gain = 1.f}}});
    while(true)
    {
        manager.update();
        manager.monitor();
    }
    stb_vorbis_close(data.vorbisData);
}
