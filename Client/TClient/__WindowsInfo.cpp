#include "stdafx.h"
#include "__WindowsInfo.h"

const std::string __WindowsInfo::ARCHITECTURE_IDENTIFIER_STRING = "OSArchitecture=";
const std::string __WindowsInfo::CAPTION_INDENTIFIER_STRING = "Caption=";
const std::string __WindowsInfo::NAME_IDENTIFIER_STRING = "Name=";
const std::string __WindowsInfo::MANUFACTURER_IDENTIFIER_STRING = "Manufacturer=";
const std::string __WindowsInfo::VERSION_IDENTIFIER_STRING = "Version=";
const std::string __WindowsInfo::CURRENT_USER_IDENTIFIER_STRING = "RegisteredUser=";
const std::string __WindowsInfo::INSTALL_DATE_IDENTIFIER_STRING = "InstallDate=";
const std::string __WindowsInfo::BOOT_DEVICE_IDENTIFIER_STRING = "BootDevice=";
const std::string __WindowsInfo::SERIAL_NUMBER_IDENTIFIER_STRING = "SerialNumber=";
const std::string __WindowsInfo::TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING = "TotalVisibleMemorySize=";

const int __WindowsInfo::KILOBYTES_PER_MEGABYTE = 1000;



__WindowsInfo::__WindowsInfo(const std::vector<std::string> &rawData, int osNumber) :
	_name{ "" },
	_manufacturer{ "" },
	_architecture{ "" },
	_caption{ "" },
	_version{ "" },
	_currentUser{ "" },
	_installDate{ "" },
	_bootDevice{ "" },
	_serialNumber{ "" },
	_totalVisibleMemory{ "" },
	_osNumber{ osNumber }

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

		//Architecture
		if ((iter->find(ARCHITECTURE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(ARCHITECTURE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(ARCHITECTURE_IDENTIFIER_STRING);
			this->_architecture = iter->substr(foundPosition + ARCHITECTURE_IDENTIFIER_STRING.length());
		}

		//Caption
		if ((iter->find(CAPTION_INDENTIFIER_STRING) != std::string::npos) && (iter->find(CAPTION_INDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(CAPTION_INDENTIFIER_STRING);
			this->_caption = iter->substr(foundPosition + CAPTION_INDENTIFIER_STRING.length());
		}

		//Version
		if ((iter->find(VERSION_IDENTIFIER_STRING) != std::string::npos) && (iter->find(VERSION_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(VERSION_IDENTIFIER_STRING);
			this->_version = iter->substr(foundPosition + VERSION_IDENTIFIER_STRING.length());
		}

		//Current User
		if ((iter->find(CURRENT_USER_IDENTIFIER_STRING) != std::string::npos) && (iter->find(CURRENT_USER_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(CURRENT_USER_IDENTIFIER_STRING);
			this->_currentUser = iter->substr(foundPosition + CURRENT_USER_IDENTIFIER_STRING.length());
		}

		//Install Date
		if ((iter->find(INSTALL_DATE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(INSTALL_DATE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(INSTALL_DATE_IDENTIFIER_STRING);
			this->_installDate = iter->substr(foundPosition + INSTALL_DATE_IDENTIFIER_STRING.length());
		}

		//Boot Device
		if ((iter->find(BOOT_DEVICE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(BOOT_DEVICE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(BOOT_DEVICE_IDENTIFIER_STRING);
			this->_bootDevice = iter->substr(foundPosition + BOOT_DEVICE_IDENTIFIER_STRING.length());
		}

		//Serial Number
		if ((iter->find(SERIAL_NUMBER_IDENTIFIER_STRING) != std::string::npos) && (iter->find(SERIAL_NUMBER_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(SERIAL_NUMBER_IDENTIFIER_STRING);
			this->_serialNumber = iter->substr(foundPosition + SERIAL_NUMBER_IDENTIFIER_STRING.length());
		}

		//Total Visible Memory
		if ((iter->find(TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING) != std::string::npos) && (iter->find(TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING) == 0)) {
			size_t foundPosition = iter->find(TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING);
			std::string totalVisibleMemoryString = iter->substr(foundPosition + TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING.length());
			if (totalVisibleMemoryString == "") {
				this->_totalVisibleMemory = "";
				continue;
			}
			else {
				long long int totalVisibleMemory{ 0 };
				try {
					totalVisibleMemory = std::stoll(totalVisibleMemoryString);
					this->_totalVisibleMemory = toString(totalVisibleMemory / KILOBYTES_PER_MEGABYTE) + "MB (" + toString(totalVisibleMemory) + " KB)";
				}
				catch (std::exception &e) {
					(void)e;
					this->_totalVisibleMemory = totalVisibleMemoryString + " KB";
				}
			}
		}
	}
	//In case any of these values are missing or don't get assigned
	if (this->_name == "") {
		this->_name = "Unknown";
	}
	if (this->_manufacturer == "") {
		this->_manufacturer = "Unknown";
	}
	if (this->_architecture == "") {
		this->_architecture = "Unknown";
	}
	if (this->_caption == "") {
		this->_caption = "Unknown";
	}
	if (this->_version == "") {
		this->_version = "Unknown";
	}
	if (this->_currentUser == "") {
		this->_currentUser = "Unknown";
	}
	if (this->_installDate == "") {
		this->_installDate = "Unknown";
	}
	if (this->_bootDevice == "") {
		this->_bootDevice = "Unknown";
	}
	if (this->_serialNumber == "") {
		this->_serialNumber = "Unknown";
	}
	if ((this->_totalVisibleMemory == "") || (this->_totalVisibleMemory == " KB")) {
		this->_totalVisibleMemory = "Unknown";
	}
}

std::string __WindowsInfo::name() const
{
	return this->_name;
}

std::string __WindowsInfo::manufacturer() const
{
	return this->_manufacturer;
}

std::string __WindowsInfo::architecture() const
{
	return this->_architecture;
}

std::string __WindowsInfo::caption() const
{
	return this->_caption;
}
std::string __WindowsInfo::version() const
{
	return this->_version;
}

std::string __WindowsInfo::currentUser() const
{
	return this->_currentUser;
}

std::string __WindowsInfo::installDate() const
{
	return this->_installDate;
}

std::string __WindowsInfo::bootDevice() const
{
	return this->_bootDevice;
}

std::string __WindowsInfo::serialNumber() const
{
	return this->_serialNumber;
}

std::string __WindowsInfo::totalVisibleMemory() const
{
	return this->_totalVisibleMemory;
}

int __WindowsInfo::osNumber() const
{
	return this->_osNumber;
}

/// WINDOWS INFO DELEGATE IMPL


const std::string __WindowsInfoDelegate::OS_INFO_QUERY_STRING = "wmic os get /format:list";
const std::string __WindowsInfoDelegate::OS_INSTANCE_QUERY_STRING = "PortableOperatingSystem=";
const std::string __WindowsInfoDelegate::OS_INFO_END_IDENTIFIER_STRING = "WindowsDirectory=";

__WindowsInfoDelegate::__WindowsInfoDelegate() :
	_numberOfOSInfoItems{ 0 }
{
	__SysCommand systemCommand{ OS_INFO_QUERY_STRING };
	systemCommand.execute();
	std::vector<std::string> tempVector = systemCommand.outputAsVector();
	if (!systemCommand.hasError()) {
		std::vector<std::string> raw = { systemCommand.outputAsVector() };
		determineNumberOfOSInfoItems(raw);
		std::vector<std::string> singleOSInfoItem;
		std::vector<std::string>::const_iterator iter = raw.begin();
		int osNumber = 0;
		while (osNumber < this->_numberOfOSInfoItems) {
			while (iter->find(OS_INFO_END_IDENTIFIER_STRING) == std::string::npos) {
				if ((*iter != "") && (*iter != "\r")) {
					singleOSInfoItem.push_back(*iter);
				}
				iter++;
			}
			singleOSInfoItem.push_back(*iter);
			this->_winInfoVector.emplace_back(singleOSInfoItem, osNumber);
			singleOSInfoItem.clear();
			iter++;
			osNumber++;
		}
	}
}

void __WindowsInfoDelegate::determineNumberOfOSInfoItems(const std::vector<std::string> &data)
{
	for (auto iter = data.begin(); iter != data.end(); iter++) {
		if (iter->find(OS_INSTANCE_QUERY_STRING) != std::string::npos) {
			this->_numberOfOSInfoItems++;
		}
	}
}

int __WindowsInfoDelegate::numberOfOSInfoItems() const
{
	return this->_numberOfOSInfoItems;
}

std::vector<__WindowsInfo> __WindowsInfoDelegate::winInfoVector() const
{
	return this->_winInfoVector;
}