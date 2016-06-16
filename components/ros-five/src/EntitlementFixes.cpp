/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"
#include "base64.h"

#include <botan/auto_rng.h>
#include <botan/rsa.h>
#include <botan/sha160.h>
#include <botan/pubkey.h>

static void*(*g_origRosWrap)(void*, const char*, void*, void*);

static int StoreDecryptedBlob(void* a1, void* a2, uint32_t a3, void* inOutBlob, uint32_t a5, void* a6);

void* RosWrap(void* a1, const char* filename, void* a3, void* a4)
{
	char* pointer;
	uint32_t size;

	sscanf(filename, "memory:$%p,%d", &pointer, &size);

	const char* newResponse = nullptr;

	if (strstr(pointer, "GetEntitlementBlockResponse"))
	{
		uint8_t inStr[4196] = { 0 };
		*(uint32_t*)inStr = 0x1000000;

		StoreDecryptedBlob(nullptr, nullptr, 0xC0FFEE, &inStr[4 + 16], 42, nullptr);

		size_t strLen;
		// FIXME: 'possible' typo in variable name
		char* base64Demmy = base64_encode(inStr, sizeof(inStr), &strLen);

		char outStr[6144] = { 0 };
		memcpy(outStr, base64Demmy, strLen);
		outStr[strLen] = '\0';

		const char* fakeResponse = strdup(va("<?xml version=\"1.0\" encoding=\"utf-8\"?><Response xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ms=\"0\" xmlns=\"GetEntitlementBlockResponse\"><Status>1</Status><Result Version=\"1\"><Data>%s</Data></Result></Response>", outStr));

		free(base64Demmy);

		newResponse = fakeResponse;
	}
	else if (strstr(pointer, "EntitlementsResponse"))
	{
		const char* fakeResponse = strdup("<?xml version=\"1.0\" encoding=\"utf-8\"?><Response xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ms=\"0\" xmlns=\"EntitlementsResponse\">\r\n\r\n  <Status>1</Status>\r\n\r\n  <Entitlements xsi:type=\"EntitlementsListXmlMD5\">\r\n\r\n    <Entitlement InstanceId=\"1\" EntitlementCode=\"1972D87D58D9790D41A19FCDC1C3600A\" FriendlyName=\"$500,000 for Grand Theft Auto V Story Mode\" Count=\"1\" Visible=\"true\" Type=\"Durable\">\r\n\r\n      <CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate>\r\n\r\n    </Entitlement>\r\n\r\n    <Entitlement InstanceId=\"2\" EntitlementCode=\"27BF767F361818E864967CBF808DC6C2\" FriendlyName=\"Access to Grand Theft Auto V for PC\" Count=\"1\" Visible=\"false\" Type=\"Durable\">\r\n\r\n      <CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate>\r\n\r\n    </Entitlement>\r\n\r\n<Entitlement InstanceId=\"3\" EntitlementCode=\"4D754F8EF1B135DBD3DDDE760A9352DA\" FriendlyName=\"Access to Grand Theft Auto V for PC\" Count=\"1\" Visible=\"true\" Type=\"Durable\"><CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate></Entitlement><Entitlement InstanceId=\"4\" EntitlementCode=\"4748A48AFB22BAE2FD6A4506655B2D95\" FriendlyName=\"Access to Grand Theft Auto V for PC Steam\" Count=\"1\" Visible=\"true\" Type=\"Durable\">\r\n\r\n      <CreatedDate>2015-04-14T00:00:000Z</CreatedDate>\r\n\r\n    </Entitlement>\r\n\r\n  </Entitlements>\r\n\r\n</Response>");
		newResponse = fakeResponse;
	}
	else if (strstr(pointer, "AdvertiseResponse"))
	{
		const char* fakeResponse = strdup("<?xml version=\"1.0\" encoding=\"utf-8\"?><Response xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ms=\"15.6263\" xmlns=\"AdvertiseResponse\"><Status>1</Status><MatchId>875fd057-fe8d-4145-a4e1-76b57a81817d</MatchId></Response>");
		newResponse = fakeResponse;
	}
	else if (strstr(pointer, "UpdateResponse"))
	{
		const char* fakeResponse = strdup("<?xml version=\"1.0\" encoding=\"utf-8\"?><Response xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ms=\"15.6263\" xmlns=\"UpdateResponse\"><Status>1</Status></Response>");
		newResponse = fakeResponse;
	}
	else if (strstr(pointer, "Unadvertise"))
	{
		const char* fakeResponse = strdup("<?xml version=\"1.0\" encoding=\"utf-8\"?><Response xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ms=\"15.6263\" xmlns=\"UnadvertiseResponse\"><Status>1</Status></Response>");
		newResponse = fakeResponse;
	}

	if (newResponse)
	{
		filename = va("memory:$%p,%d,%d,%s", newResponse, strlen(newResponse), 0, "ros");
	}

	return g_origRosWrap(a1, filename, a3, a4);
}

