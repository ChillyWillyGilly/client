/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ResourceManager.h"
#include "DownloadMgr.h"
#include "ResourceCache.h"
#include "BoundStreaming.h"
#include <Streaming.h>
#include <mmsystem.h>
#include "CrossLibraryInterfaces.h"
#include "StreamingTypes.h"
#include "IdeStore.h"

static bool _wbnMode;

static HttpClient* httpClient;

class CitizenStreamingFile : public StreamingFile
{
private:
	StreamingResource m_entry;

	rage::fiDevice* m_device;

	uint32_t m_handle;

	ResourceDownload m_currentDownload;

	uint64_t m_queuedReadPtr;

	void* m_queuedReadBuffer;

	OVERLAPPED m_queuedOverlapped;

	uint32_t m_queuedReadLen;

	bool m_downloading;

private:
	void QueueDownload();

	void QueueRead(uint64_t ptr, void* buffer, uint32_t toRead, LPOVERLAPPED overlapped);

	uint32_t InternalReadBulk(uint32_t handle, uint64_t ptr, void* buffer, uint32_t toRead);

public:
	CitizenStreamingFile(StreamingResource& entry)
		: m_entry(entry), m_queuedReadBuffer(nullptr), m_downloading(false)
	{
	}

	fwString GetFileName();

	bool Exists();

	void PreCache();

	virtual void Open();

	virtual uint32_t Read(uint64_t ptr, void* buffer, uint32_t toRead);

	virtual void Close();

	virtual uint32_t GetUniqueIdentifier() override;
};

class CitizenStreamingModule : public StreamingModule
{
private:
	//std::unordered_map<uint32_t, std::shared_ptr<CitizenStreamingFile>> m_streamingFiles;
	std::shared_ptr<CitizenStreamingFile> m_streamingFiles[65536];
	std::shared_ptr<CitizenStreamingFile> m_streamingBounds[65536];
	std::shared_ptr<CitizenStreamingFile> m_streamingIdes[2048];

public:
	virtual void ScanEntries();

	virtual StreamingFile* GetEntryFromIndex(uint32_t handle);

	void CreateStreamingFile(int index, const StreamingResource& entry);
};

void CitizenStreamingFile::Open()
{
	auto resCache = TheResources.GetCache();

	auto device = resCache->GetCacheDevice();
	auto filename = resCache->GetMarkedFilenameFor(m_entry.resData.GetName(), m_entry.filename);

	uint64_t ptr;
	m_device = device;
	m_handle = device->OpenBulk(filename.c_str(), &ptr);

	if (m_handle == -1)
	{
		m_currentDownload = resCache->GetResourceDownload(m_entry.resData, m_entry);

		trace("[Streaming] Queued download for file %s from resource %s.\n", m_entry.filename.c_str(), m_entry.resData.GetName().c_str());
	}
	else
	{
		trace("[Streaming] Locally opened file %s from resource %s, got handle %d.\n", m_entry.filename.c_str(), m_entry.resData.GetName().c_str(), m_handle);
	}
}

bool CitizenStreamingFile::Exists()
{
	auto resCache = TheResources.GetCache();

	auto device = resCache->GetCacheDevice();
	auto filename = resCache->GetMarkedFilenameFor(m_entry.resData.GetName(), m_entry.filename);

	auto handle = device->Open(filename.c_str(), true);

	if (handle == -1)
	{
		return false;
	}

	device->Close(handle);

	return true;
}

void CitizenStreamingFile::PreCache()
{
	if (m_downloading)
	{
		return;
	}

	auto resCache = TheResources.GetCache();

	m_currentDownload = resCache->GetResourceDownload(m_entry.resData, m_entry);

	QueueDownload();

	trace("[Streaming] Queued precache for file %s from resource %s.\n", m_entry.filename.c_str(), m_entry.resData.GetName().c_str());
}

uint32_t CitizenStreamingFile::Read(uint64_t ptr, void* buffer, uint32_t toRead)
{
	int streamThreadNum = ((ptr & (0x4000000000000000))) ? 1 : 0;
	LPOVERLAPPED overlapped = StreamWorker_GetOverlapped(streamThreadNum);

	if (m_handle == -1)
	{
		trace("[Streaming] Queueing read for file %s (we haven't downloaded the file yet).\n", m_entry.filename.c_str());

		QueueRead(ptr, buffer, toRead, overlapped);

		QueueDownload();

		return -1;
	}
	else
	{
		return m_device->ReadBulk(m_handle, ptr, buffer, toRead);
	}
}

