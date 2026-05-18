#pragma once
#ifndef __SysCommand_H
#define __SysCommand_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <limits>
#include <ctime>
#include <cstdio>
#include <cassert>
#include <functional>

#ifndef PATH_MAX
#define PATH_MAX 128
#endif

#if defined(_WIN32) && defined(_MSC_VER)
#if _MSC_VER < 1700
#error This library needs at least a C++11 compliant compiler
#endif
#else
#if __cplusplus <= 199711L
#error This library needs at least a C++11 compliant compiler
#endif
#endif

#if defined(_WIN32) || defined(_MSC_VERSION)
#include <cstdlib>
#else
#include <unistd.h>
#endif

class __SysCommand
{
public:

	__SysCommand();
	__SysCommand(std::initializer_list<std::string> command);
	__SysCommand(const std::string &command);
	__SysCommand(const std::string &command, int maxOutput);

	void execute();
	void executeWithoutPipe();
	std::vector<std::string> executeAndWaitForOutputAsVector();
	std::string executeAndWaitForOutputAsString();
	void insertIntoCommand(int position, const std::string &stringToInsert);
	void insertIntoCommand(int position, char charToInsert);
	void printCommand();

	int maxOutput();

	std::string command();
	bool hasError();
	int returnValue();
	int sizeOfOutput();

	std::string outputAsString();
	std::vector<std::string> outputAsVector();

	void setCommand(const std::string &command);
	void appendToCommand(const std::string &stringToAppend);
	void setMaxOutput(int maxOutput);


private:
	int _returnValue;
	std::string _command;
	int _maxOutput;
	bool _hasError;
	int _sizeOfOutput;
	bool _repeatCommand;
	std::ifstream _readFromFile;
	std::vector<std::string> _outputAsVector;
	const static int  _DEFAULT_MAX_OUTPUT;

	void verifyValidMaxOutput();
	void stripShellControlCharactersFromCommand();
	void SysCommandLaunch(bool withPipe);

	FILE* popenHandler(const std::string &directory, const std::string &fileMode);
	int pcloseHandler(FILE *filePtr);

	const bool _WITH_PIPE = true;
	const bool _WITHOUT_PIPE = false;

	std::string stripAllFromString(const std::string &stringToStrip, const std::string &whatToStrip);
	std::string stripFromString(const std::string &stringToStrip, const std::string &whatToStrip);
	void stripPipeFromCommand();

	template <typename T>
	std::string toString(const T &thingToConvert)
	{
		std::stringstream stringTransfer;
		std::string returnString;
		stringTransfer << thingToConvert;
		stringTransfer >> returnString;
		return returnString;
	}

	void addFormattedThing(std::vector<std::string>& container, const std::string& thingToAdd, const std::function <std::string(const std::string&)>& lambdaFunction) {
		container.push_back(lambdaFunction(thingToAdd));
	}

	template <typename T>
	void logDataMember(const std::string & nameOfDataMemberToLog, const T &dataMemberToLog) { std::cout << nameOfDataMemberToLog << " = " << toString(dataMemberToLog) << std::endl; }

	template <typename T>
	void logDataMember(const T &dataMemberToLog) { std::cout << toString(dataMemberToLog) << std::endl; }

	void logString(const std::string &stringToLog) { std::cout << stringToLog << std::endl; }
};

#endif //__SysCommand_H
