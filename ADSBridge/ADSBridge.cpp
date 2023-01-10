// ADSBridge.cpp : Defines the entry point for the application.
//
#include "ADSBridge.h"


typedef enum AdsDataType
{
	ADST_VOID = 0,
	ADST_INT16 = 2,
	ADST_INT32 = 3,
	ADST_REAL32 = 4,
	ADST_REAL64 = 5,
	ADST_INT8 = 16,
	ADST_UINT8 = 17,
	ADST_UINT16 = 18,
	ADST_UINT32 = 19,
	ADST_INT64 = 20,
	ADST_UINT64 = 21,
	ADST_STRING = 30,
	ADST_WSTRING = 31,
	ADST_REAL80 = 32,
	ADST_BIT = 33,
	ADST_MAXTYPES = 34,
	ADST_BIGTYPE = 65
} ADSDATATYPE;

// Splits string by slash (path separator)
std::vector<std::string> splitPath(std::string path) {
	std::vector<std::string> paths{};
	for (size_t pos = 0; (pos = path.find("/")) != std::string::npos; (pos = path.find("/"))) {
		paths.push_back(path.substr(0, pos));
		path.erase(0, pos + 1);
	}
	paths.push_back(path);
	return paths;
}

// Gets handle for symbol/variable with given name
auto getSymHandleByName(PAmsAddr pAddr, std::string& varName) {
	ULONG symHandle{};
	long nErr = AdsSyncReadWriteReq(pAddr, ADSIGRP_SYM_HNDBYNAME, 0x0, sizeof(symHandle), &symHandle, static_cast<ULONG>(varName.length()), varName.data());
	return std::make_pair(nErr, symHandle);
}

// Returns data at index group and offset depending on data type
auto readGroupOffset(PAmsAddr pAddr, const ULONG& indexGroup, const ULONG& indexOffset, auto&& pData) {
	auto data{ pData };
	long nErr = AdsSyncReadReq(pAddr, indexGroup, indexOffset, sizeof(data), &data);
	return std::pair(nErr, data);
}

// Returns data at index group and offset depending on data type and updates nErr parameter accordingly
auto readGroupOffset(PAmsAddr pAddr, const ULONG& indexGroup, const ULONG& indexOffset, auto&& pData, long& nErr) {
	auto [err, data] = readGroupOffset(pAddr, indexGroup, indexOffset, pData);
	nErr = err;
	return std::pair(nErr, data);
}

// Returns data at index group and offset depending on data type and updates nErr as well as str parameter accordingly
auto readGroupOffset(PAmsAddr pAddr, const ULONG& indexGroup, const ULONG& indexOffset, auto&& pData, long& nErr, std::string& str) {
	auto [err, data] = readGroupOffset(pAddr, indexGroup, indexOffset, pData, nErr);
	std::stringstream dstream;
	dstream << data;
	str = dstream.str();
	return std::pair(nErr, str);
}

// Writes given data at index group and offset
auto writeGroupOffset(PAmsAddr pAddr, const ULONG& indexGroup, const ULONG& indexOffset, const auto& data) {
	auto rData{ data };
	long nErr = AdsSyncWriteReq(pAddr, indexGroup, indexOffset, sizeof(rData), &rData);
	return nErr;
}

// Reads from handle
auto getSymValueByHandle(PAmsAddr pAddr, const ULONG& symHandle, auto& nData) {
	return readGroupOffset(ADSIGRP_SYM_VALBYHND, symHandle, nData);
}

struct TwinCatArray {
	unsigned long   bound;
	unsigned long   size;
};

struct TwinCatType {
	std::string name;
	std::string type;
	std::string comment;
	std::map<std::string, TwinCatType> subItems;
	ADS_UINT32		entryLength;
	ADS_UINT32		version;
	ADS_UINT32		size;
	ADS_UINT32		offs;
	ADS_UINT32		dataType;
	ADS_UINT32		flags;
	ADS_UINT16		arrayDim;
	std::vector<TwinCatArray>   arrayVector;
};

