#pragma once

#include <memory>
#include <resource/ResourceManager.h>

#include <platform/Filesystem.h>
#include <math/Vector.h>
#include "handy/StringId.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <vorbis/vorbisfile.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ad {
namespace sounds {

struct OggSoundData
{
    handy::StringId soundId;
    std::vector<ALuint> buffers;
    int channels;
    long sampleRate;
    ALenum format;
    std::uint8_t bitsPerSample;
    OggVorbis_File oggFile;
    bool streamedData = false;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> position = math::Position<3, float>::Zero();
    math::Vec<3, float> velocity = math::Vec<3, float>::Zero();
    ALboolean looping = AL_FALSE;
};

//Should always return by value
OggSoundData loadOggFileFromPath(const filesystem::path & path, bool streamed);
OggSoundData loadOggFile(std::istream & aInputStream, handy::StringId aSoundId, bool streamed);

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
        ALuint playSound(handy::StringId & aSoundId, SoundOption aOptions = {});
        void modifySound(ALuint aSource, SoundOption aOptions);
        bool stopSound(ALuint aSource);
        bool pauseSound(ALuint aSource);
        ALint getSourceState(ALuint aSource);
        void deleteSources(std::vector<ALuint> aSourcesToDelete);

    private:
        std::unordered_map<handy::StringId, OggSoundData> mLoadedSoundList;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
};

} // namespace grapito
} // namespace ad
