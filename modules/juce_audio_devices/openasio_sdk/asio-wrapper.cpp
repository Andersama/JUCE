/* openASIO SDK
 * compatibility layer to allow compilation of ASIO hosts in GPL compliant way
 * asio-wrapper.cpp
 *
 * current version: as in original sdk only a single device can be loaded at any time
 * next version: will allow loading of several ASIO devices concurrently
 *
 * Copyright (c) 2018   Andersama <anderson.john.alexander@gmail.com>
 *                      pkv <pkv.stream@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 ******************************************************************************
 * Disclaimer:                                                                *
 ******************************************************************************
 * None of the contents of this file were derived from Steinberg ASIO sdk.    *
 * The content was derived from analysis and cross-examination of asio hosts, *
 * drivers, wrapper API, forum discussions, etc                               *
 * For headers the sources for each item are given in comments.               *
 ******************************************************************************
 * Most of the general info on asio sdk structure was drawn from this paper:
 *
 * https://app.assembla.com/spaces/portaudio/git/source/master/src/hostapi/asio/Pa_ASIO.pdf
 *
 * The starting point for this implementation is Synchronous Audio Router tinyASIO (released under GPL v2+):
 * https://github.com/eiz/SynchronousAudioRouter/blob/master/SarAsio/tinyasio.h
 * (function InstalledAsioDrivers() which queries the windows Registry)
 *
 * Links to wrapper APIs, asio hosts or asio drivers where we found infos:
 *
 * COM interface:
 * https://app.assembla.com/spaces/portaudio/git/source/master/src/hostapi/asio/iasiothiscallresolver.cpp
 *
 * c++ wrappers and audio API:
 * https://app.assembla.com/spaces/portaudio/git/source/master/src/hostapi/asio/pa_asio.cpp
 *https://www.codeguru.com/cpp/cpp/cpp_mfc/oop/article.php/c15891/A-Multidevice-ASIO-Output-Plugin-for-WinAMP.htm and Wasiona_source.zip
 * https://github.com/WeAreROLI/JUCE/blob/master/modules/juce_audio_devices/native/juce_win32_ASIO.cpp
 *
 * java wrapper:
 * http://jsasio.sourceforge.net/
 *
 * .NET wrapper:
 * http://www.bass.radio42.com/help/html/7740a793-3341-3e90-c62c-c7bee8311772.htm
 *
 * c# wrapper:
 * https://github.com/SjB/NAudio/blob/master/NAudio/Wave/Asio/ASIOStructures.cs
 *
 * delphi wrapper:
 * http://lakeofsoft.com/vc/doc/unaASIOAPI.html
 *
 * Golang wrapper:
 * https://github.com/JamesDunne/go-asio/blob/master/asio.go
 *
 * c/c++ driver:
 * https://github.com/nikkov/Win-Widget/blob/master/Driver/asiouac2.cpp
 * https://github.com/eiz/SynchronousAudioRouter/blob/master/SarAsio/tinyasio.h
 *
 * c/c++ hosts:
 * https://github.com/steveschow/osxwineasio/blob/master/asio.c
 * http://www.oifii.org/ns-org/nsd/ar/cp/audio_spivsthost/vsthostsrc/AsioHost.cpp
 * http://kana-soft.com/tech/sample_0004_5.htm
 * https://github.com/dechamps/FlexASIO/blob/master/flexasio.h
 *
 * Additional links given within files.
 *
 ******************************************************************************
 * Legal Disclaimer:                                                          *
 * use this file in your project only if your jurisdiction allows it          *
 ******************************************************************************
 *	USA: complies with Section 103(f) of the Digital Millennium Copyright Act
 *	(DMCA) (17 USC § 1201 (f) - Reverse Engineering) which allows reverse
 *	engineering for the purpose of interoperability between computer programs
 *	(such as information transfer between applications). Interoperability is
 *	defined in paragraph 4 of Section 103(f).
 *		Paragraph (1) of Section 103(f):
 *		(1) Notwithstanding the provisions of subsection (a)(1)(A), a person who
 *		has lawfully obtained the right to use a copy of a computer program may
 *		circumvent a technological measure that effectively controls access to a
 *		particular portion of that program for the sole purpose of identifying
 *		and analyzing those elements of the program that are necessary to
 *		achieve interoperability of an independently created computer program with
 *		other programs, and that have not previously been readily available to the
 *		 person engaging in the circumvention, to the extent any such acts of
 *		identification and analysis do not constitute infringement under this title.
 *
 *	EU: complies with EU Directive 2009/24 which superseded Article 6 of the 1991
 *	EU Computer Programs Directive.
 *	Article (15) allows reverse engineering for the purposes of interoperability :
 *		circumstances may exist when such a reproduction of the code and
 *		translation of its form are indispensable to obtain the necessary
 *		information to achieve the interoperability of an independently created
 *		program with other programs.
 *		It has therefore to be considered that, in these limited circumstances
 *		only, performance of the acts of reproduction and translation by or on
 *		behalf of a person having a right to use a copy of the program is
 *		legitimate and compatible with fair practice and must therefore be deemed
 *		not to require the authorisation of the rightholder.
 *		An objective of this exception is to make it possible to connect all
 *		components of a computer system, including those of different
 *		manufacturers, so that they can work together.
 */

