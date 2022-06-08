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
constexpr float MINIMUM_DURATION_EXTRACTED = 1.f;
constexpr int SAMPLE_APPROXIMATION = 44100;
constexpr int MINIMUM_SAMPLE_EXTRACTED = MINIMUM_DURATION_EXTRACTED * SAMPLE_APPROXIMATION;
constexpr int READ_CHUNK_SIZE = 16384;

OggSoundData CreateOggData(const filesystem::path & aPath)
{
    std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateOggData(soundStream, soundStringId);
}

OggSoundData CreateOggData(std::shared_ptr<std::istream> aInputStream, const handy::StringId aSoundId)
{
    short * decoded;
    int channels;
    int error;
    int sampleRate;
    std::istreambuf_iterator<char> it{*aInputStream}, end;
    std::vector<std::uint8_t> data{it, end};
    int len = stb_vorbis_decode_memory(data.data(), data.size(), &channels, &sampleRate,
                                       &decoded);
    OggSoundData resultSoundData{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .fullyRead = true,
        .lengthDecoded = len,
        .fullyDecoded = true,
        .dataFormat = SOUNDS_AL_FORMAT[channels],
        .sampleRate = sampleRate,
    };

    if (len == -1) {
        spdlog::get("sounds")->error("A read from the media returned an error");
    }
    

    resultSoundData.decodedData.push_back({decoded, decoded + len});

    return resultSoundData;
}

OggSoundData CreateStreamedOggData(const filesystem::path & aPath)
{
    std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateStreamedOggData(soundStream, soundStringId);
}

