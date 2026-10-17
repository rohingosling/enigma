//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   The table (spec 5.3): R data rows, each holding the perceptron's output y and the inputs x_0 .. x_7 it was
//   computed from. GenerateTable draws every row's inputs and evaluates y, and is one of the two functions that hold
//   the draw order (spec RNG-08). The other is CreateRandomPerceptron.
//
//   The table, the statistics, and the report share one column numbering, which matches the printed column order:
//   column 0 is y, and columns 1 to 8 are x_0 to x_7.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <vector>

#include "perceptron.h"
#include "random_source.h"

// The table's dimensions. Each row has a value column for y and one for each input (spec 5.3). The row count is a
// source constant, and the four-digit row index limits it to 9999 (spec OUT-22).

constexpr int COLUMN_COUNT      = INPUT_COUNT + 1;          // y, x_0 .. x_7.
constexpr int DEFAULT_ROW_COUNT = 16;                       // R.
constexpr int MAX_ROW_COUNT     = 9999;                     // Four-digit row index.

// One data row: the output y and the inputs x_0 .. x_7 it was computed from.

struct TableRow
{
	double      output;                                     // y.
	InputVector inputs;                                     // x_0 .. x_7.
};

using Table = std::vector <TableRow>;

// The value in one column of a row: column 0 is y, and column c from 1 to 8 is x_(c-1).

double ColumnValue ( const TableRow& row, int column );

// Draws each row's inputs in turn and evaluates its y, which is exactly 8 draws per row (spec RNG-08).

Table GenerateTable ( const Perceptron& perceptron, RandomSource& random_source, int row_count );
