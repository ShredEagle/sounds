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
constexpr float MINIMUM_DURATION_BUFFERED_ON_CREATION = 0.2f;
constexpr float MINIMUM_DURATION_EXTRACTED = 0.5f;
constexpr float MAXIMUM_DURATION_FOR_NON_STREAM = 10.f;
constexpr unsigned int SAMPLE_APPROXIMATION = 44100;
constexpr unsigned int MAX_SAMPLES_FOR_NON_STREAM_DATA = MAXIMUM_DURATION_FOR_NON_STREAM * SAMPLE_APPROXIMATION;
constexpr unsigned int MINIMUM_SAMPLE_BUFFERED_ON_CREATION = MINIMUM_DURATION_BUFFERED_ON_CREATION * SAMPLE_APPROXIMATION;
constexpr unsigned int MINIMUM_SAMPLE_EXTRACTED = MINIMUM_DURATION_EXTRACTED * SAMPLE_APPROXIMATION;
constexpr unsigned int READ_CHUNK_SIZE = 16384.f * MINIMUM_DURATION_EXTRACTED * 2.f;
constexpr std::array<ALenum, 3> SOUNDS_AL_FORMAT = {0, AL_FORMAT_MONO_FLOAT32, AL_FORMAT_MONO_FLOAT32};

template<>
SoundCue * Handle<SoundCue>::toObject() const
{
    SoundCue * cue = mCues.at(*this).get();

    if (cue != nullptr && cue->id == mUniqueId)
    {
        return cue;
    }

    return nullptr;
}

template<>
PlayingSoundCue * Handle<PlayingSoundCue>::toObject() const
{
    PlayingSoundCue * cue = mPlayingCues.at(*this).get();

    if (cue != nullptr && cue->id == mUniqueId)
    {
        return cue;
    }

    return nullptr;
}

bool CmpHandlePriority(const Handle<PlayingSoundCue> & lhs, const Handle<PlayingSoundCue> & rhs)
{
    return lhs.toObject()->priority < rhs.toObject()->priority;
}


SoundManager::SoundManager(std::vector<SoundCategory> && aCategories):
    mOpenALDevice{alcOpenDevice(nullptr)},
    mOpenALContext{nullptr},
    mContextIsCurrent{AL_FALSE},
    mSources{}
{
    if (!mOpenALDevice) {
        /* fail */
        spdlog::get("sounds")->error("Cannot open OpenAL sound device");
    } else {
        if (!alcCall(alcCreateContext, mOpenALContext, mOpenALDevice, mOpenALDevice,
                     nullptr)) {
            spdlog::get("sounds")->error("Cannot create OpenAL context");
        } else {
            if (!alcCall(alcMakeContextCurrent, mContextIsCurrent, mOpenALDevice,
                         mOpenALContext)) {
                spdlog::get("sounds")->error("Cannot set OpenAL to current context");
            }
        }
    }

    alCall(alGenSources, MAX_SOURCES, mSources.data());

    int i = 0;
    for (ALuint source : mSources)
    {
        alCall(alSourcei, source, AL_SOURCE_RELATIVE, AL_TRUE);
        mFreeSources.push_back(i++);
    }

    alCall(alListener3f, AL_POSITION, 0.f, 0.f, 0.f);

    mCategoryOptions.insert({MASTER_SOUND_CATEGORY, {}});

    for (int category : aCategories)
    {
        if (category == MASTER_SOUND_CATEGORY)
        {
            spdlog::get("sounds")->error("Can't add a category in place of MASTER_SOUND_CATEGORY ({})", MASTER_SOUND_CATEGORY);
        }

        mCuesByCategories.insert({category, {}});
        mCategoryOptions.insert({category, {}});
    }
}

SoundManager::~SoundManager()
{
    if (mContextIsCurrent) {
        if (!alcCall(alcMakeContextCurrent, mContextIsCurrent, mOpenALDevice, nullptr)) {
            spdlog::get("sounds")->error("Well we're leaking audio memory now");
        }

        if (!alcCall(alcDestroyContext, mOpenALDevice, mOpenALContext)) {
            spdlog::get("sounds")->error("Well we're leaking audio memory now");
        }

        ALCboolean closed;
        if (!alcCall(alcCloseDevice, closed, mOpenALDevice, mOpenALDevice)) {
            spdlog::get("sounds")->error("Device just disappeared and I don't know why");
        }
    }
}

