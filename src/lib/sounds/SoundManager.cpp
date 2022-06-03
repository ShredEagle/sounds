#include "SoundManager.h"

#include <AL/al.h>
#include <cstddef>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <spdlog/spdlog.h>
#include <vector>

namespace ad {
namespace sounds {

constexpr std::streamsize headerBlockReadSize = 8192;
// Duration of music we want to extract from the stream (in s)
constexpr float MINIMUM_DURATION_EXTRACTED = 0.1;
constexpr int SAMPLE_APPROXIMATION = 22050;
constexpr int MINIMUM_SAMPLE_EXTRACTED = MINIMUM_DURATION_EXTRACTED * SAMPLE_APPROXIMATION;
constexpr int READ_CHUNK_SIZE = 16384;

OggSoundData CreateOggData(const filesystem::path & aPath)
{
    std::ifstream soundStream{aPath.string(), std::ios::binary};
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateOggData(soundStream, soundStringId);
}

OggSoundData CreateOggData(std::istream & aInputStream, const handy::StringId aSoundId)
{
    short * decoded;
    int channels;
    int error;
    int sampleRate;
    std::istreambuf_iterator<char> it{aInputStream}, end;
    std::vector<std::uint8_t> data{it, end};
    int len = stb_vorbis_decode_memory(data.data(), data.size(), &channels, &sampleRate,
                                       &decoded);
    OggSoundData resultSoundData{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .fullyRead = true,
        .lengthDecoded = len,
        .fullyDecoded = true,
        .sampleRate = sampleRate,
    };

    if (len == -1) {
        spdlog::get("sounds")->error("A read from the media returned an error");
    }

    if (channels == 1) {
        channels = AL_FORMAT_MONO16;
    } else {
        channels = AL_FORMAT_STEREO16;
    }

    ALuint buffer;
    alCall(alGenBuffers, 1, &buffer);
    alCall(alBufferData, buffer, channels, decoded, len, sampleRate);

    resultSoundData.leftBuffers.push_back(buffer);

    return resultSoundData;
}

OggSoundData CreateStreamedOggData(const filesystem::path & aPath)
{
    std::ifstream soundStream{aPath.string(), std::ios::binary};
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateStreamedOggData(soundStream, soundStringId);
}

OggSoundData CreateStreamedOggData(std::istream & aInputStream,
                                   const handy::StringId aSoundId)
{
    int used = 0;
    int error;

    std::vector<char> headerData(headerBlockReadSize);

    aInputStream.read(headerData.data(), headerBlockReadSize);

    std::streamsize lengthRead = aInputStream.gcount();

    stb_vorbis * vorbisData = nullptr;
    int tries = 0;

    while (vorbisData == nullptr) {
        vorbisData =
            stb_vorbis_open_pushdata(reinterpret_cast<unsigned char *>(headerData.data()),
                                     headerData.size(), &used, &error, nullptr);

        if (vorbisData == nullptr) {
            if (error == VORBIS_need_more_data) [[likely]] {
                std::vector<char> moreHeaderData(headerBlockReadSize);
                aInputStream.read(moreHeaderData.data(), headerBlockReadSize);
                headerData.insert(headerData.end(), moreHeaderData.begin(),
                                  moreHeaderData.end());
                lengthRead += aInputStream.gcount();
                spdlog::get("sounds")->info(
                    "Unusually large headers required proceeding with a bigger chunk");
            } else {
                spdlog::get("sounds")->error(
                    "Stb vorbis error while opening pushdata decoder: {}", error);
                return {
                    .soundId = aSoundId,
                    .dataStream = aInputStream,
                    .vorbisData = nullptr,
                };
            }
        }
    }

    spdlog::get("sounds")->error("Used bytes {}", used);

    stb_vorbis_info info = stb_vorbis_get_info(vorbisData);

    OggSoundData resultSoundData{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .lengthRead = lengthRead,
        .undecodedReadData = {headerData.begin() + used, headerData.end()},
        .fullyRead = true,
        .vorbisData = vorbisData,
        .vorbisInfo = info,
        .lengthDecoded = 0,
        .fullyDecoded = true,
        .streamedData = true,
        .sampleRate = static_cast<int>(info.sample_rate),
    };

    return resultSoundData;
}

void decodeSoundData(OggSoundData & aData)
{
    stb_vorbis * vorbisData = aData.vorbisData;
    stb_vorbis_info info = aData.vorbisInfo;

    std::istream & inputStream = aData.dataStream;
    auto soundData = aData.undecodedReadData;

    int channels;

    int used = 0;
    float ** output;
    int samplesRead = 0;
    int lengthRead = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    while (samplesRead / MINIMUM_SAMPLE_EXTRACTED) {
        int passSampleRead;
        int currentUsed;

        currentUsed = stb_vorbis_decode_frame_pushdata(
            vorbisData, reinterpret_cast<unsigned char *>(soundData.data() + used),
            soundData.size() - used, &channels, &output, &passSampleRead);

        used += currentUsed;

        samplesRead += passSampleRead;
        std::vector<char> moreHeaderData(READ_CHUNK_SIZE);
        inputStream.read(moreHeaderData.data(), READ_CHUNK_SIZE);
        soundData.insert(soundData.end(), moreHeaderData.begin(), moreHeaderData.end());
        lengthRead += inputStream.gcount();

        if (lengthRead < READ_CHUNK_SIZE)
        {
            aData.fullyRead = true;
        }
    }
    spdlog::get("sounds")->info("total used bytes {}", used);
    spdlog::get("sounds")->info("Samples {}", samplesRead);
    std::chrono::steady_clock::time_point after = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = after - now;

    spdlog::get("sounds")->info("Elapsed time : {}", diff.count());

    std::array<std::vector<float>, 2> buffers;
    aData.decodedLeftData.assign(output[0], output[0] + samplesRead);
    aData.decodedRightData.assign(output[1], output[1] + samplesRead);
}

SoundManager::SoundManager()
{
    mOpenALDevice = alcOpenDevice(nullptr);
    if (!mOpenALDevice) {
        /* fail */
        spdlog::get("grapito")->error("Cannot open OpenAL sound device");
    } else {
        if (!alcCall(alcCreateContext, mOpenALContext, mOpenALDevice, mOpenALDevice,
                     nullptr)) {
            spdlog::get("grapito")->error("Cannot create OpenAL context");
        } else {
            if (!alcCall(alcMakeContextCurrent, mContextIsCurrent, mOpenALDevice,
                         mOpenALContext)) {
                spdlog::get("grapito")->error("Cannot set OpenAL to current context");
            }
        }
    }
}

SoundManager::~SoundManager()
{
    if (mContextIsCurrent) {
        if (!alcCall(alcMakeContextCurrent, mContextIsCurrent, mOpenALDevice, nullptr)) {
            spdlog::get("grapito")->error("Well we're leaking audio memory now");
        }

        if (!alcCall(alcDestroyContext, mOpenALDevice, mOpenALContext)) {
            spdlog::get("grapito")->error("Well we're leaking audio memory now");
        }

        ALCboolean closed;
        if (!alcCall(alcCloseDevice, closed, mOpenALDevice, mOpenALDevice)) {
            spdlog::get("grapito")->error("Device just disappeared and I don't know why");
        }
    }
}

std::size_t SoundManager::playSound(
    std::vector<std::tuple<handy::StringId, SoundOption>> aSoundQueue)
{
    SoundQueue soundQueue;
    for (auto [soundId, option] : aSoundQueue) {
        const OggSoundData & soundData = mLoadedSoundList.at(soundId);
        PlayingSound sound{soundData};

        soundQueue.sounds.push_back(sound);

        ALuint source;
        alCall(alGenSources, 1, &source);

        soundQueue.source = source; 
    }

    // This is used if the sound is not created by an entity
    // And we need someone to own the data
    mStoreQueues.push_back(soundQueue);
    std::size_t index = mStoreQueues.size() - 1;

    return index;
}

void SoundManager::update()
{
    for (auto currentQueue : mStoreQueues)
    {
        std::size_t currentSoundIndex = currentQueue.currentSoundIndex;
        ALuint source = currentQueue.source;
        // queue is not playing
        if (currentSoundIndex == -1)
        {
            //start the first buffer
            currentQueue.currentSoundIndex = 0;
        }

        auto sound = currentQueue.sounds[currentSoundIndex];
        auto data = sound.soundData;
        int bufferQueued;

        alCall(alGetSourcei, source, AL_BUFFERS_QUEUED, &bufferQueued);

        // We always want 2 buffer queued on a source
        if (bufferQueued < 2)
        {
            if (!sound.isStale)
            {
                if (data.lengthDecoded < sound.positionInData + MINIMUM_SAMPLE_EXTRACTED)
                {
                    decodeSoundData(data);
                }

                std::size_t nextPositionInData = sound.positionInData + MINIMUM_DURATION_EXTRACTED * data.vorbisInfo.sample_rate;

                if (nextPositionInData > data.lengthDecoded && data.fullyDecoded)
                {
                }
            }
        }

    }
}

void SoundManager::modifySound(ALuint aSource, SoundOption aOptions)
{
    alCall(alSourcef, aSource, AL_GAIN, aOptions.gain);
    alCall(alSource3f, aSource, AL_POSITION, aOptions.position.x(), aOptions.position.y(),
           aOptions.position.z());
    alCall(alSource3f, aSource, AL_VELOCITY, aOptions.velocity.x(), aOptions.velocity.y(),
           aOptions.velocity.z());
    alCall(alSourcei, aSource, AL_LOOPING, aOptions.looping);
}

bool SoundManager::stopSound(ALuint aSource) { return alCall(alSourceStop, aSource); }

bool SoundManager::stopSound(handy::StringId aId)
{
    if (mStoredSources.contains(aId)) {
        ALuint source = mStoredSources.at(aId);
        mStoredSources.erase(aId);
        return alCall(alSourceStop, source);
    }
    return false;
}

bool SoundManager::pauseSound(ALuint aSource) { return alCall(alSourcePause, aSource); }

void SoundManager::storeDataInLoadedSound(const OggSoundData & aSoundData)
{
    mLoadedSoundList.insert({aSoundData.soundId, aSoundData});
}

ALint SoundManager::getSourceState(ALuint aSource)
{
    ALint sourceState;
    alCall(alGetSourcei, aSource, AL_SOURCE_STATE, &sourceState);
    return sourceState;
}

void SoundManager::deleteSources(std::vector<ALuint> aSourcesToDelete)
{
    alCall(alDeleteSources, aSourcesToDelete.size(), aSourcesToDelete.data());
}

} // namespace sounds
} // namespace ad
