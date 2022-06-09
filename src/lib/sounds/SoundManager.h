#pragma once

#include "SoundUtilities.h"
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

constexpr int BUFFER_PER_CHANNEL = 5;
constexpr std::streamsize HEADER_BLOCK_SIZE = 8192;
// Duration of music we want to extract from the stream (in s)
constexpr float MINIMUM_DURATION_EXTRACTED = 3.f;
constexpr int SAMPLE_APPROXIMATION = 44100;
constexpr int MINIMUM_SAMPLE_EXTRACTED = MINIMUM_DURATION_EXTRACTED * SAMPLE_APPROXIMATION;
constexpr int READ_CHUNK_SIZE = 16384;
constexpr std::array<ALenum, 3> SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr std::array<ALenum, 3> STREAM_SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO_FLOAT32, AL_FORMAT_MONO_FLOAT32};

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

template<int N_channels>
struct CueHandle
{
    std::size_t handle;

    bool operator<(const CueHandle<N_channels> & rHs) const
    {
        return handle < rHs.handle;
    }
};

template<int N_channels>
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

    int sampleRate;

    std::vector<typename std::conditional<N_channels == 1, short, float>::type> decodedData;
};

struct SoundOption
{
    float gain = 1.f;
    math::Position<3, float> position = math::Position<3, float>::Zero();
    math::Vec<3, float> velocity = math::Vec<3, float>::Zero();

    ALboolean looping = AL_FALSE;
};

template<int N_channels>
struct PlayingSound
{
    // Order of channels in ogg vorbis is left right
    // 3 buffers: processed buffer, queued buffer and playing buffer
    PlayingSound(std::shared_ptr<OggSoundData<N_channels>> aSoundData, SoundOption option):
        soundData{aSoundData},
        soundOption{option}
    {
        buffers.resize(aSoundData->vorbisInfo.channels * BUFFER_PER_CHANNEL);
        alCall(alGenBuffers, aSoundData->vorbisInfo.channels * BUFFER_PER_CHANNEL, buffers.data());

        for (auto buf : buffers)
        {
            freeBuffers.push_back(buf);
        }
    }

    std::shared_ptr<OggSoundData<N_channels>> soundData;
    //Left is first 3 buffers Right is last 3 buffers
    std::list<ALuint> freeBuffers;
    std::vector<ALuint> stagedBuffers;
    std::vector<ALuint> buffers;


    SoundOption soundOption;
    size_t positionInData = 0;
    //This means that there is no more sound data to load into buffers
    //(i.e. the nextPositionInData we want is passed, or at, the end of the decodedData)
    //and the sound will be discarded once all its buffers are played
    bool isStale = false;
};

template<int N_channels>
struct SoundCue
{
    ALuint source;
    int currentSoundIndex = -1;
    bool playing = false;
    std::size_t channels;
    std::vector<std::shared_ptr<PlayingSound<N_channels>>> sounds;
};

typedef OggSoundData<1> PointSoundData;
typedef OggSoundData<2> TwoDSoundData;

typedef SoundCue<1> PointSoundCue;
typedef CueHandle<1> PointSoundCueHandle;
typedef SoundCue<2> TwoDSoundCue;
typedef CueHandle<2> TwoDSoundCueHandle;

//Should always return by value
std::shared_ptr<PointSoundData> CreateMonoData(const filesystem::path & path);
std::shared_ptr<PointSoundData> CreateMonoData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

template<int N_channels>
std::shared_ptr<OggSoundData<N_channels>> CreateStreamedOggData(const filesystem::path & aPath);
template<int N_channels>
std::shared_ptr<OggSoundData<N_channels>> CreateStreamedOggData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId);

