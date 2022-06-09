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

    std::shared_ptr<ad::sounds::TwoDSoundData> test = ad::sounds::CreateStreamedOggData<2>("test.ogg");
    manager.storeDataInLoadedSound(test);
    std::shared_ptr<ad::sounds::PointSoundData> ahouais = ad::sounds::CreateMonoData("ahouais.ogg");
    manager.storeDataInLoadedSound(ahouais);
    for (int i = 0; i < 1; i++)
    {
        ad::sounds::PointSoundCueHandle handle = manager.createSoundCue<1>({
                {ahouais->soundId, {.gain = 0.4f}}
                });
        manager.playSound(handle);
    }
    while(true)
    {
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        //manager.monitor();
    }
}
