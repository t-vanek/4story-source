#pragma once
#ifndef _CpuInfo_H
#define _CpuInfo_H

#include <vector>
#include <string>

#include "__SysCommand.h"

#ifndef _WIN32
#error This methods works only on Windows !!
#endif

class __CpuInfo
{
public:
	__CpuInfo::__CpuInfo(const std::vector<std::string> &rawData, int cpuNumber);
	std::string name() const;
	std::string manufacturer() const;
	std::string numberOfCores() const;
	std::string architecture() const;
	std::string L2CacheSize() const;
	std::string L3CacheSize() const;
	int cpuNumber() const;

private:
	std::string _name;
	std::string _manufacturer;
	std::string _numberOfCores;
	std::string _architecture;
	std::string _L2CacheSize;
	std::string _L3CacheSize;
	int _cpuNumber;

	std::string getArchitecture(std::string &dataWidth) const;

	template <typename T>
	std::string toString(const T &convert) const
	{
		std::stringstream transfer;
		std::string returnString;
		transfer << convert;
		transfer >> returnString;
		return returnString;
	}

	static const std::string TEMPERATURE_QUERY_STRING;
	static const std::string TEMPERATURE_ERROR_IDENTIFIER_STRING;
	static const std::string NAME_IDENTIFIER_STRING;
	static const std::string NUMBER_OF_CORES_IDENTIFIER_STRING;
	static const std::string MANUFACTURER_IDENTIFIER_STRING;
	static const std::string CLOCK_SPEED_QUERY_STRING;
	static const std::string CURRENT_CLOCK_SPEED_IDENTIFIER_STRING;
	static const std::string ARCHITECTURE_IDENTIFIER_STRING;
	static const std::string L2_CACHE_SIZE_IDENTIFIER_STRING;
	static const std::string L3_CACHE_SIZE_IDENTIFIER_STRING;
};
#endif

#ifndef __CPUINFO_DELEGATE
#define __CPUINFO_DELEGATE
#include "__SysCommand.h"


class __CpuInfoDelegate
{
public:
	__CpuInfoDelegate();
	std::vector<__CpuInfo> cpuInfoVector() const;
	int numberOfCPUInfoItems() const;
private:
	std::vector<__CpuInfo> _cpuInfoVector;
	int _numberOfCPUInfoItems;

	void determineNumberOfCPUInfoItems(const std::vector<std::string> &data);
	static const std::string CPU_INFO_QUERY_STRING;
	static const std::string CPU_INSTANCE_QUERY_STRING;
	static const std::string CPU_INFO_END_IDENTIFIER_STRING;
};

#endif