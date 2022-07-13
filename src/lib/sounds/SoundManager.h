#pragma once

#include "SoundUtilities.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "stb_vorbis.h"

#include <resource/ResourceManager.h>

#include <platform/Filesystem.h>
#include <math/Vector.h>
#include <handy/Guard.h>
#include <handy/StringId.h>
#include <handy/StringId_Interning.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>

#define TEMPORARY

namespace ad {
namespace sounds {

/*
 * Ideas :
 * - Max sources per radius (2 is a good number)
 * - better ducking (like playWithDucking to lower all sound for the duration of the sound)
 * - Removing source that are basically inaudible
 * - Start sound paused to avoid sound playing before being placed
 * - Find a way to manage memory consumption
 * - Threaded decoding
 * - Threaded mixing
 * - Threaded feeding to openal
 */

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

enum PlayingSoundCueState
{
    PlayingSoundCueState_PLAYING,
    PlayingSoundCueState_STALE,
    PlayingSoundCueState_NOT_PLAYING,
    PlayingSoundCueState_INTERRUPTED,
};

typedef int SoundCategory;

constexpr int MASTER_SOUND_CATEGORY = -1;
constexpr int HIGHEST_PRIORITY = -1;
constexpr int BUFFER_PER_CHANNEL = 5;
constexpr std::size_t MAX_SOURCES = 5;
const std::size_t MAX_SOURCE_PER_CUE = 3;

template<typename T>
inline std::vector<T> interleave(T * left, T * right, int size)
{
    std::vector<float> result(size);

    for (std::size_t i = 0; i < static_cast<std::size_t>(size); i++)
    {
        result.push_back(left[i]);
        result.push_back(right[i]);
    }

    return result;
}

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

struct CueElementOption
{
    int loops = 0;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> position = math::Position<3, float>::Zero();
    math::Vec<3, float> velocity = math::Vec<3, float>::Zero();
};

struct CategoryOption
{
    float userGain = 1.f;
    float gameGain = 1.f;
};

struct PlayingSound
{
    // Order of channels in ogg vorbis is left right
    // 3 buffers: processed buffer, queued buffer and playing buffer
    PlayingSound(const std::shared_ptr<OggSoundData> & aSoundData, const CueElementOption & option):
        soundData{aSoundData},
        loops{option.loops}
    {
        if (aSoundData != nullptr)
        {
            buffers.resize(static_cast<std::size_t>(aSoundData->vorbisInfo.channels) * BUFFER_PER_CHANNEL);
            alCall(alGenBuffers, aSoundData->vorbisInfo.channels * BUFFER_PER_CHANNEL, buffers.data());

            for (auto buf : buffers)
            {
                freeBuffers.push_back(buf);
            }
        }
    }

    std::shared_ptr<OggSoundData> soundData;
    //Left is first 3 buffers Right is last 3 buffers
    std::list<ALuint> freeBuffers;
    std::vector<ALuint> stagedBuffers;
    std::vector<ALuint> buffers;

    int loops;


    size_t positionInData = 0;
    PlayingSoundState state = PlayingSoundState_WAITING;
};

struct SoundCue
{
    SoundCue(int aId, int aHandleIndex, SoundCategory aCategory, int aPriority) :
        id{aId},
        handleIndex{aHandleIndex},
        category{aCategory},
        priority{aPriority}
    {}

    int id;
    int handleIndex;
    SoundCategory category;
    int priority;
    std::vector<std::pair<std::shared_ptr<OggSoundData>, CueElementOption>> sounds;
    std::shared_ptr<OggSoundData> interruptSound = nullptr;
};

struct PlayingSoundCue
{
    PlayingSoundCue(
            const SoundCue & aSoundCue,
            ALuint source,
            int aId,
            int aHandleIndex
            ) :
        id{aId},
        handleIndex{aHandleIndex},
        priority{aSoundCue.priority},
        category{aSoundCue.category},
        source{source}
    {
        alCall(alSourcei, source, AL_SOURCE_RELATIVE, AL_TRUE);
        for (const auto & [data, option] : aSoundCue.sounds)
        {
            sounds.push_back(std::make_shared<PlayingSound>(data, option));
        }

        if (aSoundCue.interruptSound != nullptr)
        {
            interruptSound = std::make_shared<PlayingSound>(aSoundCue.interruptSound, CueElementOption{});
        }
    }

    std::shared_ptr<PlayingSound> getWaitingSound() const
    {
        if (state == PlayingSoundCueState_INTERRUPTED)
        {
            return interruptSound;
        }

        return sounds[currentWaitingForBufferSoundIndex];
    }

