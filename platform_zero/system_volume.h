#pragma once

namespace bitmap16::Zero {

// Changes the same PipeWire output level used by HOME, in five-percent steps.
// Returns the new percentage, or -1 when the system output is unavailable.
int adjustSystemVolume(bool increase);

}  // namespace bitmap16::Zero
