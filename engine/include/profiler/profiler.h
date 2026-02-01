#pragma once

#include "TracyC.h"

#define RL_PROFILE_ZONE(var, name) TracyCZoneN(var, name, true)
#define RL_PROFILE_ZONE_END(var) TracyCZoneEnd(var)
#define RL_PROFILE_FRAME_MARK() TracyCFrameMark
