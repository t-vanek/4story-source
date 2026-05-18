#pragma once
#ifndef __MOTHERBOARD_INFO
#define __MOTHERBOARD_INFO
#include <iostream>
#include <vector>
#include <string>
#include "__SysCommand.h"


class __MotherboardInfo
{
public:
	__MotherboardInfo(const std::vector<std::string> &rawData, int motherboardNumber);
	std::string name() const;
	std::string manufacturer() const;
	std::string chipset() const;
	std::string serialNumber() const;
	std::string version() const;
	int motherboardNumber() const;

private:
	std::string _name;
	std::string _manufacturer;
	std::string _chipset;
	std::string _serialNumber;
	std::string _version;
	int _motherboardNumber;

	template <typename T>
	std::string toString(const T &convert) const
	{
		std::stringstream transfer;
		std::string returnString;
		transfer << convert;
		transfer >> returnString;
		return returnString;
	}

	void determineChipset();

	static const std::string CHIPSET_QUERY_STRING;
	static const std::string NAME_IDENTIFIER_STRING;
	static const std::string MANUFACTURER_IDENTIFIER_STRING;
	static const std::string CHIPSET_IDENTIFIER_STRING;
	static const std::string CHIPSET_END_IDENTIFIER_STRING;
	static const std::string SERIAL_NUMBER_IDENTIFIER_STRING;
	static const std::string VERSION_IDENTIFIER_STRING;
};
#endif

#ifndef __MOTHERBOARD_INFO_DELEGATE
#define __MOTHERBOARD_INFO_DELEGATE
class __MotherboardInfoDelegate
{
public:
	__MotherboardInfoDelegate();
	std::vector<__MotherboardInfo> motherboardInfoVector() const;
	int numberOfMotherboardInfoItems() const;

private:
	std::vector<__MotherboardInfo> _motherboardInfoVector;
	int _numberOfMotherboardInfoItems;

	void determineNumberOfMotherboardInfoItems(const std::vector<std::string> &data);

	static const std::string MOTHERBOARD_INFO_QUERY_STRING;
	static const std::string MOTHERBOARD_INSTANCE_QUERY_STRING;
	static const std::string MOTHERBOARD_INFO_END_IDENTIFIER_STRING;
};
#endif