#include <iostream>

#include "logging.h"

void logging::raw_log(const std::string& msg)
{
    std::clog << msg << std::endl;
}
