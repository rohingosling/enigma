//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   Implements the transfer function, the forward pass, and the random parameters (spec 6). The transfer function is
//   built on std::expm1 alone, because production code must not call the library's tanh (spec NET-05).
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#include "perceptron.h"

#include <cmath>

//---------------------------------------------------------------------------------------------------------------------
// Function: TransferFunction
//
// Description:
//
//   Computes tanh ( a ) without overflow (spec NET-06). For a >= 0, with m = expm1 ( -2a ), so that e^-2a = 1 + m,
//   tanh ( a ) = ( 1 - e^-2a ) / ( 1 + e^-2a ) = -m / ( 2 + m ). The exponent is never positive, so the result
//   saturates at exactly 1 instead of overflowing, and expm1 keeps full precision near 0. A negative activation goes
//   through odd symmetry, which makes TransferFunction ( -a ) == -TransferFunction ( a ) exact (spec NET-08).
//
// Arguments:
//
//   - activation : The neuron's activation, a.
//
// Returns:
//
//   - tanh ( a ), in [-1, 1].
//
//---------------------------------------------------------------------------------------------------------------------

double TransferFunction ( double activation )
{
	// Reflect a negative activation through odd symmetry.

	if ( activation < 0.0 )
	{
		return -TransferFunction ( -activation );
	}

	// Here a >= 0, so -1 < m <= 0, and neither the exponent nor the quotient can overflow.

	double exponential_minus_one = std::expm1 ( -2.0 * activation );

	return -exponential_minus_one / ( 2.0 + exponential_minus_one );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: Perceptron::Weights
//
// Description:
//
//   Value accessor for the weights.
//
// Returns:
//
//   - The weights w_0 .. w_7.
//
//---------------------------------------------------------------------------------------------------------------------

const WeightVector& Perceptron::Weights () const
{
	return m_weights;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: Perceptron::Bias
//
// Description:
//
//   Value accessor for the bias.
//
// Returns:
//
//   - The bias b.
//
//---------------------------------------------------------------------------------------------------------------------

double Perceptron::Bias () const
{
	return m_bias;
}

//---------------------------------------------------------------------------------------------------------------------
// Function: Perceptron::Perceptron
//
// Description:
//
//   Constructs a neuron with the given parameters, which stay fixed for its lifetime (spec NET-02).
//
// Arguments:
//
//   - weights : The weights w_0 .. w_7.
//   - bias    : The bias b.
//
// Returns:
//
//   - None.
//
//---------------------------------------------------------------------------------------------------------------------

Perceptron::Perceptron ( const WeightVector& weights, double bias )
	: m_weights ( weights ),
	  m_bias    ( bias )
{
}

//---------------------------------------------------------------------------------------------------------------------
// Function: Perceptron::Evaluate
//
// Description:
//
//   The forward pass (spec 6.2). The activation starts from the bias and adds w_j * x_j for j = 0 to 7 in ascending
//   order (spec NET-04). The order is fixed, and the build forbids fused multiply-add, so a seed gives the same output
//   bit for bit.
//
// Arguments:
//
//   - inputs : The inputs x_0 .. x_7.
//
// Returns:
//
//   - The output y = tanh ( a ), in [-1, 1].
//
//---------------------------------------------------------------------------------------------------------------------

double Perceptron::Evaluate ( const InputVector& inputs ) const
{
	// Accumulate the activation from the bias, in ascending input order.

	double activation = m_bias;

	for ( int j = 0; j < INPUT_COUNT; ++j )
	{
		activation += m_weights [ j ] * inputs [ j ];
	}

	// Apply the transfer function.

	return TransferFunction ( activation );
}

//---------------------------------------------------------------------------------------------------------------------
// Function: CreateRandomPerceptron
//
// Description:
//
//   Draws the run's parameters: w_0 .. w_7 in index order, then b, each in [-1, 1) (spec NET-09). The draws happen in
//   statement order, before any input is drawn (spec RNG-08).
//
// Arguments:
//
//   - random_source : The run's single random source, advanced by 9 draws.
//
// Returns:
//
//   - The new perceptron.
//
//---------------------------------------------------------------------------------------------------------------------

Perceptron CreateRandomPerceptron ( RandomSource& random_source )
{
	// Draw the weights in index order.

	WeightVector weights {};

	for ( int j = 0; j < INPUT_COUNT; ++j )
	{
		weights [ j ] = random_source.NextUniform ();
	}

	// Draw the bias last.

	double bias = random_source.NextUniform ();

	return Perceptron ( weights, bias );
}
