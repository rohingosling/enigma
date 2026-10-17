//---------------------------------------------------------------------------------------------------------------------
// Project: Enigma
// Version: 1.0
// Date:    2026-10-16
// Author:  Rohin Gosling
//
// Description:
//
//   The network (spec 6): a single neuron with 8 inputs, 8 weights, and a bias, and its transfer function, computed
//   from first principles. CreateRandomPerceptron draws the parameters, and is one of the two functions that hold the
//   draw order (spec RNG-08). The other is GenerateTable.
//
// TODO:
//
//   1. None.
//
//---------------------------------------------------------------------------------------------------------------------

#pragma once

#include <array>

#include "random_source.h"

// The number of inputs, and so of weights (spec NET-01).

constexpr int INPUT_COUNT = 8;

using WeightVector = std::array <double, INPUT_COUNT>;     // w_0 .. w_7.
using InputVector  = std::array <double, INPUT_COUNT>;     // x_0 .. x_7.

// The hyperbolic tangent, in the overflow-free form of spec 6.3.

double TransferFunction ( double activation );

//---------------------------------------------------------------------------------------------------------------------
// Class: Perceptron
//
// Description:
//
//   One untrained neuron: 8 weights and a bias, fixed at construction (spec NET-01, NET-02). The constructor takes
//   explicit parameters, so tests can build a neuron with known weights. A run builds its neuron with
//   CreateRandomPerceptron.
//
//---------------------------------------------------------------------------------------------------------------------

class Perceptron
{
private:

	WeightVector m_weights;
	double       m_bias;

public:

	const WeightVector& Weights () const;
	double              Bias    () const;

	Perceptron ( const WeightVector& weights, double bias );

	double Evaluate ( const InputVector& inputs ) const;
};

// Draws w_0 .. w_7 and then b, which is exactly 9 draws (spec NET-09).

Perceptron CreateRandomPerceptron ( RandomSource& random_source );