    std::shared_ptr<PlayingSound> getPlayingSound() const
    {
        if (state == PlayingSoundCueState_INTERRUPTED)
        {
            return interruptSound;
        }

        std::shared_ptr<PlayingSound> sound = sounds[currentPlayingSoundIndex];

        return sound;
    }

    int id;
    int handleIndex;

    int priority;
    SoundCategory category;

    PlayingSoundCueState state = PlayingSoundCueState_NOT_PLAYING;
    ALuint source;
    std::size_t currentPlayingSoundIndex = 0;
    std::size_t currentWaitingForBufferSoundIndex = 0;
    SoundOption option;
    std::vector<std::shared_ptr<PlayingSound>> sounds;
    std::shared_ptr<PlayingSound> interruptSound = nullptr;
};

void decodeSoundData(const std::shared_ptr<OggSoundData> & aData, unsigned int aMinSamples);
void bufferPlayingSound(const std::shared_ptr<PlayingSound> & aSound);

template<typename T>
struct Handle
{
    Handle() :
        mHandleIndex{-1},
        mUniqueId{-1}
    {}
    Handle(const std::unique_ptr<T> & aCue) :
        mHandleIndex{aCue->handleIndex},
        mUniqueId{aCue->id}
    {}

    int mHandleIndex;
    int mUniqueId;

    bool operator<(const Handle<T> & rHs) const
    {
        return mHandleIndex < rHs.mHandleIndex;
    }

    bool operator==(const Handle<T> & rHs) const
    {
        return mHandleIndex == rHs.mHandleIndex && mUniqueId == rHs.mUniqueId;
    }

    T * toObject() const;
};

bool CmpHandlePriority(const Handle<PlayingSoundCue> & lhs, const Handle<PlayingSoundCue> & rhs);

typedef std::vector<Handle<PlayingSoundCue>> PlayingSoundCueQueue;

struct SoundManagerInfo
{
    const std::map<Handle<PlayingSoundCue>, std::unique_ptr<PlayingSoundCue>> & playingCues;
    const std::array<ALuint, MAX_SOURCES> & sources;
    const std::vector<std::size_t> & freeSources;
    const std::unordered_map<handy::StringId, std::shared_ptr<OggSoundData>> & loadedSounds;
};

//There is three step to play sound
//First load the file into RAM
//Second load the audio data into audio memory
//Last play the sound
class SoundManager
{
    public:
        SoundManager(std::vector<SoundCategory> && aCategories);
        ~SoundManager();

        handy::StringId createData(const filesystem::path & aPath);
        handy::StringId createData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

        handy::StringId createStreamedOggData(const filesystem::path & aPath);
        handy::StringId createStreamedOggData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

        Handle<PlayingSoundCue> playSound(const Handle<SoundCue> & aSoundCue);

        bool stopSound(const Handle<PlayingSoundCue> & aHandle);
        void stopCategory(SoundCategory aSoundCategory);
        void stopAllSound();

        bool pauseSound(const Handle<PlayingSoundCue> & aHandle);
        std::vector<Handle<PlayingSoundCue>> pauseCategory(SoundCategory aSoundCategory);
        std::vector<Handle<PlayingSoundCue>> pauseAllSound();

        bool startSound(const Handle<PlayingSoundCue> & aHandle);
        void startCategory(SoundCategory aSoundCategory);
        void startAllSound();

        bool interruptSound(const Handle<PlayingSoundCue> & aHandle);

        ALint getSourceState(ALuint aSource);
        Handle<SoundCue> createSoundCue(
                const std::vector<std::pair<handy::StringId, CueElementOption>> & aSoundList,
                SoundCategory aCategory,
                int priority,
                const handy::StringId & aInterruptSoundId = handy::StringId::Null()
                );

        void update();
        void updateCue(PlayingSoundCue & currentCue, const Handle<PlayingSoundCue> & aHandle);
        void monitor();

        const SoundManagerInfo getInfo() const;


    private:
        std::map<SoundCategory, PlayingSoundCueQueue> mCuesByCategories;
        std::map<
            SoundCategory, CategoryOption> mCategoryOptions;
        std::map<Handle<SoundCue>, std::vector<Handle<PlayingSoundCue>>> mPlayingCuesByCue;

        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;

        std::unordered_map<handy::StringId, std::shared_ptr<OggSoundData>> mLoadedSounds;

        std::array<ALuint, MAX_SOURCES> mSources;
        std::vector<std::size_t> mFreeSources;

        std::size_t mCurrentCueId = 0;
};

inline std::map<Handle<SoundCue>, std::unique_ptr<SoundCue>> mCues;
inline std::map<Handle<PlayingSoundCue>, std::unique_ptr<PlayingSoundCue>> mPlayingCues;

} // namespace grapito
} // namespace ad
