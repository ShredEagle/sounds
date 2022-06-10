#include "SoundManager.h"

#include <AL/al.h>
#include <cstddef>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>

namespace ad {
namespace sounds {

constexpr std::streamsize HEADER_BLOCK_SIZE = 8192;
// Duration of music we want to extract from the stream (in s)
constexpr float MINIMUM_DURATION_EXTRACTED = 3.f;
constexpr float MINIMUM_DURATION_FOR_NON_STREAM = 10.f;
constexpr unsigned int SAMPLE_APPROXIMATION = 44100;
constexpr unsigned int MAX_SAMPLES_FOR_NON_STREAM_DATA = MINIMUM_DURATION_FOR_NON_STREAM * SAMPLE_APPROXIMATION;
constexpr unsigned int MINIMUM_SAMPLE_EXTRACTED = MINIMUM_DURATION_EXTRACTED * SAMPLE_APPROXIMATION;
constexpr unsigned int READ_CHUNK_SIZE = 16384;
constexpr std::array<ALenum, 3> SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr std::array<ALenum, 3> STREAM_SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO_FLOAT32, AL_FORMAT_MONO_FLOAT32};


SoundManager::SoundManager():
    mOpenALDevice{alcOpenDevice(nullptr)}
{
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

std::shared_ptr<OggSoundData> CreateData(const filesystem::path & aPath)
{
    std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateData(soundStream, soundStringId);
}

std::shared_ptr<OggSoundData> CreateData(const std::shared_ptr<std::istream> & aInputStream, const handy::StringId aSoundId)
{
    std::istreambuf_iterator<char> it{*aInputStream}, end;
    std::vector<std::uint8_t> data{it, end};
    int error = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    stb_vorbis * vorbisData = stb_vorbis_open_memory(data.data(), static_cast<int>(data.size()), &error, nullptr);
    stb_vorbis_info vorbisInfo = stb_vorbis_get_info(vorbisData);

    if (vorbisInfo.channels == 2)
    {
        spdlog::get("sounds")->warn("Do not load sound stereo sound without streaming. Only PointSound source should be loaded using CreatePointSound and PointSound cannot be stereo.");
    }

    float decoded[MAX_SAMPLES_FOR_NON_STREAM_DATA];
    int samplesRead = stb_vorbis_get_samples_float_interleaved(vorbisData, vorbisInfo.channels, decoded, MAX_SAMPLES_FOR_NON_STREAM_DATA);

    if (samplesRead == MAX_SAMPLES_FOR_NON_STREAM_DATA)
    {
        spdlog::get("sounds")->error("Read max samples for non stream data. File is probably too long for non streaming");
    }

    if (samplesRead == -1) {
        spdlog::get("sounds")->error("A read from the media returned an error");
    }
    
    std::shared_ptr<OggSoundData> resultSoundData = std::make_shared<OggSoundData>(OggSoundData{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .usedData = static_cast<std::streamsize>(data.size()),
        .fullyRead = true,
        .vorbisData = {vorbisData, &stb_vorbis_close},
        .vorbisInfo = vorbisInfo,
        .lengthDecoded = static_cast<std::size_t>(samplesRead) * vorbisInfo.channels,
        .fullyDecoded = true,
        .dataFormat = SOUNDS_AL_FORMAT[vorbisInfo.channels],
        .streamedData = false,
        .sampleRate = vorbisInfo.sample_rate,
        .decodedData = {decoded, decoded + samplesRead * vorbisInfo.channels},
    });

    std::chrono::steady_clock::time_point after = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = after - now;

    spdlog::get("sounds")->info("Samples: {}, total used bytes: {}, Elapsed time: {}, length decoded: {}", samplesRead, resultSoundData->usedData, diff.count(), resultSoundData->lengthDecoded * resultSoundData->vorbisInfo.channels);

    return resultSoundData;
}

//Streamed version of ogg data
std::shared_ptr<OggSoundData> CreateStreamedOggData(const filesystem::path & aPath)
{
    const std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    if (soundStream->fail())
    {
        spdlog::get("sounds")->error("File {} does not exists", aPath.string());
    }
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateStreamedOggData(soundStream, soundStringId);
}
std::shared_ptr<OggSoundData> CreateStreamedOggData(const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId)
{
    int used = 0;
    int error = 0;

    std::vector<char> headerData(HEADER_BLOCK_SIZE);

    aInputStream->read(headerData.data(), HEADER_BLOCK_SIZE);

    std::streamsize lengthRead = aInputStream->gcount();
    spdlog::get("sounds")->info("length read for header bytes {}", lengthRead);

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
                return std::make_shared<OggSoundData>(OggSoundData{
                    .soundId = aSoundId,
                    .dataStream = aInputStream,
                    .vorbisData = {nullptr, &stb_vorbis_close},
                });
            }
        }
    }

    spdlog::get("sounds")->info("Used bytes for header {}", used);

    stb_vorbis_info info = stb_vorbis_get_info(vorbisData);

    std::shared_ptr<OggSoundData> resultSoundData = std::make_shared<OggSoundData>(OggSoundData{
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
        .sampleRate = info.sample_rate,
    });

    return resultSoundData;
}

