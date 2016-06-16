/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "ResourceManager.h"
#include "DownloadMgr.h"
#include <SHA1.h>
#include <rapidjson/document.h>
#include <nutsnbolts.h>
//#include "../ui/CefOverlay.h"
//#include "../ui/CustomLoadScreens.h"

static NetLibrary* g_netLibrary;

bool DownloadManager::Process()
{
	switch (m_downloadState)
	{
		case DS_IDLE:
		{
			// initialize the download manager
			m_downloadState = DS_FETCHING_CONFIG;

			// create a new HTTP client
			if (!m_httpClient)
			{
				m_httpClient = new HttpClient();
			}

			// set up the target request
			auto hostname = m_gameServer.GetWAddress();

			fwMap<fwString, fwString> postMap;
			postMap["method"] = "getConfiguration";

			// list the resources that we'd want to update
			bool isUpdate = false;

			if (!m_updateQueue.empty())
			{
				fwString resourceString;

				while (!m_updateQueue.empty())
				{
					resourceString += m_updateQueue.front();

					m_updateQueue.pop();

					if (!m_updateQueue.empty())
					{
						resourceString += ";";
					}
				}

				postMap["resources"] = resourceString;

				isUpdate = true;
			}

			// mark this being an update
			m_isUpdate = isUpdate;

			// clear the resource list
			if (!m_isUpdate)
			{
				m_requiredResources.clear();
			}

			// perform the request
			InterlockedIncrement(&m_numPendingConfigRequests); // increment the request count

			m_httpClient->DoPostRequest(hostname.c_str(), m_gameServer.GetPort(), L"/client", postMap, [=] (bool result, const char* data, size_t size)
			{
				ConfigurationRequestData request;
				request.serverName = m_gameServer.GetAddress();

				ParseConfiguration(request, result, data, size);
			});

			break;
		}

		case DS_CONFIG_FETCHED:
		{
			// check cache existence (TODO: and integrity?)
			auto resourceCache = TheResources.GetCache();

			auto downloadList = resourceCache->GetDownloadsFromList(m_requiredResources);

			if (m_isUpdate)
			{
				resourceCache->MarkList(m_requiredResources);
			}
			else
			{
				resourceCache->ClearMark();
				resourceCache->MarkList(m_requiredResources);
				resourceCache->MarkStreamingList(m_streamingFiles);
			}

			m_downloadList = std::queue<ResourceDownload>();

			for (auto& download : downloadList)
			{
				m_downloadList.push(download);
			}
			
			if (m_downloadList.empty())
			{
				m_downloadState = DS_DOWNLOADED_SINGLE;
			}
			else
			{
				m_downloadState = DS_DOWNLOADING;
			}

			break;
		}

		case DS_DOWNLOADING:
		{
			if (!m_currentDownload.get())
			{
				m_currentDownload = std::make_shared<ResourceDownload>(m_downloadList.front());
				m_downloadList.pop();

				fwWString hostname, path;
				uint16_t port;

				m_httpClient->CrackUrl(m_currentDownload->sourceUrl, hostname, path, port);

				m_httpClient->DoFileGetRequest(hostname, port, path, TheResources.GetCache()->GetCacheDevice(), m_currentDownload->targetFilename, [=] (bool result, const char* connData, size_t)
				{
					m_downloadState = DS_DOWNLOADED_SINGLE;
				});
			}

			break;
		}

		case DS_DOWNLOADED_SINGLE:
		{
			if (m_currentDownload.get())
			{
				TheResources.GetCache()->AddFile(m_currentDownload->targetFilename, m_currentDownload->filename, m_currentDownload->resname);

				m_currentDownload = nullptr;
			}

			if (!m_downloadList.empty())
			{
				m_downloadState = DS_DOWNLOADING;
			}
			else
			{
				if (!m_isUpdate)
				{
					TheResources.Reset();
				}
				else
				{
					m_loadedResources.clear(); // to clear the references that will otherwise be left over after DeleteResource

					// unload any resources we already know that are currently unprocessed
					for (auto& resource : m_requiredResources)
					{
						// this is one we just got from the configuration redownload
						if (!resource.IsProcessed())
						{
							auto resourceData = TheResources.GetResource(resource.GetName());

							if (!resourceData.GetRef())
							{
								continue;
							}

							// sanity check: is the resource not running?
							if (resourceData->GetState() == ResourceStateRunning)
							{
								FatalError("Tried to unload a running resource in DownloadMgr. (%s)", resource.GetName().c_str());
							}

							// remove all packfiles related to this old resource
							auto packfiles = resourceData->GetPackFiles();

							for (auto& packfile : packfiles)
							{
								// FIXME: implementation detail from same class
								fiDevice::Unmount(va("resources:/%s/", resourceData->GetName().c_str()));

								packfile->ClosePackfile();

								// remove from the to-close list (!)
								for (auto it = m_packFiles.begin(); it != m_packFiles.end(); it++)
								{
									if (it->second == packfile)
									{
										m_packFiles.erase(it);
										break;
									}
								}
							}

							// and delete the resource (hope nobody kept a reference to that sucker, ha!)
							TheResources.DeleteResource(resourceData);
						}
					}
				}

				//std::string resourcePath = "citizen:/resources/";
				//TheResources.ScanResources(fiDevice::GetDevice("citizen:/setup2.xml", true), resourcePath);

				std::list<fwRefContainer<Resource>> loadedResources;

				// mount any RPF files that we include
				for (auto& resource : m_requiredResources)
				{
					if (m_isUpdate && resource.IsProcessed())
					{
						continue;
					}

					fwVector<rage::fiPackfile*> packFiles;

					for (auto& file : resource.GetFiles())
					{
						if (file.filename.find(".rpf") != std::string::npos)
						{
							// get the path of the RPF
							fwString markedFile = TheResources.GetCache()->GetMarkedFilenameFor(resource.GetName(), file.filename);

							rage::fiPackfile* packFile = new rage::fiPackfile();
							packFile->OpenPackfile(markedFile.c_str(), true, false, 0);
							packFile->Mount(va("resources:/%s/", resource.GetName().c_str()));

							packFiles.push_back(packFile);
							m_packFiles.push_back(std::make_pair(va("resources:/%s/", resource.GetName().c_str()), packFile));
						}
					}

					// load the resource
					auto resourceLoad = TheResources.AddResource(resource.GetName(), va("resources:/%s/", resource.GetName().c_str()));

					if (resourceLoad.GetRef())
					{
						resourceLoad->AddPackFiles(packFiles);

						loadedResources.push_back(resourceLoad);
					}

					resource.SetProcessed();
				}

				if (m_isUpdate)
				{
					for (auto& resource : loadedResources)
					{
						resource->Start();
					}
				}

				m_loadedResources = loadedResources;

				m_downloadState = DS_DONE;
			}

			break;
		}

		case DS_DONE:
			// we're done, go back to idle mode next frame
			m_downloadState = DS_IDLE;

			// reset resources if need be
			if (m_isUpdate && !m_updateQueue.empty())
			{
				ProcessQueuedUpdates();
			}
			else
			{
				m_isUpdate = false;
			}

			/*if (!m_serverLoadScreen.empty() && !m_isUpdate)
			{
				CustomLoadScreens::PrepareSwitchTo(m_serverLoadScreen);
			}*/

			// destroy the HTTP client
			//delete m_httpClient;

			// signal the network library, and allow it to process further tasks
			g_netLibrary->DownloadsComplete();

			while (!g_netLibrary->ProcessPreGameTick())
			{
				HANDLE hThread = GetCurrentThread();

				MsgWaitForMultipleObjects(1, &hThread, TRUE, 15, 0);
			}
			
			return true;
	}

	return false;
}

