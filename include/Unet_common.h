#pragma once

#include <Unet/guid.hpp>
#include <Unet/json.hpp>
using json = nlohmann::json;

#include <Unet/Utils.h>
#include <Unet/System.h>

namespace Unet
{
	class IContext;
	class ICallbacks;

	class Lobby;
	class Service;

	namespace Internal
	{
		class Context;
	}
}