handy::StringId SoundManager::createData(const filesystem::path & aPath)
{
    std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    handy::StringId soundStringId = ad::handy::internalizeString(aPath.stem().string());
    return createData(soundStream, soundStringId);
}

handy::StringId SoundManager::createData(
        const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId)
{
    std::istreambuf_iterator<char> it{*aInputStream}, end;
    std::vector<std::uint8_t> data{it, end};
    int error = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    stb_vorbis * vorbisData = stb_vorbis_open_memory(
            data.data(), static_cast<int>(data.size()), &error, nullptr);
    stb_vorbis_info vorbisInfo = stb_vorbis_get_info(vorbisData);

    if (vorbisInfo.channels == 2)
    {
        spdlog::get("sounds")->warn("Do not load stereo sound without streaming. Only mono source should be loaded using CreatePointSound and PointSound cannot be stereo.");
    }

    float decoded[MAX_SAMPLES_FOR_NON_STREAM_DATA];
    int samplesRead = stb_vorbis_get_samples_float_interleaved(
            vorbisData, vorbisInfo.channels, decoded, MAX_SAMPLES_FOR_NON_STREAM_DATA);

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

    mLoadedSounds.insert({resultSoundData->soundId, resultSoundData});

    return resultSoundData->soundId;
}

//Streamed version of ogg data
handy::StringId SoundManager::createStreamedOggData(const filesystem::path & aPath)
{
    const std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    if (soundStream->fail())
    {
        spdlog::get("sounds")->error("File {} does not exists", aPath.string());
    }
    handy::StringId soundStringId = ad::handy::internalizeString(aPath.stem().string());
    return createStreamedOggData(soundStream, soundStringId);
}

handy::StringId SoundManager::createStreamedOggData(
        const std::shared_ptr<std::istream> & aInputStream, handy::StringId aSoundId)
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
            stb_vorbis_open_pushdata(
                    reinterpret_cast<unsigned char *>(headerData.data()),
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

                return handy::StringId::Null();
            }
        }
    }

    spdlog::get("sounds")->info("Used bytes for header {}", used);

    stb_vorbis_info info = stb_vorbis_get_info(vorbisData);

    spdlog::get("sounds")->info("Number of channels {}", info.channels);

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
        .dataFormat = SOUNDS_AL_FORMAT[info.channels],
        .streamedData = true,
        .sampleRate = info.sample_rate,
    });

    mLoadedSounds.insert({resultSoundData->soundId, resultSoundData});

    return resultSoundData->soundId;
}

