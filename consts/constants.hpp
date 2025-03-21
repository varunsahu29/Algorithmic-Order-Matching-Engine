#pragma once

#include <limits>

#include "usings.hpp"

struct Constants
{
    static const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();
};