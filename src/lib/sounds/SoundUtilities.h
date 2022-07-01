#pragma once

#include <spdlog/spdlog.h>

#include <vorbis/vorbisfile.h>
#include <AL/al.h>
#include <AL/alc.h>

#include <chrono>
#include <thread>
#include <cstdint>
#include <iostream>
#include <string>
#include <map>

namespace ad {
namespace sounds {
    
//Macro to get the file and line where the openAL call is made
#define alCall(function, ...) alCallImpl(__FILE__, __LINE__, function, __VA_ARGS__)
#define alcCall(function, device, ...) alcCallImpl(__FILE__, __LINE__, function, device, __VA_ARGS__)


//Helper function to help handle openAL error which can be confusing
bool check_al_errors(const std::string & filename, const std::uint_fast32_t line);

//These template are here to handle the openAL function that returns non void values
template<typename alFunction, typename... Params>
static auto alCallImpl(const char* filename,
    const std::uint_fast32_t line,
    alFunction function,
    Params... params)
    ->typename std::enable_if_t<!std::is_same_v<void, decltype(function(params...))>, decltype(function(params...))>
{
    auto ret = function(std::forward<Params>(params)...);
    check_al_errors(filename, line);
    return ret;
}
//And void values
template<typename alFunction, typename... Params>
static auto alCallImpl(const char* filename,
    const std::uint_fast32_t line,
    alFunction function,
    Params... params)
    ->typename std::enable_if_t<std::is_same_v<void, decltype(function(params...))>, bool>
{
    function(std::forward<Params>(params)...);
    return check_al_errors(filename, line);
}

//Helper function to help handle openAL context error which can be confusing
//The code error are unfortunately different for context and not context
bool check_alc_errors(const std::string& filename, const std::uint_fast32_t line, ALCdevice* device);

//These template are here to handle the openAL function that returns non void values
template<typename alcFunction, typename ReturnType, typename... Params>
static auto alcCallImpl(const char* filename,
                 const std::uint_fast32_t line,
                 alcFunction function,
                 ReturnType& returnValue,
                 ALCdevice* device, 
                 Params... params)
->typename std::enable_if_t<!std::is_same_v<void,decltype(function(params...))>,bool>
{
    returnValue = function(std::forward<Params>(params)...);
    return check_alc_errors(filename,line,device);
}

//And void values
template<typename alcFunction, typename... Params>
static auto alcCallImpl(const char* filename, 
                 const std::uint_fast32_t line, 
                 alcFunction function, 
                 ALCdevice* device, 
                 Params... params)
->typename std::enable_if_t<std::is_same_v<void,decltype(function(params...))>,bool>
{
    function(std::forward<Params>(params)...);
    return check_alc_errors(filename,line,device);
}

} // namespace sounds
} // namespace ad
