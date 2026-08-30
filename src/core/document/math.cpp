//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @class cMath
 *
 * @brief Math expression evaluator for WordStar dot-command calculations.
 *
 * Implements the cMath class, which evaluates inline math expressions found
 * in WordStar documents (e.g., column arithmetic in dot commands). Uses the
 * PicoMath lightweight expression parser to evaluate string expressions and
 * return numeric results.
 *
 * @section math_design Design Rationale
 * This class is intentionally compiled in a separate translation unit from
 * layoutbase.cpp to isolate the math library's header (picomath.hpp) and its
 * slow compilation from the main layout engine. This significantly speeds up
 * incremental builds when layout code changes but math evaluation does not.
 * (carry over from using exprtk, which had very slow compilation times)
 *
 * @section math_usage Usage
 * The layout engine calls Evaluate() with a string expression (e.g., "3+4*2")
 * and receives a double result. The cMath instance is owned by cLayoutBase and
 * used during dot command processing for computed values.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cLayoutBase Layout engine that owns the math evaluator
 * @see cDotCommandParser Dot command parser that triggers math evaluation
 */

#include "math.h"

//#include "exprtk.hpp"
#include "picomath.hpp"

//exprtk::parser<double> gParser;                         ///< math expression parser

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor.
///
/////////////////////////////////////////////////////////////////////////////
cMath::cMath(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cMath::~cMath(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string expression_str [in] math expression to evaluate
///
/// @return the result of the expression, or 0.0 on failure
///
/// @brief
/// Evaluate a math expression string using the exprtk parser.
/// Returns 0.0 if the expression fails to compile.
///
/////////////////////////////////////////////////////////////////////////////
double cMath::DoMath(std::string expression_str)
{
    double value = 0.0 ;

    // Evaluate the math expression using PicoMath
    picomath::PicoMath pm ;

    auto result = pm.evalExpression(expression_str.c_str()) ;
    if (result.isOk())
    {
        value = result.getResult() ;
    }
    else
    {
        value = 0.0 ;
    }

/*
    // Old exprtk implementation (kept for reference)
    typedef exprtk::expression<double>     expression_t;
    expression_t expression;
    if(gParser.compile(expression_str, expression))
    {
        value = expression.value();
    }
    else
    {
        value = 0.0 ;
    }
*/

    return value ;
}
