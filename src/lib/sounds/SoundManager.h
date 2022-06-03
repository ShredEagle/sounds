#pragma once

#include "SoundUtilities.h"
#include "stb_vorbis.h"

#include <memory>
#include <resource/ResourceManager.h>

#include <platform/Filesystem.h>
#include <math/Vector.h>
#include <handy/StringId.h>

#include <AL/al.h>
#include <AL/alc.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ad {
namespace sounds {

constexpr std::array<ALenum, 3> SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO16, AL_FORMAT_STEREO16};

struct OggSoundData
{
    handy::StringId soundId;

    std::istream & dataStream;
    std::streamsize lengthRead;
    std::vector<char> undecodedReadData;
    bool fullyRead;

    stb_vorbis * vorbisData;
    stb_vorbis_info vorbisInfo;
    int lengthDecoded;
    bool fullyDecoded;

    bool streamedData = false;
    bool cacheData = false;

    int sampleRate;
    std::vector<float> decodedLeftData;
    std::vector<float> decodedRightData;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> leftPosition = math::Position<3, float>::Zero();
    math::Vec<3, float> leftVelocity = math::Vec<3, float>::Zero();
    math::Position<3, float> rightPosition = math::Position<3, float>::Zero();
    math::Vec<3, float> rightVelocity = math::Vec<3, float>::Zero();

    ALboolean looping = AL_FALSE;
};

struct PlayingSound
{
    // Order of channels in ogg vorbis is left right
    // 3 buffers: processed buffer, queued buffer and playing buffer
    explicit PlayingSound(const OggSoundData & aSoundData):
        soundData{aSoundData}
    {
        std::array<ALuint, 6> buffers;  
        alCall(alGenBuffers, 6, buffers.data());

        std::move(buffers.data(), buffers.data() + 3, leftBuffers.data());
        std::move(buffers.data() + 3, buffers.data(), rightBuffers.data());

    }

    const OggSoundData & soundData;
    std::array<ALuint, 3> leftBuffers;
    std::size_t leftPlayingBufferIndex;
    std::array<ALuint, 3> rightBuffers;
    std::size_t rightPlayingBufferIndex;

    SoundOption leftSoundOption;
    SoundOption rightSoundOption;
    size_t positionInData = 0;
    //This means that there is no more sound data to load into buffers
    //and the sound will be discarded once all its buffers are played
    bool isStale = false;
};

struct SoundQueue
{
    ALuint source;
    std::vector<PlayingSound> sounds;
    int currentSoundIndex = -1;
};

//Should always return by value
OggSoundData CreateOggData(const filesystem::path & path);
OggSoundData CreateOggData(std::istream & aInputStream, handy::StringId aSoundId);

//Streamed version of ogg data
OggSoundData CreateStreamedOggData(const filesystem::path & path);
OggSoundData CreateStreamedOggData(std::istream & aInputStream, handy::StringId aSoundId);

void decodeSoundData(OggSoundData & aData);

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
        std::size_t playSound(std::vector<std::tuple<handy::StringId, SoundOption>> soundQueue);
        void modifySound(ALuint aSource, SoundOption aOptions);
        bool stopSound(ALuint aSource);
        bool stopSound(handy::StringId aId);
        bool pauseSound(ALuint aSource);
        ALint getSourceState(ALuint aSource);
        void deleteSources(std::vector<ALuint> aSourcesToDelete);
        void update();

    private:
        std::unordered_map<handy::StringId, OggSoundData> mLoadedSoundList;
        std::vector<SoundQueue> mStoreQueues;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
        std::vector<ALuint> freebuffer;
};

} // namespace grapito
} // namespace ad
