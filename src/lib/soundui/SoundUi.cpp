#include "SoundUi.h"

#include <imgui.h>
#include <implot.h>

namespace ad {
namespace sounds {

constexpr int SOURCE_RECT_SIZE = 20;
constexpr int MAX_SAMPLE_DRAWN = 10000;

void DisplaySoundUi(const SoundManagerInfo & managerInfo)
{
    static bool newSelection = false;
    ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_Once);
    ImGui::Begin("Sound manager info");
    if (ImGui::BeginTabBar("Sound manager info")) {
        if (ImGui::BeginTabItem("Playing resources")) {
            ImDrawList * drawList = ImGui::GetWindowDrawList();
            ImGui::Text("Sources");
            ImGui::Separator();
            ImGui::Spacing();
            static ALuint hovered = -1;

            // Show Sources
            ImGui::BeginGroup();
            for (std::size_t i = 0; i < managerInfo.sources.size(); i++) {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                int size = SOURCE_RECT_SIZE;
                int x = p.x;
                int y = p.y;

                char popupName[32];

                if (std::find(managerInfo.freeSources.begin(),
                              managerInfo.freeSources.end(), i)
                    == managerInfo.freeSources.end()) {
                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + size, y + size),
                                            ImColor(1.f, 1.f, 0.f, 1.f));
                } else {
                    drawList->AddRect(ImVec2(x, y), ImVec2(x + size, y + size),
                                      ImColor(1.f, 1.f, 0.f, 1.f));
                }

                //ImGui::SetCursorScreenPos(position);
                if(ImGui::Button(popupName, ImVec2{(float)size, (float)size}))
                {
                    hovered = managerInfo.sources[i];
                }
                ImGui::SameLine();
            }
            ImGui::EndGroup();
            ImGui::Text("hovered: %d", hovered);

            ImGui::BeginGroup();
            if (hovered != -1)
            {
                const auto & cueIt = std::find_if(managerInfo.playingCues.begin(), managerInfo.playingCues.end(),[](auto & cue){
                    if (cue.second != nullptr)
                    {
                        return cue.second->source == hovered;
                    }

                    return false;
                });
                if (cueIt != managerInfo.playingCues.end())
                {
                    const ad::sounds::PlayingSoundCue & cue = *cueIt->second;
                    const ad::sounds::PlayingSound * playingSound = cue.getPlayingSound().get();
                    const ad::sounds::PlayingSound * waitingSound = cue.getWaitingSound().get();

                    ImGui::Spacing();
                    ImGui::Text("Playing cue info");
                    ImGui::Separator();
                    ImGui::Text("Category: %d", cue.category);
                    ImGui::Text("Gain: %f", cue.option.gain);
                    ImGui::Separator();
                    ImGui::Text("Currently playing sound: %s", revertStringId(playingSound->soundData->soundId).c_str());
                    ImGui::Text("Currently waiting sound: %s", revertStringId(waitingSound->soundData->soundId).c_str());
                    ImGui::Spacing();

                    ImGui::Text("Sound list");
                    ImGui::Separator();

                    for (const std::shared_ptr<ad::sounds::PlayingSound> & sound : cue.sounds)
                    {
                        if (ImGui::CollapsingHeader(revertStringId(sound->soundData->soundId).c_str(), ImGuiTreeNodeFlags_None))
                        {
                            ImGui::Text("Loops %d", sound->loops);
                            ImGui::Text("buffers %zu", sound->buffers.size());
                            ImGui::Text("stagedBuffers %zu", sound->stagedBuffers.size());
                            ImGui::Text("freeBuffers %zu", sound->freeBuffers.size());
                        }
                    }
                }
            }
            ImGui::EndGroup();

