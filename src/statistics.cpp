//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements the column statistics (spec 8). Each sum is accumulated in a fixed order, so a seed gives the same
//   statistics bit for bit (architecture 8.3).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "statistics.h"

#include <algorithm>
#include <cassert>

//---------------------------------------------------------------------------------------------------------------------
// Function: ComputeStatistics
//
// Description:
//
//   Computes min, max, avg, and sum for each value column over the table's rows (spec STA-01), from the full-precision
//   values rather than the rounded values that are printed (spec STA-02). Each sum starts from 0.0 and adds the rows
//   in order, i = 0 to R - 1, and each average is that sum divided by R. The minimum and maximum start from the first
//   row's value, so a column that is entirely negative or entirely positive still gets the right extremes. The table
//   must hold at least one row (spec STA-04).
//
// Arguments:
//
//   - table : The data rows, at least one.
//
// Returns:
//
//   - The statistics of each value column, numbered as ColumnValue numbers them.
//
//---------------------------------------------------------------------------------------------------------------------

StatisticsSet ComputeStatistics ( const Table& table )
{
	// Check that the table has a row, so that the average never divides by zero (spec STA-04, architecture 11).

	assert ( !table.empty () );

	// Summarise each column in turn, visiting the rows in order.

	StatisticsSet statistics {};
	double        row_count = static_cast <double> ( table.size () );

	for ( int column = 0; column < COLUMN_COUNT; ++column )
	{
		ColumnStatistics& column_statistics = statistics [ column ];
		double            first_value       = ColumnValue ( table.front (), column );

		column_statistics.minimum = first_value;
		column_statistics.maximum = first_value;
		column_statistics.sum     = 0.0;

		for ( const TableRow& row : table )
		{
			double value = ColumnValue ( row, column );

			column_statistics.minimum  = std::min ( column_statistics.minimum, value );
			column_statistics.maximum  = std::max ( column_statistics.maximum, value );
			column_statistics.sum     += value;
		}

		column_statistics.average = column_statistics.sum / row_count;
	}

	return statistics;
}
