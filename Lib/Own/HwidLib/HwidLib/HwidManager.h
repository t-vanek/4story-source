#pragma once
#ifndef HWID_MANAGER
#define HWID_MANAGER
#include <vector>
#include <string>
#include <memory>
#include <iostream>

class HwidDataObject
{
public:
	std::string CpuSegment;
	std::string MotherboardSegment;
	std::string WinSerialSegment;
	std::string WinDeviceSegment;
	std::string WinUserSegment; // comming soon ...
};

class HwidManager
{
private:
	static std::string m_WinUserQueryResult;
	static std::string m_WinDeviceQueryResult;
	static std::string m_WinSerialQueryResult;
	static std::string m_CpuQueryResult;
	static std::string m_MbQueryResult;

public:
	static void Initialize();
	static void GetHwid(HwidDataObject& hwidObject);

private:
	static HwidDataObject ComputeHash();
};


#endif