OggSoundData CreateStreamedOggData(std::shared_ptr<std::istream> aInputStream,
                                   const handy::StringId aSoundId)
{
    int used = 0;
    int error;

    std::vector<char> headerData(headerBlockReadSize);

    aInputStream->read(headerData.data(), headerBlockReadSize);

    std::streamsize lengthRead = aInputStream->gcount();
    spdlog::get("sounds")->error("length read for header bytes {}", lengthRead);

    stb_vorbis * vorbisData = nullptr;
    int tries = 0;

    while (vorbisData == nullptr) {
        vorbisData =
            stb_vorbis_open_pushdata(reinterpret_cast<unsigned char *>(headerData.data()),
                                     headerData.size(), &used, &error, nullptr);

        if (vorbisData == nullptr) {
            if (error == VORBIS_need_more_data) [[likely]] {
                std::vector<char> moreHeaderData(headerBlockReadSize);
                aInputStream->read(moreHeaderData.data(), headerBlockReadSize);
                headerData.insert(headerData.end(), moreHeaderData.begin(),
                                  moreHeaderData.end());
                lengthRead += aInputStream->gcount();
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
        .undecodedReadData = {headerData.begin(), headerData.end()},
        .fullyRead = false,
        .vorbisData = vorbisData,
        .vorbisInfo = info,
        .usedData = used,
        .fullyDecoded = false,
        .dataFormat = STREAM_SOUNDS_AL_FORMAT[info.channels],
        .streamedData = true,
        .sampleRate = static_cast<int>(info.sample_rate),
    };

    resultSoundData.decodedData.resize(info.channels);

    return resultSoundData;
}

int decodeSoundData(OggSoundData & aData)
{
    stb_vorbis * vorbisData = aData.vorbisData;
    stb_vorbis_info info = aData.vorbisInfo;

    std::istream & inputStream = *aData.dataStream;
    auto & soundData = aData.undecodedReadData;

    int channels;

    int used = aData.usedData;
    float ** output;
    int samplesRead = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    while (samplesRead < MINIMUM_SAMPLE_EXTRACTED) {
        int currentUsed = -1;

        while (currentUsed != 0)
        {
            int passSampleRead;

            currentUsed = stb_vorbis_decode_frame_pushdata(
                vorbisData, reinterpret_cast<unsigned char *>(soundData.data() + used),
                soundData.size() - used, &channels, &output, &passSampleRead);

            used += currentUsed;

            samplesRead += passSampleRead;


            if (passSampleRead > 0)
            {
                for (int i = 0; i < channels; i++)
                {
                    aData.decodedData.at(i).insert(aData.decodedData.at(i).end(), &output[i][0], &output[i][passSampleRead]);
                }
            }
        }

        //TODO signal fullyDecoded

        if (!aData.fullyRead)
        {
            std::array<char, READ_CHUNK_SIZE> moreHeaderData;
            inputStream.read(moreHeaderData.data(), READ_CHUNK_SIZE);
            soundData.insert(soundData.end(), moreHeaderData.begin(), moreHeaderData.end());
            int lengthRead = inputStream.gcount();

            if (lengthRead < READ_CHUNK_SIZE)
            {
                aData.fullyRead = true;
            }
        }
    }

    aData.lengthDecoded += samplesRead;
    aData.usedData = used;

    spdlog::get("sounds")->info("total used bytes {}", used);
    spdlog::get("sounds")->info("Samples {}", samplesRead);
    std::chrono::steady_clock::time_point after = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = after - now;

    spdlog::get("sounds")->warn("Elapsed time : {}", diff.count());
    spdlog::get("sounds")->info("channels : {}", channels);


    return samplesRead;
}

void loadDataIntoBuffers(OggSoundData & aData, PlayingSound & sound)
{
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
    int maxChannels = 0;

    for (auto [soundId, option] : aSoundQueue) {
        OggSoundData & soundData = mLoadedSoundList.at(soundId);
        maxChannels = std::max(soundData.vorbisInfo.channels, maxChannels);

        PlayingSound sound{soundData, option};

        soundQueue.sounds.push_back(sound);
    }

    soundQueue.sources.resize(maxChannels);

    alCall(alGenSources, maxChannels, soundQueue.sources.data());

    // This is used if the sound is not created by an entity
    // And we need someone to own the data
    mStoreQueues.push_back(soundQueue);
    std::size_t index = mStoreQueues.size() - 1;


    return index;
}

void SoundManager::update()
{
    for (auto & currentQueue : mStoreQueues)
    {
        int currentSoundIndex = currentQueue.currentSoundIndex;
        // queue is not playing
        if (currentSoundIndex == -1)
        {
            //start the first buffer
            currentSoundIndex = 0;
            currentQueue.currentSoundIndex = currentSoundIndex;
        }

        spdlog::get("sounds")->info("number of sounds in queue: {}", currentQueue.sounds.size());
        spdlog::get("sounds")->info("currentSoundIndex: {}", currentSoundIndex);

        PlayingSound & sound = currentQueue.sounds[currentSoundIndex];
        OggSoundData & data = sound.soundData;
        std::list<ALuint> & freeBuffers = sound.freeBuffers;
        int bufferProcessed;


        spdlog::get("sounds")->info("number of free buffers in queue: {}", freeBuffers.size());

        for (auto source : currentQueue.sources)
        {
            alCall(alGetSourceiv, source, AL_BUFFERS_PROCESSED, &bufferProcessed);
            spdlog::get("sounds")->info("number of buffer to free: {}", bufferProcessed);

            //add used buffer to freeBuffers list
            if (bufferProcessed > 1)
            {
                std::vector<ALuint> bufferUnqueued(bufferProcessed);
                alCall(alSourceUnqueueBuffers, source, bufferProcessed, bufferUnqueued.data());
                freeBuffers.insert(freeBuffers.end(), bufferUnqueued.begin(), bufferUnqueued.end());
            }
        }


        // We always want 2 buffer queued on a source
        std::vector<ALuint> bufferToLoad;
        if (freeBuffers.size() >= sound.soundData.vorbisInfo.channels)
        {
            if (!sound.isStale)
            {
                auto bufIt = freeBuffers.begin();
                ALuint freeBuf = *bufIt;
                int samplesRead = 0;

                if (data.lengthDecoded < sound.positionInData + MINIMUM_SAMPLE_EXTRACTED && !data.fullyDecoded)
                {
                    samplesRead = decodeSoundData(data);
                }

                std::size_t nextPositionInData = std::min(static_cast<std::size_t>(data.lengthDecoded), sound.positionInData + MINIMUM_SAMPLE_EXTRACTED);

                if (nextPositionInData >= data.lengthDecoded && data.fullyDecoded)
                {
                    sound.isStale = true;
                }

                if (nextPositionInData <= data.lengthDecoded)
                {
                    int i = 0;
                    for (auto decodedChannel : data.decodedData)
                    {
                        std::vector<float> dataToLoadInOpenAL = {
                            decodedChannel.begin() + sound.positionInData,
                            decodedChannel.begin() + nextPositionInData,
                        };
                        spdlog::get("sounds")->warn("add data to {}", freeBuf);
                        spdlog::get("sounds")->warn("from {} to {}", sound.positionInData, nextPositionInData);
                        spdlog::get("sounds")->warn("sample size: {}", dataToLoadInOpenAL.size());
                        spdlog::get("sounds")->warn("data format: {}", data.dataFormat);
                        spdlog::get("sounds")->warn("data rate: {}", data.vorbisInfo.sample_rate);
                        spdlog::get("sounds")->warn("source: {}", currentQueue.sources.at(i));

                        //This is fucked because
                        //However for it to work we need at least 2 buffer to be free
                        alCall(
                                alBufferData,
                                freeBuf,
                                AL_FORMAT_MONO_FLOAT32,
                                dataToLoadInOpenAL.data(),
                                sizeof(float) * dataToLoadInOpenAL.size(),
                                data.vorbisInfo.sample_rate
                                );

                        alCall(alSourceQueueBuffers, currentQueue.sources.at(i), 1, &freeBuf);
                        ALint sourceState;
                        alCall(alGetSourcei, currentQueue.sources.at(i), AL_SOURCE_STATE, &sourceState);
                        alCall(alSource3f, currentQueue.sources.at(i), AL_POSITION, sound.soundOption.position.x(), sound.soundOption.position.y(), sound.soundOption.position.z());

                        if (sourceState != AL_PLAYING)
                        {
                            alCall(alSourcePlay, currentQueue.sources.at(i));
                        }

                        bufIt = freeBuffers.erase(bufIt);
                        freeBuf = *bufIt;
                        i++;
                    }
                    sound.positionInData = nextPositionInData;
                }
            }
        }

        spdlog::get("sounds")->info("remaining free buffer: {}", freeBuffers.size());
        spdlog::get("sounds")->info("paying buffer: {}", bufferToLoad.size());
        //alCall(alSourceQueueBuffers, source, bufferToLoad.size(), bufferToLoad.data());

    }
}

void SoundManager::monitor()
{
    for (auto & currentQueue : mStoreQueues)
    {
        ALint sourceState;
        alCall(alGetSourcei, currentQueue.sources.at(0), AL_SOURCE_STATE, &sourceState);
        spdlog::get("sounds")->info("Source state {}", sourceState);
    }
}

void SoundManager::modifySound(ALuint aSource, SoundOption aOptions)
{
    //alCall(alSourcef, aSource, AL_GAIN, aOptions.gain);
    //alCall(alSource3f, aSource, AL_POSITION, aOptions.position.x(), aOptions.position.y(),
    //       aOptions.position.z());
    //alCall(alSource3f, aSource, AL_VELOCITY, aOptions.velocity.x(), aOptions.velocity.y(),
    //       aOptions.velocity.z());
    //alCall(alSourcei, aSource, AL_LOOPING, aOptions.looping);
}

bool SoundManager::stopSound(ALuint aSource) { return alCall(alSourceStop, aSource); }

bool SoundManager::stopSound(handy::StringId aId)
{
    //if (mStoredSources.contains(aId)) {
    //    ALuint source = mStoredSources.at(aId);
    //    mStoredSources.erase(aId);
    //    return alCall(alSourceStop, source);
    //}
    //return false;
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