void decodeSoundData(const std::shared_ptr<OggSoundData> & aData)
{
    stb_vorbis * vorbisData = aData->vorbisData;

    std::istream & inputStream = *aData->dataStream;
    std::vector<char> & soundData = aData->undecodedReadData;

    int channels = 0;

    int used = static_cast<int>(aData->usedData);
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
                static_cast<int>(soundData.size()) - used, &channels, &output, &passSampleRead);

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
            std::streamsize lengthRead = inputStream.gcount();
            aData->lengthRead += lengthRead;

            if (lengthRead < READ_CHUNK_SIZE)
            {
                aData->fullyRead = true;
            }
        }

        if (aData->fullyRead && static_cast<std::size_t>(used) == aData->lengthRead)
        {
            spdlog::get("sounds")->info("Fully decoded");
            aData->fullyDecoded = true;
            break;
        }
    }

    aData->lengthDecoded += static_cast<std::size_t>(samplesRead) * channels;
    aData->usedData = used;

    std::chrono::steady_clock::time_point after = std::chrono::steady_clock::now();

    std::chrono::duration<double> diff = after - now;

    spdlog::get("sounds")->info("Samples: {}, total used bytes: {}, Elapsed time: {}, length decoded: {}", samplesRead, aData->usedData, diff.count(), aData->lengthDecoded * aData->vorbisInfo.channels);
}


void SoundManager::storeCueInMap(CueHandle aHandle, const std::shared_ptr<SoundCue> & aSoundCue)
{
    mCues.insert({aHandle, aSoundCue});
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

void SoundManager::update()
{
    for (auto & cueHandlePair : mCues)
    {
        std::shared_ptr<SoundCue> currentCue = cueHandlePair.second;
        if (currentCue->state != SoundCueState_NOT_PLAYING)
        {
            updateCue(currentCue);
        }
    }
}

void SoundManager::monitor()
{
    for (auto [handle, currentCue] : mCues)
    {
        ALint sourceState;
        alCall(alGetSourcei, currentCue->source, AL_SOURCE_STATE, &sourceState);
        spdlog::get("sounds")->trace("Source state {}", sourceState);
    }
}

void SoundManager::modifySound(ALuint aSource, SoundOption aOptions)
{
    alCall(alSourcef, aSource, AL_GAIN, aOptions.gain);
    //alCall(alSource3f, aSource, AL_POSITION, aOptions.position.x(), aOptions.position.y(),
    //       aOptions.position.z());
    //alCall(alSource3f, aSource, AL_VELOCITY, aOptions.velocity.x(), aOptions.velocity.y(),
    //       aOptions.velocity.z());
    //alCall(alSourcei, aSource, AL_LOOPING, aOptions.looping);
}

bool SoundManager::stopSound(ALuint aSource) { return alCall(alSourceStop, aSource); }

bool SoundManager::stopSound(handy::StringId /*aId*/)
{
    //if (mStoredSources.contains(aId)) {
    //    ALuint source = mStoredSources.at(aId);
    //    mStoredSources.erase(aId);
    //    return alCall(alSourceStop, source);
    //}
    return false;
}

bool SoundManager::pauseSound(ALuint aSource) { return alCall(alSourcePause, aSource); }

std::shared_ptr<OggSoundData> & SoundManager::getSoundData(handy::StringId aSoundId)
{
    return mLoadedSounds.at(aSoundId);
}

void SoundManager::storeDataInLoadedSound(const std::shared_ptr<OggSoundData> & aSoundData)
{
    mLoadedSounds.insert({aSoundData->soundId, aSoundData});
}


CueHandle SoundManager::createSoundCue(const std::vector<std::tuple<handy::StringId, SoundOption>> & aSoundList)
{
    std::shared_ptr<SoundCue> soundCue = std::make_shared<SoundCue>();
    int channels = 0;

    for (auto [soundId, option] : aSoundList)
    {
        std::shared_ptr<OggSoundData> soundData = getSoundData(soundId);
        if (channels == 0 || channels == soundData->vorbisInfo.channels)
        {
            channels = soundData->vorbisInfo.channels;

            std::shared_ptr<PlayingSound> sound = std::make_shared<PlayingSound>(
                    PlayingSound{soundData, option}
                    );

            soundCue->sounds.push_back(sound);
        }
        else
        {
            spdlog::get("sounds")->error("Cannot add sounds of different format on a cue");
        }
    }

    alCall(alGenSources, 1, &soundCue->source);

    currentHandle += 1;

    storeCueInMap(CueHandle{currentHandle}, soundCue);

    return {currentHandle};
}

void SoundManager::playSound(CueHandle aHandle)
{
    std::shared_ptr<SoundCue> soundCue = mCues.at(aHandle);

    std::shared_ptr<PlayingSound> sound = soundCue->sounds[soundCue->currentPlayingSoundIndex];
    std::shared_ptr<OggSoundData> data = sound->soundData;

    if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_EXTRACTED && !data->fullyDecoded)
    {
        decodeSoundData(data);
    }

    soundCue->state = SoundCueState_PLAYING;
    sound->state = PlayingSoundState_PLAYING;
    bufferPlayingSound(sound);
    alCall(alSourceQueueBuffers, soundCue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

    //empty staged buffers
    sound->stagedBuffers.resize(0);

    alCall(alSourcef, soundCue->source, AL_GAIN, 10.f);
    alCall(alSourcePlay, soundCue->source);
}

void SoundManager::bufferPlayingSound(const std::shared_ptr<PlayingSound> & aSound)
{
    std::list<ALuint> & freeBuffers = aSound->freeBuffers;
    std::shared_ptr<OggSoundData> data = aSound->soundData;

    if (freeBuffers.size() > 0)
    {
        auto bufIt = freeBuffers.begin();
        ALuint freeBuf = *bufIt;

        std::size_t nextPositionInData = 0;

        if (data->streamedData)
        {
        nextPositionInData = std::min(
                static_cast<std::size_t>(data->lengthDecoded),
                aSound->positionInData + static_cast<std::size_t>(MINIMUM_SAMPLE_EXTRACTED) * data->vorbisInfo.channels
                );
        }
        else
        {
            nextPositionInData = data->lengthDecoded;
        }

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
                data->vorbisInfo.channels == 1 ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32,
                data->decodedData.data() + aSound->positionInData,
                sizeof(float) * (nextPositionInData - aSound->positionInData),
                data->vorbisInfo.sample_rate
                );

        aSound->positionInData = nextPositionInData;
        bufIt = freeBuffers.erase(bufIt);
        aSound->stagedBuffers.push_back(freeBuf);

        if (nextPositionInData == data->lengthDecoded && data->fullyDecoded)
        {
            aSound->state = PlayingSoundState_STALE;
        }
    }
}

