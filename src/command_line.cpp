//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements the command-line interface (spec 4). The seed is parsed by hand, one digit at a time, because
//   std::stoull accepts leading spaces and a sign, and reads "-1" as 18446744073709551615 (development plan, RK-06).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "command_line.h"

#include <limits>

// The largest seed, 2^64 - 1 (spec CLI-02).

constexpr std::uint64_t MAXIMUM_SEED = std::numeric_limits <std::uint64_t>::max ();

//---------------------------------------------------------------------------------------------------------------------
// Function: ParseSeed
//
// Description:
//
//   Parses the value of --seed (spec CLI-02): one or more decimal digits 0 to 9, with no sign, spaces, or 0x prefix,
//   and a value of at most MAXIMUM_SEED. Leading zeros are allowed.
//
// Arguments:
//
//   - text : The argument that follows --seed.
//   - seed : Receives the value if the text is valid, and is left unchanged otherwise.
//
// Returns:
//
//   - True if the text is a valid seed.
//
//---------------------------------------------------------------------------------------------------------------------

static bool ParseSeed ( const std::string& text, std::uint64_t& seed )
{
	// At least one digit is required, so an empty value is invalid.

	if ( text.empty () )
	{
		return false;
	}

	// Accumulate one digit at a time. Any other character is rejected, including a sign, a space, and the x of 0x.
	// Before each step, reject a digit that would carry the value past MAXIMUM_SEED.

	std::uint64_t value = 0;

	for ( char character : text )
	{
		if ( character < '0' || character > '9' )
		{
			return false;
		}

		std::uint64_t digit = static_cast <std::uint64_t> ( character - '0' );

		if ( value > ( MAXIMUM_SEED - digit ) / 10 )
		{
			return false;
		}

		value = value * 10 + digit;
	}

	seed = value;

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ParseCommandLine
//
// Description:
//
//   Scans the arguments left to right and stops at the first error, whose message is the text of spec 4.4 without the
//   "enigma: " prefix. A request for help does not stop the scan, because errors take precedence over --help
//   (spec CLI-05, CLI-06).
//
// Arguments:
//
//   - argument_count  : The number of arguments, including the program name.
//   - argument_values : The arguments, with the program name first.
//   - options         : Receives the parsed options. It is reset to its defaults first.
//   - error_message   : Receives the message of the first error, if there is one.
//
// Returns:
//
//   - True if every argument is valid. False if one is in error.
//
//---------------------------------------------------------------------------------------------------------------------

bool ParseCommandLine
(
	int                 argument_count,
	const char* const   argument_values [],
	CommandLineOptions& options,
	std::string&        error_message
)
{
	// Start from the defaults: no help, and no seed.

	options = CommandLineOptions {};

	// Scan the arguments that follow the program name. Option names are case-sensitive (spec CLI-01).

	for ( int argument_index = 1; argument_index < argument_count; ++argument_index )
	{
		std::string argument = argument_values [ argument_index ];

		if ( argument == "--help" || argument == "-h" )
		{
			options.show_help = true;
		}
		else if ( argument == "--seed" )
		{
			// A second --seed is reported before its value is read (spec CLI-03).

			if ( options.has_seed )
			{
				error_message = "option '--seed' given more than once";
				return false;
			}

			// The value is the next argument, whatever it looks like (spec CLI-02).

			if ( argument_index + 1 >= argument_count )
			{
				error_message = "option '--seed' requires a value";
				return false;
			}

			++argument_index;

			std::string value = argument_values [ argument_index ];

			if ( !ParseSeed ( value, options.seed ) )
			{
				error_message = "invalid seed '" + value + "': expected an integer from 0 to 18446744073709551615";
				return false;
			}

			options.has_seed = true;
		}
		else if ( !argument.empty () && argument [ 0 ] == '-' )
		{
			// Anything else that looks like an option, including the form --seed=<n> (spec CLI-04).

			error_message = "unknown option '" + argument + "'";
			return false;
		}
		else
		{
			// A positional argument, which Enigma never takes (spec CLI-04).

			error_message = "unexpected argument '" + argument + "'";
			return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: UsageText
//
// Description:
//
//   Returns the usage text of spec 4.5, verbatim.
//
// Returns:
//
//   - The usage text, ending in a newline.
//
//---------------------------------------------------------------------------------------------------------------------

const char* UsageText ()
{
	return
		"Usage: enigma.exe [--seed <n>] [--help]\n"
		"\n"
		"Options:\n"
		"  --seed <n>  Seed the random number generator with <n>, an integer from\n"
		"              0 to 18446744073709551615. The same seed reproduces the same table.\n"
		"  -h, --help  Show this help and exit.\n";
}