// Store symbol/variable declaration
struct TwinCatVar {
	std::string name;
	ULONG indexGroup;
	ULONG indexOffset;
	ULONG size;
	std::string type;
	std::string comment;
	TwinCatType datatype;
	std::string str() const {
		std::stringstream strstream;
		strstream << "{\"Name\":\"" << name << "\",";
		strstream << "\"IndexGroup\":" << indexGroup << ",";
		strstream << "\"IndexOffset\":" << indexOffset << ",";
		strstream << "\"Size\":" << size << ",";
		strstream << "\"Type\":\"" << type << "\",";
		strstream << "\"Comment\":\"" << comment << "\"}";
		return strstream.str();
	}
};

auto getUploadInfo(PAmsAddr pAddr) {
	AdsSymbolUploadInfo2 tAdsSymbolUploadInfo;
	long nErr = AdsSyncReadReq(pAddr, ADSIGRP_SYM_UPLOADINFO2, 0x0, sizeof(tAdsSymbolUploadInfo), &tAdsSymbolUploadInfo);
	return std::make_pair(nErr, tAdsSymbolUploadInfo);
}

auto getSymbolUpload(PAmsAddr pAddr, AdsSymbolUploadInfo2 info) {
	char* symbolUpload = new char[info.nSymSize];
	long nErr = AdsSyncReadReq(pAddr, ADSIGRP_SYM_UPLOAD, 0, info.nSymSize, symbolUpload);
	return std::make_pair(nErr, symbolUpload);
}

auto getDatatypeUpload(PAmsAddr pAddr, AdsSymbolUploadInfo2 info) {
	char* dataUpload = new char[info.nDatatypeSize];
	long nErr = AdsSyncReadReq(pAddr, ADSIGRP_SYM_DT_UPLOAD, 0, info.nDatatypeSize, dataUpload);
	return std::make_pair(nErr, dataUpload);
}

bool isDatatype(PAdsDatatypeEntry datatype) {
	if (datatype->flags == 0) return FALSE;
	return (datatype->flags & ADSDATATYPEFLAG_DATATYPE) == ADSDATATYPEFLAG_DATATYPE;
}

bool isDataitem(PAdsDatatypeEntry datatype) {
	if (datatype->flags == 0) return FALSE;
	return (datatype->flags & ADSDATATYPEFLAG_DATAITEM) == ADSDATATYPEFLAG_DATAITEM;
}

TwinCatType getDatatype(PAdsDatatypeEntry datatypeEntry) {
	std::string name{ PADSDATATYPENAME(datatypeEntry) };
	std::string type{ PADSDATATYPETYPE(datatypeEntry) };
	std::string comment{ PADSDATATYPECOMMENT(datatypeEntry) };
	std::map<std::string, TwinCatType> subItems{};
	for (UINT uiIndex = 0; uiIndex < datatypeEntry->subItems; uiIndex++)
	{
		PAdsDatatypeEntry subEntry = AdsDatatypeStructItem(datatypeEntry, uiIndex);
		subItems[PADSDATATYPENAME(subEntry)] = getDatatype(subEntry);
	}
	ADS_UINT32		entryLength = datatypeEntry->entryLength;
	ADS_UINT32		version = datatypeEntry->version;
	ADS_UINT32		size = datatypeEntry->size;
	ADS_UINT32		offs = datatypeEntry->offs;
	ADS_UINT32		dataType = datatypeEntry->dataType;
	ADS_UINT32		flags = datatypeEntry->flags;
	ADS_UINT16		arrayDim = datatypeEntry->arrayDim;
	std::vector<TwinCatArray> arrayVector{};
	PAdsDatatypeArrayInfo arrayInfo = PADSDATATYPEARRAYINFO(datatypeEntry);
	for (UINT uiIndex = 0; uiIndex < arrayDim; uiIndex++) {
		unsigned long bound = arrayInfo->lBound;
		unsigned long size = arrayInfo->elements;
		arrayVector.push_back(TwinCatArray{bound, size});
		arrayInfo = (*((unsigned long*)(((char*)arrayInfo) + (sizeof(AdsDatatypeArrayInfo)))) \
			? ((PAdsDatatypeArrayInfo)(((char*)arrayInfo) + (sizeof(AdsDatatypeArrayInfo)))) : NULL);
	}
	return TwinCatType{ name, type, comment, subItems, entryLength, version, size, offs, dataType, flags, arrayDim, arrayVector };
}