void SoundManager::updateCue(const std::shared_ptr<SoundCue> & currentCue)
{
    std::size_t currentWaitingForBufferSoundIndex = currentCue->currentWaitingForBufferSoundIndex;

    std::shared_ptr<PlayingSound> sound = currentCue->sounds[currentWaitingForBufferSoundIndex];

    ALuint source = currentCue->source;

    int bufferProcessed = 0;
    alCall(alGetSourceiv, source, AL_BUFFERS_PROCESSED, &bufferProcessed);

    std::list<ALuint> & freeBuffers = sound->freeBuffers;

    //add used buffer to freeBuffers list
    if (bufferProcessed > 0)
    {
        std::vector<ALuint> bufferUnqueued(bufferProcessed);
        alCall(alSourceUnqueueBuffers, source, bufferProcessed, bufferUnqueued.data());
        freeBuffers.insert(freeBuffers.end(), bufferUnqueued.begin(), bufferUnqueued.end());

        if (sound->state == PlayingSoundState_STALE && freeBuffers.size() == sound->buffers.size())
        {
            sound->state = PlayingSoundState_FINISHED;
            currentWaitingForBufferSoundIndex++;
        }
    }

    if (sound->state == PlayingSoundState_FINISHED &&
            currentCue->sounds.size() == ++currentCue->currentWaitingForBufferSoundIndex)
    {
        currentCue->state = SoundCueState_NOT_PLAYING;
    }

    if (currentCue->state == SoundCueState_PLAYING)
    {
        sound = currentCue->sounds[currentCue->currentPlayingSoundIndex];

        if (sound->state == PlayingSoundState_STALE)
        {
            if (currentCue->sounds.size() == ++currentCue->currentPlayingSoundIndex)
            {
                currentCue->state = SoundCueState_STALE;
            }
            else
            {
                sound = currentCue->sounds[currentCue->currentPlayingSoundIndex];
                sound->state = PlayingSoundState_PLAYING;
            }
        }

        std::shared_ptr<OggSoundData> data = sound->soundData;

        if (currentCue->state == SoundCueState_PLAYING)
        {
            //if (!data->fullyDecoded)
            if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_EXTRACTED && !data->fullyDecoded)
            {
                decodeSoundData(data);
            }

            if (sound->state == PlayingSoundState_PLAYING)
            {
                bufferPlayingSound(sound);
            }

            alCall(alSourceQueueBuffers, currentCue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

            //empty staged buffers
            sound->stagedBuffers.resize(0);
        }
    }
}

} // namespace sounds
} // namespace ad
