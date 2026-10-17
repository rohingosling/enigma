//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Program identity constants: the program name and version shown in the report's header block, and the
//   incarnation, the build counter that build.bat compiles in with -DENIGMA_INCARNATION=<n> (spec 5.2, 9.2).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

// build.bat defines ENIGMA_INCARNATION for every release build. Any other compile, such as the test build or a direct
// g++ invocation, falls back to 0 (spec BLD-14).

#ifndef ENIGMA_INCARNATION
#define ENIGMA_INCARNATION 0
#endif

// Program identity (spec OUT-11 to OUT-13). The version is edited by hand for a release.

constexpr const char* PROGRAM_NAME    = "Enigma";
constexpr const char* PROGRAM_VERSION = "1.0";
constexpr       int   INCARNATION     = ENIGMA_INCARNATION;
