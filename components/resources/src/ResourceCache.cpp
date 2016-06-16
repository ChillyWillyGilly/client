/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ResourceCache.h"
#include <regex>
#include <strsafe.h>
#include "SHA1.h"

void ResourceCache::Initialize()
{
	m_cache.reserve(15000);
}

bool ResourceCache::ParseFileName(const char* inString, fwString& fileNameOut, fwString& resourceNameOut, fwString& hashOut)
{
	// check if the file name meets the minimum length for there to be a hash
	int length = strlen(inString);

	if (length < 44)
	{
		return false;
	}

	// find the file extension
	const char* dotPos = strchr(inString, '.');

	if (!dotPos)
	{
		return false;
	}

	// find the first underscore following the file extension
	const char* underscorePos = strchr(inString, '_');

	if (!underscorePos)
	{
		return false;
	}

	// store the file name
	fileNameOut = fwString(inString, underscorePos - inString);

	// check if we have a hash
	const char* hashStart = &inString[length - 41];

	if (*hashStart != '_')
	{
		return false;
	}

	hashOut = hashStart + 1;

	// then, make a string between the underscore and the hash
	resourceNameOut = fwString(underscorePos + 1, hashStart - (underscorePos + 1));

	return true;
}

void ResourceCache::LoadCache(rage::fiDevice* device)
{
	rage::fiFindData findData;
	int handle = device->FindFirst("rescache:/", &findData);

	m_cache.clear();

	if (!handle || handle == -1)
	{
		return;
	}

	do 
	{
		fwString resourceName;
		fwString fileName;
		fwString hash;

		if (!ParseFileName(findData.fileName, fileName, resourceName, hash))
		{
			continue;
		}

		AddEntry(fileName, resourceName, hash);
	} while (device->FindNext(handle, &findData));

	device->FindClose(handle);

	// store the cache device
	m_cacheDevice = device;
}

fwVector<ResourceDownload> ResourceCache::GetDownloadsFromList(fwVector<ResourceData>& resourceList)
{
	fwVector<ResourceDownload> downloads;

	// for all resources, check files in cache
	for (auto& resource : resourceList)
	{
		for (auto& file : resource.GetFiles())
		{
			// create a cache entry to look up
			CacheEntry entry;
			entry.resource = resource.GetName();
			entry.filename = file.filename;
			entry.hash = file.hash;

			LowerString(entry.resource);
			LowerString(entry.filename);
			LowerString(entry.hash);

			// look up the cache entry
			if (m_cacheSet.find(entry) == m_cacheSet.end())
			{
				downloads.push_back(GetResourceDownload(resource, file));
			}
		}
	}

	return downloads;
}

ResourceDownload ResourceCache::GetResourceDownload(const ResourceData& resource, const ResourceFile& file)
{
	ResourceDownload download;
	download.targetFilename = va("rescache:/unconfirmed/%s_%s_%s", file.filename.c_str(), resource.GetName().c_str(), file.hash.c_str());
	download.sourceUrl = va("%s/%s/%s", resource.GetBaseURL().c_str(), resource.GetName().c_str(), file.filename.c_str());
	download.filename = file.filename;
	download.resname = resource.GetName();

	return download;
}

void ResourceCache::AddFile(fwString& sourcePath, fwString& filename, fwString& resource)
{
	m_dataLock.lock();

	// hash the file
	rage::fiDevice* device = rage::fiDevice::GetDevice(sourcePath.c_str(), true);

	if (!device)
	{
		FatalError("Tried to add non-existent file %s to cache.", sourcePath.c_str());
	}

	int handle = device->Open(sourcePath.c_str(), true);

	int read;
	char buffer[4096];

	sha1nfo sha;
	sha1_init(&sha);

	while ((read = device->Read(handle, buffer, sizeof(buffer))) > 0)
	{
		sha1_write(&sha, buffer, read);
	}

	device->Close(handle);

	uint8_t* hash = sha1_result(&sha);

	// format the hash as a string
	fwString hashString = va("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
							 hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7], hash[8], hash[9],
							 hash[10], hash[11], hash[12], hash[13], hash[14], hash[15], hash[16], hash[17], hash[18], hash[19]);

	device->RenameFile(sourcePath.c_str(), va("rescache:/%s_%s_%s", filename.c_str(), resource.c_str(), hashString.c_str()));

	m_dataLock.unlock();

	AddEntry(filename, resource, hashString);
}

void ResourceCache::AddEntry(fwString fileName, fwString resourceName, fwString hash)
{
	CacheEntry entry;
	entry.resource = resourceName;
	entry.filename = fileName;
	entry.hash = hash;

	LowerString(entry.resource);
	LowerString(entry.filename);
	LowerString(entry.hash);

	m_dataLock.lock();
	m_cache.push_back(entry);
	m_cacheSet.insert(entry);
	m_dataLock.unlock();
}

void ResourceCache::ClearMark()
{
	m_dataLock.lock();
	m_markList.clear();
	m_dataLock.unlock();
}

fwString ResourceCache::GetMarkedFilenameFor(fwString resource, fwString filename)
{
	LowerString(resource);
	LowerString(filename);

	m_dataLock.lock();

	auto& entry = m_markList[resource + "__" + filename];

	const char* str = va("rescache:/%s_%s_%s", entry.filename.c_str(), entry.resource.c_str(), entry.hash.c_str());

	m_dataLock.unlock();

	return str;
}

void ResourceCache::MarkList(fwVector<ResourceData>& resourceList)
{
	for (auto& resource : resourceList)
	{
		for (auto& file : resource.GetFiles())
		{
			// create a cache entry to look up
			CacheEntry entry;
			entry.resource = resource.GetName();
			entry.filename = file.filename;
			entry.hash = file.hash;

			LowerString(entry.resource);
			LowerString(entry.filename);
			LowerString(entry.hash);

			m_markList[entry.resource + "__" + entry.filename] = entry;
		}
	}
}

void ResourceCache::MarkStreamingList(fwVector<StreamingResource>& streamList)
{
	for (auto& stream : streamList)
	{
		CacheEntry entry;
		entry.resource = stream.resData.GetName();
		entry.filename = stream.filename;
		entry.hash = stream.hash;

		LowerString(entry.resource);
		LowerString(entry.filename);
		LowerString(entry.hash);

		m_markList[entry.resource + "__" + entry.filename] = entry;
	}
}

// ----------------------------------------------------------------------------

ResourceData::ResourceData(fwString name, fwString baseUrl)
	: m_name(name), m_baseUrl(baseUrl), m_processed(false)
{

}

void ResourceData::AddFile(fwString filename, fwString hash)
{
	ResourceFile file;
	file.filename = filename;
	file.hash = hash;

	m_files.push_back(file);
}

bool CacheEntry::operator==(const CacheEntry& right) const
{
	if (filename != right.filename)
	{
		return false;
	}

	if (resource != right.resource)
	{
		return false;
	}

	if (hash != right.hash)
	{
		return false;
	}

	return true;
}

bool CacheEntry::operator!=(const CacheEntry& right) const
{
	return !(*this == right);
}