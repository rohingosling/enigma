//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements column access and table generation (spec 5.3, 7.4). Both functions assert their preconditions. The
//   build never defines NDEBUG, so the asserts stay active in release builds (architecture 11).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "table.h"

#include <cassert>
#include <cstddef>

//---------------------------------------------------------------------------------------------------------------------
// Function: ColumnValue
//
// Description:
//
//   Reads one value from a row, by the column numbering that the table, the statistics, and the report share: column
//   0 is y, and column c from 1 to 8 is x_(c-1). The column must lie in [0, COLUMN_COUNT).
//
// Arguments:
//
//   - row    : The row to read.
//   - column : The column number, from 0 to 8.
//
// Returns:
//
//   - The value in that column.
//
//---------------------------------------------------------------------------------------------------------------------

double ColumnValue ( const TableRow& row, int column )
{
	// Check the column range (architecture 11).

	assert ( column >= 0 && column < COLUMN_COUNT );

	// Column 0 is the output, and the inputs follow in index order.

	if ( column == 0 )
	{
		return row.output;
	}

	return row.inputs [ column - 1 ];
}

//---------------------------------------------------------------------------------------------------------------------
// Function: GenerateTable
//
// Description:
//
//   Generates the rows in order. For each row it draws x_0 .. x_7 in index order and then evaluates y, which draws
//   nothing, so the run's draws continue from the perceptron's parameters exactly as spec RNG-08 lists them. The row
//   count must lie in [1, MAX_ROW_COUNT] (spec OUT-22).
//
// Arguments:
//
//   - perceptron    : The run's neuron, which computes each row's y.
//   - random_source : The run's single random source, advanced by 8 draws per row.
//   - row_count     : The number of rows, R, from 1 to MAX_ROW_COUNT.
//
// Returns:
//
//   - The table, with its R rows in row order.
//
//---------------------------------------------------------------------------------------------------------------------

Table GenerateTable ( const Perceptron& perceptron, RandomSource& random_source, int row_count )
{
	// Check the row-count range (spec OUT-22, architecture 11).

	assert ( row_count >= 1 && row_count <= MAX_ROW_COUNT );

	// Draw each row's inputs in index order, then evaluate its output (spec RNG-08).

	Table table;

	table.reserve ( static_cast <std::size_t> ( row_count ) );

	for ( int i = 0; i < row_count; ++i )
	{
		TableRow row {};

		for ( int j = 0; j < INPUT_COUNT; ++j )
		{
			row.inputs [ j ] = random_source.NextUniform ();
		}

		row.output = perceptron.Evaluate ( row.inputs );

		table.push_back ( row );
	}

	return table;
}