#include "asio-wrapper.hpp"



IASIO *DriverPtr = 0; // is global unfortunately
AsioDrivers *asioDrivers = NULL;

/******************************************************************************/
/*				get drivers from registry , load COM						  */
/* 				code partially from:										  */
/*       SynchronousAudioRouter/blob/master/SarAsio/tinyasio.cpp#ln23         */
/******************************************************************************/

/* global vector storing the info retrieved from registry */
std::vector<AsioDriver> theDrivers;

/* utility functions for conversion of strings, utf8 etc */
std::wstring CHARToWstring(const char *c) {
	const size_t cSize = strlen(c) + 1;
	std::wstring wc(cSize, L'#');
	mbstowcs(&wc[0], c, cSize);

	return wc;
}

std::string TCHARToUTF8(const TCHAR *ptr)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
#if !UNICODE
	std::wstring wc = CHARToWstring(ptr);
	return converter.to_bytes(wc);
#else
	return converter.to_bytes(ptr);
#endif
}

std::wstring UTF8ToWide(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

	return converter.from_bytes(str);
}

/* main function querying the registry */
std::vector<AsioDriver> InstalledAsioDrivers()
{
	std::vector<AsioDriver> result;
	HKEY asio;
	DWORD index = 0, nameSize = 256, valueSize = 256;
	LONG err;
	TCHAR name[256], value[256], value2[256];

	printf("ASIO info: Querying installed ASIO drivers.\n");

	if (!SUCCEEDED(err = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\ASIO"), 0, KEY_READ, &asio))) {

		printf("ASIO Error: Failed to open HKLM\\SOFTWARE\\ASIO: status %i", err);
		return result;
	}

	while ((err = RegEnumKeyEx(
		asio, index++, name, &nameSize, nullptr, nullptr, nullptr, nullptr)) ==
		ERROR_SUCCESS) {

		AsioDriver driver;

		nameSize = 256;
		valueSize = 256;

		if ((err = RegGetValue(
			asio, name, TEXT("CLSID"), RRF_RT_REG_SZ,
			nullptr, value, &valueSize)) != ERROR_SUCCESS) {

			printf("ASIO Error: Skipping key %s: Couldn't get CLSID, error %i\n", TCHARToUTF8(name).c_str(), err);
			continue;
		}

		driver.clsid = TCHARToUTF8(value);
		valueSize = 256;

		if (RegGetValue(
			asio, name, TEXT("Description"), RRF_RT_REG_SZ,
			nullptr, value2, &valueSize) != ERROR_SUCCESS) {

			// Workaround for drivers with incomplete ASIO registration.
			// Observed with M-Audio drivers: the main (64bit) registration is
			// fine but the Wow6432Node version is missing the description.
			driver.name = TCHARToUTF8(name);
			printf("ASIO Error: Unable to get ASIO driver description for %s, using key name instead.\n", driver.name.c_str());

		}
		else {
			driver.name = TCHARToUTF8(value2);
		}

		printf("Found ASIO driver: %s with CLSID %s\n", driver.name.c_str(), driver.clsid.c_str());
		result.emplace_back(driver);
	}

	printf("ASIO Info: Done querying ASIO drivers. Status:%i\n ", err);

	RegCloseKey(asio);
	return result;
}

/*****************************************************************************/
/*					AsioDriverList class methods							 */
/*****************************************************************************/
// constructor initializes COM
AsioDriverList::AsioDriverList()
{
	numdrv = 0;
	if (theDrivers.size() == 0) {
		theDrivers = InstalledAsioDrivers();
	}
	numdrv = (int)theDrivers.size();

	// initialization of  COM
	if (numdrv) CoInitialize(0);
}

AsioDriverList::~AsioDriverList()
{
	if (numdrv) {
		CoUninitialize();
	}
}

LONG AsioDriverList::asioGetNumDev(VOID)
{
	return numdrv;
}

// see for instance https://github.com/eiz/SynchronousAudioRouter/blob/master/SarAsio/tinyasio.cpp#ln86
// or https://github.com/sjoerdvankreel/xt-audio/blob/master/src/core/xt-asio.cpp
// also WASIONA, COMSmartPtrWrapper.inl#ln102 for COM instance creation
LONG AsioDriverList::asioOpenDriver(int drvID, LPVOID *asiodrv)
{
	if (theDrivers.size() == 0) {
		theDrivers = InstalledAsioDrivers();
	}
	printf("Trying to open the driver\n");
	long res;
	HRESULT status;
	CLSID clsid;

	if (drvID >= 0 && drvID < numdrv) {
		// convert from string to clsid because we store in struct as string
		//printf("Printing the clsid c string: %s\n", theDrivers[drvID].clsid.c_str());
		//printf("Printing the name c string: %s\n", theDrivers[drvID].name.c_str());
		auto wstr = UTF8ToWide(theDrivers[drvID].clsid);
		status = CLSIDFromString(wstr.c_str(), &clsid);

		if (!SUCCEEDED(status)) {
			return DRVERR_INVALID_PARAM;
		}

		res = CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER, clsid, asiodrv);
		if (res == S_OK) {
			printf("ASIO Info: Driver %s successfully opened\n", theDrivers[drvID].name.c_str());
			theDrivers[drvID].Device = *asiodrv;
			DriverPtr = (IASIO *)theDrivers[drvID].Device;
			return 0;
		}
		else {
			return DRVERR_DEVICE_ALREADY_OPEN;
		}
	}
	else {
		printf("ASIO Error: Invalid driver index");
		return DRVERR_DEVICE_NOT_FOUND;
	}

}

