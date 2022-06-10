#pragma once

#include "SoundUtilities.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "stb_vorbis.h"

#include <cassert>
#include <memory>
#include <resource/ResourceManager.h>

#include <platform/Filesystem.h>
#include <math/Vector.h>
#include <handy/Guard.h>
#include <handy/StringId.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <fstream>
#include <iostream>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>

#define TEMPORARY

namespace ad {
namespace sounds {

enum PlayingSoundState
{
    PlayingSoundState_WAITING,
    PlayingSoundState_PLAYING,

    //Sound is completely loaded into a source
    //but not finished playing
    PlayingSoundState_STALE,

    //Sound is finished playing
    PlayingSoundState_FINISHED,
};

enum SoundCueState
{
    SoundCueState_PLAYING,
    SoundCueState_STALE,
    SoundCueState_NOT_PLAYING,
};

constexpr int BUFFER_PER_CHANNEL = 5;

template<typename T>
inline std::vector<T> interleave(T * left, T * right, int size)
{
    std::vector<float> result;

    for (std::size_t i = 0; i < static_cast<std::size_t>(size); i++)
    {
        result.push_back(left[i]);
        result.push_back(right[i]);
    }

    return result;
}

struct CueHandle
{
    std::size_t handle;

    bool operator<(const CueHandle & rHs) const
    {
        return handle < rHs.handle;
    }
};

struct OggSoundData
{
    handy::StringId soundId;

    std::shared_ptr<std::istream> dataStream;
    std::streamsize usedData = 0;
    std::vector<char> undecodedReadData;
    std::size_t lengthRead = 0;
    bool fullyRead;

    ResourceGuard<stb_vorbis *> vorbisData;
    stb_vorbis_info vorbisInfo;
    std::size_t lengthDecoded = 0;
    bool fullyDecoded;
    ALenum dataFormat;

    bool streamedData = false;
    bool cacheData = false;

    unsigned int sampleRate;

    std::vector<float> decodedData;
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
    PlayingSound(const std::shared_ptr<OggSoundData> & aSoundData):
        soundData{aSoundData}
    {
        buffers.resize(static_cast<std::size_t>(aSoundData->vorbisInfo.channels) * BUFFER_PER_CHANNEL);
        alCall(alGenBuffers, aSoundData->vorbisInfo.channels * BUFFER_PER_CHANNEL, buffers.data());

        for (auto buf : buffers)
        {
            freeBuffers.push_back(buf);
        }
    }

    std::shared_ptr<OggSoundData> soundData;
    //Left is first 3 buffers Right is last 3 buffers
    std::list<ALuint> freeBuffers;
    std::vector<ALuint> stagedBuffers;
    std::vector<ALuint> buffers;


    size_t positionInData = 0;
    PlayingSoundState state = PlayingSoundState_WAITING;
};

struct SoundCue
{
    std::vector<std::shared_ptr<OggSoundData>> sounds;
};

struct PlayingSoundCue
{
    PlayingSoundCue(const std::shared_ptr<SoundCue> & aSoundCue)
    {
        alCall(alGenSources, 1, &source);
        alCall(alSourcei, source, AL_SOURCE_RELATIVE, AL_TRUE);
        for (const std::shared_ptr<OggSoundData> & data : aSoundCue->sounds)
        {
            sounds.push_back(std::make_shared<PlayingSound>(data));
        }
    }

    SoundCueState state = SoundCueState_NOT_PLAYING;
    ALuint source;
    std::size_t currentPlayingSoundIndex = 0;
    std::size_t currentWaitingForBufferSoundIndex = 0;
    SoundOption option;
    std::vector<std::shared_ptr<PlayingSound>> sounds;
};


//Should always return by value
std::shared_ptr<OggSoundData> CreateData(const filesystem::path & path);
std::shared_ptr<OggSoundData> CreateData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

std::shared_ptr<OggSoundData> CreateStreamedOggData(const filesystem::path & aPath);
std::shared_ptr<OggSoundData> CreateStreamedOggData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

void decodeSoundData(const std::shared_ptr<OggSoundData> & aData);

//There is three step to play sound
//First load the file into RAM
//Second load the audio data into audio memory
//Last play the sound
class SoundManager
{
    public:
        SoundManager();
        ~SoundManager();

        void modifySound(ALuint aSource, SoundOption aOptions);
        bool stopSound(ALuint aSource);
        bool stopSound(handy::StringId aId);
        bool pauseSound(ALuint aSource);
        ALint getSourceState(ALuint aSource);
        void deleteSources(std::vector<ALuint> aSourcesToDelete);
        void monitor();
        CueHandle createSoundCue(const std::vector<handy::StringId> & aSoundList);
        void bufferPlayingSound(const std::shared_ptr<PlayingSound> & aSound);
        void updateCue(const std::shared_ptr<PlayingSoundCue> & currentCue);
        void update();
        void storeDataInLoadedSound(const std::shared_ptr<OggSoundData> & aSoundData);
        std::shared_ptr<PlayingSoundCue> playSound(CueHandle aSoundCue);

    private:

        void storeCueInMap(CueHandle aHandle, const std::shared_ptr<SoundCue> & aSoundCue);

        std::unordered_map<handy::StringId, std::shared_ptr<OggSoundData>> mLoadedSounds;
        std::map<CueHandle, std::shared_ptr<SoundCue>> mCues;
        std::list<std::shared_ptr<PlayingSoundCue>> mPlayingCues;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
        std::array<ALuint, 16> sources;
        std::vector<ALuint> freeSources;
        std::size_t currentHandle = 0;
};
} // namespace grapito
} // namespace ad
