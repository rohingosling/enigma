//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Program entry point, and the only module that writes output or sets the exit code (architecture 5.1). It parses
//   the command line, builds the run's perceptron and table from one random source, summarises the table, renders the
//   report to a string, and prints it in one write (architecture 7).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include <cstdio>
#include <string>

#include "build_info.h"
#include "command_line.h"
#include "perceptron.h"
#include "random_source.h"
#include "report.h"
#include "statistics.h"
#include "table.h"

// Exit codes (spec 4.3).

constexpr int EXIT_CODE_SUCCESS            = 0;
constexpr int EXIT_CODE_COMMAND_LINE_ERROR = 1;
constexpr int EXIT_CODE_OUTPUT_ERROR       = 2;

//---------------------------------------------------------------------------------------------------------------------
// Function: main
//
// Description:
//
//   Runs the ten steps of architecture 7. A command-line error prints the two-line diagnostic of spec 4.4 to standard
//   error, and a request for help prints the usage text. Otherwise, main seeds the run's single random source, draws
//   the perceptron's parameters and then the table's inputs in that order (spec RNG-08), computes the statistics, and
//   prints the report. If writing the report fails, it prints a diagnostic instead.
//
// Arguments:
//
//   - argument_count  : The number of command-line arguments, including the program name.
//   - argument_values : The command-line arguments, with the program name first.
//
// Returns:
//
//   - EXIT_CODE_SUCCESS if the report or the usage text was printed.
//   - EXIT_CODE_COMMAND_LINE_ERROR if an argument is in error (spec CLI-05).
//   - EXIT_CODE_OUTPUT_ERROR if writing the report to standard output failed (spec 4.3).
//
//---------------------------------------------------------------------------------------------------------------------

int main ( int argument_count, char* argument_values [] )
{
	// Parse the command line. On an error, print the diagnostic and stop (spec CLI-05).

	CommandLineOptions options;
	std::string        error_message;

	if ( !ParseCommandLine ( argument_count, argument_values, options, error_message ) )
	{
		std::fprintf ( stderr, "enigma: %s\nTry 'enigma.exe --help' for usage.\n", error_message.c_str () );
		return EXIT_CODE_COMMAND_LINE_ERROR;
	}

	// Print the usage text instead of the report if help was requested (spec CLI-06).

	if ( options.show_help )
	{
		std::fputs ( UsageText (), stdout );
		return EXIT_CODE_SUCCESS;
	}

	// Build the network and the table from the run's single random source, seeded from --seed or unpredictably
	// (spec RNG-05, RNG-06). Statement order is draw order: the 9 parameters first, then 8 inputs per row (spec 7.4).

	RandomSource  random_source ( options.has_seed ? options.seed : CreateDefaultSeed () );
	Perceptron    perceptron    = CreateRandomPerceptron ( random_source );
	Table         table         = GenerateTable ( perceptron, random_source, DEFAULT_ROW_COUNT );
	StatisticsSet statistics    = ComputeStatistics ( table );

	// Render the report.

	ReportHeader header      { PROGRAM_NAME, PROGRAM_VERSION, INCARNATION };
	std::string  report_text = FormatReport ( header, table, statistics );

	// Print it in one write, and flush, so that any write failure surfaces here (spec 4.3, 4.4).

	if ( std::fputs ( report_text.c_str (), stdout ) == EOF || std::fflush ( stdout ) != 0 )
	{
		std::fputs ( "enigma: failed to write output\n", stderr );
		return EXIT_CODE_OUTPUT_ERROR;
	}

	return EXIT_CODE_SUCCESS;
}
