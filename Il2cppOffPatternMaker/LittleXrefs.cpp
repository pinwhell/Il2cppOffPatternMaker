#include "LittleXrefs.h"
#include <ShObjIdl.h>

LX::LXERROR LX::MakeLittleXrefs(LX::LittleXrefs** pLXrefs)
{
	*pLXrefs = new LittleXrefs();

	if (!(*pLXrefs)->LoadFiles())
		return LX_FILESLOAD;

	return LX_OK;
}

bool LX::LittleXrefs::LoadFiles()
{
	std::wstring	assemblyFilePath = L"";
	std::wstring	scriptDumpFilePath = L"";
	Utils::LXFile	assemblyFile;
	Utils::LXFile	scriptDumpFile;
	size_t			assemblyFileSize = 0x0;
	size_t			scriptDumpFileSize = 0x0;
	char* assemblyFileRawBuff = nullptr;
	char* scriptDumpFileRawBuff = nullptr;

	// getting necessary files paths

#ifdef ASK_FILES
	if (!Utils::get_assembly_path(assemblyFilePath) || !Utils::get_script_path(scriptDumpFilePath))
		return false;
#else 
	assemblyFilePath = L"libil2cpp.so";
	scriptDumpFilePath = L"script.json";
#endif


	//opening necesary files
	assemblyFile = Utils::LXFile(assemblyFilePath, LXFILE_MODE_IN_BINARY);
	scriptDumpFile = Utils::LXFile(scriptDumpFilePath, LXFILE_MODE_IN_BINARY);

	//handling unopened files
	if (!assemblyFile.isOpen() || !scriptDumpFile.isOpen())
		return false;

	//getting necesary files sizes
	assemblyFileSize = assemblyFile.getFileSize();
	scriptDumpFileSize = scriptDumpFile.getFileSize();

	// handle invalid files sizes
	if (assemblyFileSize == 0 || scriptDumpFileSize == 0)
		return false;

	// allocating memory for reading raw files
	assemblyFileRawBuff = (char*)_aligned_malloc(assemblyFileSize, 4096);
	scriptDumpFileRawBuff = new char[scriptDumpFileSize];

	// handle invalid buffers allocation
	if (!assemblyFileRawBuff || !scriptDumpFileRawBuff)
	{
		if (assemblyFileRawBuff)
			_aligned_free( assemblyFileRawBuff);

		if (scriptDumpFileRawBuff)
			delete[] scriptDumpFileRawBuff;

		return false;
	}

	//reading raw files to buffers and handling read errors
	if (!assemblyFile.ReadFile(assemblyFileRawBuff, assemblyFileSize) || !scriptDumpFile.ReadFile(scriptDumpFileRawBuff, scriptDumpFileSize))
	{
		_aligned_free(assemblyFileRawBuff);
		delete[] scriptDumpFileRawBuff;

		return false;
	}

	//assigning results
	m_AssemblyBuffEntry = (unsigned char*)assemblyFileRawBuff;
	m_AssemblyBuffSize = assemblyFileSize;

	//parse json cstr to json object  and handle errors
	if (!Utils::cstr_to_json_obj(scriptDumpFileRawBuff, m_ScriptJsonObj))
	{
		_aligned_free(assemblyFileRawBuff);
		delete[] scriptDumpFileRawBuff;

		return false;
	}

	// freeing raw buffers
	delete[] scriptDumpFileRawBuff;

	return true;
}

Json::Value& LX::LittleXrefs::getDumpJsonObj()
{
	return m_ScriptJsonObj;
}

unsigned char* LX::LittleXrefs::getAssemblyEntry()
{
	return m_AssemblyBuffEntry;
}

uintptr_t LX::LittleXrefs::getAssemblySize()
{
	return m_AssemblyBuffSize;
}

LX::LittleXrefs::LittleXrefs()
{

}

LX::LittleXrefs::~LittleXrefs()
{
	delete[] m_AssemblyBuffEntry;
	m_ScriptJsonObj.clear();
}

bool create_path_dialog(std::wstring& result, const COMDLG_FILTERSPEC* type, uintptr_t size)
{
	bool success = false;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED |
		COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		IFileOpenDialog* pFileOpen;


		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
			IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr))
		{

			pFileOpen->SetFileTypes(size, type);

			// Show the Open dialog box.
			hr = pFileOpen->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr))
			{
				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);
				if (SUCCEEDED(hr))
				{
					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					// Display the file name to the user.
					if (SUCCEEDED(hr))
					{
						success = true;
						result = std::wstring(pszFilePath);
						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}

	return success;
}

bool LX::Utils::get_assembly_path(std::wstring& out_path)
{
	const COMDLG_FILTERSPEC assemblyExt[] = {
		{L"libil2cpp (*.so)",       L"*.so"},
		{L"GameAssembly (*.dll)",    L"*.dll"},
		{L"All (*)",    L"*.*"}
	};

	return create_path_dialog(out_path, assemblyExt, ARRAYSIZE(assemblyExt));
}

bool LX::Utils::get_script_path(std::wstring& out_path)
{
	const COMDLG_FILTERSPEC dumpScriptExt[] =
	{
		{L"il2cppScript (*.json)",    L"*.json"}
	};

	return create_path_dialog(out_path, dumpScriptExt, ARRAYSIZE(dumpScriptExt));
}

bool LX::Utils::cstr_to_json_obj(const char* json_char_buff, Json::Value& json_obj)
{
	Json::String errs_jstr;
	Json::CharReaderBuilder builder{};
	auto reader = builder.newCharReader();

	return reader->parse(json_char_buff, json_char_buff + strlen(json_char_buff), &json_obj, &errs_jstr);
}

LX::Utils::LXFile::LXFile()
{
}

LX::Utils::LXFile::LXFile(const std::wstring& path, uintptr_t mode) :
	m_FileStream(new std::fstream(path, mode))
{
}

LX::Utils::LXFile::~LXFile()
{
}

bool LX::Utils::LXFile::isOpen()
{
	return m_FileStream->is_open();
}

size_t LX::Utils::LXFile::getFileSize()
{
	uintptr_t fileSize = 0;

	if (m_FileStream->seekg(0, std::ios_base::end))
	{
		fileSize = (uintptr_t)m_FileStream->tellg();

		if (!m_FileStream->seekg(0, std::ios_base::beg))
			return 0;
	}

	return fileSize;
}

bool LX::Utils::LXFile::ReadFile(void* buff, uintptr_t buffSize)
{
	uintptr_t fileSize = getFileSize();

	//handle buff out of space
	if (fileSize > buffSize)
		return false;

	//handle bad reading
	if (!m_FileStream->read((char*)buff, fileSize))
		return false;

	return true;
}