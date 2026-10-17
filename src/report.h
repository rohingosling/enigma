//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   The report (spec 5): the header block, the table, and its statistics rows, rendered as one string. The module
//   writes nothing, because main does all of the output, and it does not include build_info: main passes the program
//   name, version, and incarnation in a ReportHeader, so the golden tests never depend on the real build counter
//   (architecture 5.2).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <string>

#include "statistics.h"
#include "table.h"

// The values shown in the report's header block (spec 5.2).

struct ReportHeader
{
	std::string program_name;                               // Program: Enigma.
	std::string version;                                    // Version: 1.0.
	int         incarnation;                                // Incarnation: the build counter.
};

// Formats one value as printf ( "%.3f" ) does (spec 5.4).

std::string FormatValue ( double value );

// Renders the whole report, R + 15 lines that each end in '\n' (spec 5.1).

std::string FormatReport ( const ReportHeader& header, const Table& table, const StatisticsSet& statistics );