static void(*g_origAddArg)(char* buffer, const char* format, const char* ampersand, const char* key);

// save the machine hash for the response
static const char* g_lastArg;
static std::string g_machineHash;

void AddArgWrap(char* buffer, const char* format, const char* ampersand, const char* key)
{
	// last argument key
	g_lastArg = key;
	g_origAddArg(buffer, format, ampersand, key);
}

static void(*g_origAddVal)(char* buffer, const char* value, size_t len);

void AddValWrap(char* buffer, const char* value, size_t len)
{
	g_origAddVal(buffer, value, len);

	// if this is a machine hash, save it
	if (_stricmp(g_lastArg, "machineHash") == 0)
	{
		g_machineHash = std::string(value, len);
	}
}

static uint32_t BigLong(uint32_t val)
{
	val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
	return (val << 16) | (val >> 16);
}

static uint64_t BigLongLong(uint64_t val)
{
	val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
	val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
	return (val << 32) | (val >> 32);
}

static char* g_socialClubProfileLoc;

bool g_isPre372RosService;

std::shared_ptr<Botan::RSA_PrivateKey> g_privateKey;

static int StoreDecryptedBlob(void* a1, void* a2, uint32_t a3, void* inOutBlob, uint32_t a5, void* a6)
{
	uint8_t outBlob[4096] = { 0 };
	uint8_t* ptr = outBlob;

	const char* boringXml = "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<EntitlementsListXml xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:type=\"EntitlementsListSHA256\" xmlns=\"EntitlementsResponse\">\r\n<Entitlement InstanceId=\"3\" EntitlementCode=\"D962AD7FF354A224C6599EB313EFA50E2E04B3E82274480A2143697681FC5DC6\" FriendlyName=\"Access to Grand Theft Auto V for PC\" Count=\"1\" Visible=\"true\" Type=\"Durable\"><CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate></Entitlement><Entitlement InstanceId=\"1\" EntitlementCode=\"92694D1B738D91C40DA97F68D6946D658C64A420AE2CF5BFF92A819CF8DBAB6E\" FriendlyName=\"$500,000 for Grand Theft Auto V Story Mode\" Count=\"1\" Visible=\"true\" Type=\"Durable\">\r\n    <CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate>\r\n  </Entitlement>\r\n  <Entitlement InstanceId=\"2\" EntitlementCode=\"FCD037247F823B5C3060A9B53C01DA4C3E3C6CE5191A8E80696C631079D3D17A\" FriendlyName=\"Access to Grand Theft Auto V for PC\" Count=\"1\" Visible=\"false\" Type=\"Durable\">\r\n    <CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate>\r\n  </Entitlement>\r\n  <Entitlement InstanceId=\"4\" EntitlementCode=\"96C261A9C3EF0DA32CBC68A83FB7699CA1D5973D6B4D4EA1F9EF8B4E5C7D85B8\" FriendlyName=\"Access to Grand Theft Auto V for PC Steam\" Count=\"1\" Visible=\"true\" Type=\"Durable\">\r\n    <CreatedDate>2015-04-14T00:00:00.000Z</CreatedDate>\r\n  </Entitlement>\r\n</EntitlementsListXml>";

	*(uint32_t*)ptr = BigLong(20 + 19 + g_machineHash.size() + strlen(boringXml));
	ptr += 4;

	*(uint64_t*)ptr = BigLongLong(*(uint64_t*)(&g_socialClubProfileLoc[56 + ((g_isPre372RosService) ? 0 : 8)])); // post-372 added an additional 8 bytes, 393 added another?
	ptr += 8;

	*(uint32_t*)ptr = BigLong(g_machineHash.size());
	ptr += 4;

	memcpy(ptr, g_machineHash.c_str(), g_machineHash.size());
	ptr += g_machineHash.size();

	*(uint32_t*)ptr = BigLong(19);
	ptr += 4;

	// get expiration time
	char expirationTime[20];

	{
		time_t curTime;
		time(&curTime);

		curTime += 86400 * 3;

		tm convTime;
		localtime_s(&convTime, &curTime);

		strftime(expirationTime, sizeof(expirationTime), "%FT%T", &convTime);
	}

	memcpy(ptr, expirationTime, strlen(expirationTime));

	ptr += strlen(expirationTime);

	*(uint32_t*)ptr = BigLong(strlen(boringXml));
	ptr += 4;

	memcpy(ptr, boringXml, strlen(boringXml));
	ptr += strlen(boringXml);

	*(uint32_t*)ptr = BigLong(128);
	ptr += 4;

	// placeholder signature
	memset(ptr, 0x55, 128);

	// calculate a more sensible signature
	Botan::SHA_160 hashFunction;
	auto result = hashFunction.process(&outBlob[4], BigLong(*(uint32_t*)&outBlob[0]));

	std::vector<uint8_t> msg(result.size() + 1);
	msg[0] = 2;
	memcpy(&msg[1], &result[0], result.size());

	Botan::AutoSeeded_RNG rng;
	Botan::PK_Signer signer = Botan::PK_Signer(*g_privateKey, "EMSA_PKCS1(SHA-1)");
	auto signature = signer.sign_message(msg, rng);

	memcpy(ptr, &signature[0], 128);

	memcpy(inOutBlob, outBlob, sizeof(outBlob));

	return 0;
}