TwinCatType getDatatypeRecursive(std::map<std::string, TwinCatType> datatypes, std::string name, bool firstLevel = true) {
	TwinCatType oldtype = datatypes[name];
	TwinCatType newtype = TwinCatType{ oldtype.name, oldtype.type, oldtype.comment, {}, oldtype.entryLength, oldtype.version, oldtype.size, oldtype.offs, oldtype.dataType, 0, oldtype.arrayDim, {} };
	if (oldtype.subItems.size() > 0) {
		for (const auto& [key, value] : oldtype.subItems) {
			TwinCatType subtype = getDatatypeRecursive(datatypes, value.type, false);
			subtype.type = subtype.name;
			subtype.name = key;
			subtype.offs = value.offs;
			subtype.comment = value.comment;
			newtype.subItems[key] = subtype;
		}
	}
	else if ((oldtype.type == "" || oldtype.dataType < ADST_MAXTYPES) && oldtype.arrayVector.size() == 0) {
	} else if (oldtype.dataType == ADST_BIGTYPE && oldtype.arrayVector.size() == 0) {
		if (oldtype.size == 4) {
			newtype.dataType = ADST_UINT32;
		}
		else if (oldtype.size == 8) {
			newtype.dataType = ADST_UINT64;
		}
	} else if (oldtype.arrayVector.size() > 0) {
		newtype = getDatatypeRecursive(datatypes, oldtype.type, false);
		std::vector<TwinCatArray> vec = oldtype.arrayVector;
		vec.insert(vec.end(), newtype.arrayVector.begin(), newtype.arrayVector.end());
		newtype.arrayVector = vec;
	}
	else {
		newtype = getDatatypeRecursive(datatypes, oldtype.type, false);
	}
	return newtype;
}


// Returns all datatype declarations
auto getDatatypeMap(PAmsAddr pAddr, AdsSymbolUploadInfo2 info) {
	std::map<std::string, TwinCatType> datatypes{};
	auto [nErr, datatypeUpload] = getDatatypeUpload(pAddr, info);
	if (nErr) return std::make_pair(nErr, datatypes);
	PAdsDatatypeEntry datatypeEntry = (PAdsDatatypeEntry)datatypeUpload;
	for (UINT uiIndex = 0; uiIndex < info.nDatatypes; uiIndex++)
	{
		std::string name{ PADSDATATYPENAME(datatypeEntry) };
		datatypes[name] = getDatatype(datatypeEntry);
		datatypeEntry = (*((unsigned long*)(((char*)datatypeEntry) + ((PAdsDatatypeEntry)datatypeEntry)->entryLength)) \
			? ((PAdsDatatypeEntry)(((char*)datatypeEntry) + ((PAdsDatatypeEntry)datatypeEntry)->entryLength)) : NULL);
	}
	if (datatypeUpload) delete(datatypeUpload);
	return std::make_pair(nErr, datatypes);
}

