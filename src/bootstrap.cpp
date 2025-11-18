#include "bootstrap.h"

#include <model.hpp>

namespace stemsmith
{
bool demucs_dependencies_ready()
{
    const auto four_source = demucscpp::initialize_crosstransformer(true);
    const auto six_source = demucscpp::initialize_crosstransformer(false);

    return static_cast<bool>(four_source) && static_cast<bool>(six_source);
}
} // namespace stemsmith
