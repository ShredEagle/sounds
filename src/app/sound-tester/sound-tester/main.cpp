#include <sounds/SoundManager.h>

#include <math/Angle.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <thread>
#include <spdlog/common.h>

constexpr ad::math::Radian<float> OMEGA = .25f * ad::math::pi<ad::math::Radian<float>>;
constexpr float R = 1.f;

int main()
{
    spdlog::stdout_color_mt("sounds");
    spdlog::get("sounds")->set_level(spdlog::level::info);
    ad::sounds::SoundManager manager;

    std::shared_ptr<ad::sounds::OggSoundData> test = ad::sounds::CreateStreamedOggData("test.ogg");
    manager.storeDataInLoadedSound(test);
    std::shared_ptr<ad::sounds::OggSoundData> testmono = ad::sounds::CreateStreamedOggData("testmono.ogg");
    manager.storeDataInLoadedSound(testmono);
    std::shared_ptr<ad::sounds::OggSoundData> ahouaismono = ad::sounds::CreateData("ahouaismono.ogg");
    manager.storeDataInLoadedSound(ahouaismono);
    std::shared_ptr<ad::sounds::OggSoundData> ahouais = ad::sounds::CreateStreamedOggData("ahouais.ogg");
    manager.storeDataInLoadedSound(ahouais);
    ad::sounds::CueHandle handle = manager.createSoundCue({
            testmono->soundId
            });

    std::shared_ptr<ad::sounds::PlayingSoundCue> testCue = manager.playSound(handle);

    float t = 0;
    while(true)
    {
        testCue->option.position.x() = R * cosf(OMEGA.value() * t);
        testCue->option.position.y() = R * sinf(OMEGA.value() * t);
        testCue->option.velocity.x() = R * -sinf(OMEGA.value() * t);
        testCue->option.velocity.y() = R * cosf(OMEGA.value() * t);
        spdlog::get("sounds")->trace("t {}, omega {}, Position {}, {}, {}",
                t,
                OMEGA.value(),
                testCue->option.position.x(),
                testCue->option.position.y(),
                testCue->option.position.z()
                );
        t += 0.016f;
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        //manager.monitor();
    }
}
