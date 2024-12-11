#pragma once


namespace ad::sounds {


constexpr const char * gMainLogger = "sounds";


/// \brief Safe initialization of the logger, guaranteed to happen only once.
/// Must be called before any logging operation take place.
void initializeLogging();


} // namespace ad::sounds