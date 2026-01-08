#ifndef WRAPPERS_CHRONO_HPP
#define WRAPPERS_CHRONO_HPP
#include <chrono>

namespace Flux {
namespace chrono {
template<typename clock = std::chrono::system_clock>
double seconds_since_epoch ()
{
    auto duration_since_epoch = clock::now ().time_since_epoch ();
    return std::chrono::duration<double> (duration_since_epoch).count ();
}
}  // namespace chrono
}  // namespace Flux

#endif
