#include "SoundManager.h"

#include <AL/al.h>
#include <cstddef>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>

namespace ad {
namespace sounds {

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

std::shared_ptr<PointSoundData> CreateMonoData(const filesystem::path & aPath)
{
    std::shared_ptr<std::ifstream> soundStream = std::make_shared<std::ifstream>(aPath.string(), std::ios::binary);
    handy::StringId soundStringId{aPath.stem().string()};
    return CreateMonoData(soundStream, soundStringId);
}

std::shared_ptr<PointSoundData> CreateMonoData(const std::shared_ptr<std::istream> & aInputStream, const handy::StringId aSoundId)
{
    short * decoded = {};
    int channels = 0;
    int sampleRate = 0;
    std::istreambuf_iterator<char> it{*aInputStream}, end;
    std::vector<std::uint8_t> data{it, end};
    int len = stb_vorbis_decode_memory(data.data(), data.size(), &channels, &sampleRate,
                                       &decoded);
    std::shared_ptr<PointSoundData> resultSoundData = std::make_shared<PointSoundData>(PointSoundData{
        .soundId = aSoundId,
        .dataStream = aInputStream,
        .fullyRead = true,
        .vorbisData = {nullptr, &stb_vorbis_close},
        .lengthDecoded = static_cast<std::size_t>(len),
        .fullyDecoded = true,
        .dataFormat = SOUNDS_AL_FORMAT[channels],
        .sampleRate = sampleRate,
    });

    if (len == -1) {
        spdlog::get("sounds")->error("A read from the media returned an error");
    }

    if (channels == 2)
    {
        spdlog::get("sounds")->error("Do not load sound stereo sound without streaming. Only PointSound source should be loaded using CreatePointSound and PointSound cannot be stereo.");
    }
    

    resultSoundData->decodedData = {decoded, decoded + len};

    return resultSoundData;
}

void SoundManager::storeCueInMap(PointSoundCueHandle aHandle, std::shared_ptr<PointSoundCue> aSoundCue)
{
    mPointCues.insert({aHandle, aSoundCue});
}

void SoundManager::storeCueInMap(TwoDSoundCueHandle aHandle, std::shared_ptr<TwoDSoundCue> aSoundCue)
{
    mTwoDCues.insert({aHandle, aSoundCue});
}

template<>
std::shared_ptr<PointSoundCue> SoundManager::getSoundCue(PointSoundCueHandle aCue)
{
    return mPointCues.at(aCue);
}

template<>
std::shared_ptr<TwoDSoundCue> SoundManager::getSoundCue(TwoDSoundCueHandle aCue)
{
    return mTwoDCues.at(aCue);
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
    for (auto & cueHandlePair : mPointCues)
    {
        std::shared_ptr<PointSoundCue> currentCue = cueHandlePair.second;
        updateCue(currentCue);
    }
    for (auto & cueHandlePair : mTwoDCues)
    {
        std::shared_ptr<TwoDSoundCue> currentCue = cueHandlePair.second;
        updateCue(currentCue);
    }
}

void SoundManager::monitor()
{
    for (auto [handle, currentQueue] : mPointCues)
    {
        ALint sourceState;
        alCall(alGetSourcei, currentQueue->source, AL_SOURCE_STATE, &sourceState);
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

template<>
std::shared_ptr<PointSoundData> & SoundManager::getSoundData(handy::StringId aSoundId)
{
    return mLoadedPointSounds.at(aSoundId);
}

template<>
std::shared_ptr<TwoDSoundData> & SoundManager::getSoundData(handy::StringId aSoundId)
{
    return mLoadedTwoDSounds.at(aSoundId);
}

template<>
void SoundManager::storeDataInLoadedSound(std::shared_ptr<PointSoundData> aSoundData)
{
    mLoadedPointSounds.insert({aSoundData->soundId, aSoundData});
}

template<>
void SoundManager::storeDataInLoadedSound(std::shared_ptr<TwoDSoundData> aSoundData)
{
    mLoadedTwoDSounds.insert({aSoundData->soundId, aSoundData});
}

} // namespace sounds
} // namespace ad
