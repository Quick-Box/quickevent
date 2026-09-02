#pragma once
#include <QString>

namespace Typst {

// Resolves the typst binary: next to the application first (bundled installs), falling back to PATH.
// Returns an empty string if typst cannot be found anywhere.
QString executablePath();

} // namespace Typst