            // End Show sources
            ImGui::EndTabItem();
        }
        // Show Data
        if (ImGui::BeginTabItem("Loaded sound data")) {
            ImGui::BeginChild("sound list", ImVec2(150.f, 0.f), true);
            static ad::handy::StringId selectedSound{
                managerInfo.loadedSounds.begin()->first};
            for (auto [stringId, sound] : managerInfo.loadedSounds) {
                char label[64];
                sprintf(label, "%s", ad::handy::revertStringId(stringId).c_str());
                if (ImGui::Selectable(label, selectedSound == stringId)) {
                    if (selectedSound != stringId)
                    {
                        newSelection = true;
                    }
                    selectedSound = stringId;
                }
            }
            ImGui::EndChild();
            ImGui::SameLine();

            std::shared_ptr<ad::sounds::OggSoundData> sound =
                managerInfo.loadedSounds.at(selectedSound);

            ImGui::BeginChild("sound data view");
            ImGui::Text("Sound info");
            ImGui::Separator();
            ImGui::Text("sample rate: %d", sound->vorbisInfo.sample_rate);
            ImGui::Text("channels: %d", sound->vorbisInfo.channels);
            ImGui::Spacing();

            ImGui::Text("Stream info");
            ImGui::Separator();
            ImGui::Text("length read: %lu", sound->lengthRead);
            ImGui::Text("Used data size: %lu", sound->usedData);
            ImGui::Text("Is fully read: %d", sound->fullyRead);
            ImGui::Spacing();

            ImGui::Text("Raw data info");
            ImGui::Separator();
            ImGui::Text("length decoded: %lu", sound->lengthDecoded);
            ImGui::Text("Is fully decoded: %d", sound->fullyDecoded);
            ImGui::Spacing();

            ImGui::Text("Raw data info");
            ImGui::Separator();
            if (sound->decodedData.size() && ImPlot::BeginPlot("Decoded data", ImVec2(-1, 0),
                                  ImPlotFlags_CanvasOnly)) {
                if (!sound->streamedData)
                {
                    ImPlot::SetupAxes(
                            NULL, NULL,
                            newSelection ? ImPlotAxisFlags_AutoFit : 0 | (ImPlotAxisFlags_NoDecorations ^ ImPlotAxisFlags_NoGridLines),
                            ImPlotAxisFlags_AutoFit | (ImPlotAxisFlags_NoDecorations ^ ImPlotAxisFlags_NoGridLines));
                    ImPlot::PlotLine("", sound->decodedData.data(),
                                     sound->decodedData.size());
                    newSelection = false;
                }
                else
                {
                    ImPlot::SetupAxes(
                            NULL, NULL, ImPlotAxisFlags_NoDecorations ^ ImPlotAxisFlags_NoGridLines,  ImPlotAxisFlags_NoDecorations ^ ImPlotAxisFlags_NoGridLines);

                    int sampleDrawn = sound->decodedData.size();
                    int realSampleDrawn = std::min((std::size_t)MAX_SAMPLE_DRAWN, sound->decodedData.size());
                    sampleDrawn = std::max(0, sampleDrawn - (sampleDrawn % realSampleDrawn));
                    int sampleStride = sampleDrawn / realSampleDrawn;

                    float drawnSampleToTotalReadSize = (float)sound->usedData / (float)sound->lengthRead;
                    float plotLengthOfReadData = (float)realSampleDrawn / drawnSampleToTotalReadSize;

                    ImPlot::SetupAxesLimits(0.f, plotLengthOfReadData, -1.f, 1.f, ImPlotCond_Always);

                    ImVec2 rmin = ImPlot::PlotToPixels(ImPlotPoint(0.f, -1.f));
                    ImVec2 rmax = ImPlot::PlotToPixels(ImPlotPoint(plotLengthOfReadData, 1.f));
                    ImPlot::PushPlotClipRect();
                    ImPlot::GetPlotDrawList()->AddRectFilled(rmin, rmax, ImColor(1.f, 0.6f, 0.1f, 0.2f));
                    ImPlot::GetPlotDrawList()->AddRect(rmin, rmax, ImColor(1.f, 0.6f, 0.1f, 0.8f));
                    ImPlot::PopPlotClipRect();

                    ImPlot::PlotLine("",
                            sound->decodedData.data(),
                            realSampleDrawn, 1, 0, 0, sizeof(float) * sampleStride);


                    newSelection = false;
                    
                }
                ImPlot::EndPlot();
            }
            ImGui::EndChild();

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

}
}
