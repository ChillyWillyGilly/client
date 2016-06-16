/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
//#include "GameFlags.h"
#include "ResourceManager.h"
#include <MissionCleanup.h>

fwRefContainer<Resource> ResourceManager::GetResource(fwString& name)
{
	if (m_isResetting)
	{
		return nullptr;
	}

	auto it = m_resources.find(name);

	if (it == m_resources.end())
	{
		return nullptr;
	}

	return it->second;
}

void ResourceManager::Tick()
{
	for (auto& resource : m_resources)
	{
		resource.second->Tick();
	}

	//for (auto& qEvent : m_eventQueue)
	for (auto it = m_eventQueue.begin(); it < m_eventQueue.end(); it++) // as the queue's end may change during this iteration
	{
		auto& qEvent = *it;

		TriggerEvent(qEvent.eventName, qEvent.argsSerialized, qEvent.source);
	}

	m_eventQueue.clear();
}

bool ResourceManager::TriggerEvent(fwString& eventName, fwString& argsSerialized, int source)
{
	/*if (eventName == "playerActivated")
	{
		GameFlags::SetFlag(GameFlag::PlayerActivated, true);
		
		for (auto& resource : m_resources)
		{
			msgpack::sbuffer nameArgs;
			msgpack::packer<msgpack::sbuffer> packer(nameArgs);

			packer.pack_array(1);
			packer.pack(resource.first);

			TriggerEvent(std::string("onClientResourceStart"), nameArgs);
		}
	}*/

	OnTriggerEvent(eventName, argsSerialized, source);

	m_eventCancelationState.push(false);

	for (auto& resource : m_resources)
	{
		resource.second->TriggerEvent(eventName, argsSerialized, source);

		/*if (g_errorOccurredThisFrame)
		{
			return false;
		}*/
	}

	m_eventCanceled = m_eventCancelationState.top();
	m_eventCancelationState.pop();

	return !WasEventCanceled();
}

bool ResourceManager::TriggerEvent(fwString& eventName, msgpack::sbuffer& argsSerialized, int source)
{
	return TriggerEvent(eventName, fwString(argsSerialized.data(), argsSerialized.size()), source);
}

bool ResourceManager::WasEventCanceled()
{
	return m_eventCanceled;
}

void ResourceManager::CancelEvent()
{
	m_eventCancelationState.top() = true;
}

void ResourceManager::QueueEvent(fwString& eventName, fwString& argsSerialized, uint64_t source)
{
	QueuedScriptEvent ev;
	ev.eventName = eventName;
	ev.argsSerialized = argsSerialized;
	ev.source = (int)source;

	m_eventQueue.push_back(ev);
}

fwRefContainer<Resource> ResourceManager::AddResource(fwString name, fwString path)
{
	auto resource = new Resource(name, path);

	m_resources[resource->GetName()] = resource;

	if (resource->Parse())
	{
		return resource;
	}

	m_resources.erase(resource->GetName());

	return nullptr;
}

void ResourceManager::DeleteResource(fwRefContainer<Resource> resource)
{
	m_resources.erase(resource->GetName());
}

void ResourceManager::ScanResources(fiDevice* device, fwString& path)
{
	rage::fiFindData findData;
	int handle = device->FindFirst(path.c_str(), &findData);

	if (!handle || handle == -1)
	{
		return;
	}

	do 
	{
		// dotfiles we don't want
		if (findData.fileName[0] == '.')
		{
			continue;
		}

		fwString fullPath = path;

		if (path[path.length() - 1] != '/')
		{
			fullPath.append("/");
		}

		fullPath.append(findData.fileName);

		// is this a directory?
		if (!(device->GetFileAttributes(fullPath.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
		{
			continue;
		}

		// as this is a directory, is this a resource subdirectory?
		if (findData.fileName[0] == '[')
		{
			char* endBracket = strrchr(findData.fileName, ']');

			if (!endBracket)
			{
				// invalid name
				trace("ignored %s - no end bracket\n", findData.fileName);
				continue;
			}

			// traverse the directory
			ScanResources(device, fullPath);
		}
		else
		{
			// probably a resource directory...
			AddResource(fwString(findData.fileName), fullPath);
		}
	} while (device->FindNext(handle, &findData));

	device->FindClose(handle);
}

void ResourceManager::ScanResources(fwVector<std::pair<fiDevice*, fwString>>& paths)
{
	for (auto& path : paths)
	{
		ScanResources(path.first, path.second);
	}
}

void ResourceManager::CleanUp()
{
	trace("CLEANUP - STOPPING ALL RESOURCES\n");

	for (auto& resource : m_resources)
	{
		resource.second->Stop();

		trace("stopped %s\n", resource.first.c_str());
	}

	trace("Resetting state...\n");

	Reset();

	trace("Cleanup complete.\n");
}

void ResourceManager::ForAllResources(fwAction<fwRefContainer<Resource>> cb)
{
	for (auto& resource : m_resources)
	{
		cb(resource.second);
	}
}

void ResourceManager::Reset()
{
	m_stateNumber++;

	// lock state so __gc callbacks won't mess up trying to reference us
	m_isResetting = true;

	m_resources.clear();

	m_isResetting = false;
}

ResourceCache* ResourceManager::GetCache()
{
	if (!m_resourceCache)
	{
		m_resourceCache = new ResourceCache();
		m_resourceCache->Initialize();
	}

	return m_resourceCache;
}

static InitFunction initFunction([] ()
{
	CMissionCleanup::OnCheckCollision.Connect([] ()
	{
		TheResources.ForAllResources([] (fwRefContainer<Resource> resource)
		{
			if (resource->GetState() != ResourceStateRunning)
			{
				return;
			}

			for (auto& environment : resource->GetScriptEnvironments())
			{
				auto cleanup = environment->GetMissionCleanup();

				if (cleanup)
				{
					cleanup->CheckIfCollisionHasLoadedForMissionObjects();
				}
			}
		});
	});
});

fwEvent<> ResourceManager::OnScriptReset;

__declspec(dllexport) ResourceManager TheResources;