//Streamed version of ogg data
template<int N_channels>
std::shared_ptr<OggSoundData<N_channels>> CreateStreamedOggData(const filesystem::path & aPath)
{
    const std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    if (soundStream->fail())
    {
        spdlog::get("sounds")->error("File {} does not exists", aPath.string());
    }
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateStreamedOggData<N_channels>(soundStream, soundStringId);
}
template<int N_channels>
std::shared_ptr<OggSoundData<N_channels>> CreateStreamedOggData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId)
{
    int used = 0;
    int error = 0;

    std::vector<char> headerData(HEADER_BLOCK_SIZE);

    aInputStream->read(headerData.data(), HEADER_BLOCK_SIZE);

    std::streamsize lengthRead = aInputStream->gcount();
    spdlog::get("sounds")->error("length read for header bytes {}", lengthRead);

    stb_vorbis * vorbisData = nullptr;

    while (vorbisData == nullptr) {
        vorbisData =
            stb_vorbis_open_pushdata(reinterpret_cast<unsigned char *>(headerData.data()),
                                     headerData.size(), &used, &error, nullptr);

        if (vorbisData == nullptr) {
            if (error == VORBIS_need_more_data) [[likely]] {
                std::vector<char> moreHeaderData(HEADER_BLOCK_SIZE);
                aInputStream->read(moreHeaderData.data(), HEADER_BLOCK_SIZE);
                headerData.insert(headerData.end(), moreHeaderData.begin(),
                                  moreHeaderData.end());
                lengthRead += aInputStream->gcount();
                spdlog::get("sounds")->info(
                    "Unusually large headers required proceeding with a bigger chunk");
            } else {
                spdlog::get("sounds")->error(
                    "Stb vorbis error while opening pushdata decoder: {}", error);
                return std::make_shared<OggSoundData<N_channels>>(OggSoundData<N_channels>{
                    .soundId = aSoundId,
                    .dataStream = aInputStream,
                    .vorbisData = {nullptr, &stb_vorbis_close},
                });
            }
        }
    }

    spdlog::get("sounds")->error("Used bytes {}", used);

    stb_vorbis_info info = stb_vorbis_get_info(vorbisData);

    if (info.channels != N_channels)
    {
        spdlog::get("sounds")->error("Loaded {} channels sound into {} sound data", info.channels, N_channels);
    }

    std::shared_ptr<OggSoundData<N_channels>> resultSoundData = std::make_shared<OggSoundData<N_channels>>(OggSoundData<N_channels>{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .usedData = used,
        .undecodedReadData = {headerData.begin(), headerData.end()},
        .lengthRead = static_cast<std::size_t>(lengthRead),
        .fullyRead = false,
        .vorbisData = {vorbisData, &stb_vorbis_close},
        .vorbisInfo = info,
        .fullyDecoded = false,
        .dataFormat = STREAM_SOUNDS_AL_FORMAT[info.channels],
        .streamedData = true,
        .sampleRate = static_cast<int>(info.sample_rate),
    });

    return resultSoundData;
}

