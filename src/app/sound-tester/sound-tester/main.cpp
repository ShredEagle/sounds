#include <sounds/SoundManager.h>

#include "spdlog/sinks/stdout_color_sinks.h"

#include <chrono>
#include <thread>
#include <spdlog/common.h>

int main()
{
    spdlog::stdout_color_mt("sounds");
    spdlog::get("sounds")->set_level(spdlog::level::info);
    ad::sounds::SoundManager manager;

    std::shared_ptr<ad::sounds::OggSoundData> test = ad::sounds::CreateStreamedOggData("test.ogg");
    manager.storeDataInLoadedSound(test);
    std::shared_ptr<ad::sounds::OggSoundData> ahouaismono = ad::sounds::CreateData("ahouaismono.ogg");
    manager.storeDataInLoadedSound(ahouaismono);
    std::shared_ptr<ad::sounds::OggSoundData> ahouais = ad::sounds::CreateStreamedOggData("ahouais.ogg");
    manager.storeDataInLoadedSound(ahouais);
    ad::sounds::CueHandle handle = manager.createSoundCue({
            ahouais->soundId,
            test->soundId
            });
    for (int i = 0; i < 3; i++)
    {
        manager.playSound(handle);
    }
    while(true)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        //manager.monitor();
    }
}
