#include "stdafx.h"
#include "HwidManager.h"
#include <__Sha256.h>
#include <__CpuInfo.h>
#include <__MotherboardInfo.h>
#include <__WindowsInfo.h>

std::string HwidManager::m_WinUserQueryResult;
std::string HwidManager::m_WinDeviceQueryResult;
std::string HwidManager::m_WinSerialQueryResult;
std::string HwidManager::m_CpuQueryResult;
std::string HwidManager::m_MbQueryResult;


void HwidManager::Initialize()
{
#pragma region Query CPU Informations
	auto cpuInfo = std::make_unique<__CpuInfoDelegate>();
	auto cpuInfoVector{ cpuInfo->cpuInfoVector() };


	for (auto entry = cpuInfoVector.begin(); entry != cpuInfoVector.end(); entry++)
	{ // The following three blocks aren't going to change unless a new processor is being installed, or some fucked up engineer increases the cache due
	  // hardware modifications.
		m_CpuQueryResult += entry->L2CacheSize();
		m_CpuQueryResult += entry->L3CacheSize();
		m_CpuQueryResult += entry->numberOfCores();
	}
#pragma endregion

#pragma region Query Motherboard Informations
	auto mbInfo = std::make_unique<__MotherboardInfoDelegate>();
	auto mbInfoVector{ mbInfo->motherboardInfoVector() };


	for (auto entry = mbInfoVector.begin(); entry != mbInfoVector.end(); entry++)
	{ // We concenate the following three blocks to create a quite unique cipher
		m_MbQueryResult += entry->serialNumber();
		m_MbQueryResult += entry->name();
		m_MbQueryResult += entry->manufacturer();
	}
#pragma endregion

#pragma region Query Windows Informations
	auto winInfo = std::make_unique<__WindowsInfoDelegate>();
	auto winInfoVector{ winInfo->winInfoVector() };


	for (auto entry = winInfoVector.begin(); entry != winInfoVector.end(); entry++)
	{
		m_WinDeviceQueryResult += entry->bootDevice();
		m_WinDeviceQueryResult += entry->caption();
		m_WinDeviceQueryResult += entry->totalVisibleMemory();

		// the serial number is unique on its own enough.
		m_WinSerialQueryResult += entry->serialNumber();
	}
#pragma endregion
}

void HwidManager::GetHwid(HwidDataObject& hwidObject)
{
	hwidObject = HwidManager::ComputeHash();
}

HwidDataObject HwidManager::ComputeHash()
{
	HwidDataObject hwidData;

	hwidData.CpuSegment = sha256(m_CpuQueryResult);
	hwidData.MotherboardSegment = sha256(m_MbQueryResult);
	hwidData.WinDeviceSegment = sha256(m_WinDeviceQueryResult);
	hwidData.WinSerialSegment = sha256(m_WinSerialQueryResult);

	return hwidData;
}