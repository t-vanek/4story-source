#include "stdafx.h"
#include "__MotherboardInfo.h"


const std::string __MotherboardInfo::CHIPSET_QUERY_STRING = "wmic path Win32_PnPEntity get /format:list | findstr /R /C:\"Chipset\"";
const std::string __MotherboardInfo::NAME_IDENTIFIER_STRING = "Product=";
const std::string __MotherboardInfo::MANUFACTURER_IDENTIFIER_STRING = "Manufacturer=";
const std::string __MotherboardInfo::CHIPSET_IDENTIFIER_STRING = "Description=";
const std::string __MotherboardInfo::CHIPSET_END_IDENTIFIER_STRING = "Chipset";
const std::string __MotherboardInfo::SERIAL_NUMBER_IDENTIFIER_STRING = "SerialNumber=";
const std::string __MotherboardInfo::VERSION_IDENTIFIER_STRING = "Version=";

__MotherboardInfo::__MotherboardInfo(const std::vector<std::string> &rawData, int motherboardNumber) :
	_name{ "" },
	_manufacturer{ "" },
	_chipset{ "" },
	_serialNumber{ "" },
	_version{ "" },
	_motherboardNumber{ motherboardNumber }
{
	determineChipset();
	for (auto iter = rawData.begin(); iter != rawData.end(); iter++) {
		//Name
		if (iter->find(NAME_IDENTIFIER_STRING) != std::string::npos) {
			size_t foundPosition = iter->find(NAME_IDENTIFIER_STRING);
			this->_name = iter->substr(foundPosition + NAME_IDENTIFIER_STRING.length());
		}

		//Manufacturer
		if (iter->find(MANUFACTURER_IDENTIFIER_STRING) != std::string::npos) {
			size_t foundPosition = iter->find(MANUFACTURER_IDENTIFIER_STRING);
			this->_manufacturer = iter->substr(foundPosition + MANUFACTURER_IDENTIFIER_STRING.length());
		}

		//Serial Number
		if (iter->find(SERIAL_NUMBER_IDENTIFIER_STRING) != std::string::npos) {
			size_t foundPosition = iter->find(SERIAL_NUMBER_IDENTIFIER_STRING);
			this->_serialNumber = iter->substr(foundPosition + SERIAL_NUMBER_IDENTIFIER_STRING.length());
		}

		//Motherboard Version
		if (iter->find(VERSION_IDENTIFIER_STRING) != std::string::npos) {
			size_t foundPosition = iter->find(VERSION_IDENTIFIER_STRING);
			this->_version = iter->substr(foundPosition + VERSION_IDENTIFIER_STRING.length());
		}
	}
	if (this->_name == "") {
		this->_name = "Unknown";
	}
	if (this->_manufacturer == "") {
		this->_manufacturer = "Unknown";
	}
	if (this->_serialNumber == "") {
		this->_serialNumber = "Unknown";
	}
	if (this->_version == "") {
		this->_version = "Unknown";
	}
}

std::string __MotherboardInfo::name() const
{
	return this->_name;
}

std::string __MotherboardInfo::manufacturer() const
{
	return this->_manufacturer;
}

std::string __MotherboardInfo::chipset() const
{
	return this->_chipset;
}

std::string __MotherboardInfo::serialNumber() const
{
	return this->_serialNumber;
}

std::string __MotherboardInfo::version() const
{
	return this->_version;
}

int __MotherboardInfo::motherboardNumber() const
{
	return this->_motherboardNumber;
}

void __MotherboardInfo::determineChipset()
{
	__SysCommand systemCommand{ CHIPSET_QUERY_STRING };
	systemCommand.execute();
	if (!systemCommand.hasError()) {
		std::vector<std::string> rawData = systemCommand.outputAsVector();
		for (auto iter = rawData.begin(); iter != rawData.end(); iter++) {
			if (iter->find(CHIPSET_IDENTIFIER_STRING) != std::string::npos) {
				size_t foundPosition = iter->find(CHIPSET_IDENTIFIER_STRING);
				size_t endPosition = iter->find(CHIPSET_END_IDENTIFIER_STRING);
				this->_chipset = iter->substr(foundPosition + CHIPSET_IDENTIFIER_STRING.length(), endPosition - CHIPSET_END_IDENTIFIER_STRING.length() + 2);
			}
		}
	}
}


/// MOTHERBOARD INFO DELEGATE IMPL
const std::string __MotherboardInfoDelegate::MOTHERBOARD_INFO_QUERY_STRING = "wmic baseboard get /format: list";
const std::string __MotherboardInfoDelegate::MOTHERBOARD_INSTANCE_QUERY_STRING = "RequiresDaughterBoard=";
const std::string __MotherboardInfoDelegate::MOTHERBOARD_INFO_END_IDENTIFIER_STRING = "Width=";

__MotherboardInfoDelegate::__MotherboardInfoDelegate() :
	_numberOfMotherboardInfoItems{ 0 }
{
	__SysCommand systemCommand{ MOTHERBOARD_INFO_QUERY_STRING };
	systemCommand.execute();
	std::vector<std::string> tempVector = systemCommand.outputAsVector();
	if (!systemCommand.hasError()) {
		std::vector<std::string> raw = { systemCommand.outputAsVector() };
		determineNumberOfMotherboardInfoItems(raw);
		std::vector<std::string> singleMotherboardInfoItem;
		std::vector<std::string>::const_iterator iter = raw.begin();
		int motherboardNumber = 0;
		while (motherboardNumber < this->_numberOfMotherboardInfoItems) {
			while (iter->find(MOTHERBOARD_INFO_END_IDENTIFIER_STRING) == std::string::npos) {
				if ((*iter != "") && (*iter != "\r")) {
					singleMotherboardInfoItem.push_back(*iter);
				}
				iter++;
			}
			singleMotherboardInfoItem.push_back(*iter);
			this->_motherboardInfoVector.emplace_back(singleMotherboardInfoItem, motherboardNumber);
			singleMotherboardInfoItem.clear();
			iter++;
			motherboardNumber++;
		}
	}
}

void __MotherboardInfoDelegate::determineNumberOfMotherboardInfoItems(const std::vector<std::string> &data)
{
	for (auto iter = data.begin(); iter != data.end(); iter++) {
		if (iter->find(MOTHERBOARD_INSTANCE_QUERY_STRING) != std::string::npos) {
			this->_numberOfMotherboardInfoItems++;
		}
	}
}

std::vector<__MotherboardInfo> __MotherboardInfoDelegate::motherboardInfoVector() const
{
	return this->_motherboardInfoVector;
}

int __MotherboardInfoDelegate::numberOfMotherboardInfoItems() const
{
	return this->_numberOfMotherboardInfoItems;
}
