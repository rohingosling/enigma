//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Column statistics (spec 8): the minimum, maximum, average, and sum of each of the nine value columns, computed
//   over the data rows from full-precision values (spec STA-01, STA-02). The index column has none (spec STA-03).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <array>

#include "table.h"

// The statistics of one value column, in the order of the report's statistics rows (spec OUT-23).

struct ColumnStatistics
{
	double minimum;                                         // min: the smallest value.
	double maximum;                                         // max: the largest value.
	double average;                                         // avg: sum / R.
	double sum;                                             // sum: the total, from 0.0 in row order.
};

// One ColumnStatistics per value column, numbered as ColumnValue numbers them: 0 is y, and 1 to 8 are x_0 to x_7.

using StatisticsSet = std::array <ColumnStatistics, COLUMN_COUNT>;

// Computes every column's statistics over a table of at least one row (spec STA-04).

StatisticsSet ComputeStatistics ( const Table& table );
