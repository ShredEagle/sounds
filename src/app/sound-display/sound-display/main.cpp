
#include <imguiui/ImguiUi.h>
#include <sounds/SoundManager.h>
#include <soundui/SoundUi.h>

#include <handy/StringId_Interning.h>
#include <implot.h>
#include <graphics/ApplicationGlfw.h>
#include <math/Vector.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <algorithm>


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
    ad::sounds::SoundManager manager{
        {SoundCategory_SFX, SoundCategory_Music, SoundCategory_Dialog}};
    constexpr ad::math::Size<2, int> gWindowSize{1280, 1024};
    ad::graphics::ApplicationGlfw application("glTF Viewer", gWindowSize);

    ad::handy::StringId ahouaismono = manager.createData("ahouaismono.ogg");
    ad::handy::StringId ahouaismonocourt = manager.createData("ahouaismonocourt.ogg");
    ad::sounds::Handle<ad::sounds::SoundCue> music = manager.createSoundCue(
        {
        {ahouaismonocourt, {30}},
        {ahouaismono, {0}},
        }, SoundCategory_Music, ad::sounds::HIGHEST_PRIORITY);
    ad::sounds::Handle<ad::sounds::PlayingSoundCue> testCueHandle =
        manager.playSound(music);

    ad::imguiui::ImguiUi a = ad::imguiui::ImguiUi{application};
    ImPlot::CreateContext();

    while (application.nextFrame()) {

        manager.update();

        const ad::sounds::SoundManagerInfo managerInfo = manager.getInfo();

        a.newFrame();

        ad::sounds::DisplaySoundUi(managerInfo);
        //ImGui::ShowDemoWindow();
        //ImPlot::ShowDemoWindow();
        a.render();
    }

    ImPlot::DestroyContext();
}
