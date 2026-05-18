#include "stdafx.h"
#include "__SysCommand.h"
#include <Windows.h>
#undef max
const int __SysCommand::_DEFAULT_MAX_OUTPUT = 2147483647;


__SysCommand::__SysCommand(std::initializer_list<std::string> command) :
	_command{ *std::begin(command) },
	_maxOutput{ _DEFAULT_MAX_OUTPUT },
	_hasError{ false },
	_repeatCommand{ false }
{

}

__SysCommand::__SysCommand() :
	_command{ "" },
	_maxOutput{ _DEFAULT_MAX_OUTPUT },
	_hasError{ false },
	_repeatCommand{ false }
{

}

__SysCommand::__SysCommand(const std::string &command) :
	_command{ command },
	_maxOutput{ _DEFAULT_MAX_OUTPUT },
	_hasError{ false },
	_repeatCommand{ false }
{

}

__SysCommand::__SysCommand(const std::string &command, int maxOutput) :
	_command{ command },
	_maxOutput{ maxOutput },
	_hasError{ false },
	_repeatCommand{ false }
{

}


void __SysCommand::printCommand()
{
	std::cout << this->_command << std::endl;
}

void __SysCommand::insertIntoCommand(int position, const std::string &stringToInsert)
{
	if (static_cast<unsigned int>(position) > this->_command.length()) {
		return;
	}
	this->_command.insert(position, stringToInsert);
	this->_repeatCommand = false;
}

void __SysCommand::insertIntoCommand(int position, char charToInsert)
{
	std::string temp = "";
	temp += charToInsert;
	this->insertIntoCommand(position, temp);
}

int __SysCommand::maxOutput()
{
	return this->_maxOutput;
}

std::string __SysCommand::command()
{
	return this->_command;
}

bool __SysCommand::hasError()
{
	return this->_hasError;
}

void __SysCommand::appendToCommand(const std::string &stringToAppend)
{
	_command += stringToAppend;
	this->_repeatCommand = false;
}

int __SysCommand::returnValue()
{
	return this->_returnValue;
}

void __SysCommand::setCommand(const std::string &command)
{
	this->_repeatCommand = false;
	this->_command = command;
	this->_hasError = false;
	this->_sizeOfOutput = 0;
	this->_outputAsVector.clear();
}

void __SysCommand::setMaxOutput(int maxOutput)
{
	this->_maxOutput = maxOutput;
}

std::string __SysCommand::outputAsString()
{
	std::string returnString = "";
	for (std::vector<std::string>::const_iterator iter = _outputAsVector.begin(); iter != _outputAsVector.end(); iter++) {
		returnString += (*iter);
	}
	return returnString;
}

void __SysCommand::stripShellControlCharactersFromCommand()
{
	//TODO: Implement
	return;
}

std::vector<std::string> __SysCommand::outputAsVector()
{
	return this->_outputAsVector;
}

void __SysCommand::execute()
{
	this->SysCommandLaunch(_WITH_PIPE);
}

std::vector<std::string> __SysCommand::executeAndWaitForOutputAsVector()
{
	this->SysCommandLaunch(_WITH_PIPE);
	return this->_outputAsVector;
}

void __SysCommand::executeWithoutPipe()
{
	this->SysCommandLaunch(_WITHOUT_PIPE);
}

std::string __SysCommand::stripAllFromString(const std::string &stringToStrip, const std::string &whatToStrip)
{
	std::string returnString = stringToStrip;
	if (returnString.find(whatToStrip) == std::string::npos) {
		return returnString;
	}
	while (returnString.find(whatToStrip) != std::string::npos) {
		returnString = stripFromString(returnString, whatToStrip);
	}
	return returnString;
}


std::string __SysCommand::stripFromString(const std::string &stringToStrip, const std::string &whatToStrip)
{
	std::string returnString = stringToStrip;
	if (returnString.find(whatToStrip) == std::string::npos) {
		return returnString;
	}
	size_t foundPosition = stringToStrip.find(whatToStrip);
	if (foundPosition == 0) {
		returnString = returnString.substr(whatToStrip.length());
	}
	else if (foundPosition == (returnString.length() - whatToStrip.length())) {
		returnString = returnString.substr(0, foundPosition);
	}
	else {
		returnString = returnString.substr(0, foundPosition) + returnString.substr(foundPosition + whatToStrip.length());
	}
	return returnString;
}

void __SysCommand::stripPipeFromCommand()
{
	_command = stripAllFromString(_command, "2>&1");
	_command = stripAllFromString(_command, ">");
}


std::string __SysCommand::executeAndWaitForOutputAsString()
{
	this->SysCommandLaunch(_WITH_PIPE);
	return this->outputAsString();
}

int __SysCommand::sizeOfOutput()
{
	return this->_sizeOfOutput;
}

void __SysCommand::verifyValidMaxOutput()
{
	if ((_maxOutput <= 8) || (_maxOutput > std::numeric_limits<int>::max())) {
		_maxOutput = _DEFAULT_MAX_OUTPUT;
	}
}

FILE* __SysCommand::popenHandler(const std::string &directory, const std::string &fileMode)
{
#if defined(_WIN32) && defined(_MSC_VER)
	FILE* pFILE = _popen(directory.c_str(), fileMode.c_str());


	ShowWindow(GetConsoleWindow(), SW_HIDE);
	return pFILE;
#else
	return popen(directory.c_str(), fileMode.c_str());
#endif
}

int __SysCommand::pcloseHandler(FILE *filePtr)
{
#if defined(_WIN32) && defined(_MSC_VER)
	int returnValue = _pclose(filePtr);
	return returnValue;
#else
	int returnValue = pclose(filePtr);
	return returnValue == -1 ? -1 : returnValue / 256;
#endif
}

void __SysCommand::SysCommandLaunch(bool withPipe)
{
	stripPipeFromCommand();
	if (this->_repeatCommand) {
		this->_hasError = false;
		this->_sizeOfOutput = 0;
		this->_outputAsVector.clear();
	}
	else {
		this->_repeatCommand = true;
	}
	this->_command += " 2>&1"; //Merges stderror with stdout
	verifyValidMaxOutput();
	if (withPipe) {
		FILE *fp;
		char path[PATH_MAX];

		fp = popenHandler(this->_command.c_str(), "r");
		if (fp == NULL) {
			std::cout << "ERROR: Failed to execute command \"" << this->_command << "\"" << std::endl;
			this->_returnValue = -1;
			return;
		}
		int outputSize{ 0 };
		while ((fgets(path, PATH_MAX, fp) != NULL) && (outputSize <= this->_maxOutput)) {
			std::string formattedPath{ std::string(path) };
			addFormattedThing(this->_outputAsVector, formattedPath, [](const std::string &stringToStrip) -> std::string
			{
				std::string returnString{ stringToStrip };
				std::vector<std::string> newLines{ "\r\n", "\n\r", "\n" };
				for (std::vector<std::string>::const_iterator iter = newLines.begin(); iter != newLines.end(); iter++) {
					if (returnString.find(*iter) != std::string::npos) {
						size_t foundPosition = returnString.find(*iter);
						returnString = returnString.substr(0, foundPosition);
					}
				}
				return returnString;
			});
			outputSize += sizeof(*(std::end(_outputAsVector) - 1));
		}
		this->_returnValue = pcloseHandler(fp);
	}
	else {
		this->_returnValue = system(this->_command.c_str());
	}
	this->_hasError = (this->_returnValue != 0);
}