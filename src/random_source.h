//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   The run's single source of randomness (spec 7). RandomSource wraps one std::mt19937_64 engine and maps each output
//   to a double in [-1, 1) with exact arithmetic, so a seed yields the same values under every conforming standard
//   library. CreateDefaultSeed supplies an unpredictable seed for a run without --seed.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <random>

//---------------------------------------------------------------------------------------------------------------------
// Class: RandomSource
//
// Description:
//
//   One seeded std::mt19937_64 engine whose outputs are mapped to [-1, 1) (spec RNG-01 to RNG-05). Exactly one exists
//   per run, and it is passed by reference. Copying is deleted, because a copy would fork the sequence and silently
//   break the draw order (spec RNG-08).
//
//---------------------------------------------------------------------------------------------------------------------

class RandomSource
{
private:

	std::mt19937_64 m_engine;

public:

	explicit RandomSource ( std::uint64_t seed );

	RandomSource ( const RandomSource& )             = delete;
	RandomSource& operator = ( const RandomSource& ) = delete;

	double NextUniform ();
};

// An unpredictable seed, for a run without --seed (spec RNG-06).

std::uint64_t CreateDefaultSeed ();
