// ADSBridge.cpp : Defines the entry point for the application.
//
#include "ADSBridge.h"

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

// Store symbol/variable declaration
struct TwinCatVar {
	std::string name;
	ULONG indexGroup;
	ULONG indexOffset;
	ULONG size;
	std::string type;
	std::string comment;
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

// Returns all symbol/variable declarations
auto getSymbolMap(PAmsAddr pAddr) {
	std::map<std::string, TwinCatVar> symbols{};
	AdsSymbolUploadInfo tAdsSymbolUploadInfo;
	long nErr = AdsSyncReadReq(pAddr, ADSIGRP_SYM_UPLOADINFO, 0x0, sizeof(tAdsSymbolUploadInfo), &tAdsSymbolUploadInfo);
	if (nErr) return std::make_pair(nErr, symbols);
	char* pchSymbols = new char[tAdsSymbolUploadInfo.nSymSize];
	nErr = AdsSyncReadReq(pAddr, ADSIGRP_SYM_UPLOAD, 0, tAdsSymbolUploadInfo.nSymSize, pchSymbols);
	if (nErr) return std::make_pair(nErr, symbols);
	PAdsSymbolEntry pAdsSymbolEntry = (PAdsSymbolEntry)pchSymbols;
	for (UINT uiIndex = 0; uiIndex < tAdsSymbolUploadInfo.nSymbols; uiIndex++)
	{
		std::string name{ PADSSYMBOLNAME(pAdsSymbolEntry) };
		ULONG indexGroup = pAdsSymbolEntry->iGroup;
		ULONG indexOffset = pAdsSymbolEntry->iOffs;
		ULONG size = pAdsSymbolEntry->size;
		std::string type{ PADSSYMBOLTYPE(pAdsSymbolEntry) };
		std::string comment{ PADSSYMBOLCOMMENT(pAdsSymbolEntry) };
		symbols[name] = TwinCatVar{ name,indexGroup,indexOffset,size,type,comment };
		pAdsSymbolEntry = PADSNEXTSYMBOLENTRY(pAdsSymbolEntry);
	}
	if (pchSymbols) delete(pchSymbols);
	return std::make_pair(nErr, symbols);
}

// Reads value of symbol/variable and returns JSON string representation
auto getVariableJSONValue(PAmsAddr pAddr, TwinCatVar variable) {
	long nErr{};
	std::string value;
	if (variable.type == "BOOL") {
		auto [err, data] = readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, bool{}, nErr);
		value = (data ? "true" : "false");
	}
	else if (variable.type == "USINT") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, UINT8{}, nErr, value);
	}
	else if (variable.type == "SINT") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, INT8{}, nErr, value);
	}
	else if (variable.type == "UINT" || variable.type == "WORD") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, UINT16{}, nErr, value);
	}
	else if (variable.type == "INT") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, INT16{}, nErr, value);
	}
	else if (variable.type == "UDINT" || variable.type == "DWORD") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, UINT32{}, nErr, value);
	}
	else if (variable.type == "DINT") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, INT32{}, nErr, value);
	}
	else if (variable.type == "ULINT" || variable.type == "LWORD") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, UINT64{}, nErr, value);
	}
	else if (variable.type == "LINT") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, INT64{}, nErr, value);
	}
	else if (variable.type == "REAL") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, float{}, nErr, value);
	}
	else if (variable.type == "LREAL") {
		readGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, double{}, nErr, value);
	}
	else {
		char pData[256];
		nErr = AdsSyncReadReq(pAddr, variable.indexGroup, variable.indexOffset, variable.size, &pData);
		if (!nErr) {
			std::string extendedValue{ pData };
			std::stringstream vstream;
			vstream << "\"" << extendedValue.substr(0, variable.size) << "\"";
			value = vstream.str();
		}
	}
	return std::pair(nErr, value);
}

// Updates symbol/variable based on provided json value
auto setVariableJSONValue(PAmsAddr pAddr, TwinCatVar variable, const nlohmann::json jsonValue) {
	long nErr{};
	std::string value;
	if (variable.type == "BOOL" && jsonValue.is_boolean()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<bool>());
	}
	else if (variable.type == "USINT" && jsonValue.is_number_unsigned()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<uint8_t>());
	}
	else if (variable.type == "SINT" && jsonValue.is_number_integer()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<int8_t>());
	}
	else if ((variable.type == "UINT" || variable.type == "WORD") && jsonValue.is_number_unsigned()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<uint16_t>());
	}
	else if (variable.type == "INT" && jsonValue.is_number_integer()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<int16_t>());
	}
	else if ((variable.type == "UDINT" || variable.type == "DWORD") && jsonValue.is_number_unsigned()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<uint32_t>());
	}
	else if (variable.type == "DINT" && jsonValue.is_number_integer()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<int32_t>());
	}
	else if ((variable.type == "ULINT" || variable.type == "LWORD") && jsonValue.is_number_unsigned()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<uint64_t>());
	}
	else if (variable.type == "LINT" && jsonValue.is_number_integer()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<int64_t>());
	}
	else if (variable.type == "REAL" && jsonValue.is_number_float()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<float>());
	}
	else if (variable.type == "LREAL" && jsonValue.is_number_float()) {
		nErr = writeGroupOffset(pAddr, variable.indexGroup, variable.indexOffset, jsonValue.get<double>());
	}
	else if (variable.type.starts_with("STRING") && jsonValue.is_string()) {
		std::string valueStr = jsonValue.get<std::string>();
		nErr = AdsSyncWriteReq(pAddr, variable.indexGroup, variable.indexOffset, variable.size, valueStr.data());
	}
	else {
		nErr = ADSERR_DEVICE_INVALIDDATA;
	}
	return nErr;
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

	// Regulary fetch infromation about symbols/variables
	std::cout << "Starting symbol declaration update thread..." << '\n';
	std::jthread t1([pAddr, &symbols] {
		using namespace std::chrono_literals;
	while (true) {
		symbols = getSymbolMap(pAddr).second;
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

	svr.Get(R"(/symbol/((\w|\.)+)/value)", [pAddr, &symbols](const httplib::Request& req, httplib::Response& res) {
		std::vector<std::string> paths = splitPath(req.path);
	std::string nameStr = paths.at(2);
	std::stringstream strstream;
	if (!symbols.contains(nameStr)) {
		strstream << "{\"Error\":\"Symbol/Variable not found.\",\"ErrorNum\":" << 404 << '}';
	}
	else {
		TwinCatVar variable = symbols[nameStr];
		auto [nErr, value] = getVariableJSONValue(pAddr, variable);
		if (nErr) {
			strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
		}
		else {
			strstream << "{\"Data\":" << value << "}";
		}
	}

	res.set_content(strstream.str(), "text/json");
		});

	svr.Post(R"(/symbol/((\w|\.)+)/value)", [pAddr, &symbols](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
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
		long nErr = setVariableJSONValue(pAddr, variable, json["Data"]);
		if (nErr) {
			strstream << "{\"Error\":\"ADS request unsuccessful.\",\"ErrorNum\":" << nErr << '}';
		}
		else {
			strstream << body;
		}
	}

	res.set_content(strstream.str(), "text/json");
		});

	svr.Post(R"(/state)", [pAddr, &symbols](const httplib::Request& req, httplib::Response& res, const httplib::ContentReader& content_reader) {
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