LONG AsioDriverList::asioCloseDriver(int drvID)
{
	IASIO			*iasio;
	AsioDriver driver;

	if (drvID >= 0 && drvID < numdrv) {
		driver = theDrivers[drvID];
		iasio = (IASIO *)driver.Device;
		iasio->Release();
	}
	else {
		printf("ASIO Error: Invalid driver index");
		return DRVERR_DEVICE_NOT_FOUND;
	}

	return 0;
}

LONG AsioDriverList::asioGetDriverName(int drvID, char *drvname, int drvnamesize)
{
	AsioDriver driver;
	if (theDrivers.size() == 0) {
		theDrivers = InstalledAsioDrivers();
	}

	if (drvID >= 0 && drvID < numdrv) {
		driver = theDrivers[drvID];
	}
	else {
		printf("ASIO Error: Invalid driver index\n");
		return DRVERR_DEVICE_NOT_FOUND;
	}

	if (!drvname) return DRVERR_INVALID_PARAM;

	std::string str = driver.name;
	//	printf("asiogetdrivername: drive string name is %s\n", str.c_str());
	if (str.length() < drvnamesize) {
		memcpy(drvname, str.c_str(), str.length() + 1);
		drvname[str.length()] = '\0';
		//		printf("asiogetdrivername: drive char name is %s\n", drvname);
		return ASE_OK;
	}
	else {
		printf("ASIO Error: Invalid driver name\n");
		return DRVERR_DEVICE_NOT_FOUND;
	}

}