// Returns all symbol/variable declarations
auto getSymbolMap(PAmsAddr pAddr, AdsSymbolUploadInfo2 info, std::map<std::string, TwinCatType> datatypes) {
	std::map<std::string, TwinCatVar> symbols{};
	auto [nErr, pchSymbols] = getSymbolUpload(pAddr, info);
	if (nErr) return std::make_pair(nErr, symbols);
	PAdsSymbolEntry pAdsSymbolEntry = (PAdsSymbolEntry)pchSymbols;
	for (UINT uiIndex = 0; uiIndex < info.nSymbols; uiIndex++)
	{
		std::string name{ PADSSYMBOLNAME(pAdsSymbolEntry) };
		ULONG indexGroup = pAdsSymbolEntry->iGroup;
		ULONG indexOffset = pAdsSymbolEntry->iOffs;
		ULONG size = pAdsSymbolEntry->size;
		std::string type{ PADSSYMBOLTYPE(pAdsSymbolEntry) };
		std::string comment{ PADSSYMBOLCOMMENT(pAdsSymbolEntry) };
		TwinCatType datatype = datatypes[type];
		symbols[name] = TwinCatVar{ name,indexGroup,indexOffset,size,type,comment,datatype };
		pAdsSymbolEntry = PADSNEXTSYMBOLENTRY(pAdsSymbolEntry);
	}
	if (pchSymbols) delete(pchSymbols);
	return std::make_pair(nErr, symbols);
}

std::pair<long, std::string> getVariableJSONValue(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, bool aryItem = false);

// Adapted from: https://github.com/jisotalo/ads-client/blob/master/src/ads-client.js
std::pair<std::string, ULONG> parseArray(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, ADS_UINT16 dim) {
	std::stringstream rstream;
	rstream << "[";
	ULONG offset = indexOffset;
	bool first = true;
	for (ADS_UINT16 i = 0; i < datatype.arrayVector[dim].size; i++) {
		if (!first) {
			rstream << ",";
		}
		else {
			first = false;
		}
		if ((dim + 1) < datatype.arrayVector.size()) {
			auto [value, noffset] = parseArray(pAddr, datatype, indexGroup, offset, dim + 1);
			rstream << value;
			offset = noffset;
		}
		else {
			rstream << getVariableJSONValue(pAddr, datatype, indexGroup, offset, true).second;
			offset += datatype.size;
		}
	}
	rstream << "]";
	return std::make_pair(rstream.str(), offset);
}

// Reads value of symbol/variable and returns JSON string representation
std::pair<long, std::string> getVariableJSONValue(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, bool aryItem) {
	long nErr{};
	std::cout << datatype.name << "," << datatype.type << "," << datatype.dataType << "\n";
	std::string value;
	if ((datatype.arrayVector.size() == 0 || aryItem) && datatype.subItems.size() > 0) {
		std::stringstream vstream;
		vstream << "{";
		ULONG offset = indexOffset + datatype.offs;
		bool first = true;
		for (const auto& [key, value] : datatype.subItems) {
			if (!first) {
				vstream << ",";
			}
			else {
				first = false;
			}
			vstream << "\"" << key << "\":";
			auto [err, data] = getVariableJSONValue(pAddr, value, indexGroup, offset);
			offset += value.size;
			if (err) {
				nErr = err;
				vstream << "null";
			}
			else {
				vstream << data;
			}
		}
		vstream << "}";
		value = vstream.str();
	}
	else if (datatype.arrayVector.size() > 0 && !aryItem) {
		value = parseArray(pAddr, datatype, indexGroup, indexOffset, 0).first;
	}
	else {
		switch ((ADSDATATYPE)datatype.dataType)
		{
		case ADST_VOID:
			value = "null";
			break;
		case ADST_BIT:
		{
			auto [err, data] = readGroupOffset(pAddr, indexGroup, indexOffset, bool{}, nErr);
			value = (data ? "true" : "false");
		}
		break;
		case ADST_INT8:
			readGroupOffset(pAddr, indexGroup, indexOffset, INT8{}, nErr, value);
			break;
		case ADST_INT16:
			readGroupOffset(pAddr, indexGroup, indexOffset, INT16{}, nErr, value);
			break;
		case ADST_INT32:
			readGroupOffset(pAddr, indexGroup, indexOffset, INT32{}, nErr, value);
			break;
		case ADST_INT64:
			readGroupOffset(pAddr, indexGroup, indexOffset, INT64{}, nErr, value);
			break;
		case ADST_UINT8:
			readGroupOffset(pAddr, indexGroup, indexOffset, UINT8{}, nErr, value);
			break;
		case ADST_UINT16:
			readGroupOffset(pAddr, indexGroup, indexOffset, UINT16{}, nErr, value);
			break;
		case ADST_UINT32:
			readGroupOffset(pAddr, indexGroup, indexOffset, UINT32{}, nErr, value);
			break;
		case ADST_UINT64:
			readGroupOffset(pAddr, indexGroup, indexOffset, UINT64{}, nErr, value);
			break;
		case ADST_REAL32:
			readGroupOffset(pAddr, indexGroup, indexOffset, float{}, nErr, value);
			break;
		case ADST_REAL64:
			readGroupOffset(pAddr, indexGroup, indexOffset, double{}, nErr, value);
			break;
		case ADST_STRING:
		{
			char pData[256];
			nErr = AdsSyncReadReq(pAddr, indexGroup, indexOffset, datatype.size, &pData);
			if (!nErr) {
				std::string extendedValue{ pData };
				std::stringstream vstream;
				vstream << "\"" << extendedValue.substr(0, datatype.size) << "\"";
				value = vstream.str();
			}
		}
		break;
		default:
			nErr = ADSERR_DEVICE_INVALIDDATA;
			break;
		}
	}
	return std::pair(nErr, value);
}

