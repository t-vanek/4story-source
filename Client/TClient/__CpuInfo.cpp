#include "stdafx.h"
#include "__CpuInfo.h"


const std::string __CpuInfo::NAME_IDENTIFIER_STRING = "Name=";
const std::string __CpuInfo::NUMBER_OF_CORES_IDENTIFIER_STRING = "NumberOfCores=";
const std::string __CpuInfo::MANUFACTURER_IDENTIFIER_STRING = "Manufacturer=";
const std::string __CpuInfo::ARCHITECTURE_IDENTIFIER_STRING = "DataWidth=";
const std::string __CpuInfo::L2_CACHE_SIZE_IDENTIFIER_STRING = "L2CacheSize=";
const std::string __CpuInfo::L3_CACHE_SIZE_IDENTIFIER_STRING = "L3CacheSize=";


__CpuInfo::__CpuInfo(const std::vector<std::string> &rawData, int cpuNumber) :
	_name{ "" },
	_manufacturer{ "" },
	_numberOfCores{ "" },
	_architecture{ "" },
	_L2CacheSize{ "" },
	_L3CacheSize{ "" },
	_cpuNumber{ cpuNumber }
{
	for (auto iter = rawData.begin(); iter != rawData.end(); iter++) {

		//Name
		if ((iter->find(NAME_IDENTIFIER_STRING) != std::string::npos) && (iter->find(NAME_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(NAME_IDENTIFIER_STRING);
			this->_name = iter->substr(foundPosition + NAME_IDENTIFIER_STRING.length());
		}

		//Manufacturer
		if ((iter->find(MANUFACTURER_IDENTIFIER_STRING) != std::string::npos) && (iter->find(MANUFACTURER_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(MANUFACTURER_IDENTIFIER_STRING);
			this->_manufacturer = iter->substr(foundPosition + MANUFACTURER_IDENTIFIER_STRING.length());
		}

		//Number Of Cores
		if ((iter->find(NUMBER_OF_CORES_IDENTIFIER_STRING) != std::string::npos) && (iter->find(NUMBER_OF_CORES_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(NUMBER_OF_CORES_IDENTIFIER_STRING);
			this->_numberOfCores = iter->substr(foundPosition + NUMBER_OF_CORES_IDENTIFIER_STRING.length());
		}

		//Architecture
		if ((iter->find(ARCHITECTURE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(ARCHITECTURE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(ARCHITECTURE_IDENTIFIER_STRING);
			std::string dataWidth = iter->substr(foundPosition + ARCHITECTURE_IDENTIFIER_STRING.length());
			this->_architecture = getArchitecture(dataWidth);
		}

		//L2 Cache Size
		if ((iter->find(L2_CACHE_SIZE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(L2_CACHE_SIZE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(L2_CACHE_SIZE_IDENTIFIER_STRING);
			this->_L2CacheSize = iter->substr(foundPosition + L2_CACHE_SIZE_IDENTIFIER_STRING.length());
		}

		//L3 Cache Size
		if ((iter->find(L3_CACHE_SIZE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(L3_CACHE_SIZE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(L3_CACHE_SIZE_IDENTIFIER_STRING);
			this->_L3CacheSize = iter->substr(foundPosition + L3_CACHE_SIZE_IDENTIFIER_STRING.length());
		}
	}

	//In case any of these values are missing or don't get assigned
	if (this->_name == "") {
		this->_name = "Unknown";
	}
	if (this->_manufacturer == "") {
		this->_manufacturer = "Unknown";
	}
	if (this->_numberOfCores == "") {
		this->_numberOfCores = "Unknown";
	}
	if (this->_architecture == "") {
		this->_architecture = "Unknown";
	}
	if (this->_L2CacheSize == "") {
		this->_L2CacheSize = "Unknown";
	}
	if (this->_L3CacheSize == "") {
		this->_L3CacheSize = "Unknown";
	}
}

std::string __CpuInfo::name() const
{
	return this->_name;
}

std::string __CpuInfo::manufacturer() const
{
	return this->_manufacturer;
}

std::string __CpuInfo::numberOfCores() const
{
	return this->_numberOfCores;
}

std::string __CpuInfo::architecture() const
{
	return this->_architecture;
}

std::string __CpuInfo::L2CacheSize() const
{
	return this->_L2CacheSize;
}

std::string __CpuInfo::L3CacheSize() const
{
	return this->_L3CacheSize;
}

int __CpuInfo::cpuNumber() const
{
	return this->_cpuNumber;
}

std::string __CpuInfo::getArchitecture(std::string &dataWidth) const
{
	try {
		int dataWidthInt = std::stoi(dataWidth);
		switch (dataWidthInt) {
		case 32: return "x86";
		case 64: return "x86_64";
		default: return "Unknown";
		}
	}
	catch (std::exception &e) {
		(void)e;
		return "Unknown";
	}
}

/// CPU INFO DELEGATE IMPL

const std::string __CpuInfoDelegate::CPU_INFO_QUERY_STRING = "wmic cpu get /format: list";
const std::string __CpuInfoDelegate::CPU_INSTANCE_QUERY_STRING = "AssetTag=";
const std::string __CpuInfoDelegate::CPU_INFO_END_IDENTIFIER_STRING = "VoltageCaps=";

__CpuInfoDelegate::__CpuInfoDelegate() :
	_numberOfCPUInfoItems{ 0 }
{
	__SysCommand systemCommand{ CPU_INFO_QUERY_STRING };
	systemCommand.execute();
	std::vector<std::string> tempVector = systemCommand.outputAsVector();
	if (!systemCommand.hasError()) {
		std::vector<std::string> raw = { systemCommand.outputAsVector() };
		determineNumberOfCPUInfoItems(raw);
		std::vector<std::string> singleCPUInfoItem;
		std::vector<std::string>::const_iterator iter = raw.begin();
		int cpuNumber = 0;
		while (cpuNumber < this->_numberOfCPUInfoItems) {
			while (iter->find(CPU_INFO_END_IDENTIFIER_STRING) == std::string::npos) {
				if ((*iter != "") && (*iter != "\r")) {
					singleCPUInfoItem.push_back(*iter);
				}
				iter++;
			}
			singleCPUInfoItem.push_back(*iter);
			this->_cpuInfoVector.emplace_back(singleCPUInfoItem, cpuNumber);
			singleCPUInfoItem.clear();
			iter++;
			cpuNumber++;
		}
	}
}

void __CpuInfoDelegate::determineNumberOfCPUInfoItems(const std::vector<std::string> &data)
{
	for (auto iter = data.begin(); iter != data.end(); iter++) {
		if (iter->find(CPU_INSTANCE_QUERY_STRING) != std::string::npos) {
			this->_numberOfCPUInfoItems++;
		}
	}
}

int __CpuInfoDelegate::numberOfCPUInfoItems() const
{
	return this->_numberOfCPUInfoItems;
}

std::vector<__CpuInfo> __CpuInfoDelegate::cpuInfoVector() const
{
	return this->_cpuInfoVector;
}