LONG AsioDriverList::asioGetDriverPath(int drvID, char *dllpath, int dllpathsize)
{
	//not implemented yet; maybe later
	printf("asioGetDriverPath: method not implemented in openASIO sdk\n");
	return DRVERR_INVALID_PARAM;
}

LONG AsioDriverList::asioGetDriverCLSID(int drvID, CLSID *clsid)
{
	AsioDriver driver;
	if (theDrivers.size() == 0) {
		theDrivers = InstalledAsioDrivers();
	}

	if (drvID >= 0 && drvID < numdrv) {
		driver = theDrivers[drvID];
		HRESULT status;
		auto wstr = UTF8ToWide(driver.clsid);
		status = CLSIDFromString(wstr.c_str(), clsid);

		if (!SUCCEEDED(status)) {
			return status;
		}
	}
	else {
		printf("ASIO Error: Invalid driver index\n");
		return DRVERR_DEVICE_NOT_FOUND;
	}

	return DRVERR_DEVICE_NOT_FOUND;
}

/*****************************************************************************/
/*						AsioDrivers class methods							 */
/*****************************************************************************/

// initialize index with no device; since index is positive, assign negative
// value for no device.
AsioDrivers::AsioDrivers() : AsioDriverList()
{
	currentIndex = -1;
}

AsioDrivers::~AsioDrivers()
{
}

long AsioDrivers::getDriverNames(char **names, long maxDrivers)
{
	long i;
	for (i = 0; i < asioGetNumDev() && i < maxDrivers; i++) {
		asioGetDriverName(i, names[i], 32);
	}
	if (asioGetNumDev() < maxDrivers) {
		return asioGetNumDev();
	}
	else {
		printf("ASIO Error: number of devices is larger than number of drivers.\n");
		return 0;
	}
}

bool AsioDrivers::getCurrentDriverName(char *name)
{
	if (currentIndex >= 0 && currentIndex < asioGetNumDev()) {
		return asioGetDriverName(currentIndex, name, 32) == 0 ? true : false;
	}
	*name = 0;
	return false;
}

// see
bool AsioDrivers::loadDriver(char *name)
{
	char currentDriverName[32];
	long i, ret;
	if (theDrivers.size() == 0) {
		theDrivers = InstalledAsioDrivers();
	}

	for (i = 0; i < theDrivers.size(); i++) {
		if (strcmp(name, theDrivers[i].name.c_str()) == 0) {
			currentDriverName[0] = 0;
			getCurrentDriverName(currentDriverName);
			removeCurrentDriver();

			ret = asioOpenDriver(i, (void **)&DriverPtr);
			switch (ret) {
			case 0:
				currentIndex = i;
				return true;
			case DRVERR_DEVICE_NOT_FOUND:
				printf("ASIO Error: device not found; driver not loaded");
				return false;
			case DRVERR_DEVICE_ALREADY_OPEN:
				DriverPtr = 0;
				if (currentDriverName[0] && strcmp(theDrivers[i].name.c_str(), currentDriverName)) {
					loadDriver(currentDriverName);
				}
				return true;
			}
		}
	}
	return false;
}

void AsioDrivers::removeCurrentDriver()
{
	if (currentIndex >=0) {
		asioCloseDriver(currentIndex);
	}
	currentIndex = -1; // reset index to no Device
}

/*****************************************************************************/
/*				main functions: start stop exit init						 */
/*****************************************************************************/