auto getVariableJSONValue(PAmsAddr pAddr, std::map<std::string, TwinCatType> datatypes, TwinCatVar variable) {
	// return getVariableJSONValue(pAddr, variable.datatype, variable.indexGroup, variable.indexOffset);
	return getVariableJSONValue(pAddr, getDatatypeRecursive(datatypes, variable.datatype.name), variable.indexGroup, variable.indexOffset);
}

long setVariableJSONValue(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, const nlohmann::json jsonValue, bool aryItem = false);

// Adapted from: https://github.com/jisotalo/ads-client/blob/master/src/ads-client.js
std::pair<long, ULONG> unparseArray(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, const nlohmann::json jsonValue, ADS_UINT16 dim) {
	long nErr{};
	ULONG offset = indexOffset;
	for (ADS_UINT16 i = 0; i < datatype.arrayVector[dim].size; i++) {
		if ((dim + 1) < datatype.arrayVector.size()) {
			auto [err, noffset] = unparseArray(pAddr, datatype, indexGroup, offset, jsonValue[i], dim + 1);
			nErr = err;
			offset = noffset;
		}
		else {
			nErr = setVariableJSONValue(pAddr, datatype, indexGroup, offset, jsonValue[i], true);
			offset += datatype.size;
		}
		if (nErr) {
			break;
		}
	}
	return std::make_pair(nErr, offset);
}

