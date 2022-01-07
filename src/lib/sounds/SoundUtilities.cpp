#include "SoundUtilities.h"

namespace ad {
namespace sounds {

bool check_al_errors(const std::string & filename, const std::uint_fast32_t line)
{
    ALenum error = alGetError();
    if(error != AL_NO_ERROR)
    {
        spdlog::get("sounds")->error("***ERROR*** ({}: {})", filename, line);
        switch(error)
        {
        case AL_INVALID_NAME:
            spdlog::get("sounds")->error("AL_INVALID_NAME: a bad name (ID) was passed to an OpenAL function");
            break;
        case AL_INVALID_ENUM:
            spdlog::get("sounds")->error("AL_INVALID_ENUM: an invalid enum value was passed to an OpenAL function");
            break;
        case AL_INVALID_VALUE:
            spdlog::get("sounds")->error("AL_INVALID_VALUE: an invalid value was passed to an OpenAL function");
            break;
        case AL_INVALID_OPERATION:
            spdlog::get("sounds")->error("AL_INVALID_OPERATION: the requested operation is not valid");
            break;
        case AL_OUT_OF_MEMORY:
            spdlog::get("sounds")->error("AL_OUT_OF_MEMORY: the requested operation resulted in OpenAL running out of memory");
            throw "No more openAL memory for you";
            break;
        default:
            spdlog::get("sounds")->error("UNKNOWN AL ERROR: {}", error);
        }
        return false;
    }
    return true;
}

bool check_alc_errors(const std::string& filename, const std::uint_fast32_t line, ALCdevice* device)
{
    ALCenum error = alcGetError(device);
    if(error != ALC_NO_ERROR)
    {
        spdlog::get("sounds")->error("***ERROR*** ({}: {})", filename, line);
        switch(error)
        {
        case ALC_INVALID_VALUE:
            spdlog::get("sounds")->error("ALC_INVALID_VALUE: an invalid value was passed to an OpenAL function");
            break;
        case ALC_INVALID_DEVICE:
            spdlog::get("sounds")->error("ALC_INVALID_DEVICE: a bad device was passed to an OpenAL function");
            break;
        case ALC_INVALID_CONTEXT:
            spdlog::get("sounds")->error("ALC_INVALID_CONTEXT: a bad context was passed to an OpenAL function");
            break;
        case ALC_INVALID_ENUM:
            spdlog::get("sounds")->error("ALC_INVALID_ENUM: an unknown enum value was passed to an OpenAL function");
            break;
        case ALC_OUT_OF_MEMORY:
            spdlog::get("sounds")->error("ALC_OUT_OF_MEMORY: an unknown enum value was passed to an OpenAL function");
            break;
        default:
            spdlog::get("sounds")->error("UNKNOWN ALC ERROR: {}", error);
        }
        return false;
    }
    return true;
}

} //sounds
} //ad