void CitizenStreamingFile::QueueDownload()
{
	if (m_downloading)
	{
		return;
	}

	m_downloading = true;

	fwWString hostname, path;
	uint16_t port;

	httpClient->CrackUrl(m_currentDownload.sourceUrl, hostname, path, port);

	uint32_t startTime = timeGetTime();

	httpClient->DoFileGetRequest(hostname, port, path, TheResources.GetCache()->GetCacheDevice(), m_currentDownload.targetFilename, [=] (bool result, const char* connData, size_t connDataLength)
	{
		if (!result)
		{
			// retry downloading the file
			trace("[Streaming] Failed downloading file %s.\n", m_entry.filename.c_str());

			m_downloading = false;

			QueueDownload();

			return;
		}

		trace("[Streaming] Downloaded file %s.\n", m_entry.filename.c_str());

		TheResources.GetCache()->AddFile(m_currentDownload.targetFilename, m_currentDownload.filename, m_currentDownload.resname);

		auto resCache = TheResources.GetCache();

		auto device = resCache->GetCacheDevice();
		auto filename = resCache->GetMarkedFilenameFor(m_entry.resData.GetName(), m_entry.filename);

		uint32_t endTime = timeGetTime();

		uint64_t ptr;
		m_device = device;
		m_handle = device->OpenBulk(filename.c_str(), &ptr);

		if (m_handle == -1)
		{
			GlobalError("Streaming error reading %s.", filename.c_str());
		}

		if (m_queuedReadBuffer)
		{
			trace("[Streaming] Completing queued read for file %s - took %imsec.\n", m_entry.filename.c_str(), endTime - startTime);

			uint32_t len;

			if ((len = InternalReadBulk(m_handle, m_queuedReadPtr, m_queuedReadBuffer, m_queuedReadLen)) != -1)
			{
				SetEvent(m_queuedOverlapped.hEvent);

				trace("[Streaming] Read completed immediately.\n");
			}
			else
			{
				trace("[Streaming] Read pending.\n");
			}
		}
	});
}

fwString CitizenStreamingFile::GetFileName()
{
	return m_entry.filename;
}

uint32_t CitizenStreamingFile::InternalReadBulk(uint32_t handle, uint64_t ptr, void* buffer, uint32_t toRead)
{
	ptr &= ~(0xC000000000000000);

	LPOVERLAPPED overlapped = &m_queuedOverlapped;

	overlapped->Offset = (ptr & 0xFFFFFFFF);
	overlapped->OffsetHigh = ptr >> 32;

	if (!ReadFile((HANDLE)(handle & ~0x80000000), buffer, toRead, nullptr, overlapped))
	{
		int err = GetLastError();

		if (err != ERROR_IO_PENDING)
		{
			trace("Stream I/O failed: %i\n", err);
		}

		if (err == ERROR_HANDLE_EOF)
		{
			return -2;
		}

		return -1;
	}

	return overlapped->InternalHigh;
}

void CitizenStreamingFile::QueueRead(uint64_t ptr, void* buffer, uint32_t toRead, LPOVERLAPPED overlapped)
{
	assert(!m_queuedReadBuffer);
	assert(buffer);

	m_queuedReadPtr = ptr;
	m_queuedReadLen = toRead;
	m_queuedReadBuffer = buffer;
	m_queuedOverlapped = *overlapped;
}

void CitizenStreamingFile::Close()
{
	m_device->CloseBulk(m_handle);
}

uint32_t CitizenStreamingFile::GetUniqueIdentifier()
{
	return HashRageString(m_entry.hash.c_str());
}

void CitizenStreamingModule::CreateStreamingFile(int index, const StreamingResource& entry)
{
	m_streamingFiles[index] = std::make_shared<CitizenStreamingFile>(const_cast<StreamingResource&>(entry));
}

void RegisterWithColCache(const std::string& extn, int extnIndex, uint32_t hash);