// Updates symbol/variable based on provided json value
long setVariableJSONValue(PAmsAddr pAddr, TwinCatType datatype, ULONG indexGroup, ULONG indexOffset, const nlohmann::json jsonValue, bool aryItem) {
	long nErr{};
	if ((datatype.arrayVector.size() == 0 || aryItem) && datatype.subItems.size() > 0) {
		for (const auto& [key, value] : datatype.subItems) {
			if (!nErr) {
				nErr = setVariableJSONValue(pAddr, value, indexGroup, indexOffset + value.offs, jsonValue[key]);
			}
		}
	}
	else if (datatype.arrayVector.size() > 0 && !aryItem) {
		nErr = unparseArray(pAddr, datatype, indexGroup, indexOffset, jsonValue, 0).first;
	}
	else {
		ADSDATATYPE type = (ADSDATATYPE)datatype.dataType;
		if (type == ADST_VOID && jsonValue.is_null()) {
		}
		else if (type == ADST_BIT && jsonValue.is_boolean()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<bool>());
		}
		else if (type == ADST_INT8 && jsonValue.is_number_integer()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<int8_t>());
		}
		else if (type == ADST_INT16 && jsonValue.is_number_integer()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<int16_t>());
			}
		else if (type == ADST_INT32 && jsonValue.is_number_integer()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<int32_t>());
		}
		else if (type == ADST_INT64 && jsonValue.is_number_integer()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<int64_t>());
		}
		else if (type == ADST_UINT8 && jsonValue.is_number_unsigned()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<uint8_t>());
		}
		else if (type == ADST_UINT16 && jsonValue.is_number_unsigned()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<uint16_t>());
		}
		else if (type == ADST_UINT32 && jsonValue.is_number_unsigned()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<uint32_t>());
		}
		else if (type == ADST_UINT64 && jsonValue.is_number_unsigned()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<uint64_t>());
		}
		else if (type == ADST_REAL32 && jsonValue.is_number_float()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<float>());
		}
		else if (type == ADST_REAL64 && jsonValue.is_number_float()) {
			nErr = writeGroupOffset(pAddr, indexGroup, indexOffset, jsonValue.get<double>());
		}
		else if (type == ADST_STRING && jsonValue.is_string()) {
			std::string valueStr = jsonValue.get<std::string>();
			nErr = AdsSyncWriteReq(pAddr, indexGroup, indexOffset, datatype.size, valueStr.data());
		} else {
			nErr = ADSERR_DEVICE_INVALIDDATA;
		}
	}
	return nErr;
}

auto setVariableJSONValue(PAmsAddr pAddr, std::map<std::string, TwinCatType> datatypes, TwinCatVar variable, const nlohmann::json jsonValue) {
	return setVariableJSONValue(pAddr, getDatatypeRecursive(datatypes, variable.datatype.name), variable.indexGroup, variable.indexOffset, jsonValue);
}