template<int N_channels>
void decodeSoundData(std::shared_ptr<OggSoundData<N_channels>> aData)
{
    stb_vorbis * vorbisData = aData->vorbisData;

    std::istream & inputStream = *aData->dataStream;
    std::vector<char> & soundData = aData->undecodedReadData;

    int channels = 0;

    int used = aData->usedData;
    float ** output;
    int samplesRead = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    while (samplesRead < MINIMUM_SAMPLE_EXTRACTED) {
        int currentUsed = -1;

        while (currentUsed != 0)
        {
            int passSampleRead = 0;

            currentUsed = stb_vorbis_decode_frame_pushdata(
                vorbisData, reinterpret_cast<unsigned char *>(soundData.data() + used),
                soundData.size() - used, &channels, &output, &passSampleRead);

            used += currentUsed;

            samplesRead += passSampleRead;

            if (passSampleRead > 0)
            {
                if (channels == 2)
                {
                    std::vector<float> interleavedData = interleave(output[0], output[1], passSampleRead);
                    aData->decodedData.insert(aData->decodedData.end(), interleavedData.begin(), interleavedData.end());
                }
                else
                {
                    aData->decodedData.insert(aData->decodedData.end(), &output[0][0], &output[0][passSampleRead]);
                }
            }
        }

        //TODO signal fullyDecoded

        if (!aData->fullyRead)
        {
            std::array<char, READ_CHUNK_SIZE> moreHeaderData;
            inputStream.read(moreHeaderData.data(), READ_CHUNK_SIZE);
            soundData.insert(soundData.end(), moreHeaderData.begin(), moreHeaderData.end());
            int lengthRead = inputStream.gcount();
            aData->lengthRead += lengthRead;

            if (lengthRead < READ_CHUNK_SIZE)
            {
                aData->fullyRead = true;
            }
        }
    }

    aData->lengthDecoded += samplesRead * channels;
    aData->usedData = used;

    if (aData->fullyRead && static_cast<std::size_t>(aData->usedData) == aData->lengthRead)
    {
        spdlog::get("sounds")->info("Fully decoded");
        aData->fullyDecoded = true;
    }

    std::chrono::steady_clock::time_point after = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = after - now;

    spdlog::get("sounds")->info("Samples: {}, total used bytes: {}, Elapsed time: {}, length decoded: {}", samplesRead, aData->usedData, diff.count(), aData->lengthDecoded * N_channels);
}

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

        template<int N_channels>
        CueHandle<N_channels> createSoundCue(std::vector<std::tuple<handy::StringId, SoundOption>> aSoundList);

        template<int N_channels>
        void bufferPlayingSound(std::shared_ptr<PlayingSound<N_channels>> aSound);

        template<int N_channels>
        void updateCue(std::shared_ptr<SoundCue<N_channels>> currentCue);

        void update();

        template<int N_channels>
        void storeDataInLoadedSound(std::shared_ptr<OggSoundData<N_channels>> aSoundData);

        template<int N_channels>
        void playSound(CueHandle<N_channels> aSoundCue);

        template<int N_channels>
        std::shared_ptr<OggSoundData<N_channels>> & getSoundData(handy::StringId aSoundId);

    private:

        void storeCueInMap(PointSoundCueHandle aHandle, std::shared_ptr<PointSoundCue> aSoundCue);
        void storeCueInMap(TwoDSoundCueHandle aHandle, std::shared_ptr<TwoDSoundCue> aSoundCue);

        template<int N_channels>
        std::shared_ptr<SoundCue<N_channels>> getSoundCue(CueHandle<N_channels> aCue);

        std::unordered_map<handy::StringId, std::shared_ptr<PointSoundData>> mLoadedPointSounds;
        std::unordered_map<handy::StringId, std::shared_ptr<TwoDSoundData>> mLoadedTwoDSounds;
        std::map<PointSoundCueHandle, std::shared_ptr<PointSoundCue>> mPointCues;
        std::map<TwoDSoundCueHandle, std::shared_ptr<TwoDSoundCue>> mTwoDCues;
        ALCdevice * mOpenALDevice;
        ALCcontext * mOpenALContext;
        ALCboolean mContextIsCurrent;
        std::array<ALuint, 16> sources;
        std::vector<ALuint> freeSources;
        std::size_t currentHandle = 0;
};

template<int N_channels>
CueHandle<N_channels> SoundManager::createSoundCue(std::vector<std::tuple<handy::StringId, SoundOption>> aSoundList)
{
    std::shared_ptr<SoundCue<N_channels>> soundCue = std::make_shared<SoundCue<N_channels>>(SoundCue<N_channels>{.channels = N_channels});
    int maxChannels = 0;

    for (auto [soundId, option] : aSoundList)
    {
        std::shared_ptr<OggSoundData<N_channels>> soundData = getSoundData<N_channels>(soundId);
        maxChannels = std::max(soundData->vorbisInfo.channels, maxChannels);

        std::shared_ptr<PlayingSound<N_channels>> sound = std::make_shared<PlayingSound<N_channels>>(
                PlayingSound<N_channels>{soundData, option}
                );

        soundCue->sounds.push_back(sound);
    }

    alCall(alGenSources, 1, &soundCue->source);

    currentHandle += 1;

    storeCueInMap(CueHandle<N_channels>{currentHandle}, soundCue);

    return {currentHandle};
}

template<int N_channels>
void SoundManager::playSound(CueHandle<N_channels> aHandle)
{
    std::shared_ptr<SoundCue<N_channels>> soundCue = getSoundCue(aHandle);
    // queue is not playing
    if (soundCue->currentSoundIndex == -1)
    {
        //start the first buffer
        soundCue->currentSoundIndex = 0;
    }

    std::shared_ptr<PlayingSound<N_channels>> sound = soundCue->sounds[soundCue->currentSoundIndex];
    std::shared_ptr<OggSoundData<N_channels>> data = sound->soundData;

    if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_EXTRACTED && !data->fullyDecoded)
    {
        decodeSoundData(data);
    }

    bufferPlayingSound(sound);
    alCall(alSourceQueueBuffers, soundCue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

    //empty staged buffers
    sound->stagedBuffers.resize(0);

    alCall(alSourcePlay, soundCue->source);
}

