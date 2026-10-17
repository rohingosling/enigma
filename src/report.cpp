//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements the report rendering of architecture 9, in two passes. The first pass formats every value of the data
//   rows and the statistics rows, which fixes the field width W (spec OUT-40). The second builds each line from those
//   texts. Every line is right-trimmed and ends in '\n', which text-mode standard output turns into CR LF (spec
//   OUT-01, OUT-03).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "report.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>

// The layout's fixed widths (spec OUT-10, OUT-20, OUT-40, OUT-42).

constexpr int HEADER_LABEL_WIDTH  = 13;                     // "Incarnation: ", so every value starts in column 14.
constexpr int INDEX_WIDTH         = 4;                      // "0000" to "9998", or a statistics row label.
constexpr int MINIMUM_VALUE_WIDTH = 6;                      // W is never below 6, the width of "-0.882".
constexpr int COLUMN_LABEL_WIDTH  = 5;                      // A label starts at offset W - 5 within its field.

// The longest "%.3f" text of any double is 314 characters, for -DBL_MAX: a sign, 309 integer digits, a point, and
// three decimals. The buffer also holds the terminating null.

constexpr int VALUE_TEXT_CAPACITY = 320;

// The column header's labels, in column order: y, then x_0 .. x_7 (spec 5.3).

static_assert ( COLUMN_COUNT == 9, "COLUMN_LABELS must list one label per value column." );

constexpr std::array <const char*, COLUMN_COUNT> COLUMN_LABELS
{
	"y", "x_0", "x_1", "x_2", "x_3", "x_4", "x_5", "x_6", "x_7"
};

// The statistics rows' labels, in the order they are printed (spec OUT-23).

constexpr int STATISTICS_ROW_COUNT = 4;

constexpr std::array <const char*, STATISTICS_ROW_COUNT> STATISTICS_ROW_LABELS { "min", "max", "avg", "sum" };

// The formatted values of one data row or statistics row, numbered as ColumnValue numbers the columns.

using RowCells = std::array <std::string, COLUMN_COUNT>;

