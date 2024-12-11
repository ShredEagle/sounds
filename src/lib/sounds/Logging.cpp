#include "Logging.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <mutex>


namespace ad::sounds {


namespace {

    std::once_flag loggingInitializationOnce;

    void initializeLogging_impl()
    {
        // Intended for the main game messages
        spdlog::stdout_color_mt(gMainLogger);
    }

} // namespace anonymous


void initializeLogging()
{
    std::call_once(loggingInitializationOnce, &initializeLogging_impl);
    //Note: Initialize all upstream libraries logging here too.
};


} // namespace ad::sounds
