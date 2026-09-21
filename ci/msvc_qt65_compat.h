// MSVC compatibility shim for Qt 6.5.3 (force-included via /FI in CI).
//
// Qt 6.5.3 headers call stdext::make_checked_array_iterator() and
// stdext::make_unchecked_array_iterator() (see qcompilerdetection.h) without
// including <iterator>. Older MSVC STL versions pulled <iterator> in
// transitively; newer toolchains (VS2022 17.13+, VS2026 on windows-latest)
// no longer do, which breaks compilation with:
//   qvarlengtharray.h(379): error C3861: 'stdext': identifier not found
// Force-including this header restores the declaration.
// The __cplusplus guard keeps C sources (miniz) safe from C++ STL headers.
#ifdef __cplusplus
#include <iterator>
#endif
