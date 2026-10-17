//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements RandomSource and CreateDefaultSeed (spec 7). The uniform mapping is written out by hand rather than
//   taken from std::uniform_real_distribution, whose algorithm is implementation-defined (spec RNG-04).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "random_source.h"

#include <chrono>

// 2^-53, which scales a 53-bit integer into [0, 1) (spec RNG-03). A hexadecimal floating literal states it exactly.

constexpr double UNIFORM_SCALE = 0x1.0p-53;

//---------------------------------------------------------------------------------------------------------------------
// Function: RandomSource::RandomSource
//
// Description:
//
//   Constructs the engine as std::mt19937_64 ( seed ) (spec RNG-05).
//
// Arguments:
//
//   - seed : The run's seed, from --seed or CreateDefaultSeed.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

RandomSource::RandomSource ( std::uint64_t seed )
	: m_engine ( seed )
{
}

//---------------------------------------------------------------------------------------------------------------------
// Function: RandomSource::NextUniform
//
// Description:
//
//   Draws one engine output v and maps it to 2 * ((v >> 11) * 2^-53) - 1 (spec RNG-03). Every step is exact in
//   binary64, so each value is a multiple of 2^-52 from -1 up to 1 - 2^-52.
//
// Returns:
//
//   - The next value in [-1, 1).
//
//---------------------------------------------------------------------------------------------------------------------

double RandomSource::NextUniform ()
{
	// Keep the top 53 bits of one engine output, and scale them into [0, 1).

	std::uint64_t engine_output = m_engine ();
	double        unit_value    = static_cast <double> ( engine_output >> 11 ) * UNIFORM_SCALE;

	// Stretch [0, 1) to [-1, 1).

	return 2.0 * unit_value - 1.0;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: CreateDefaultSeed
//
// Description:
//
//   Assembles a 64-bit seed from two 32-bit std::random_device draws, and XORs in the std::chrono::steady_clock tick
//   count (spec RNG-06). The clock guards against a deterministic random_device, which older MinGW-w64 releases had
//   (architecture 8.2). This and main are the program's only contact with the outside world.
//
// Returns:
//
//   - An unpredictable seed.
//
//---------------------------------------------------------------------------------------------------------------------

std::uint64_t CreateDefaultSeed ()
{
	// Draw 32 bits for each half of the seed from the operating system's random number generator.

	std::random_device entropy_source;

	std::uint64_t high_bits = static_cast <std::uint64_t> ( entropy_source () ) & 0xFFFFFFFFu;
	std::uint64_t low_bits  = static_cast <std::uint64_t> ( entropy_source () ) & 0xFFFFFFFFu;

	// Mix in the clock, which differs between runs even if the draws do not.

	std::chrono::steady_clock::rep tick_count = std::chrono::steady_clock::now ().time_since_epoch ().count ();

	return ( ( high_bits << 32 ) | low_bits ) ^ static_cast <std::uint64_t> ( tick_count );
}
