#pragma once

// Defining basic utility for throwing exceptions
// with an embedded stacktrace.
//
// This was taken from boost docs:
// https://www.boost.org/doc/libs/1_69_0/doc/html/stacktrace/getting_started.html#stacktrace.getting_started.exceptions_with_stacktrace

#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp>

typedef boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace> traced;

template <class E>
void throw_with_trace(const E& e) 
{
	throw boost::enable_error_info(e)
		<< traced(boost::stacktrace::stacktrace());
}