template<int N_channels>
void SoundManager::bufferPlayingSound(std::shared_ptr<PlayingSound<N_channels>> aSound)
{
    std::list<ALuint> & freeBuffers = aSound->freeBuffers;
    std::shared_ptr<OggSoundData<N_channels>> data = aSound->soundData;

    if (freeBuffers.size() > 0)
    {
        auto bufIt = freeBuffers.begin();
        ALuint freeBuf = *bufIt;

        std::size_t nextPositionInData = std::min(
                static_cast<std::size_t>(data->lengthDecoded),
                aSound->positionInData + MINIMUM_SAMPLE_EXTRACTED * N_channels
                );

        if (nextPositionInData == data->lengthDecoded && data->fullyDecoded)
        {
            aSound->isStale = true;
        }

        if (nextPositionInData <= data->lengthDecoded)
        {
            spdlog::get("sounds")->info(
                    "buffer: {}, from: {}, size: {}",
                    freeBuf,
                    aSound->positionInData,
                    nextPositionInData - aSound->positionInData
                    );

            //This is fucked because
            //However for it to work we need at least 2 buffer to be free
            alCall(
                    alBufferData,
                    freeBuf,
                    N_channels == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO_FLOAT32,
                    data->decodedData.data() + aSound->positionInData,
                    sizeof(float) * (nextPositionInData - aSound->positionInData),
                    data->vorbisInfo.sample_rate
                    );

            aSound->positionInData = nextPositionInData;
            bufIt = freeBuffers.erase(bufIt);
            aSound->stagedBuffers.push_back(freeBuf);
        }
    }
}

template<int N_channels>
void SoundManager::updateCue(std::shared_ptr<SoundCue<N_channels>> currentCue)
{
    int currentSoundIndex = currentCue->currentSoundIndex;

    spdlog::get("sounds")->trace("number of sounds in queue: {}", currentCue->sounds.size());
    spdlog::get("sounds")->trace("currentSoundIndex: {}", currentSoundIndex);

    std::shared_ptr<PlayingSound<N_channels>> sound = currentCue->sounds[currentSoundIndex];

    if (sound->isStale)
    {
        currentCue->currentSoundIndex++;
        currentSoundIndex = currentCue->currentSoundIndex;
        sound = currentCue->sounds[currentSoundIndex];
    }

    std::shared_ptr<OggSoundData<N_channels>> data = sound->soundData;
    std::list<ALuint> & freeBuffers = sound->freeBuffers;

    if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_EXTRACTED && !data->fullyDecoded)
    {
        decodeSoundData(data);
    }

    ALuint source = currentCue->source;


#if 0 TEMPORARY
    int bufferQueued = 0;

    alCall(alGetSourceiv, source, AL_BUFFERS_QUEUED, &bufferQueued);
    spdlog::get("sounds")->trace("number of buffer queued to {} : {}", source, bufferQueued);
#endif

    int bufferProcessed = 0;
    alCall(alGetSourceiv, source, AL_BUFFERS_PROCESSED, &bufferProcessed);

    //add used buffer to freeBuffers list
    if (bufferProcessed > 0)
    {
        spdlog::get("sounds")->trace("number of buffer to free from {} : {}", source, bufferProcessed);
        std::vector<ALuint> bufferUnqueued(bufferProcessed);
        alCall(alSourceUnqueueBuffers, source, bufferProcessed, bufferUnqueued.data());
        freeBuffers.insert(freeBuffers.end(), bufferUnqueued.begin(), bufferUnqueued.end());
    }

    bufferPlayingSound(sound);

    alCall(alSourceQueueBuffers, currentCue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

    spdlog::get("sounds")->trace("remaining free buffer: {}", freeBuffers.size());
    spdlog::get("sounds")->trace("paying buffer: {}", sound->stagedBuffers.size());

    //empty staged buffers
    sound->stagedBuffers.resize(0);
}

} // namespace grapito
} // namespace ad
