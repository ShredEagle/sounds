#pragma once

#include <memory>
#include <resource/ResourceManager.h>

#include <platform/Filesystem.h>
#include <math/Vector.h>
#include <handy/StringId.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <stb.h>
#include <stb_vorbis.c>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ad {
namespace sounds {

struct OggSoundData
{
    handy::StringId soundId;

    std::istream & dataStream;
    std::streamsize lengthRead;
    long sampleRate;
    bool fullyLoaded;
    bool streamedData = false;

    std::uint8_t bitsPerSample;
    std::vector<ALuint> buffersToRead;
    ALenum channels;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> position = math::Position<3, float>::Zero();
    math::Vec<3, float> velocity = math::Vec<3, float>::Zero();
    ALboolean looping = AL_FALSE;
    bool storeInManager;
};

struct SoundQueue
{
    handy::StringId queueId;
    ALuint sourceQueue;
    std::vector<std::unordered_map<handy::StringId, SoundOption>> data;
};

//Should always return by value
OggSoundData CreateOggData(const filesystem::path & path, bool streamed);
OggSoundData CreateOggData(std::istream & aInputStream, handy::StringId aSoundId, bool streamed);

//There is three step to play sound
//First load the file into RAM
//Second load the audio data into audio memory
//Last play the sound
class SoundManager
{
    public:
        SoundManager();
        ~SoundManager();
        void storeDataInLoadedSound(const OggSoundData & aSoundData);
        ALuint playSound(handy::StringId aSoundId, SoundOption aOptions = {});
        void modifySound(ALuint aSource, SoundOption aOptions);
        bool stopSound(ALuint aSource);
        bool stopSound(handy::StringId aId);
        bool pauseSound(ALuint aSource);
        ALint getSourceState(ALuint aSource);
        void deleteSources(std::vector<ALuint> aSourcesToDelete);

    private:
        std::unordered_map<handy::StringId, OggSoundData> mLoadedSoundList;
        std::unordered_map<handy::StringId, ALuint> mStoredSources;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
        std::vector<ALuint> freebuffer;
};

} // namespace grapito
} // namespace ad