void DownloadManager::ParseConfiguration(const ConfigurationRequestData& request, bool result, const char* connDataStr, size_t connDataLength)
{
	if (!result)
	{
		// TODO: make this a non-fatal error leading back to UI
		GlobalError("Obtaining configuration from server (%s) failed.", request.serverName.c_str());

		return;
	}

	fwString serverHost = m_gameServer.GetAddress();
	serverHost += va(":%d", m_gameServer.GetPort());

	// parse the received data file
	rapidjson::Document node;
	node.Parse(connDataStr);

	if (node.HasParseError())
	{
		auto err = node.GetParseError();

		GlobalError("parse error %d", err);
	}

	if (node.HasMember("loadScreen"))
	{
		m_serverLoadScreen = node["loadScreen"].GetString();
	}

	if (node.HasMember("imports") && node["imports"].IsArray())
	{
		auto& importList = node["imports"];

		for (auto it = importList.Begin(); it != importList.End(); it++)
		{
			auto& import = *it;

			if (import.IsString())
			{
				InitiateChildRequest(import.GetString());
			}
		}
	}

	bool hasValidResources = true;

	if (!node.HasMember("resources") || !node["resources"].IsArray())
	{
		hasValidResources = false;
	}

	if (hasValidResources)
	{
		if (!node.HasMember("fileServer") || !node["fileServer"].IsString())
		{
			hasValidResources = false;
		}
	}

	if (hasValidResources)
	{
		auto& resources = node["resources"];

		std::string origBaseUrl = node["fileServer"].GetString();

		m_parseMutex.lock();

		for (auto it = resources.Begin(); it != resources.End(); it++)
		{
			auto& resource = *it;

			std::string baseUrl = origBaseUrl;

			if (it->HasMember("fileServer") && (*it)["fileServer"].IsString())
			{
				baseUrl = (*it)["fileServer"].GetString();
			}

			ResourceData resData(resource["name"].GetString(), va(baseUrl.c_str(), serverHost.c_str()));

			//for (auto& file : resource["files"])
			auto& files = resource["files"];
			for (auto i = files.MemberBegin(); i != files.MemberEnd(); i++)
			{
				fwString filename = i->name.GetString();
				fwString hash = i->value.GetString();

				resData.AddFile(filename, hash);
			}

			if (resource.HasMember("streamFiles"))
			{
				auto& streamFiles = resource["streamFiles"];

				//for (auto& file : resource["streamFiles"])
				for (auto i = streamFiles.MemberBegin(); i != streamFiles.MemberEnd(); i++)
				{
					fwString filename = i->name.GetString();
					fwString hash = i->value["hash"].GetString();
					uint32_t rscFlags = i->value["rscFlags"].GetUint();
					uint32_t rscVersion = i->value["rscVersion"].GetUint();
					uint32_t size = i->value["size"].GetUint();

					AddStreamingFile(resData, filename, hash, rscFlags, rscVersion, size);
				}
			}

			if (m_isUpdate)
			{
				for (auto ite = m_requiredResources.begin(); ite != m_requiredResources.end(); ite++)
				{
					if (ite->GetName() == resData.GetName())
					{
						m_requiredResources.erase(ite);
						break;
					}
				}
			}

			trace("%s\n", resData.GetName().c_str());

			m_requiredResources.push_back(resData);
		}

		m_parseMutex.unlock();
	}

	// decrement the pending count
	uint32_t pending = InterlockedDecrement(&m_numPendingConfigRequests);

	// if this was the last request, signal the idle loop that we are done fetching the config
	if (pending == 0)
	{
		m_downloadState = DS_CONFIG_FETCHED;
	}
}