void decodeSoundData(
        const std::shared_ptr<OggSoundData> & aData,
        unsigned int aMinSamples = MINIMUM_SAMPLE_EXTRACTED)
{
    stb_vorbis * vorbisData = aData->vorbisData;

    std::istream & inputStream = *aData->dataStream;
    std::vector<char> & soundData = aData->undecodedReadData;

    int channels = 0;

    int used = static_cast<int>(aData->usedData);
    float ** output;
    int samplesRead = 0;

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    while (samplesRead < static_cast<int>(aMinSamples)) {
        int currentUsed = -1;

        while (currentUsed != 0)
        {
            int passSampleRead = 0;

            currentUsed = stb_vorbis_decode_frame_pushdata(
                vorbisData, reinterpret_cast<unsigned char *>(soundData.data() + used),
                static_cast<int>(soundData.size()) - used,
                &channels, &output, &passSampleRead);

            used += currentUsed;

            samplesRead += passSampleRead;

            if (passSampleRead > 0)
            {
                if (channels == 2)
                {
                    std::vector<float> interleavedData = interleave(
                            output[0], output[1], passSampleRead);
                    aData->decodedData.insert(
                            aData->decodedData.end(),
                            interleavedData.begin(), interleavedData.end());
                }
                else
                {
                    aData->decodedData.insert(
                            aData->decodedData.end(),
                            &output[0][0], &output[0][passSampleRead]);
                }
            }
        }

        if (!aData->fullyRead)
        {
            std::array<char, READ_CHUNK_SIZE> moreHeaderData;
            inputStream.read(moreHeaderData.data(), READ_CHUNK_SIZE);
            soundData.insert(
                    soundData.end(), moreHeaderData.begin(), moreHeaderData.end());
            std::streamsize lengthRead = inputStream.gcount();
            spdlog::get("sounds")->info("Reading new chunk from {} to {}", aData->lengthRead, aData->lengthRead + lengthRead);
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


ALint SoundManager::getSourceState(ALuint aSource)
{
    ALint sourceState;
    alCall(alGetSourcei, aSource, AL_SOURCE_STATE, &sourceState);
    return sourceState;
}

void SoundManager::update()
{
    spdlog::get("sounds")->trace("# free sources: {}", mFreeSources.size());
    int realPlayingSound = 0;
    for (auto & [handle, sound] : mPlayingCues)
    {
        if (sound.get() != nullptr)
        {
            realPlayingSound++;
        }
    }

    spdlog::get("sounds")->trace("# playing sound: {}", realPlayingSound);
    spdlog::get("sounds")->trace("# number of prioriry queue: {}", mCuesByCategories.size());

    for (auto & [cat, queue] : mCuesByCategories)
    {
        spdlog::get("sounds")->trace("# number sound in priority queue {}: {}", cat, queue.size());
    }

    for (const auto & [handle, currentCue] : mPlayingCues)
    {
        if (currentCue != nullptr && currentCue->state != PlayingSoundCueState_NOT_PLAYING)
        {
            updateCue(*currentCue, handle);
        }
    }
}

void SoundManager::monitor()
{
    for (const auto & [handle, currentCue] : mPlayingCues)
    {
        ALint sourceState;
        alCall(alGetSourcei, currentCue->source, AL_SOURCE_STATE, &sourceState);
        spdlog::get("sounds")->trace("Source state {}", sourceState);
    }
}

bool SoundManager::interruptSound(const Handle<PlayingSoundCue> & aHandle)
{
    PlayingSoundCue * cue = aHandle.toObject();
    if (cue != nullptr)
    {
        if (cue->interruptSound != nullptr)
        {
            //Free buffer of waiting and pending sound
            std::shared_ptr<PlayingSound> waitingSound = cue->getWaitingSound();
            waitingSound->stagedBuffers.resize(0);
            waitingSound->freeBuffers.assign(waitingSound->buffers.begin(), waitingSound->buffers.end());
            std::shared_ptr<PlayingSound> playingSound = cue->getPlayingSound();
            playingSound->stagedBuffers.resize(0);
            playingSound->freeBuffers.assign(waitingSound->buffers.begin(), waitingSound->buffers.end());

            cue->state = PlayingSoundCueState_INTERRUPTED;
            std::shared_ptr<PlayingSound> sound = cue->interruptSound;
            std::shared_ptr<OggSoundData> data = sound->soundData;
            if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_BUFFERED_ON_CREATION && !data->fullyDecoded)
            {
                decodeSoundData(data, MINIMUM_SAMPLE_BUFFERED_ON_CREATION);
            }
            bufferPlayingSound(sound);
            //Stop source to swap buffer
            alCall(alSourceStop, cue->source);
            //Clean buffer queue to avoid processing of interrupted sound
            alCall(alSourcei, cue->source, AL_BUFFER, NULL);
            alCall(alSourceQueueBuffers, cue->source, 1, sound->stagedBuffers.data());
            return alCall(alSourcePlay, cue->source);

            sound->stagedBuffers.erase(sound->stagedBuffers.begin());
            if (sound->stagedBuffers.size() > 0)
            {
                alCall(alSourceQueueBuffers, cue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());
            }
        }
        else
        {
            return stopSound(aHandle);
        }
    }

    return false;
}

bool SoundManager::stopSound(const Handle<PlayingSoundCue> & aHandle)
{

    PlayingSoundCue * cue = aHandle.toObject();

    if (cue != nullptr)
    {
        PlayingSoundCueQueue & cueQueue = mCuesByCategories.at(cue->category);
        std::erase(cueQueue, aHandle);
        std::make_heap(cueQueue.begin(), cueQueue.end(), CmpHandlePriority);

        for (std::size_t i = 0; i < mSources.size(); i++)
        {
            if (mSources.at(i) == cue->source)
            {
                mFreeSources.push_back(i);
                break;
            }
        }


        bool result = alCall(alSourceStop, cue->source);
        alCall(alSourcei, cue->source, AL_BUFFER, NULL);
        mPlayingCues.at(aHandle) = nullptr;
        return result;
    }

    return false;
}

void SoundManager::stopCategory(SoundCategory aSoundCategory)
{
    const PlayingSoundCueQueue & soundQueue = mCuesByCategories.at(aSoundCategory);

    for (const Handle<PlayingSoundCue> & handle : soundQueue)
    {
        stopSound(handle);
    }
}

void SoundManager::stopAllSound()
{
    for (const auto & [handle, sound]: mPlayingCues)
    {
        stopSound(handle);
    }
}

bool SoundManager::pauseSound(const Handle<PlayingSoundCue> & aHandle) {
    PlayingSoundCue * cue = aHandle.toObject();
    if (cue != nullptr)
    {
        return alCall(alSourcePause, cue->source);
    }

    return false;
}

std::vector<Handle<PlayingSoundCue>> SoundManager::pauseCategory(SoundCategory aSoundCategory)
{
    std::vector<Handle<PlayingSoundCue>> result;
    const PlayingSoundCueQueue & soundQueue = mCuesByCategories.at(aSoundCategory);

    for (const Handle<PlayingSoundCue> & handle : soundQueue)
    {
        if(pauseSound(handle))
        {
            result.push_back(handle);
        }
    }

    return result;
}

std::vector<Handle<PlayingSoundCue>> SoundManager::pauseAllSound()
{
    std::vector<Handle<PlayingSoundCue>> result;

    for (const auto & [handle, sound]: mPlayingCues)
    {
        if(pauseSound(handle))
        {
            result.push_back(handle);
        }
    }

    return result;
}

bool SoundManager::startSound(const Handle<PlayingSoundCue> & aHandle)
{
    PlayingSoundCue * cue = aHandle.toObject();

    if (cue != nullptr)
    {
        return alCall(alSourcePlay, cue->source);
    }

    return false;
}

void SoundManager::startCategory(SoundCategory aSoundCategory)
{
    const PlayingSoundCueQueue & soundQueue = mCuesByCategories.at(aSoundCategory);

    for (const Handle<PlayingSoundCue> & handle : soundQueue)
    {
        startSound(handle);
    }
}

void SoundManager::startAllSound()
{
    for (const auto & [handle, sound]: mPlayingCues)
    {
        startSound(handle);
    }
}

Handle<SoundCue> SoundManager::createSoundCue(
        const std::vector<std::pair<handy::StringId, CueElementOption>> & aSoundList,
        SoundCategory aCategory,
        int aPriority,
        const handy::StringId & aInterruptSoundId
        )
{
    std::size_t handleIndex = 0;
    for (const auto & [handle, cue] : mCues)
    {
        if (cue == nullptr)
        {
            break;
        }
        handleIndex++;
    }

    std::unique_ptr<SoundCue> soundCue = std::make_unique<SoundCue>(
            mCurrentCueId++,
            handleIndex,
            aCategory,
            aPriority
            );
    int channels = 0;

    for (auto [soundId, option] : aSoundList)
    {
        std::shared_ptr<OggSoundData> soundData = mLoadedSounds.at(soundId);
        if (channels == 0 || channels == soundData->vorbisInfo.channels)
        {
            channels = soundData->vorbisInfo.channels;

            soundCue->sounds.push_back({soundData, option});
        }
        else
        {
            spdlog::get("sounds")->error("Cannot add sounds of different format on a cue");
        }
    }

    if (aInterruptSoundId != handy::StringId::Null())
    {
        std::shared_ptr<OggSoundData> interruptSoundData = mLoadedSounds.at(aInterruptSoundId);
        if (channels == 0 || channels == interruptSoundData->vorbisInfo.channels)
        {
            soundCue->interruptSound = interruptSoundData;
        }
        else
        {
            spdlog::get("sounds")->error("Cannot add sounds of different format on a cue");
        }
    }

    Handle<SoundCue> handle{soundCue};
    mCues.insert_or_assign(handle, std::move(soundCue));
    mPlayingCuesByCue.insert_or_assign(handle, std::vector<Handle<PlayingSoundCue>>{});

    return handle;
}

Handle<PlayingSoundCue> SoundManager::playSound(const Handle<SoundCue> & aHandle)
{
    SoundCue & soundCue = *mCues.at(aHandle);

    PlayingSoundCueQueue & priorityQueue = mCuesByCategories.at(soundCue.category);

    std::vector<Handle<PlayingSoundCue>> & alreadyPlayingCue = mPlayingCuesByCue.at(aHandle);

    if (alreadyPlayingCue.size() == MAX_SOURCE_PER_CUE)
    {
        spdlog::get("sounds")->trace("Not playing because too much already");
        return Handle<PlayingSoundCue>();

        //TODO(franz): here we should try to remove the less loud sound including the new
    }

    if (mFreeSources.size() == 0) [[unlikely]]
    {
        if(!priorityQueue.empty())
        {
            std::pop_heap(priorityQueue.begin(), priorityQueue.end(), CmpHandlePriority);
            Handle<PlayingSoundCue> lessPriorizedHandle = priorityQueue.back();

            PlayingSoundCue * lessPriorizedCue = lessPriorizedHandle.toObject();

            if (lessPriorizedCue->priority <= soundCue.priority)
            {
                return Handle<PlayingSoundCue>();
            }

            if (lessPriorizedHandle.mHandleIndex >= 0)
            {
                stopSound(lessPriorizedHandle);
            }
        }
        else
        {
            return Handle<PlayingSoundCue>();
        }

    }

    std::size_t sourceIndex = mFreeSources.back();
    mFreeSources.pop_back();
    ALuint source = mSources.at(sourceIndex);

    std::size_t handleIndex = 0;
    for (auto & [handle, cue] : mPlayingCues)
    {
        if (cue == nullptr)
        {
            break;
        }
        handleIndex++;
    }

    std::unique_ptr<PlayingSoundCue> playingCue = std::make_unique<PlayingSoundCue>(soundCue, source, mCurrentCueId++, handleIndex);

    std::shared_ptr<PlayingSound> sound = playingCue->sounds[playingCue->currentPlayingSoundIndex];
    std::shared_ptr<OggSoundData> data = sound->soundData;

    if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_BUFFERED_ON_CREATION && !data->fullyDecoded)
    {
        decodeSoundData(data, MINIMUM_SAMPLE_BUFFERED_ON_CREATION);
    }

    playingCue->state = PlayingSoundCueState_PLAYING;
    sound->state = PlayingSoundState_PLAYING;
    bufferPlayingSound(sound);
    alCall(alSourceQueueBuffers, playingCue->source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

    //empty staged buffers
    sound->stagedBuffers.resize(0);

    alCall(alSourcePlay, playingCue->source);

    Handle<PlayingSoundCue> handle{playingCue};
    mPlayingCues.insert_or_assign(handle, std::move(playingCue));

    priorityQueue.push_back(handle);
    std::push_heap(priorityQueue.begin(), priorityQueue.end(), CmpHandlePriority);

    alreadyPlayingCue.push_back(handle);

    return handle;
}

void bufferPlayingSound(const std::shared_ptr<PlayingSound> & aSound)
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

        alCall(
                alBufferData,
                freeBuf,
                SOUNDS_AL_FORMAT[data->vorbisInfo.channels],
                data->decodedData.data() + aSound->positionInData,
                sizeof(float) * (nextPositionInData - aSound->positionInData),
                data->vorbisInfo.sample_rate
                );

        aSound->positionInData = nextPositionInData;
        bufIt = freeBuffers.erase(bufIt);
        aSound->stagedBuffers.push_back(freeBuf);

        if (nextPositionInData == data->lengthDecoded && data->fullyDecoded)
        {
            if (aSound->loops == 0)
            {
                aSound->state = PlayingSoundState_STALE;
            }
            else
            {
                aSound->loops--;
                aSound->positionInData = 0;
            }
        }
    }
}

