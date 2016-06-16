/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ResourceScripting.h"
#include "ResourceManager.h"

LUA_FUNCTION(StartResource)
{
	fwString resourceName(luaL_checkstring(L, 1));

	auto resource = TheResources.GetResource(resourceName);

	if (!resource.GetRef())
	{
		lua_pushstring(L, va("no such resource: %s", resourceName.c_str()));
		lua_error(L);
		return 0;
	}

	resource->Start();

	return 0;
}

LUA_FUNCTION(StopResource)
{
	fwString resourceName(luaL_checkstring(L, 1));

	auto resource = TheResources.GetResource(resourceName);

	if (!resource.GetRef())
	{
		lua_pushstring(L, va("no such resource: %s", resourceName.c_str()));
		lua_error(L);
		return 0;
	}

	resource->Stop();

	return 0;
}

LUA_FUNCTION(IsResourceRunning)
{
	fwString resourceName(luaL_checkstring(L, 1));

	auto resource = TheResources.GetResource(resourceName);

	if (!resource.GetRef())
	{
		lua_pushboolean(L, false);
	}
	else
	{
		lua_pushboolean(L, (resource->GetState() == ResourceStateRunning));
	}

	return 1;
}

LUA_FUNCTION(ReadResourceFile)
{
	fwString path =GetCurrentLuaEnvironment()->GetResource()->GetPath();

	path += "/";
	path += luaL_checkstring(L, 1);

	fiDevice* device = fiDevice::GetDevice(path.c_str(), true);
	
	if (!device)
	{
		lua_pushstring(L, "no device for path");
		lua_error(L);
	}

	uint32_t handle = device->Open(path.c_str(), true);

	if (handle == -1)
	{
		lua_pushstring(L, "could not open file");
		lua_error(L);
	}

	int length = device->GetFileLength(handle);

	char* buffer = new char[length];
	device->Read(handle, buffer, length);

	device->Close(handle);

	lua_pushlstring(L, buffer, length);

	delete[] buffer;

	return 1;
}

LUA_FUNCTION(GetCurrentResource)
{
	lua_pushstring(L, GetCurrentLuaEnvironment()->GetResource()->GetName().c_str());

	return 1;
}

static int exports_index(lua_State* L)
{
	const char* index = luaL_checkstring(L, 2);
	fwString indexStr = index;

	auto resource = TheResources.GetResource(indexStr);

	// does the resource exist?
	if (!resource.GetRef())
	{
		lua_pushnil(L);
		return 1;
	}

	// is the resource started?
	if (resource->GetState() != ResourceStateRunning)
	{
		lua_pushstring(L, va("Resource %s has not been started.", index));
		lua_error(L);
		return 0;
	}

	// return a nice meta-ified object
	lua_createtable(L, 0, 1);
	int table = lua_gettop(L);

	// set the resource pointer in the table
	lua_pushstring(L, "resource");
	lua_pushlightuserdata(L, resource.GetRef());

	lua_settable(L, table);

	// set the metatable
	luaL_getmetatable(L, "resourceExports");

	lua_setmetatable(L, table);

	// and return the table
	return 1;
}

static const luaL_reg exports_metatable[] =
{
	{ "__index", exports_index },
	{ nullptr, nullptr }
};

int lua_error_handler (lua_State *l);

static int resourceExportCall(lua_State* L)
{
	STACK_BASE;

	// get number of arguments
	int nargs = lua_gettop(L);

	// stack: 0
	luaS_serializeArgs(L, 2, nargs - 1);
	int argTable = lua_gettop(L);

	size_t len;
	const char* jsonString = lua_tolstring(L, -1, &len);

	// get the resource pointer from the table
	lua_pushstring(L, "resource");
	lua_rawget(L, 1);

	// get the pointer in fact
	Resource* resource = (Resource*)lua_topointer(L, -1);
	lua_pop(L, 1);

	// get the upvalue with the export name
	const char* exportName = lua_tostring(L, lua_upvalueindex(1));

	// call the function in the resource
	fwString retVal = resource->CallExport(fwString(exportName), fwString(jsonString, len));

	if (g_errorOccurredThisFrame)
	{
		lua_remove(L, argTable);

		lua_pushstring(L, "export call error");
		lua_error(L);
		
		return 0;
	}

	lua_remove(L, argTable);

	// deserialize the return value
	int nArgs;
	int table = luaS_deserializeArgs(L, &nArgs, retVal);

	lua_remove(L, table);

	STACK_CHECK_N(nArgs);

	return nArgs;
}

static int resourceExport_index(lua_State* L)
{
	// get the resource pointer from the table
	lua_pushstring(L, "resource");
	lua_rawget(L, 1);

	// get the pointer in fact
	Resource* resource = (Resource*)lua_topointer(L, -1);
	lua_pop(L, 1);

	// get the export name
	const char* exportName = luaL_checkstring(L, 2);

	if (!resource->HasExport(fwString(exportName)))
	{
		trace("invalid export %s in resource %s\n", exportName, resource->GetName().c_str());
		return 0;
	}

	// create a closure with the function name
	lua_pushstring(L, exportName);
	lua_pushcclosure(L, resourceExportCall, 1);

	return 1;
}

static const luaL_reg resourceExport_metatable[] =
{
	{ "__index", resourceExport_index },
	{ nullptr, nullptr }
};

static InitFunction initFunction([] ()
{
	LuaScriptEnvironment::SignalResourceScriptInit.Connect([] (lua_State* L)
	{
		// base table
		lua_createtable(L, 0, 0);
		int tableIdx = lua_gettop(L);

		// base metatable
		luaL_newmetatable(L, "exports");
		int metaIdx = lua_gettop(L);

		// register methods
		luaL_register(L, nullptr, exports_metatable);

		// set the metatable
		lua_setmetatable(L, tableIdx);

		// set the global
		lua_setglobal(L, "exports");

		// create a metatable for the resource itself
		luaL_newmetatable(L, "resourceExports");
		luaL_register(L, nullptr, resourceExport_metatable);
		lua_pop(L, 1);
	});
});