ASIOError ASIOStart(void)
{
	if (!DriverPtr) {
		printf("ASIOStart Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->start();
}

ASIOError ASIOStop(void)
{
	if(!DriverPtr) {
		printf("ASIOStop Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->stop();
}

ASIOError ASIOExit(void)
{
	if (DriverPtr) {
		asioDrivers->removeCurrentDriver();
	}
	DriverPtr = 0;
	return ASE_OK;
}

ASIOError ASIOOutputReady(void)
{
	if (!DriverPtr) {
		printf("ASIOOutputReady Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->outputReady();
}

ASIOError ASIOInit(ASIODriverInfo *info)
{
	info->driverVersion = 0;
	strcpy(info->name," Unset ");
	if(DriverPtr && (!DriverPtr->init(info->sysRef)))	{
		DriverPtr = 0;
		printf("ASIO Error: Could not initialize driver.\n");
		return ASE_NotPresent;
	}
	if (DriverPtr) {
		DriverPtr->getDriverName(info->name);
		info->driverVersion = DriverPtr->getDriverVersion();
		strcpy(info->errorMessage,"No error with the driver (yet)\n");
		printf("ASIO: driver init OK!\n");
		return ASE_OK;
	}
	return ASE_NotPresent;

}
/*****************************************************************************/
/*							Misc functions									 */
/*****************************************************************************/

ASIOError ASIOControlPanel(void)
{
	if(!DriverPtr) {
		printf("ASIOControlPanel Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->controlPanel();
}

ASIOError ASIOFuture(long selector, void *option)
{
	if(!DriverPtr) {
		printf("ASIOFuture Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->future(selector, option);
}
/*****************************************************************************/
/*		Get driver settings : sample rate , buffer size, clock				 */
/*****************************************************************************/

ASIOError ASIOGetSampleRate(ASIOSampleRate *rate)
{
	if(!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->getSampleRate(rate);
}

ASIOError ASIOGetBufferSize(long *min, long *max, long *pref, long *granularity)
{
	if(!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		*min = 0;
		*max = 0;
		*pref = 0;
		*granularity = 0;
		return ASE_NotPresent;
	}
	return DriverPtr->getBufferSize(min, max, pref, granularity);
}

ASIOError ASIOGetClockSources(ASIOClockSource *clock, long *sources)
{
	if(!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		*sources = 0;
		return ASE_NotPresent;
	}
	return DriverPtr->getClockSources(clock, sources);
}

// retrieves info on the number of channels (input and output)
ASIOError ASIOGetChannels(long *inChannels, long *outChannels)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		*inChannels = 0;
		*outChannels = 0;
		return ASE_NotPresent;
	}
	return DriverPtr->getChannels(inChannels, outChannels);
}

// retrieves info on a channel (bitdepth, name ...)
ASIOError ASIOGetChannelInfo(ASIOChannelInfo *channelInfo)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		channelInfo->channelGroup = -1;
		strcpy(channelInfo->name," ");
		channelInfo->type = ASIOSTFloat32LSB;
		return ASE_NotPresent;
	}
	return DriverPtr->getChannelInfo(channelInfo);
}

ASIOError ASIOGetLatencies(long *in, long *out)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		*in = 0;
		*out = 0;
		return ASE_NotPresent;
	}
	return DriverPtr->getLatencies(in, out);
}

ASIOError ASIOGetSamplePosition(ASIOSamples *samplePosition, ASIOTimeStamp *timeStamp)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->getSamplePosition(samplePosition, timeStamp);
}


ASIOError ASIOCanSampleRate(ASIOSampleRate rate)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->canSampleRate(rate);
}

/*****************************************************************************/
/*				Set driver settings : sample rate , clock source			 */
/*****************************************************************************/

ASIOError ASIOSetSampleRate(ASIOSampleRate rate)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->setSampleRate(rate);
}

ASIOError ASIOSetClockSource(long clock)
{
	if(!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->setClockSource(clock);
}

/*****************************************************************************/
/*						Buffer functions									 */
/*****************************************************************************/

ASIOError ASIOCreateBuffers(ASIOBufferInfo *bufInfo, long channels,	long bufSize, ASIOCallbacks *cb)
{
	int i;
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		ASIOBufferInfo *info = bufInfo;
		for (i = 0; i < channels; i++) {
			info->buffers[0] = 0;
			info->buffers[1] = 0;
			info++;
		}
		return ASE_NotPresent;
	}
	return DriverPtr->createBuffers(bufInfo, channels, bufSize, cb);
}

ASIOError ASIODisposeBuffers(void)
{
	if (!DriverPtr) {
		printf("ASIO Error: Hardware not available.\n");
		return ASE_NotPresent;
	}
	return DriverPtr->disposeBuffers();
}