int main(int argc, const char** argv)
{
	httplib::Server svr;

	// Open communication port
	std::cout << "Opening communication port..." << '\n';
	AmsAddr Addr{};
	PAmsAddr pAddr = &Addr;
	long nPort = AdsPortOpen();
	long nErr = AdsGetLocalAddress(pAddr);
	if (nErr) std::cerr << "Error: AdsGetLocalAddress: " << nErr << '\n';

	// TwinCAT3 PLC1 = 851
	pAddr->port = 851;

	// Get DLL version
	std::cout << "Checking DLL version..." << '\n';
	long nTemp = AdsGetDllVersion();
	AdsVersion* pDLLVersion = (AdsVersion*)&nTemp;

	// Create JSON string with version info
	std::stringstream dllstrstream;
	dllstrstream << "{\"Version\":" << (int)pDLLVersion->version << ',';
	dllstrstream << "\"Revision\":" << (int)pDLLVersion->revision << ',';
	dllstrstream << "\"Build\":" << pDLLVersion->build << "}";
	std::string DLLVersionStr{ dllstrstream.str() };
	std::cout << DLLVersionStr << '\n';

	// Map for storing symbol/variable definitions
	std::map<std::string, TwinCatVar> symbols{};
	std::map<std::string, TwinCatType> datatypes{};

	// Regulary fetch infromation about symbols/variables
	std::cout << "Starting symbol declaration update thread..." << '\n';
	std::jthread t1([pAddr, &symbols, &datatypes] {
		using namespace std::chrono_literals;
	while (true) {
		auto [nErr, uploadInfo] = getUploadInfo(pAddr);
		if (!nErr) {
			datatypes = getDatatypeMap(pAddr, uploadInfo).second;
			symbols = getSymbolMap(pAddr, uploadInfo, datatypes).second;
		}
		std::this_thread::sleep_for(5s);
	}
		});

	// Outputs DLL version information as json string
	svr.Get("/version", [DLLVersionStr](const httplib::Request& req, httplib::Response& res) {
		res.set_content(DLLVersionStr, "text/json");
		});

	// Gets status of PLC
	svr.Get("/state", [pAddr](const httplib::Request& req, httplib::Response& res) {
		uint16_t nAdsState;
	uint16_t nDeviceState;
	std::stringstream strstream;
	long nErr = AdsSyncReadStateReq(pAddr, &nAdsState, &nDeviceState);
	if (nErr) {
		strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
	}
	else
	{
		strstream << "{\"Ads\":" << nAdsState << ',';
		strstream << "\"Device\":" << nDeviceState << '}';
	}

	res.set_content(strstream.str(), "text/json");
		});

	// Gets device info of ADS server
	svr.Get("/device/info", [pAddr](const httplib::Request& req, httplib::Response& res) {
		char pDevName[50];
	AdsVersion Version{};
	AdsVersion* pVersion = &Version;
	std::stringstream strstream;
	long nErr = AdsSyncReadDeviceInfoReq(pAddr, pDevName, pVersion);
	if (nErr) {
		strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
	}
	else
	{
		strstream << "{\"Name\":\"" << pDevName << "\",";
		strstream << "\"Version\":{";
		strstream << "\"Version\":" << (int)pVersion->version << ',';
		strstream << "\"Revision\":" << (int)pVersion->revision << ',';
		strstream << "\"Build\":" << pVersion->build << "}}";
	}

	res.set_content(strstream.str(), "text/json");
		});

	// Reads data
	svr.Get(R"(/read/(\w+)/(\w+)/(\w+))", [pAddr](const httplib::Request& req, httplib::Response& res) {
		std::vector<std::string> paths = splitPath(req.path);
	unsigned long nIndexGroup = std::stoul(paths.at(2), nullptr, 0);
	unsigned long nIndexOffset = std::stoul(paths.at(3), nullptr, 0);
	uint64_t nLength = std::stoul(paths.at(4), nullptr, 0);
	std::stringstream strstream;
	if (nLength >= 255) {
		strstream << "{\"Error\":\"Max allowed length is 255 bytes.\"}";
	}
	else {
		unsigned char pData[256];

		long nErr = AdsSyncReadReq(pAddr, nIndexGroup, nIndexOffset, sizeof(pData), &pData);
		if (nErr) {
			strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
		}
		else {
			strstream << "{\"Data\":\"";
			for (size_t i = 0; i < nLength; ++i)
				strstream << std::hex << (int)pData[i];
			strstream << "\"}";
		}
	}

	res.set_content(strstream.str(), "text/json");
		});

	// Get handle of variable
	svr.Get(R"(/symbol/((\w|\.)+)/handle)", [pAddr](const httplib::Request& req, httplib::Response& res) {
		std::vector<std::string> paths = splitPath(req.path);
	std::string nameStr = paths.at(2);
	auto [nErr, symHandle] = getSymHandleByName(pAddr, nameStr);
	std::stringstream strstream;
	if (nErr) {
		strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
	}
	else {
		strstream << "{\"Handle\":" << symHandle << "}";
	}

	res.set_content(strstream.str(), "text/json");
		});

	// Get info of all variables
	svr.Get(R"(/symbol)", [pAddr, &symbols](const httplib::Request& req, httplib::Response& res) {
		std::stringstream strstream;
	strstream << "{";
	bool first = true;
	for (const auto& [key, value] : symbols) {
		if (!first) {
			strstream << ",";
		}
		else {
			first = false;
		}
		strstream << "\"" + key + "\":" + value.str();
	}
	strstream << "}";

	res.set_content(strstream.str(), "text/json");
		});

	// Get info of variable
	svr.Get(R"(/symbol/((\w|\.)+))", [pAddr, &symbols](const httplib::Request& req, httplib::Response& res) {
		std::vector<std::string> paths = splitPath(req.path);
	std::string nameStr = paths.at(2);
	std::stringstream strstream;
	if (!symbols.contains(nameStr)) {
		strstream << "{\"Error\":\"Symbol/Variable not found.\",\"ErrorNum\":" << 404 << '}';
	}
	else {
		TwinCatVar variable = symbols[nameStr];
		strstream << variable.str();
	}

	res.set_content(strstream.str(), "text/json");
		});

	svr.Get(R"(/symbol/((\w|\.)+)/value)", [pAddr, &symbols, &datatypes](const httplib::Request& req, httplib::Response& res) {
		std::vector<std::string> paths = splitPath(req.path);
	std::string nameStr = paths.at(2);
	std::stringstream strstream;
	if (!symbols.contains(nameStr)) {
		strstream << "{\"Error\":\"Symbol/Variable not found.\",\"ErrorNum\":" << 404 << '}';
	}
	else {
		TwinCatVar variable = symbols[nameStr];
		auto [nErr, value] = getVariableJSONValue(pAddr, datatypes, variable);
		if (nErr) {
			strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
		}
		else {
			strstream << "{\"Data\":" << value << "}";
		}
	}

	res.set_content(strstream.str(), "text/json");
		});

	svr.Post(R"(/symbol/((\w|\.)+)/value)", [pAddr, &symbols, &datatypes](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
		std::vector<std::string> paths = splitPath(req.path);
	std::string nameStr = paths.at(2);
	std::stringstream strstream;
	if (!symbols.contains(nameStr)) {
		strstream << "{\"Error\":\"Symbol/Variable not found.\",\"ErrorNum\":" << 404 << '}';
	}
	else {
		TwinCatVar variable = symbols[nameStr];
		std::string body;
		content_reader([&](const char* data, size_t data_length) {
			body.append(data, data_length);
		return true;
			});
		auto json = nlohmann::json::parse(body);
		long nErr = setVariableJSONValue(pAddr, datatypes, variable, json["Data"]);
		if (nErr) {
			strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
		}
		else {
			strstream << body;
		}
	}

	res.set_content(strstream.str(), "text/json");
		});

	svr.Post(R"(/state)", [pAddr](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
		std::vector<std::string> paths = splitPath(req.path);
	std::stringstream strstream;
	std::string body;
	content_reader([&](const char* data, size_t data_length) {
		body.append(data, data_length);
	return true;
		});
	auto json = nlohmann::json::parse(body);
	bool readState = false;
	uint16_t nAdsState{};
	uint16_t nDeviceState{};
	long nErr{};
	if (json.contains("Ads")) {
		if (!json["Ads"].is_number_unsigned()) {
			strstream << "{\"Error\":\"adsState must be unsigned integer.\"}";
			res.set_content(strstream.str(), "text/json");
			return;
		}
		nAdsState = json["Ads"].get<uint16_t>();
		if (nAdsState >= ADSSTATE_MAXSTATES) {
			strstream << "{\"Error\":\"Invalid ADSState.\"}";
			res.set_content(strstream.str(), "text/json");
			return;
		}
	}
	else {
		nErr = AdsSyncReadStateReq(pAddr, &nAdsState, &nDeviceState);
		readState = true;
	}
	if (json.contains("Device")) {
		if (!json["Device"].is_number_unsigned()) {
			strstream << "{\"error\":\"deviceState must be unsigned integer.\"}";
			res.set_content(strstream.str(), "text/json");
			return;
		}
		nDeviceState = json["Device"].get<uint16_t>();
	}
	else if (!readState) {
		nErr = AdsSyncReadStateReq(pAddr, nullptr, &nDeviceState);
		readState = true;
	}
	nErr = AdsSyncWriteControlReq(pAddr, nAdsState, nDeviceState, 0, NULL);
	if (nErr) {
		strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
	}
	else {
		strstream << body;
	}

	res.set_content(strstream.str(), "text/json");
		});

	auto port = 1234;
	if (argc > 1) { port = atoi(argv[1]); }
	std::cout << "Listening at port " << port << "..." << '\n';
	svr.listen("localhost", port);

	// Close communication port
	nErr = AdsPortClose();
	if (nErr) std::cerr << "Error: AdsPortClose: " << nErr << '\n';
}
