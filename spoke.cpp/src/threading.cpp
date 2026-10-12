// ThreadPool is header-inline (see include/spoke/threading.h).
// This TU exists so the static library always has at least one symbol and to
// host any future platform-specific thread-affinity tuning.
#include "spoke/threading.h"

namespace spoke {
// Reserved for future: setThreadAffinity(), numaHints(), etc.
}  // namespace spoke
