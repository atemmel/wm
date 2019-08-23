#pragma once
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>

namespace Logger {
	class DummyStream {
		public:
			DummyStream() = default;

			template<typename T>
			const DummyStream &operator << (const T &rhs) const {
				return *this;
			}

		private:
			DummyStream(const DummyStream &rhs) = delete;
			DummyStream(DummyStream &&rhs) = delete;
	};

	constexpr static DummyStream dummyStream;
};

#define CURRENT_LOCATION "File:" << __FILE__ << " Line:" << __LINE__ << ' '
#define DEBUG_STREAM std::cout
#define ERROR_STREAM std::cerr

#if LOG_DEBUG == 1
	#define LoggerDebug DEBUG_STREAM << CURRENT_LOCATION
#else
	#define LoggerDebug Logger::dummyStream
#endif
#if LOG_ERROR == 1
	#define LoggerError ERROR_STREAM << CURRENT_LOCATION
#else
	#define LoggerError Logger::dummyStream
#endif

#endif
