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
#include <AL/alext.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ad {
namespace sounds {

constexpr std::array<ALenum, 3> SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr std::array<ALenum, 3> STREAM_SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO_FLOAT32, AL_FORMAT_MONO_FLOAT32};
constexpr int BUFFER_PER_CHANNEL = 3;

struct OggSoundData
{
    handy::StringId soundId;

    std::shared_ptr<std::istream> dataStream;
    std::streamsize usedData = 0;
    std::vector<char> undecodedReadData;
    bool fullyRead;

    stb_vorbis * vorbisData;
    stb_vorbis_info vorbisInfo;
    int lengthDecoded;
    bool fullyDecoded;
    ALenum dataFormat;

    bool streamedData = false;
    bool cacheData = false;

    int sampleRate;
    //Left is 0 and Right is 1
    std::vector<std::vector<float>> decodedData;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> position = math::Position<3, float>::Zero();
    math::Vec<3, float> velocity = math::Vec<3, float>::Zero();

    ALboolean looping = AL_FALSE;
};

struct PlayingSound
{
    // Order of channels in ogg vorbis is left right
    // 3 buffers: processed buffer, queued buffer and playing buffer
    PlayingSound(OggSoundData & aSoundData, SoundOption option):
        soundData{aSoundData},
        soundOption{option}
    {
        buffers.resize(aSoundData.vorbisInfo.channels * BUFFER_PER_CHANNEL);
        alCall(alGenBuffers, aSoundData.vorbisInfo.channels * BUFFER_PER_CHANNEL, buffers.data());

        for (auto buf : buffers)
        {
            freeBuffers.push_back(buf);
        }
    }

    OggSoundData & soundData;
    //Left is first 3 buffers Right is last 3 buffers
    std::list<ALuint> freeBuffers;
    std::vector<ALuint> buffers;


    SoundOption soundOption;
    size_t positionInData = 0;
    //This means that there is no more sound data to load into buffers
    //(i.e. the nextPositionInData we want is passed, or at, the end of the decodedData)
    //and the sound will be discarded once all its buffers are played
    bool isStale = false;
};

struct SoundQueue
{
    std::vector<ALuint> sources;
    std::vector<PlayingSound> sounds;
    int currentSoundIndex = -1;
};

//Should always return by value
OggSoundData CreateOggData(const filesystem::path & path);
OggSoundData CreateOggData(std::shared_ptr<std::istream> aInputStream, handy::StringId aSoundId);

//Streamed version of ogg data
OggSoundData CreateStreamedOggData(const filesystem::path & path);
OggSoundData CreateStreamedOggData(std::shared_ptr<std::istream> aInputStream, handy::StringId aSoundId);
void loadDataIntoBuffers(OggSoundData & aData, std::size_t aPositionInData);

int decodeSoundData(OggSoundData & aData);

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

        void monitor();

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