void SoundManager::updateCue(PlayingSoundCue & currentCue, const Handle<PlayingSoundCue> & aHandle)
{
    std::shared_ptr<PlayingSound> sound = currentCue.getWaitingSound();

    ALuint source = currentCue.source;

    int bufferProcessed = 0;
    alCall(alGetSourceiv, source, AL_BUFFERS_PROCESSED, &bufferProcessed);

    std::list<ALuint> & freeBuffers = sound->freeBuffers;

    //updating position and velocity
    SoundOption option = currentCue.option;
    alCall(alSource3f, source, AL_POSITION, option.position.x(), option.position.y(), option.position.z());
    alCall(alSource3f, source, AL_VELOCITY, option.velocity.x(), option.velocity.y(), option.velocity.z());

    CategoryOption catOption = mCategoryOptions.at(currentCue.category);
    CategoryOption masterOption = mCategoryOptions.at(MASTER_SOUND_CATEGORY);
    alCall(
            alSourcef,
            source,
            AL_GAIN,
            option.gain * catOption.userGain * catOption.gameGain * masterOption.userGain * masterOption.gameGain
            );

    //add used buffer to freeBuffers list
    if (bufferProcessed > 0)
    {
        std::vector<ALuint> bufferUnqueued(bufferProcessed);
        alCall(alSourceUnqueueBuffers, source, bufferProcessed, bufferUnqueued.data());
        freeBuffers.insert(freeBuffers.end(), bufferUnqueued.begin(), bufferUnqueued.end());

        if (sound->state == PlayingSoundState_STALE && freeBuffers.size() == sound->buffers.size())
        {
            sound->state = PlayingSoundState_FINISHED;
            currentCue.currentWaitingForBufferSoundIndex++;
            if (currentCue.currentWaitingForBufferSoundIndex < currentCue.sounds.size())
            {
                sound = currentCue.getWaitingSound();
            }
        }
    }

    if (sound->state == PlayingSoundState_FINISHED)
    {
        currentCue.state = PlayingSoundCueState_NOT_PLAYING;

        stopSound(aHandle);
        return;
    }

    if (currentCue.state == PlayingSoundCueState_PLAYING)
    {
        sound = currentCue.getPlayingSound();

        if (sound->state == PlayingSoundState_STALE)
        {
            if (currentCue.sounds.size() == currentCue.currentPlayingSoundIndex + 1)
            {
                currentCue.state = PlayingSoundCueState_STALE;
            }
            else
            {
                sound = currentCue.sounds[++currentCue.currentPlayingSoundIndex];
                sound->state = PlayingSoundState_PLAYING;
            }
        }


        std::shared_ptr<OggSoundData> data = sound->soundData;

        if (currentCue.state == PlayingSoundCueState_PLAYING)
        {
            if (data->lengthDecoded < sound->positionInData + MINIMUM_SAMPLE_EXTRACTED && !data->fullyDecoded)
            {
                decodeSoundData(data);
            }

            if (sound->state == PlayingSoundState_PLAYING)
            {
                bufferPlayingSound(sound);
            }

            alCall(alSourceQueueBuffers, currentCue.source, sound->stagedBuffers.size(), sound->stagedBuffers.data());

            //empty staged buffers
            sound->stagedBuffers.resize(0);
        }
    }
}

const SoundManagerInfo SoundManager::getInfo()
{
    return {
        mPlayingCues,
        mSources,
        mFreeSources,
        mLoadedSounds,
    };
}

} // namespace sounds
} // namespace ad