//---------------------------------------------------------------------------------------------------------------------
// Function: PadLeft
//
// Description:
//
//   Right-aligns text in a field by adding spaces on its left. Text that already fills the field is unchanged.
//
// Arguments:
//
//   - text  : The text to pad.
//   - width : The field width.
//
// Returns:
//
//   - The padded text, at least width characters long.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string PadLeft ( const std::string& text, int width )
{
	// Add the missing spaces, if any, in front of the text.

	std::size_t field_width = static_cast <std::size_t> ( width );

	if ( text.size () >= field_width )
	{
		return text;
	}

	return std::string ( field_width - text.size (), ' ' ) + text;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: PadRight
//
// Description:
//
//   Left-aligns text in a field by adding spaces on its right. Text that already fills the field is unchanged.
//
// Arguments:
//
//   - text  : The text to pad.
//   - width : The field width.
//
// Returns:
//
//   - The padded text, at least width characters long.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string PadRight ( const std::string& text, int width )
{
	// Add the missing spaces, if any, after the text.

	std::size_t field_width = static_cast <std::size_t> ( width );

	if ( text.size () >= field_width )
	{
		return text;
	}

	return text + std::string ( field_width - text.size (), ' ' );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: RightTrim
//
// Description:
//
//   Removes the spaces at the end of a text.
//
// Arguments:
//
//   - text : The text to trim.
//
// Returns:
//
//   - The text up to and including its last character that is not a space. Empty if the text is all spaces.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string RightTrim ( const std::string& text )
{
	// Keep everything up to the last character that is not a space.

	std::size_t last_kept_offset = text.find_last_not_of ( ' ' );

	if ( last_kept_offset == std::string::npos )
	{
		return std::string ();
	}

	return text.substr ( 0, last_kept_offset + 1 );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: EndLine
//
// Description:
//
//   Completes one line of the report. Every line passes through here, so none has trailing whitespace (spec OUT-03)
//   and each ends in '\n' (spec OUT-01). Only the column header actually loses padding.
//
// Arguments:
//
//   - text : The line's content, without a newline.
//
// Returns:
//
//   - The line, right-trimmed and ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string EndLine ( const std::string& text )
{
	return RightTrim ( text ) + '\n';
}

//---------------------------------------------------------------------------------------------------------------------
// Function: FormatRowIndex
//
// Description:
//
//   Formats a data row's index zero-padded to four digits, as in 0000, 0001, .. 0015 (spec OUT-21). The index must
//   lie in [0, MAX_ROW_COUNT), so it never needs a fifth digit.
//
// Arguments:
//
//   - row_index : The row number i, from 0 to R - 1.
//
// Returns:
//
//   - The four-digit index.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string FormatRowIndex ( int row_index )
{
	// Check the range, which guarantees four digits (spec OUT-22).

	assert ( row_index >= 0 && row_index < MAX_ROW_COUNT );

	// Zero-pad to the index width.

	char text [ INDEX_WIDTH + 1 ];

	std::snprintf ( text, sizeof ( text ), "%04d", row_index );

	return text;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: HeaderBlock
//
// Description:
//
//   Renders the header block (spec 5.2): the program name, the version, and the incarnation, each after a label
//   padded to 13 characters, so that every value starts in column 14 (spec OUT-10 to OUT-13).
//
// Arguments:
//
//   - header : The values to show.
//
// Returns:
//
//   - The block's three lines, each ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string HeaderBlock ( const ReportHeader& header )
{
	// One line per value. The incarnation is plain decimal, with no padding or grouping (spec OUT-13).

	std::string block;

	block += EndLine ( PadRight ( "Program:",     HEADER_LABEL_WIDTH ) + header.program_name                    );
	block += EndLine ( PadRight ( "Version:",     HEADER_LABEL_WIDTH ) + header.version                         );
	block += EndLine ( PadRight ( "Incarnation:", HEADER_LABEL_WIDTH ) + std::to_string ( header.incarnation ) );

	return block;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: RuleLine
//
// Description:
//
//   Renders a rule: one hyphen for every character of a data row, which is 4 + 9 × (1 + W) (spec OUT-24).
//
// Arguments:
//
//   - width : The value field width, W.
//
// Returns:
//
//   - The rule, ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string RuleLine ( int width )
{
	// The index field, then each value field with the space before it.

	return EndLine ( std::string ( static_cast <std::size_t> ( INDEX_WIDTH + COLUMN_COUNT * ( 1 + width ) ), '-' ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ColumnHeaderLine
//
// Description:
//
//   Renders the column header (spec OUT-25): i in the index field, then each label after one space. A label is
//   left-aligned in a 5-character slot that is right-aligned in its W-character field, so it starts at offset W - 5,
//   above the units digit of a right-aligned d.ddd (spec OUT-42). Trimming removes the padding after the last label,
//   so the line is 2 characters shorter than a rule (spec OUT-43).
//
// Arguments:
//
//   - width : The value field width, W.
//
// Returns:
//
//   - The column header, ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string ColumnHeaderLine ( int width )
{
	// The index field's label, then each value column's label in its field.

	std::string line = PadRight ( "i", INDEX_WIDTH );

	for ( const char* label : COLUMN_LABELS )
	{
		line += ' ';
		line += PadLeft ( PadRight ( label, COLUMN_LABEL_WIDTH ), width );
	}

	return EndLine ( line );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: ValueLine
//
// Description:
//
//   Renders a data row or a statistics row: the index or the statistic's label, left-aligned in the index field, then
//   each value right-aligned in its W-character field after one space (spec OUT-20, OUT-23, OUT-41).
//
// Arguments:
//
//   - index_text : The row index, such as 0000, or a statistics row label, such as min.
//   - cells      : The row's formatted values, in column order.
//   - width      : The value field width, W.
//
// Returns:
//
//   - The row, ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

static std::string ValueLine ( const std::string& index_text, const RowCells& cells, int width )
{
	// The index field, then each value in its field.

	std::string line = PadRight ( index_text, INDEX_WIDTH );

	for ( const std::string& cell : cells )
	{
		line += ' ';
		line += PadLeft ( cell, width );
	}

	return EndLine ( line );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: FormatValue
//
// Description:
//
//   Formats one value in fixed-point notation with exactly three decimals, as printf ( "%.3f" ) does. The C runtime
//   rounds the exact binary value to nearest, keeps the sign of a negative value that rounds to zero, and never prints
//   a plus sign, a thousands separator, or an exponent (spec OUT-30 to OUT-33).
//
// Arguments:
//
//   - value : The value to format.
//
// Returns:
//
//   - The formatted value, such as 0.000, -0.000, 1.000, or -10.812.
//
//---------------------------------------------------------------------------------------------------------------------

std::string FormatValue ( double value )
{
	// Format with the C runtime. The buffer holds the text of any double.

	char text [ VALUE_TEXT_CAPACITY ];
	int  text_length = std::snprintf ( text, sizeof ( text ), "%.3f", value );

	assert ( text_length > 0 && text_length < VALUE_TEXT_CAPACITY );

	return std::string ( text, static_cast <std::size_t> ( text_length ) );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: FormatReport
//
// Description:
//
//   Renders the whole report in the two passes of architecture 9. The first pass formats all 9 × (R + 4) values, the
//   data rows first and then the statistics rows, and takes W as the larger of 6 and the longest of them, so that a
//   wide sum widens every value column (spec OUT-40). The second pass emits the R + 15 lines of spec 5.1. The table
//   must hold from 1 to MAX_ROW_COUNT rows (spec OUT-22).
//
// Arguments:
//
//   - header     : The program name, version, and incarnation for the header block.
//   - table      : The data rows.
//   - statistics : The statistics of each value column, computed from the same table.
//
// Returns:
//
//   - The report, every line ending in '\n'.
//
//---------------------------------------------------------------------------------------------------------------------

std::string FormatReport ( const ReportHeader& header, const Table& table, const StatisticsSet& statistics )
{
	// Check the row count, which also keeps every row index to four digits (spec OUT-21, OUT-22, architecture 11).

	std::size_t row_count = table.size ();

	assert ( row_count >= 1 && row_count <= static_cast <std::size_t> ( MAX_ROW_COUNT ) );

	// First pass: format every value. The data rows come first, then the statistics rows in the order of
	// STATISTICS_ROW_LABELS.

	std::vector <RowCells> row_cells ( row_count + STATISTICS_ROW_COUNT );

	for ( std::size_t i = 0; i < row_count; ++i )
	{
		for ( int column = 0; column < COLUMN_COUNT; ++column )
		{
			row_cells [ i ] [ column ] = FormatValue ( ColumnValue ( table [ i ], column ) );
		}
	}

	for ( int column = 0; column < COLUMN_COUNT; ++column )
	{
		const ColumnStatistics& column_statistics = statistics [ column ];

		row_cells [ row_count     ] [ column ] = FormatValue ( column_statistics.minimum );
		row_cells [ row_count + 1 ] [ column ] = FormatValue ( column_statistics.maximum );
		row_cells [ row_count + 2 ] [ column ] = FormatValue ( column_statistics.average );
		row_cells [ row_count + 3 ] [ column ] = FormatValue ( column_statistics.sum     );
	}

	// W is the larger of 6 and the longest value anywhere, including the statistics rows (spec OUT-40).

	int width = MINIMUM_VALUE_WIDTH;

	for ( const RowCells& cells : row_cells )
	{
		for ( const std::string& cell : cells )
		{
			width = std::max ( width, static_cast <int> ( cell.size () ) );
		}
	}

	// Second pass: emit the lines in the order of spec 5.1, starting and ending with an empty line.

	std::string rule   = RuleLine ( width );
	std::string report = "\n";

	report += HeaderBlock ( header );
	report += '\n';
	report += rule;
	report += ColumnHeaderLine ( width );
	report += rule;

	for ( std::size_t i = 0; i < row_count; ++i )
	{
		report += ValueLine ( FormatRowIndex ( static_cast <int> ( i ) ), row_cells [ i ], width );
	}

	report += rule;

	for ( std::size_t k = 0; k < STATISTICS_ROW_LABELS.size (); ++k )
	{
		report += ValueLine ( STATISTICS_ROW_LABELS [ k ], row_cells [ row_count + k ], width );
	}

	report += rule;
	report += '\n';

	return report;
}
