#include "main.h"

bool freeze = false;

#ifndef NO_NAMESPACE_GIAC
namespace giac {
#endif

#ifndef TICE
void set_abort() {}
void clear_abort() {}
#endif

#ifndef NO_NAMESPACE_GIAC
}
#endif
