//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   The command-line interface (spec 4). ParseCommandLine turns the arguments into CommandLineOptions or a single
//   error message, and UsageText holds the help text. Neither prints anything, because main does all of the output.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>

// The options of one run (spec 4.2).

struct CommandLineOptions
{
	bool          show_help = false;                        // --help or -h was given.
	bool          has_seed  = false;                        // --seed was given.
	std::uint64_t seed      = 0;                            // The --seed value, when has_seed is true.
};

// Parses the arguments that follow the program name. Returns false, with error_message set, at the first error.

bool ParseCommandLine
(
	int                 argument_count,
	const char* const   argument_values [],
	CommandLineOptions& options,
	std::string&        error_message
);

// The usage text of spec 4.5, ending in a newline.

const char* UsageText ();
