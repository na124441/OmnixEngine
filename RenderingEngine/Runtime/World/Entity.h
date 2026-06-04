#pragma once
#include "core/types/ID.h"

namespace eng::runtime {

	/** Opaque type used throughout the engine to identify an entity. */
	using Entity = eng::core::ID<struct EntityTag>;

} // namespace eng::runtime
