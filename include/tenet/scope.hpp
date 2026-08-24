#pragma once

// Scope guards: RAII-style cleanup attached to any scope.
//
// Each component lives in tenet/scope/ and can be included individually for
// faster builds; this header pulls in the whole family.

#include "tenet/concepts.hpp"
#include "tenet/scope/scope_exit.hpp"
#include "tenet/scope/scope_fail.hpp"
#include "tenet/scope/scope_success.hpp"