void CitizenStreamingModule::ScanEntries()
{
	for (int i = 0; i < _countof(m_streamingFiles); i++)
	{
		m_streamingFiles[i] = std::shared_ptr<CitizenStreamingFile>();
		m_streamingBounds[i] = std::shared_ptr<CitizenStreamingFile>();
	}

	auto& entries = TheDownloads.GetStreamingFiles();
	auto imgManager = CImgManager::GetInstance();

	CIdeStore::ResetStreamList();

	uint32_t startIndex = 0;

	for (auto& entry : entries)
	{
		std::string extn = entry.filename.substr(entry.filename.length() - 3);

		if (_wbnMode)
		{
			if (extn == "wbn")
			{
				int boundIndex = BoundStreaming::RegisterBound(entry.filename.c_str(), entry.size, entry.rscFlags, entry.rscVersion);

				m_streamingBounds[boundIndex] = std::make_shared<CitizenStreamingFile>(entry);
				
				RegisterWithColCache(extn, boundIndex, HashRageString(entry.hash.c_str()));
				continue;
			}
		}

		if (extn == "zdr") // streaming drawable
		{
			CIdeStore::RegisterDrawable(entry);

			continue;
		}

		if (extn == "ide")
		{
			int boundIndex = CIdeStore::RegisterIde(entry.filename.c_str(), entry.size);

			if (boundIndex >= 0)
			{
				m_streamingIdes[boundIndex] = std::make_shared<CitizenStreamingFile>(entry);
			}

			continue;
		}

		startIndex = imgManager->registerIMGFile(entry.filename.c_str(), 0, entry.size, 0xFE, startIndex, entry.rscVersion);

		imgManager->fileDatas[startIndex].realSize = entry.rscFlags;

		if (entry.rscFlags & 0x40000000)
		{
			imgManager->fileDatas[startIndex].flags |= 0x2000;
		}

		m_streamingFiles[startIndex] = std::make_shared<CitizenStreamingFile>(entry);

		if (extn == "wbn" || extn == "wbd")
		{
			int typeStart = 0;

			for (int i = 0; i < _countof(streamingTypes.types); i++)
			{
				if (!_stricmp(streamingTypes.types[i].ext, extn.c_str()))
				{
					typeStart = streamingTypes.types[i].startIndex;
				}
			}

			RegisterWithColCache(extn, startIndex - typeStart, HashRageString(entry.hash.c_str()));
		}
	}
}

StreamingFile* CitizenStreamingModule::GetEntryFromIndex(uint32_t handle)
{
	// is this a bound?
	CitizenStreamingFile* file;

	if (handle & 0x80000000)
	{
		file = m_streamingBounds[handle & ~0x80000000].get();
	}
	else if (handle & 0x40000000)
	{
		file = m_streamingIdes[handle & ~0x40000000].get();
	}
	else
	{
		file = m_streamingFiles[handle].get();
	}

	return file;
}

static CitizenStreamingModule* streamingModule;

void CSM_CreateStreamingFile(int index, const StreamingResource& entry)
{
	streamingModule->CreateStreamingFile(index, entry);
}

extern fwEvent<void*> OnRequestEntityDo;

static InitFunction initFunction([] ()
{
	httpClient = new HttpClient();

	streamingModule = new CitizenStreamingModule();

	/*OnRequestEntityDo.Connect([] (void* entityPtr)
	{
		auto modelIndex = *(uint16_t*)((char*)entityPtr + 46);

		static int modelInfoBase;

		if (modelInfoBase == 0)
		{
			for (int i = 0; i < _countof(streamingTypes.types); i++)
			{
				//if (streamingTypes.types[i].fileVersion == 110)
				if (streamingTypes.types[i].ext[0] == 'w' && streamingTypes.types[i].ext[1] == 'd' && streamingTypes.types[i].ext[2] == 'r')
				{
					modelInfoBase = streamingTypes.types[i].startIndex;
					break;
				}
			}
		}

		auto file = static_cast<CitizenStreamingFile*>(streamingModule.GetEntryFromIndex(modelInfoBase + modelIndex));

		if (!file)
		{
			return;
		}

		if (file->Exists())
		{
			return;
		}

		file->PreCache();
	});*/

	CStreaming::SetStreamingModule(streamingModule);
});

bool SetStreamingWbnMode(bool fs)
{
	_wbnMode = fs;

	return fs;
}