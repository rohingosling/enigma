//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Test runner for Enigma (architecture 12). build.bat test compiles this file with every src\*.cpp except main.cpp,
//   unchanged, and runs the result from the project root (spec BLD-21).
//
//   The harness needs no framework. CHECK, CHECK_NEAR, and CHECK_EQUAL_TEXT each count one check through RecordCheck,
//   which prints every failure with its file and line. The run ends with the summary line "N checks, M failed", and
//   exits with code 0 only if every check passed.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "build_info.h"
#include "command_line.h"
#include "perceptron.h"
#include "random_source.h"
#include "report.h"
#include "statistics.h"
#include "table.h"

// Exit codes.

constexpr int EXIT_CODE_SUCCESS = 0;
constexpr int EXIT_CODE_FAILURE = 1;

// The largest error allowed in a transfer function or perceptron output, against std::tanh or the reference model
// (spec NET-07, AT-13).

constexpr double TANH_TOLERANCE = 1e-15;

// The largest difference between a value and its three-decimal printed form, for comparisons with the values printed
// in spec 5.6, which the independent reference model produced (spec OUT-31).

constexpr double PRINT_TOLERANCE = 0.0005;

// The header of the golden files, and of every report the tests render: incarnation 1, as the first build prints it,
// so that no test depends on the real build counter (architecture 12).

static const ReportHeader GOLDEN_HEADER { "Enigma", "1.0", 1 };

//---------------------------------------------------------------------------------------------------------------------
// Harness.
//---------------------------------------------------------------------------------------------------------------------

// Where a check is written and how it reads, as captured by the CHECK macros.

struct CheckSite
{
	const char* file;                                       // __FILE__.
	int         line;                                       // __LINE__.
	const char* expression;                                 // The check as written, such as "CHECK ( x == 1 )".
};

// The first line on which two texts differ, as CHECK_EQUAL_TEXT reports it.

struct TextDifference
{
	int         line_number;                                // Counted from 1.
	int         column_number;                              // Counted from 1, within the line.
	std::string actual_line;                                // That line of the actual text, with its newline if any.
	std::string expected_line;                              // That line of the expected text, with its newline if any.
};

// Running totals for the summary line. These are the harness's only mutable state.

static int check_count   = 0;
static int failure_count = 0;

//---------------------------------------------------------------------------------------------------------------------
// Function: RecordCheck
//
// Description:
//
//   Counts one check. If the check failed, also counts the failure and prints "file:line: expression failed".
//
// Arguments:
//
//   - site   : Where the check is written and how it reads.
//   - passed : True if the check passed.
//
// Returns:
//
//   - passed, so that a caller can follow a failure with lines of detail.
//
//---------------------------------------------------------------------------------------------------------------------

