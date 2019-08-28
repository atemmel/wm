#pragma once
#ifndef LOG_HPP
#define LOG_HPP

#include <iostream>

//Define LOG_DEBUG and LOG_ERROR before including this header

namespace Logger {
	//Dummy class that the compiler ideally will remove
	class DummyStream {
		public:
			DummyStream() = default;

			template<typename T>
			const DummyStream &operator << (const T &rhs) const {
				return *this;	//Just allow the operation
			}

		private:
			DummyStream(const DummyStream &rhs) = delete;
			DummyStream(DummyStream &&rhs) = delete;
	};

	constexpr static DummyStream dummyStream;
}

#define CURRENT_LOCATION "File:" << __FILE__ << " Line:" << __LINE__ << ' '
#define DEBUG_STREAM std::cout
#define ERROR_STREAM std::cerr

#if LOG_DEBUG == 1
	#define LogDebug DEBUG_STREAM << CURRENT_LOCATION
#else
	#define LogDebug Logger::dummyStream
#endif
#if LOG_ERROR == 1
	#define LogError ERROR_STREAM << CURRENT_LOCATION
#else
	#define LogError Logger::dummyStream
#endif

#endif