static int StoreDecryptedBlobExp2(void* a1, void* a2, void* a3, void* a4)
{
	// a no-op to not have to decrypt anything

	return 0;
}

static HookFunction hookFunction([] ()
{
	// modify the decryption function for entitlement blocks to return our fake decoded data (as this is the only user of such anyway)
	//hook::jump(hook::pattern("53 48 81 EC B0 00 00 00 C7 44 24 50").count(1).get(0).get<void>(-0x14), StoreDecryptedBlob);
	//hook::jump(hook::pattern("53 48 81 EC B0 00 00 00 C7 44 24 50").count(1).get(0).get<void>(-0x14 + 0x92C), StoreDecryptedBlobExp2);

	hook::call(hook::pattern("48 8B 4C 24 48 48 8B 14 01 48 8B 4C 24 40").count(2).get(1).get<void>(14), StoreDecryptedBlobExp2);

	// we don't give a fuck about your RSA signatures (RSA signature check on entitlement blocks; force success)
	//hook::put<uint16_t>(hook::pattern("84 C0 74 05 41 8A C4 EB 02 32 C0 4C 8D 9C 24 D0").count(1).get(0).get<void>(2), 0x9090);
	// ^ actually we might now as R* enabled an additional packer function

	// 372-era changed socialclub.dll to unicode - use that as a marker to determine whether to add 8b of padding
	char* scData = hook::pattern("44 6F 77 6E 6C 6F 61 64  00 00 00 00 00 00 00 00").count(1).get(0).get<char>(16);

	g_isPre372RosService = memcmp(scData, "\\\0R\0", 4) != 0;

	if (g_isPre372RosService)
	{
		// also check 392 with more padding
		scData += 8;

		g_isPre372RosService = memcmp(scData, "\\\0R\0", 4) != 0;
	}

	// ros profile location (somewhere from a rline event handler)
	char* location = hook::pattern("44 8B BB 30 06 00 00 48 8B CE 48 63 F8 4C 8D B3").count(1).get(0).get<char>(38);

	g_socialClubProfileLoc = reinterpret_cast<decltype(g_socialClubProfileLoc)>(location + *(int32_t*)location + 4);

	// call hooks for requests/responses

	// response XML memory file opening
	auto callAddr = hook::pattern("8B 59 54 83 61 54 FB 48").count(1).get(0).get<char>(20);
	hook::set_call(&g_origRosWrap, callAddr);
	hook::call(callAddr, RosWrap);

	// argument key append
	callAddr = hook::pattern("48 8D 4C 24 20 4C 0F 45 C0 E8 ? ? ? ? 48 8D 44 24 20 48 FF").count(1).get(0).get<char>(9);
	hook::set_call(&g_origAddArg, callAddr);
	hook::call(callAddr, AddArgWrap);

	callAddr += 29;
	callAddr += 27;

	// argument value append
	hook::set_call(&g_origAddVal, callAddr);
	hook::call(callAddr, AddValWrap);

	// generate a single-use key for RSA stuff (let's hope the entropy source won't be that slow...)
	Botan::AutoSeeded_RNG rng;
	Botan::RSA_PrivateKey privateKey(rng, 1024);
	
	auto publicKey = privateKey.x509_subject_public_key();

	// store the key in the computer system's random-access memory

	// original rsa pubkey for ROS
	memcpy(hook::pattern("30 81 89 02 81 81 00 AB").count(1).get(0).get<void>(0), &publicKey[0], publicKey.size());

	// if at any point you hear anything other than the word 'blah', please ignore it
	// 5 4 blah blah blah 3 1 blah blah blah blah
	g_privateKey = std::make_shared<Botan::RSA_PrivateKey>(privateKey);
});