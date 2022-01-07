#pragma once

#include <memory>
#include <resource/ResourceManager.h>
#include <platform/Filesystem.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <vorbis/vorbisfile.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ad {
namespace grapito {

struct OggSoundData
{
    StringId soundId;
    std::vector<ALuint> buffers;
    int channels;
    long sampleRate;
    ALenum format;
    std::uint8_t bitsPerSample;
    OggVorbis_File oggFile;
    bool streamedData = false;
    Position3 position = Position3::Zero();
    Vec3 velocity = Vec3::Zero();
    float gain = 1.f;
};

//Should always return by value
OggSoundData loadOggFileFromPath(const filesystem::path & path, bool streamed);
OggSoundData loadOggFile(std::istream & aInputStream, StringId aSoundId, bool streamed);

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
        ALuint playSound(StringId & aSoundId, ALboolean looping = AL_FALSE);
        bool stopSound(ALuint aSource);
        bool pauseSound(ALuint aSource);

    private:
        std::unordered_map<StringId, OggSoundData> mLoadedSoundList;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
};

} // namespace grapito
} // namespace ad
