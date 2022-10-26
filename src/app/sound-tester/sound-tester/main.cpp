#include <sounds/SoundManager.h>

#include <handy/StringId.h>
#include <math/Angle.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <chrono>
#include <thread>
#include <spdlog/common.h>

constexpr ad::math::Radian<float> OMEGA = .25f * ad::math::pi<ad::math::Radian<float>>;
constexpr float R = 1.f;
enum SoundCategory
{
    SoundCategory_SFX,
    SoundCategory_Dialog,
    SoundCategory_Music,
};

int main()
{
    spdlog::stdout_color_mt("sounds");
    spdlog::get("sounds")->set_level(spdlog::level::info);
    ad::sounds::SoundManager manager{{SoundCategory_SFX, SoundCategory_Music, SoundCategory_Dialog}};

    /*ad::handy::StringId testId = manager.createStreamedOggData("test.ogg");*/
    /*ad::handy::StringId testmono =*/ manager.createStreamedOggData("testmono.ogg");
    ad::handy::StringId ahouaismono = manager.createData("ahouaismono.ogg");
    ad::handy::StringId ahouaismonocourt = manager.createData("ahouaismonocourt.ogg");
    /*ad::handy::StringId ahouais =*/ manager.createStreamedOggData("ahouais.ogg");
    /* ad::sounds::Handle<ad::sounds::SoundCue> testHandle = manager.createSoundCue( */
    /*         {{ahouaismono, {3}}}, */
    /*         SoundCategory_Music, */
    /*         1 */
    /*         ); */
    /* ad::sounds::Handle<ad::sounds::SoundCue> testHandlea = manager.createSoundCue( */
    /*         {{ahouaismono, {0}}, {testmono, {0}}}, */
    /*         SoundCategory_Music, */
    /*         2 */
    /*         ); */

    /* ad::sounds::Handle<ad::sounds::SoundCue> musicHandle = manager.createSoundCue( */
    /*         {{testmono, {0}}}, */
    /*         SoundCategory_Music, */
    /*         ad::sounds::HIGHEST_PRIORITY */
    /*         ); */

    ad::sounds::Handle<ad::sounds::SoundCue> machineGun = manager.createSoundCue(
            {
            {ahouaismonocourt, {1}},
            {ahouaismono, {1}}
            },
            SoundCategory_Music,
            ad::sounds::HIGHEST_PRIORITY,
            ahouaismono
            );

    /* manager.playSound(testHandle); */
    /* manager.update(); */
    /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */
    /* manager.playSound(testHandle); */
    /* manager.update(); */
    /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */
    /* manager.playSound(testHandle); */
    /* manager.update(); */
    /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */
    /* manager.playSound(testHandle); */
    /* manager.update(); */
    /* std::this_thread::sleep_for(std::chrono::milliseconds(100)); */
    ad::sounds::Handle<ad::sounds::PlayingSoundCue> testCueHandle = manager.playSound(machineGun);

    float t = 0;
    bool interrupted = false;
    while(true)
    {
        ad::sounds::PlayingSoundCue * testCue = testCueHandle.toObject();
        if (testCue != nullptr)
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

            if (t > 3.f && !interrupted)
            {
                interrupted = true;
                manager.interruptSound(testCueHandle);
            }
        }
        manager.update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        //manager.monitor();
    }
}