static bool RecordCheck ( const CheckSite& site, bool passed )
{
	// Count the check, and report a failure with its source location.

	++check_count;

	if ( !passed )
	{
		++failure_count;
		std::printf ( "%s:%d: %s failed\n", site.file, site.line, site.expression );
	}

	return passed;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: QuoteText
//
// Description:
//
//   Wraps text in double quotes for a failure report. Quotes and backslashes are escaped, newlines and carriage
//   returns print as \n and \r, and every other character outside printable ASCII prints as \xNN. A stray CR, a tab,
//   or a trailing space therefore stays visible.
//
// Arguments:
//
//   - text : The text to quote.
//
// Returns:
//
//   - The quoted text.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string QuoteText ( const std::string& text )
{
	// Copy the text between quotes, escaping each character that would otherwise be ambiguous or invisible.

	std::string quoted_text = "\"";

	for ( char character : text )
	{
		unsigned char code = static_cast <unsigned char> ( character );

		if ( character == '"' || character == '\\' )
		{
			quoted_text += '\\';
			quoted_text += character;
		}
		else if ( character == '\n' )
		{
			quoted_text += "\\n";
		}
		else if ( character == '\r' )
		{
			quoted_text += "\\r";
		}
		else if ( code < 0x20 || code > 0x7E )
		{
			char escape [ 8 ];

			std::snprintf ( escape, sizeof ( escape ), "\\x%02X", static_cast <unsigned int> ( code ) );
			quoted_text += escape;
		}
		else
		{
			quoted_text += character;
		}
	}

	quoted_text += '"';

	return quoted_text;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: LineStartingAt
//
// Description:
//
//   Extracts the line of text that starts at a given offset, up to and including its newline. The last line of a text
//   may have no newline, and a line that starts at the very end of the text is empty.
//
// Arguments:
//
//   - text       : The text that holds the line.
//   - line_start : The offset of the line's first character, at most text.size ().
//
// Returns:
//
//   - The line, with its newline if it has one.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string LineStartingAt ( const std::string& text, std::size_t line_start )
{
	// Take everything up to and including the next newline, or the rest of the text if no newline follows.

	std::size_t newline_offset = text.find ( '\n', line_start );

	if ( newline_offset == std::string::npos )
	{
		return text.substr ( line_start );
	}

	return text.substr ( line_start, newline_offset - line_start + 1 );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: FindFirstDifference
//
// Description:
//
//   Locates the first character at which two different texts disagree, including the point where one text ends
//   before the other. Lines and columns are counted from 1.
//
// Arguments:
//
//   - actual   : The text produced by the code under test.
//   - expected : The text it should equal. It must differ from actual.
//
// Returns:
//
//   - The line and column of the first difference, and that line as it reads in each text.
//
//---------------------------------------------------------------------------------------------------------------------

static TextDifference FindFirstDifference ( const std::string& actual, const std::string& expected )
{
	// Advance over the prefix the texts share, noting where its last line starts.

	std::size_t offset      = 0;
	std::size_t line_start  = 0;
	int         line_number = 1;

	while ( offset < actual.size () && offset < expected.size () && actual [ offset ] == expected [ offset ] )
	{
		if ( actual [ offset ] == '\n' )
		{
			line_start = offset + 1;
			++line_number;
		}

		++offset;
	}

	// The difference lies on the line that starts at line_start, which is the same offset in both texts.

	TextDifference difference;

	difference.line_number   = line_number;
	difference.column_number = static_cast <int> ( offset - line_start ) + 1;
	difference.actual_line   = LineStartingAt ( actual,   line_start );
	difference.expected_line = LineStartingAt ( expected, line_start );

	return difference;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: CheckNear
//
// Description:
//
//   Implements CHECK_NEAR. The check passes if the two values are equal, or if they differ by no more than the
//   tolerance. A tolerance of 0.0 therefore demands exact equality. Equal infinities pass, and a NaN never does.
//
// Arguments:
//
//   - site      : Where the check is written and how it reads.
//   - actual    : The value produced by the code under test.
//   - expected  : The value it should be near.
//   - tolerance : The largest absolute difference allowed.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void CheckNear ( const CheckSite& site, double actual, double expected, double tolerance )
{
	// Compare. Testing equality first lets equal infinities pass, where their difference would be NaN.

	bool passed = ( actual == expected ) || ( std::fabs ( actual - expected ) <= tolerance );

	// On failure, print both values to 17 significant digits, which identifies a double exactly.

	if ( !RecordCheck ( site, passed ) )
	{
		std::printf ( "  actual:     %.17g\n", actual );
		std::printf ( "  expected:   %.17g\n", expected );
		std::printf ( "  difference: %.3g\n",  std::fabs ( actual - expected ) );
		std::printf ( "  tolerance:  %.3g\n",  tolerance );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Function: CheckEqualText
//
// Description:
//
//   Implements CHECK_EQUAL_TEXT. The check passes if the two texts are identical. On failure, it reports the line and
//   column of the first difference, and quotes that line from each text.
//
// Arguments:
//
//   - site     : Where the check is written and how it reads.
//   - actual   : The text produced by the code under test.
//   - expected : The text it should equal.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void CheckEqualText ( const CheckSite& site, const std::string& actual, const std::string& expected )
{
	// Compare, and on failure report the first line that differs.

	if ( !RecordCheck ( site, actual == expected ) )
	{
		TextDifference difference = FindFirstDifference ( actual, expected );

		std::printf ( "  first difference at line %d, column %d\n", difference.line_number, difference.column_number );
		std::printf ( "  actual:   %s\n", QuoteText ( difference.actual_line   ).c_str () );
		std::printf ( "  expected: %s\n", QuoteText ( difference.expected_line ).c_str () );
	}
}

// The check macros. Each records where it is written and how it reads, then counts one check. CHECK_SITE is the
// shared helper.

#define CHECK_SITE( expression ) CheckSite { __FILE__, __LINE__, expression }

#define CHECK( condition ) \
	RecordCheck ( CHECK_SITE ( "CHECK ( " #condition " )" ), static_cast <bool> ( condition ) )

#define CHECK_NEAR( actual, expected, tolerance ) \
	CheckNear ( CHECK_SITE ( "CHECK_NEAR ( " #actual ", " #expected ", " #tolerance " )" ), \
	            ( actual ), ( expected ), ( tolerance ) )

#define CHECK_EQUAL_TEXT( actual, expected ) \
	CheckEqualText ( CHECK_SITE ( "CHECK_EQUAL_TEXT ( " #actual ", " #expected " )" ), ( actual ), ( expected ) )

//---------------------------------------------------------------------------------------------------------------------
// Test groups.
//---------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------------------
// Function: TestHarness
//
// Description:
//
//   Harness self-check. Each check macro accepts a passing case, and FindFirstDifference and QuoteText produce the
//   locations and quoting that CHECK_EQUAL_TEXT reports. The failure path is proven by hand (development plan,
//   Phase 2).
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestHarness ()
{
	// Each macro passes on a true case. A tolerance of 0.0 demands exact equality.

	CHECK            ( 1 + 1 == 2 );
	CHECK_NEAR       ( 0.1 + 0.2, 0.3, 1e-15 );
	CHECK_NEAR       ( 0.5, 0.5, 0.0 );
	CHECK_EQUAL_TEXT ( std::string ( "line 1\nline 2\n" ), "line 1\nline 2\n" );

	// A difference within a line is located by line and column.

	TextDifference difference = FindFirstDifference ( "i     y\n0000 -0.882\n", "i     y\n0000  0.882\n" );

	CHECK            ( difference.line_number   == 2 );
	CHECK            ( difference.column_number == 6 );
	CHECK_EQUAL_TEXT ( difference.actual_line,   "0000 -0.882\n" );
	CHECK_EQUAL_TEXT ( difference.expected_line, "0000  0.882\n" );

	// A missing final newline is also a difference, and it shows in the quoted lines.

	difference = FindFirstDifference ( "sum\n", "sum" );

	CHECK            ( difference.line_number   == 1 );
	CHECK            ( difference.column_number == 4 );
	CHECK_EQUAL_TEXT ( difference.actual_line,   "sum\n" );
	CHECK_EQUAL_TEXT ( difference.expected_line, "sum"   );

	// Quoting keeps a trailing space, a tab, and a CR LF visible.

	CHECK_EQUAL_TEXT ( QuoteText ( "sum \t\r\n" ), "\"sum \\x09\\r\\n\"" );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestBuildInfo
//
// Description:
//
//   Checks the program identity constants (spec OUT-11, OUT-12), and that the test build falls back to incarnation 0
//   because build.bat test does not define ENIGMA_INCARNATION (spec BLD-14, BLD-21).
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestBuildInfo ()
{
	// Program identity (spec OUT-11, OUT-12).

	CHECK_EQUAL_TEXT ( PROGRAM_NAME,    "Enigma" );
	CHECK_EQUAL_TEXT ( PROGRAM_VERSION, "1.0"    );

	// No injected incarnation in the test build (spec BLD-14).

	CHECK ( INCARNATION == 0 );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestRandomSource
//
// Description:
//
//   Checks the engine parts of AT-16: the engine's known-answer test, seed 42's first raw outputs, and the first
//   uniform values for seeds 42 and 0 (spec 7.1, 7.2). Then checks that 10^6 draws all lie in [-1, 1) on the 2^-52
//   lattice, and that two default seeds differ (spec RNG-06).
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestRandomSource ()
{
	// The known-answer test: the 10000th output of a default-constructed engine, seeded with 5489 (spec RNG-02). Raw
	// outputs are compared as text, so a failure shows the actual value.

	std::mt19937_64 default_engine;

	default_engine.discard ( 9999 );

	CHECK_EQUAL_TEXT ( std::to_string ( default_engine () ), "9981545732273789042" );

	// Seed 42's first three raw outputs (spec RNG-02).

	std::mt19937_64 seed_42_engine ( 42 );

	CHECK_EQUAL_TEXT ( std::to_string ( seed_42_engine () ), "13930160852258120406" );
	CHECK_EQUAL_TEXT ( std::to_string ( seed_42_engine () ), "11788048577503494824" );
	CHECK_EQUAL_TEXT ( std::to_string ( seed_42_engine () ), "13874630024467741450" );

	// The first uniform values, compared exactly. For seed 42 they are w_0, w_1, and w_2 of spec 6.4.

	RandomSource seed_42_source ( 42 );

	CHECK_NEAR ( seed_42_source.NextUniform (), 0.51031106590907793, 0.0 );
	CHECK_NEAR ( seed_42_source.NextUniform (), 0.27806278770939485, 0.0 );
	CHECK_NEAR ( seed_42_source.NextUniform (), 0.5042904014960532,  0.0 );

	RandomSource seed_0_source ( 0 );

	CHECK_NEAR ( seed_0_source.NextUniform (), -0.68041327325907841, 0.0 );

	// Every value lies in [-1, 1) and is a multiple of 2^-52 (spec 7.2). Violations are counted, so that each property
	// is one check, and the extremes show that the draws reach both ends of the interval.

	constexpr int DRAW_COUNT = 1000000;

	RandomSource lattice_source ( 42 );

	int    out_of_range_count = 0;
	int    off_lattice_count  = 0;
	double smallest_value     = 1.0;
	double largest_value      = -1.0;

	for ( int i = 0; i < DRAW_COUNT; ++i )
	{
		double value         = lattice_source.NextUniform ();
		double lattice_steps = value * 0x1.0p52;

		if ( value < -1.0 || value >= 1.0 )
		{
			++out_of_range_count;
		}

		if ( lattice_steps != std::floor ( lattice_steps ) )
		{
			++off_lattice_count;
		}

		smallest_value = std::min ( smallest_value, value );
		largest_value  = std::max ( largest_value,  value );
	}

	CHECK ( out_of_range_count == 0      );
	CHECK ( off_lattice_count  == 0      );
	CHECK ( smallest_value     <  -0.999 );
	CHECK ( largest_value      >   0.999 );

	// Two default seeds differ, with overwhelming probability (spec RNG-06).

	CHECK ( CreateDefaultSeed () != CreateDefaultSeed () );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestTransferFunction
//
// Description:
//
//   Checks AT-12 with std::tanh as the oracle: a 40001-point sweep of [-20, 20] in steps of 0.001, within 1e-15 at
//   every point (spec NET-07), with odd symmetry exact at every point. Then checks the exact values of NET-08, the
//   relative precision near zero that expm1 provides, the reference values of spec 6.3, and seed 42's worked examples
//   from spec 6.2.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestTransferFunction ()
{
	// Sweep [-20, 20] in steps of 0.001. Each activation is i / 1000, the double nearest its grid point. Violations
	// are counted, so that each property is one check. The tolerance test is written so that a NaN also fails it.

	constexpr int SWEEP_STEP_COUNT = 20000;                 // Steps on each side of 0, giving 40001 points.

	int point_count            = 0;
	int out_of_tolerance_count = 0;
	int asymmetric_count       = 0;

	for ( int i = -SWEEP_STEP_COUNT; i <= SWEEP_STEP_COUNT; ++i )
	{
		double activation = i / 1000.0;
		double error      = std::fabs ( TransferFunction ( activation ) - std::tanh ( activation ) );

		if ( !( error <= TANH_TOLERANCE ) )
		{
			++out_of_tolerance_count;
		}

		if ( TransferFunction ( -activation ) != -TransferFunction ( activation ) )
		{
			++asymmetric_count;
		}

		++point_count;
	}

	CHECK ( point_count            == 40001 );
	CHECK ( out_of_tolerance_count == 0     );
	CHECK ( asymmetric_count       == 0     );

	// The exact values of NET-08: zero at zero, and saturation at +-800, where the textbook form computes inf / inf.

	CHECK_NEAR ( TransferFunction ( 0.0 ),     0.0, 0.0 );
	CHECK_NEAR ( TransferFunction ( 800.0 ),   1.0, 0.0 );
	CHECK_NEAR ( TransferFunction ( -800.0 ), -1.0, 0.0 );

	// Full relative precision near zero, which expm1 provides and a direct 1 - e^-2a loses to cancellation (spec 6.3).
	// The sweep cannot see this, because near zero both forms are within 1e-15 of std::tanh in absolute terms.

	CHECK_NEAR ( TransferFunction ( 1e-10 ) / std::tanh ( 1e-10 ), 1.0, TANH_TOLERANCE );

	// The reference values of spec 6.3, and the activations of seed 42's rows 0000 and 0004 (spec 6.2).

	CHECK_NEAR ( TransferFunction ( 0.5 ),                      0.46211715726000974,  TANH_TOLERANCE );
	CHECK_NEAR ( TransferFunction ( -2.0 ),                    -0.96402758007581679,  TANH_TOLERANCE );
	CHECK_NEAR ( TransferFunction ( -1.3855600797745766 ),     -0.88219022676616421,  TANH_TOLERANCE );
	CHECK_NEAR ( TransferFunction ( -0.00032708750661689101 ), -0.000327087494952271, TANH_TOLERANCE );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestPerceptron
//
// Description:
//
//   Checks the known-parameter cases of AT-13 (spec 6.2), plus a case that exercises every weight and the bias. Then
//   checks the parameter part of AT-16: seed 42's weights and bias equal spec 6.4 exactly, and CreateRandomPerceptron
//   makes exactly 9 draws, so the next value is row 0's first input (spec NET-09, RNG-08).
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestPerceptron ()
{
	// Inputs with x_0 = 0.5 and every other input non-zero, so that a zero weight visibly cancels its input.

	InputVector inputs { 0.5, -0.25, 0.75, -1.0, 0.125, -0.5, 0.875, -0.625 };

	// AT-13: all-zero parameters give 0.

	Perceptron zero_perceptron ( WeightVector { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, 0.0 );

	CHECK_NEAR ( zero_perceptron.Evaluate ( inputs ), 0.0, TANH_TOLERANCE );

	// AT-13: w = ( 1, 0, ..., 0 ), b = 0, and x_0 = 0.5 give the spec 6.3 reference value for 0.5. A bias of 0.5 with
	// zero weights gives the same value, which shows that the bias enters the activation.

	Perceptron unit_perceptron ( WeightVector { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, 0.0 );
	Perceptron bias_perceptron ( WeightVector { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }, 0.5 );

	CHECK_NEAR ( unit_perceptron.Evaluate ( inputs ), 0.46211715726000974, TANH_TOLERANCE );
	CHECK_NEAR ( bias_perceptron.Evaluate ( inputs ), 0.46211715726000974, TANH_TOLERANCE );

	// Every weight paired with its own input. The values are dyadic, so the activation is exact: the bias, 0.0625,
	// plus the terms 0.0625, -0.125, -0.1875, 0.25, 0.15625, 0.1875, 0.875, and 1.0, is 2.28125. Pairing a weight with
	// the wrong input, or dropping a term or the bias, gives a different activation.

	WeightVector mixed_weights {  0.125, -0.25,  0.375, -0.5,  0.625, -0.75,  0.875, -1.0 };
	InputVector  mixed_inputs  {  0.5,    0.5,  -0.5,   -0.5,  0.25,  -0.25,  1.0,   -1.0 };
	Perceptron   mixed_perceptron ( mixed_weights, 0.0625 );

	CHECK_NEAR ( mixed_perceptron.Evaluate ( mixed_inputs ), std::tanh ( 2.28125 ), TANH_TOLERANCE );

	// AT-16: seed 42's parameters (spec 6.4), compared exactly.

	RandomSource        seed_42_source ( 42 );
	Perceptron          seed_42_perceptron = CreateRandomPerceptron ( seed_42_source );
	const WeightVector& seed_42_weights    = seed_42_perceptron.Weights ();

	CHECK_NEAR ( seed_42_weights [ 0 ],       0.51031106590907793, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 1 ],       0.27806278770939485, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 2 ],       0.5042904014960532,  0.0 );
	CHECK_NEAR ( seed_42_weights [ 3 ],      -0.72745463273512589, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 4 ],       0.80653793285675657, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 5 ],      -0.81186337647432594, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 6 ],       0.14914060821652786, 0.0 );
	CHECK_NEAR ( seed_42_weights [ 7 ],      -0.25422460108763034, 0.0 );
	CHECK_NEAR ( seed_42_perceptron.Bias (), -0.4522517965256585,  0.0 );

	// Exactly 9 draws were made, so the next value is x_0 of seed 42's row 0 (spec 7.4).

	CHECK_NEAR ( seed_42_source.NextUniform (), -0.21945823717141288, 0.0 );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestTable
//
// Description:
//
//   Checks the table parts of AT-16 and AT-13 on seed 42's table, then AT-11. Row 0 starts and ends with the inputs
//   of spec 7.4, exactly, and all eight of its inputs print as spec 5.6 shows. Every input of every row is the next
//   value of a parallel random source that skipped the 9 parameter draws, which pins the whole draw order of RNG-08,
//   and the table makes exactly 8 draws per row. Rows 0 and 4 give the outputs of spec 6.2, and every output is its
//   row's forward pass. ColumnValue numbers the columns in printed order, and GenerateTable gives the row count it is
//   asked for. Last, over seeds 0 to 999, every y and every x lies in [-1, 1], and every x is below 1 (AT-11).
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestTable ()
{
	// Seed 42's table, generated as a run generates it: the parameters first, then the rows (spec RNG-08).

	RandomSource seed_42_source ( 42 );
	Perceptron   seed_42_perceptron = CreateRandomPerceptron ( seed_42_source );
	Table        seed_42_table      = GenerateTable ( seed_42_perceptron, seed_42_source, DEFAULT_ROW_COUNT );

	CHECK ( seed_42_table.size () == 16 );

	// AT-16: row 0 starts and ends with the inputs of spec 7.4, compared exactly.

	const InputVector& row_0_inputs = seed_42_table [ 0 ].inputs;

	CHECK_NEAR ( row_0_inputs [ 0 ], -0.21945823717141288, 0.0 );
	CHECK_NEAR ( row_0_inputs [ 7 ],  0.5064370209944522,  0.0 );

	// All eight of row 0's inputs, against the values that spec 5.6 prints for them.

	CHECK_NEAR ( row_0_inputs [ 0 ], -0.219, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 1 ], -0.975, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 2 ],  0.047, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 3 ],  0.371, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 4 ],  0.275, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 5 ],  0.653, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 6 ],  0.891, PRINT_TOLERANCE );
	CHECK_NEAR ( row_0_inputs [ 7 ],  0.506, PRINT_TOLERANCE );

	// AT-13: rows 0000 and 0004 give the outputs of spec 6.2. Row 0004 is the one that prints as -0.000.

	CHECK_NEAR ( seed_42_table [ 0 ].output, -0.88219022676616421,  TANH_TOLERANCE );
	CHECK_NEAR ( seed_42_table [ 4 ].output, -0.000327087494952271, TANH_TOLERANCE );

	// The whole draw order (spec RNG-08). A parallel source skips the 9 parameter draws, and must then yield every
	// row's inputs, in row order and index order. Every output must be exactly its row's forward pass. Mismatches are
	// counted, so that each property is one check.

	RandomSource parallel_source ( 42 );

	for ( int k = 0; k < INPUT_COUNT + 1; ++k )
	{
		parallel_source.NextUniform ();
	}

	int out_of_order_count = 0;
	int wrong_output_count = 0;

	for ( const TableRow& row : seed_42_table )
	{
		for ( int j = 0; j < INPUT_COUNT; ++j )
		{
			if ( row.inputs [ j ] != parallel_source.NextUniform () )
			{
				++out_of_order_count;
			}
		}

		if ( row.output != seed_42_perceptron.Evaluate ( row.inputs ) )
		{
			++wrong_output_count;
		}
	}

	CHECK ( out_of_order_count == 0 );
	CHECK ( wrong_output_count == 0 );

	// Exactly 8 draws per row were made, 137 in all, so both sources now yield the same next value.

	CHECK_NEAR ( seed_42_source.NextUniform (), parallel_source.NextUniform (), 0.0 );

	// ColumnValue numbers the columns in printed order: column 0 is y, and column c is x_(c-1).

	TableRow numbered_row { -0.5, InputVector { 0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875 } };

	CHECK_NEAR ( ColumnValue ( numbered_row, 0 ), -0.5,   0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 1 ),  0.0,   0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 2 ),  0.125, 0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 3 ),  0.25,  0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 4 ),  0.375, 0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 5 ),  0.5,   0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 6 ),  0.625, 0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 7 ),  0.75,  0.0 );
	CHECK_NEAR ( ColumnValue ( numbered_row, 8 ),  0.875, 0.0 );

	// GenerateTable gives the row count it is asked for, at both ends of the range of spec OUT-22.

	RandomSource count_source ( 0 );
	Perceptron   count_perceptron = CreateRandomPerceptron ( count_source );

	CHECK ( GenerateTable ( count_perceptron, count_source, 1             ).size () == 1    );
	CHECK ( GenerateTable ( count_perceptron, count_source, MAX_ROW_COUNT ).size () == 9999 );

	// AT-11: over seeds 0 to 999, every y and every x lies in [-1, 1], and every x is below 1 (spec NET-02, RNG-03).
	// Violations are counted, so that each property is one check. The range tests are written so that a NaN also
	// fails them.

	constexpr int SEED_COUNT = 1000;

	int row_count                 = 0;
	int output_out_of_range_count = 0;
	int input_out_of_range_count  = 0;

	for ( int seed = 0; seed < SEED_COUNT; ++seed )
	{
		RandomSource random_source ( static_cast <std::uint64_t> ( seed ) );
		Perceptron   perceptron    = CreateRandomPerceptron ( random_source );
		Table        table         = GenerateTable ( perceptron, random_source, DEFAULT_ROW_COUNT );

		for ( const TableRow& row : table )
		{
			if ( !( row.output >= -1.0 && row.output <= 1.0 ) )
			{
				++output_out_of_range_count;
			}

			for ( double input : row.inputs )
			{
				if ( !( input >= -1.0 && input < 1.0 ) )
				{
					++input_out_of_range_count;
				}
			}

			++row_count;
		}
	}

	CHECK ( row_count                 == 16000 );
	CHECK ( output_out_of_range_count == 0     );
	CHECK ( input_out_of_range_count  == 0     );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: CheckStatistics
//
// Description:
//
//   Checks each value column's statistics against the expected ones, as four CHECK_NEAR checks per column. A failure
//   reports the line of the CHECK_NEAR here, and the values it prints identify the column and the statistic.
//
// Arguments:
//
//   - actual    : The statistics computed by ComputeStatistics.
//   - expected  : The statistics they should equal.
//   - tolerance : The largest absolute difference allowed. 0.0 demands exact equality.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void CheckStatistics ( const StatisticsSet& actual, const StatisticsSet& expected, double tolerance )
{
	// Compare the four statistics of each column in turn.

	for ( int column = 0; column < COLUMN_COUNT; ++column )
	{
		CHECK_NEAR ( actual [ column ].minimum, expected [ column ].minimum, tolerance );
		CHECK_NEAR ( actual [ column ].maximum, expected [ column ].maximum, tolerance );
		CHECK_NEAR ( actual [ column ].average, expected [ column ].average, tolerance );
		CHECK_NEAR ( actual [ column ].sum,     expected [ column ].sum,     tolerance );
	}
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestStatistics
//
// Description:
//
//   Checks AT-14 on synthetic tables of 3 rows and 1 row, against hand-computed statistics (spec STA-01 to STA-04).
//   The values are multiples of 1/8, and each sum is a multiple of 3/8, so every sum and average is exact. Then checks
//   that each sum is accumulated in row order, and that seed 42's statistics print as spec 5.6 shows.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestStatistics ()
{
	// AT-14: a 3-row table. Each column has a different sum, so a column mix-up shows. Across the columns, the minimum
	// and the maximum each fall in every row, so neither a skipped row nor a missed comparison can pass. Column x_0 is
	// entirely negative and x_1 entirely positive, so neither extreme can start from 0.0.

	Table three_row_table
	{
		TableRow {  0.25, InputVector { -0.75,  0.5,    0.0,    0.75,  -0.125,  0.875, -0.375, -1.0   } },
		TableRow { -0.5,  InputVector { -0.25,  0.125,  0.375, -1.0,   -0.875,  0.25,   0.375,  0.125 } },
		TableRow {  1.0,  InputVector { -0.5,   0.875, -0.375,  0.625,  0.625,  0.0,   -0.75,  -0.25  } }
	};

	StatisticsSet three_row_expected
	{
		ColumnStatistics { -0.5,    1.0,    0.25,   0.75  },  // y.
		ColumnStatistics { -0.75,  -0.25,  -0.5,   -1.5   },  // x_0, entirely negative.
		ColumnStatistics {  0.125,  0.875,  0.5,    1.5   },  // x_1, entirely positive.
		ColumnStatistics { -0.375,  0.375,  0.0,    0.0   },  // x_2.
		ColumnStatistics { -1.0,    0.75,   0.125,  0.375 },  // x_3.
		ColumnStatistics { -0.875,  0.625, -0.125, -0.375 },  // x_4.
		ColumnStatistics {  0.0,    0.875,  0.375,  1.125 },  // x_5.
		ColumnStatistics { -0.75,   0.375, -0.25,  -0.75  },  // x_6.
		ColumnStatistics { -1.0,    0.125, -0.375, -1.125 }   // x_7.
	};

	CheckStatistics ( ComputeStatistics ( three_row_table ), three_row_expected, 0.0 );

	// AT-14: a 1-row table, in which every statistic of a column is that column's value. The values have both signs,
	// so neither extreme can start from 0.0 here either.

	Table one_row_table
	{
		TableRow { -0.625, InputVector { 0.125, -0.25, 0.375, -0.5, 0.625, -0.75, 0.875, -1.0 } }
	};

	StatisticsSet one_row_expected {};

	for ( int column = 0; column < COLUMN_COUNT; ++column )
	{
		double value = ColumnValue ( one_row_table [ 0 ], column );

		one_row_expected [ column ] = ColumnStatistics { value, value, value, value };
	}

	CheckStatistics ( ComputeStatistics ( one_row_table ), one_row_expected, 0.0 );

	// Each sum adds the rows in order from 0.0 (spec STA-01). In row order, 1 + 2^-53 rounds back to 1 twice, so y
	// sums to exactly 1. Reverse order would first add 2^-53 + 2^-53 = 2^-52 and reach 1 + 2^-52, and so would a
	// compensated sum.

	Table order_table
	{
		TableRow { 1.0,       InputVector {} },
		TableRow { 0x1.0p-53, InputVector {} },
		TableRow { 0x1.0p-53, InputVector {} }
	};

	CHECK_NEAR ( ComputeStatistics ( order_table ) [ 0 ].sum, 1.0, 0.0 );

	// Seed 42's statistics rows, against the values that spec 5.6 prints for them.

	RandomSource  seed_42_source ( 42 );
	Perceptron    seed_42_perceptron = CreateRandomPerceptron ( seed_42_source );
	Table         seed_42_table      = GenerateTable ( seed_42_perceptron, seed_42_source, DEFAULT_ROW_COUNT );
	StatisticsSet seed_42_expected
	{
		ColumnStatistics { -0.986, 0.656, -0.448, -7.172 },   // y.
		ColumnStatistics { -0.812, 0.993, -0.103, -1.652 },   // x_0.
		ColumnStatistics { -0.975, 0.633, -0.235, -3.762 },   // x_1.
		ColumnStatistics { -0.871, 0.852, -0.087, -1.393 },   // x_2.
		ColumnStatistics { -0.852, 0.723,  0.187,  2.988 },   // x_3.
		ColumnStatistics { -0.755, 0.807,  0.112,  1.792 },   // x_4.
		ColumnStatistics { -0.815, 0.710,  0.033,  0.530 },   // x_5.
		ColumnStatistics { -0.961, 0.891,  0.089,  1.431 },   // x_6.
		ColumnStatistics { -0.869, 0.991,  0.228,  3.649 }    // x_7.
	};

	CheckStatistics ( ComputeStatistics ( seed_42_table ), seed_42_expected, PRINT_TOLERANCE );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ReadGoldenFile
//
// Description:
//
//   Reads a golden file in binary mode and removes every CR, so that the line endings git checks out cannot cause a
//   false failure (architecture 12). The path is relative to the project root, where build.bat test runs the tests.
//
// Arguments:
//
//   - path : The golden file's path, such as test/golden/report_seed_42.txt.
//   - text : Receives the file's text, without CRs.
//
// Returns:
//
//   - True if the file was read. False if it could not be opened or read.
//
//---------------------------------------------------------------------------------------------------------------------

static bool ReadGoldenFile ( const char* path, std::string& text )
{
	// Read the whole file in binary mode, so that the C runtime translates nothing.

	std::ifstream file ( path, std::ios::binary );

	if ( !file )
	{
		return false;
	}

	text.assign ( std::istreambuf_iterator <char> ( file ), std::istreambuf_iterator <char> () );

	// Remove every CR, whatever line endings git checked out.

	text.erase ( std::remove ( text.begin (), text.end (), '\r' ), text.end () );

	return !file.bad ();
}

//---------------------------------------------------------------------------------------------------------------------
// Function: SeededReport
//
// Description:
//
//   Renders the report for a seed as a run renders it (architecture 7), but with the header of the golden files.
//
// Arguments:
//
//   - seed : The seed, as --seed would give it.
//
// Returns:
//
//   - The report.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string SeededReport ( std::uint64_t seed )
{
	// Draw the parameters and then the table from one source (spec RNG-08), summarise, and render.

	RandomSource  random_source ( seed );
	Perceptron    perceptron    = CreateRandomPerceptron ( random_source );
	Table         table         = GenerateTable ( perceptron, random_source, DEFAULT_ROW_COUNT );
	StatisticsSet statistics    = ComputeStatistics ( table );

	return FormatReport ( GOLDEN_HEADER, table, statistics );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: SplitLines
//
// Description:
//
//   Splits a text into lines at each newline. The newline that ends the last line does not start another, and a final
//   piece with no newline is kept as a line of its own.
//
// Arguments:
//
//   - text : The text to split.
//
// Returns:
//
//   - The lines, without their newlines.
//
//---------------------------------------------------------------------------------------------------------------------

static std::vector <std::string> SplitLines ( const std::string& text )
{
	// Cut the text after each newline.

	std::vector <std::string> lines;
	std::size_t               line_start = 0;

	while ( line_start < text.size () )
	{
		std::size_t newline_offset = text.find ( '\n', line_start );

		if ( newline_offset == std::string::npos )
		{
			newline_offset = text.size ();
		}

		lines.push_back ( text.substr ( line_start, newline_offset - line_start ) );
		line_start = newline_offset + 1;
	}

	return lines;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ExpectedColumnHeader
//
// Description:
//
//   Builds the column header from the positions of spec OUT-25 and OUT-42, independently of the renderer: i in column
//   1, then each label at offset W - 5 of its field. Value field k starts after the 4-character index field, k earlier
//   fields, and k + 1 separating spaces. Nothing follows the last label.
//
// Arguments:
//
//   - width : The value field width, W.
//
// Returns:
//
//   - The column header, without a newline.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string ExpectedColumnHeader ( int width )
{
	// Place each label at its offset, filling the gaps with spaces.

	const char* labels []     = { "y", "x_0", "x_1", "x_2", "x_3", "x_4", "x_5", "x_6", "x_7" };
	std::string column_header = "i";

	for ( int k = 0; k < COLUMN_COUNT; ++k )
	{
		column_header.resize ( static_cast <std::size_t> ( 4 + k * ( 1 + width ) + 1 + ( width - 5 ) ), ' ' );
		column_header += labels [ k ];
	}

	return column_header;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ExpectedValueLines
//
// Description:
//
//   Lays out a report's data rows and statistics rows from the rules of spec 5.3 to 5.5, independently of the
//   renderer, and finds W on the way. Each row is its index, or its statistics label, in a 4-character field, then
//   each value after one space, right-aligned in W characters. W is the larger of 6 and the longest formatted value.
//
// Arguments:
//
//   - table      : The data rows.
//   - statistics : The statistics of each value column.
//   - width      : Receives W.
//
// Returns:
//
//   - The data rows, then the min, max, avg, and sum rows, each without a newline.
//
//---------------------------------------------------------------------------------------------------------------------

static std::vector <std::string> ExpectedValueLines
(
	const Table&         table,
	const StatisticsSet& statistics,
	int&                 width
)
{
	// Each printed row's index field and formatted values: the data rows, then the statistics rows (spec OUT-21,
	// OUT-23).

	std::vector <std::string>               index_fields;
	std::vector <std::vector <std::string>> value_texts;

	for ( std::size_t i = 0; i < table.size (); ++i )
	{
		char index_field [ 16 ];

		std::snprintf ( index_field, sizeof ( index_field ), "%04d", static_cast <int> ( i ) );
		index_fields.push_back ( index_field );
		value_texts.emplace_back ();

		for ( int column = 0; column < COLUMN_COUNT; ++column )
		{
			value_texts.back ().push_back ( FormatValue ( ColumnValue ( table [ i ], column ) ) );
		}
	}

	std::vector <std::string> minimum_texts;
	std::vector <std::string> maximum_texts;
	std::vector <std::string> average_texts;
	std::vector <std::string> sum_texts;

	for ( const ColumnStatistics& column_statistics : statistics )
	{
		minimum_texts.push_back ( FormatValue ( column_statistics.minimum ) );
		maximum_texts.push_back ( FormatValue ( column_statistics.maximum ) );
		average_texts.push_back ( FormatValue ( column_statistics.average ) );
		sum_texts.push_back     ( FormatValue ( column_statistics.sum     ) );
	}

	index_fields.insert ( index_fields.end (), { "min ", "max ", "avg ", "sum " } );
	value_texts.insert  ( value_texts.end (),  { minimum_texts, maximum_texts, average_texts, sum_texts } );

	// W is the larger of 6 and the longest formatted value anywhere (spec OUT-40).

	width = 6;

	for ( const std::vector <std::string>& texts : value_texts )
	{
		for ( const std::string& text : texts )
		{
			width = std::max ( width, static_cast <int> ( text.size () ) );
		}
	}

	// Lay out each row: the index field, then each value right-aligned after one space (spec OUT-20, OUT-41).

	std::vector <std::string> lines;

	for ( std::size_t k = 0; k < index_fields.size (); ++k )
	{
		std::string line = index_fields [ k ];

		for ( const std::string& text : value_texts [ k ] )
		{
			line += std::string ( static_cast <std::size_t> ( 1 + width ) - text.size (), ' ' ) + text;
		}

		lines.push_back ( line );
	}

	return lines;
}

// The layout rules that rendered reports break, as CheckLayout counts them for AT-10. Each counter becomes one check.

struct LayoutViolations
{
	int unterminated_report;                                // The report does not end in a newline (spec OUT-01).
	int wrong_line_count;                                   // The report does not have R + 15 lines (spec 5.1).
	int wrong_empty_lines;                                  // Lines 1, 5, and R + 15 are not all empty (spec 5.1).
	int wrong_header_block;                                 // The header block is wrong (spec 5.2).
	int wrong_rules;                                        // A rule is not 4 + 9 × (1 + W) hyphens (spec OUT-24).
	int wrong_column_header;                                // The column header is misplaced (spec OUT-25, OUT-42).
	int wrong_row_index;                                    // A row's index or label is wrong (spec OUT-21, OUT-23).
	int wrong_value_row;                                    // A row's values are misaligned (spec OUT-20, OUT-41).
	int trailing_space;                                     // A line ends in a space (spec OUT-03).
	int indented_line;                                      // A line starts with a space (spec OUT-02).
	int non_ascii_character;                                // A character is not printable 7-bit ASCII (OUT-04).
};

//---------------------------------------------------------------------------------------------------------------------
// Function: CheckLayout
//
// Description:
//
//   Renders the report for a table, with the header of the golden files, and checks it line by line against the
//   layout of spec 5, counting each rule it breaks (AT-10). The expected rows and W come from ExpectedValueLines and
//   ExpectedColumnHeader, which apply the layout rules independently of the renderer. A report with the wrong number
//   of lines is counted once and not examined further.
//
// Arguments:
//
//   - table      : The data rows, from 1 to MAX_ROW_COUNT of them.
//   - violations : The counters to increase.
//
// Returns:
//
//   - W for this report, so that the caller can tell which reports widened.
//
//---------------------------------------------------------------------------------------------------------------------

static int CheckLayout ( const Table& table, LayoutViolations& violations )
{
	// Render the report, and lay out its value rows independently, which also gives W.

	StatisticsSet             statistics  = ComputeStatistics ( table );
	std::string               report      = FormatReport ( GOLDEN_HEADER, table, statistics );
	std::vector <std::string> lines       = SplitLines ( report );
	std::size_t               row_count   = table.size ();
	int                       width       = 0;
	std::vector <std::string> value_lines = ExpectedValueLines ( table, statistics, width );
	std::string               rule        ( static_cast <std::size_t> ( 4 + 9 * ( 1 + width ) ), '-' );

	// Every line ends in a newline, and there are R + 15 of them (spec OUT-01, 5.1).

	if ( report.empty () || report.back () != '\n' )
	{
		++violations.unterminated_report;
	}

	if ( lines.size () != row_count + 15 )
	{
		++violations.wrong_line_count;
		return width;
	}

	// The fixed lines: the empty lines, the header block, the four rules, and the column header (spec 5.1 to 5.3).

	if ( !lines [ 0 ].empty () || !lines [ 4 ].empty () || !lines [ row_count + 14 ].empty () )
	{
		++violations.wrong_empty_lines;
	}

	if ( lines [ 1 ] != "Program:     Enigma" || lines [ 2 ] != "Version:     1.0" || lines [ 3 ] != "Incarnation: 1" )
	{
		++violations.wrong_header_block;
	}

	if ( lines [ 5 ]             != rule || lines [ 7 ]              != rule ||
	     lines [ row_count + 8 ] != rule || lines [ row_count + 13 ] != rule )
	{
		++violations.wrong_rules;
	}

	if ( lines [ 6 ] != ExpectedColumnHeader ( width ) )
	{
		++violations.wrong_column_header;
	}

	// The data rows start at line 9, and the statistics rows follow the rule after them (spec 5.1). The first five
	// characters hold the index field and the space after it.

	for ( std::size_t k = 0; k < value_lines.size (); ++k )
	{
		const std::string& line = lines [ k < row_count ? 8 + k : 9 + k ];

		if ( line.compare ( 0, 5, value_lines [ k ], 0, 5 ) != 0 )
		{
			++violations.wrong_row_index;
		}

		if ( line != value_lines [ k ] )
		{
			++violations.wrong_value_row;
		}
	}

	// Every line: no trailing space, no indent, and only printable 7-bit ASCII, so no tab, CR, or escape sequence
	// (spec OUT-02 to OUT-04).

	for ( const std::string& line : lines )
	{
		if ( !line.empty () && line.back () == ' ' )
		{
			++violations.trailing_space;
		}

		if ( !line.empty () && line.front () == ' ' )
		{
			++violations.indented_line;
		}

		for ( char character : line )
		{
			unsigned char code = static_cast <unsigned char> ( character );

			if ( code < 0x20 || code > 0x7E )
			{
				++violations.non_ascii_character;
			}
		}
	}

	return width;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestReport
//
// Description:
//
//   Checks AT-17 on FormatValue, then AT-06 and AT-07: the reports for seeds 42 and 6 equal the golden files, which
//   are spec 5.6 copied verbatim. Then AT-10: the reports for seeds 0 to 999 follow every layout rule of spec 5, as do
//   reports at both ends of the row-count range and two synthetic tables that show the sum row deciding W.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestReport ()
{
	// AT-17: the cases of spec 5.4, compared exactly (spec OUT-30 to OUT-32).

	CHECK_EQUAL_TEXT ( FormatValue (   0.0     ),   "0.000" );
	CHECK_EQUAL_TEXT ( FormatValue (  -0.0003  ),  "-0.000" );
	CHECK_EQUAL_TEXT ( FormatValue (   0.9996  ),   "1.000" );
	CHECK_EQUAL_TEXT ( FormatValue (  -1.0     ),  "-1.000" );
	CHECK_EQUAL_TEXT ( FormatValue ( -10.8121  ), "-10.812" );

	// Rounding applies to the exact binary value (spec OUT-31). The double nearest 1.0005 lies just below the halfway
	// point, and the one nearest 2.0005 just above it, so rounding the decimal text or truncating the binary value
	// would get one of them wrong. The double nearest 0.9995 lies above it, so an input of 0.9995 prints as 1.000.

	CHECK_EQUAL_TEXT ( FormatValue ( 1.0005 ), "1.000" );
	CHECK_EQUAL_TEXT ( FormatValue ( 2.0005 ), "2.001" );
	CHECK_EQUAL_TEXT ( FormatValue ( 0.9995 ), "1.000" );

	// No exponent or thousands separator, however large the value (spec OUT-33). -9999 is the lowest sum that 9999
	// rows can reach. The longest text of any double, 314 characters for -DBL_MAX, fits FormatValue's buffer whole.

	CHECK_EQUAL_TEXT ( FormatValue ( -9999.0 ), "-9999.000"                 );
	CHECK_EQUAL_TEXT ( FormatValue (  1e20   ), "100000000000000000000.000" );

	std::string longest_text = FormatValue ( -std::numeric_limits <double>::max () );

	CHECK ( longest_text.size () == 314 && longest_text.compare ( 310, 4, ".000" ) == 0 );

	// AT-06 and AT-07: the reports for seeds 42 and 6, with incarnation 1, equal the golden files.

	std::string golden_report_42;
	std::string golden_report_6;

	CHECK ( ReadGoldenFile ( "test/golden/report_seed_42.txt", golden_report_42 ) );
	CHECK ( ReadGoldenFile ( "test/golden/report_seed_6.txt",  golden_report_6  ) );

	std::string report_42 = SeededReport ( 42 );
	std::string report_6  = SeededReport ( 6 );

	CHECK_EQUAL_TEXT ( report_42, golden_report_42 );
	CHECK_EQUAL_TEXT ( report_6,  golden_report_6  );

	// Seed 42 keeps W = 6, with 67-character rules and a 65-character column header (spec OUT-43). Seed 6's y sum of
	// -10.812 widens W to 7, and its rules to 76 characters (AT-07).

	std::vector <std::string> lines_42 = SplitLines ( report_42 );
	std::vector <std::string> lines_6  = SplitLines ( report_6 );

	CHECK ( lines_42.size () == 31 && lines_42 [ 5 ].size () == 67 && lines_42 [ 6 ].size () == 65 );
	CHECK ( lines_6.size ()  == 31 && lines_6  [ 5 ].size () == 76 );

	// AT-10: the reports for seeds 0 to 999. Violations are counted, so that each layout rule is one check, and the
	// reports that widen past W = 6 are counted too.

	constexpr int SEED_COUNT = 1000;

	LayoutViolations violations {};
	int              widened_report_count = 0;

	for ( int seed = 0; seed < SEED_COUNT; ++seed )
	{
		RandomSource random_source ( static_cast <std::uint64_t> ( seed ) );
		Perceptron   perceptron    = CreateRandomPerceptron ( random_source );
		Table        table         = GenerateTable ( perceptron, random_source, DEFAULT_ROW_COUNT );

		if ( CheckLayout ( table, violations ) > 6 )
		{
			++widened_report_count;
		}
	}

	CHECK ( widened_report_count > 0 );

	// Both ends of the row-count range (spec OUT-22). A 1-row report has 16 lines, and a 9999-row report has 10014,
	// whose last data row is 9998 and whose sums widen W.

	RandomSource range_source ( 0 );
	Perceptron   range_perceptron = CreateRandomPerceptron ( range_source );
	Table        one_row_table    = GenerateTable ( range_perceptron, range_source, 1 );
	Table        full_table       = GenerateTable ( range_perceptron, range_source, MAX_ROW_COUNT );

	CHECK ( CheckLayout ( one_row_table, violations ) == 6 );
	CHECK ( CheckLayout ( full_table,    violations ) >  6 );

	// The sum row decides W (spec OUT-40). Sixteen rows of y = 0.625 sum to 10.000, which still fits in 6 characters,
	// but sixteen rows of y = -0.625 sum to -10.000, which widens every value column to 7.

	Table positive_sum_table ( DEFAULT_ROW_COUNT, TableRow {  0.625, InputVector {} } );
	Table negative_sum_table ( DEFAULT_ROW_COUNT, TableRow { -0.625, InputVector {} } );

	CHECK ( CheckLayout ( positive_sum_table, violations ) == 6 );
	CHECK ( CheckLayout ( negative_sum_table, violations ) == 7 );

	// No report broke any layout rule.

	CHECK ( violations.unterminated_report == 0 );
	CHECK ( violations.wrong_line_count    == 0 );
	CHECK ( violations.wrong_empty_lines   == 0 );
	CHECK ( violations.wrong_header_block  == 0 );
	CHECK ( violations.wrong_rules         == 0 );
	CHECK ( violations.wrong_column_header == 0 );
	CHECK ( violations.wrong_row_index     == 0 );
	CHECK ( violations.wrong_value_row     == 0 );
	CHECK ( violations.trailing_space      == 0 );
	CHECK ( violations.indented_line       == 0 );
	CHECK ( violations.non_ascii_character == 0 );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ParseOutcome
//
// Description:
//
//   Runs ParseCommandLine on an argument list, with "enigma.exe" prepended as the C runtime would, and describes the
//   outcome in one line. A rejected command line gives its error message. An accepted one gives what the program would
//   print, "report" or "help", and how it would seed the run, as in "report, seed 42" or "help, default seed". Each
//   AT-15 case is then a single check whose failure shows the whole outcome.
//
// Arguments:
//
//   - arguments : The arguments that follow the program name.
//
// Returns:
//
//   - The description of the outcome.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string ParseOutcome ( std::initializer_list <const char*> arguments )
{
	// Build the argument vector, with the program name first.

	std::vector <const char*> argument_values { "enigma.exe" };

	argument_values.insert ( argument_values.end (), arguments.begin (), arguments.end () );

	// Parse it, and describe a rejection by its error message.

	CommandLineOptions options;
	std::string        error_message;
	int                argument_count = static_cast <int> ( argument_values.size () );

	if ( !ParseCommandLine ( argument_count, argument_values.data (), options, error_message ) )
	{
		return error_message;
	}

	// Describe an acceptance by what the program would print and how it would seed the run.

	std::string outcome = options.show_help ? "help" : "report";

	if ( options.has_seed )
	{
		outcome += ", seed " + std::to_string ( options.seed );
	}
	else
	{
		outcome += ", default seed";
	}

	return outcome;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: InvalidSeed
//
// Description:
//
//   Builds the error message of spec 4.4 for a --seed value that violates CLI-02.
//
// Arguments:
//
//   - value : The rejected value.
//
// Returns:
//
//   - The message "invalid seed '<value>': expected an integer from 0 to 18446744073709551615".
//
//---------------------------------------------------------------------------------------------------------------------

static std::string InvalidSeed ( const std::string& value )
{
	return "invalid seed '" + value + "': expected an integer from 0 to 18446744073709551615";
}

//---------------------------------------------------------------------------------------------------------------------
// Function: TestCommandLine
//
// Description:
//
//   Runs every AT-15 case against ParseCommandLine (spec 4), including the duplicate --seed cases of CLI-03, plus
//   cases that follow from CLI-02 and spec 4.4. Then checks the usage text against spec 4.5.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

static void TestCommandLine ()
{
	// AT-15, accepted command lines that print the report. Leading zeros are allowed, even before the largest seed.

	CHECK_EQUAL_TEXT ( ParseOutcome ( {} ),                                     "report, default seed"              );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "42" } ),                     "report, seed 42"                   );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "007" } ),                    "report, seed 7"                    );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "18446744073709551615" } ),   "report, seed 18446744073709551615" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "0018446744073709551615" } ), "report, seed 18446744073709551615" );

	// AT-15, accepted command lines that print the usage text (spec CLI-06).

	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--help" } ),                "help, default seed" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "-h" } ),                    "help, default seed" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "5", "--help" } ), "help, seed 5"       );

	// AT-15, seed values that violate CLI-02: one past the largest seed, a sign, a 0x prefix, and no digits at all.

	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "18446744073709551616" } ), InvalidSeed ( "18446744073709551616" ) );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "-1" } ),                   InvalidSeed ( "-1" )                   );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "0x10" } ),                 InvalidSeed ( "0x10" )                 );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "" } ),                     InvalidSeed ( "" )                     );

	// Further CLI-02 cases. std::stoull would accept the first two (RK-06). The value is always the next argument, so
	// --help after --seed is a seed value, not an option.

	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", " 42" } ),    InvalidSeed ( " 42" )    );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "+42" } ),    InvalidSeed ( "+42" )    );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "--help" } ), InvalidSeed ( "--help" ) );

	// AT-15, the other errors. A duplicate --seed is reported even when its value is missing or invalid (spec CLI-03),
	// and an error takes precedence over --help (spec CLI-05).

	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed" } ),                     "option '--seed' requires a value"     );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "1", "--seed", "2" } ), "option '--seed' given more than once" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "1", "--seed" } ),      "option '--seed' given more than once" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed", "1", "--seed", "x" } ), "option '--seed' given more than once" );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--seed=42" } ),                  "unknown option '--seed=42'"           );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--HELP" } ),                     "unknown option '--HELP'"              );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "42" } ),                         "unexpected argument '42'"             );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "--help", "--bogus" } ),          "unknown option '--bogus'"             );

	// Further spec 4.4 cases: a lone hyphen starts with - so it is an unknown option, and an empty argument is
	// positional.

	CHECK_EQUAL_TEXT ( ParseOutcome ( { "-" } ), "unknown option '-'"     );
	CHECK_EQUAL_TEXT ( ParseOutcome ( { "" } ),  "unexpected argument ''" );

	// The usage text, transcribed from spec 4.5.

	const char* expected_usage_text =
		"Usage: enigma.exe [--seed <n>] [--help]\n"
		"\n"
		"Options:\n"
		"  --seed <n>  Seed the random number generator with <n>, an integer from\n"
		"              0 to 18446744073709551615. The same seed reproduces the same table.\n"
		"  -h, --help  Show this help and exit.\n";

	CHECK_EQUAL_TEXT ( UsageText (), expected_usage_text );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: main
//
// Description:
//
//   Runs every test group, then prints the summary line "N checks, M failed".
//
// Returns:
//
//   - EXIT_CODE_SUCCESS if every check passed.
//   - EXIT_CODE_FAILURE if any check failed.
//
//---------------------------------------------------------------------------------------------------------------------

int main ()
{
	// Run every test group, one per module (architecture 12).

	TestHarness          ();
	TestBuildInfo        ();
	TestRandomSource     ();
	TestTransferFunction ();
	TestPerceptron       ();
	TestTable            ();
	TestStatistics       ();
	TestReport           ();
	TestCommandLine      ();

	// Print the summary line, and pass only if no check failed.

	std::printf ( "%d checks, %d failed\n", check_count, failure_count );

	if ( failure_count != 0 )
	{
		return EXIT_CODE_FAILURE;
	}

	return EXIT_CODE_SUCCESS;
}