void DownloadManager::InitiateChildRequest(fwString url)
{
	// parse the URL
	fwWString hostname;
	fwWString path;
	uint16_t port;

	if (!m_httpClient->CrackUrl(url, hostname, path, port))
	{
		return;
	}

	// perform the request
	InterlockedIncrement(&m_numPendingConfigRequests); // increment the request count

	m_httpClient->DoGetRequest(hostname, port, path, [=] (bool result, const char* data, size_t size)
	{
		ConfigurationRequestData request;
		request.serverName = url;

		ParseConfiguration(request, result, data, size);
	});
}

void DownloadManager::AddStreamingFile(ResourceData data, fwString& filename, fwString& hash, uint32_t rscFlags, uint32_t rscVersion, uint32_t size)
{
	StreamingResource file;
	file.resData = data;
	file.filename = filename;
	file.hash = hash;
	file.rscFlags = rscFlags;
	file.rscVersion = rscVersion;
	file.size = size;

	m_streamingFiles.push_back(file);
}

bool DownloadManager::DoingQueuedUpdate()
{
	return m_isUpdate && m_downloadState != DS_IDLE;
}

void DownloadManager::QueueResourceUpdate(fwString resourceName)
{
	m_updateQueue.push(resourceName);

	if (m_downloadState == DS_IDLE)
	{
		ProcessQueuedUpdates();
	}
}

void DownloadManager::ProcessQueuedUpdates()
{
	m_downloadState = DS_IDLE;

	Process();
}

void DownloadManager::ReleaseLastServer()
{
	m_serverLoadScreen = "";

	for (auto& packfile : m_packFiles)
	{
		fiDevice::Unmount(packfile.first.c_str());

		packfile.second->ClosePackfile();
	}

	//nui::ReloadNUI();
	for (auto& resource : m_requiredResources)
	{
		//nui::DestroyFrame(resource.GetName());
	}
}

void DownloadManager::SetServer(NetAddress& address)
{
	m_gameServer = address;
}

DownloadManager TheDownloads;

static InitFunction initFunction([] ()
{
	NetLibrary::OnNetLibraryCreate.Connect([] (NetLibrary* library)
	{
		g_netLibrary = library;
	});

#if 0
	ResourceManager::OnQueueResourceStart.Connect([] (fwString resourceName)
	{
		TheDownloads.QueueResourceUpdate(resourceName);
	});
#endif

	ResourceManager::OnScriptReset.Connect([] ()
	{
		auto& loadedResources = TheDownloads.GetLoadedResources();

		// and start all of them!
		for (auto& resource : loadedResources)
		{
			if (resource->GetState() == ResourceStateStopped)
			{
				resource->Start();
			}
		}

		TheDownloads.ClearLoadedResources();
	});

	OnGameFrame.Connect([] ()
	{
		if (TheDownloads.DoingQueuedUpdate())
		{
			TheDownloads.Process();
		}
	});
});