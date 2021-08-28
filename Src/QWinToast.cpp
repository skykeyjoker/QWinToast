#include "QWinToast.h"
#include <memory>
#include <assert.h>
#include <unordered_map>
#include <array>
#include <QDebug>

#pragma comment(lib,"shlwapi")
#pragma comment(lib,"user32")

#ifdef NDEBUG
#define DEBUG_MSG(str) do { } while ( false )
#else
#define DEBUG_MSG(str) do { std::wcout << str << std::endl; } while( false )
#endif

#define DEFAULT_SHELL_LINKS_PATH	L"\\Microsoft\\Windows\\Start Menu\\Programs\\"
#define DEFAULT_LINK_FORMAT			L".lnk"
#define STATUS_SUCCESS (0x00000000)


// Quickstart: Handling toast activations from Win32 apps in Windows 10
// https://blogs.msdn.microsoft.com/tiles_and_toasts/2015/10/16/quickstart-handling-toast-activations-from-win32-apps-in-windows-10/
namespace DllImporter
{
	// Function load a function from library
	template <typename Function>
	HRESULT loadFunctionFromLibrary(HINSTANCE library, LPCSTR name, Function& func)
	{
		if (!library)
		{
			return E_INVALIDARG;
		}
		func = reinterpret_cast<Function>(GetProcAddress(library, name));
		return (func != nullptr) ? S_OK : E_FAIL;
	}

	using f_SetCurrentProcessExplicitAppUserModelID = HRESULT(FAR STDAPICALLTYPE*)(__in PCWSTR AppID);
	using f_PropVariantToString = HRESULT(FAR STDAPICALLTYPE*)(
		_In_ REFPROPVARIANT propvar, _Out_writes_(cch) PWSTR psz, _In_ UINT cch);
	using f_RoGetActivationFactory = HRESULT(FAR STDAPICALLTYPE*)(_In_ HSTRING activatableClassId, _In_ REFIID iid,
	                                                              _COM_Outptr_ void** factory);
	using f_WindowsCreateStringReference = HRESULT(FAR STDAPICALLTYPE*)(
		_In_reads_opt_(length + 1) PCWSTR sourceString, UINT32 length, _Out_ HSTRING_HEADER* hstringHeader,
		_Outptr_result_maybenull_ _Result_nullonfailure_ HSTRING* string);
	using f_WindowsGetStringRawBuffer = PCWSTR(FAR STDAPICALLTYPE*)(_In_ HSTRING string, _Out_opt_ UINT32* length);
	using f_WindowsDeleteString = HRESULT(FAR STDAPICALLTYPE*)(_In_opt_ HSTRING string);

	static f_SetCurrentProcessExplicitAppUserModelID SetCurrentProcessExplicitAppUserModelID;
	static f_PropVariantToString PropVariantToString;
	static f_RoGetActivationFactory RoGetActivationFactory;
	static f_WindowsCreateStringReference WindowsCreateStringReference;
	static f_WindowsGetStringRawBuffer WindowsGetStringRawBuffer;
	static f_WindowsDeleteString WindowsDeleteString;


	template <class T>
	_Check_return_ __inline HRESULT _1_GetActivationFactory(_In_ HSTRING activatableClassId, _COM_Outptr_ T** factory)
	{
		return RoGetActivationFactory(activatableClassId, IID_INS_ARGS(factory));
	}

	template <typename T>
	inline HRESULT Wrap_GetActivationFactory(_In_ HSTRING activatableClassId,
	                                         _Inout_ Details::ComPtrRef<T> factory) noexcept
	{
		return _1_GetActivationFactory(activatableClassId, factory.ReleaseAndGetAddressOf());
	}

	inline HRESULT initialize()
	{
		HINSTANCE LibShell32 = LoadLibraryW(L"SHELL32.DLL");
		HRESULT hr = loadFunctionFromLibrary(LibShell32, "SetCurrentProcessExplicitAppUserModelID",
		                                     SetCurrentProcessExplicitAppUserModelID);
		if (SUCCEEDED(hr))
		{
			HINSTANCE LibPropSys = LoadLibraryW(L"PROPSYS.DLL");
			hr = loadFunctionFromLibrary(LibPropSys, "PropVariantToString", PropVariantToString);
			if (SUCCEEDED(hr))
			{
				HINSTANCE LibComBase = LoadLibraryW(L"COMBASE.DLL");
				const bool succeded = SUCCEEDED(
						loadFunctionFromLibrary(LibComBase, "RoGetActivationFactory", RoGetActivationFactory))
					&& SUCCEEDED(
						loadFunctionFromLibrary(LibComBase, "WindowsCreateStringReference", WindowsCreateStringReference
						))
					&& SUCCEEDED(
						loadFunctionFromLibrary(LibComBase, "WindowsGetStringRawBuffer", WindowsGetStringRawBuffer))
					&& SUCCEEDED(loadFunctionFromLibrary(LibComBase, "WindowsDeleteString", WindowsDeleteString));
				return succeded ? S_OK : E_FAIL;
			}
		}
		return hr;
	}
}

class WinToastStringWrapper
{
public:
	WinToastStringWrapper(_In_reads_(length) PCWSTR stringRef, _In_ UINT32 length) noexcept
	{
		HRESULT hr = DllImporter::WindowsCreateStringReference(stringRef, length, &_header, &_hstring);
		if (!SUCCEEDED(hr))
		{
			RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
		}
	}

	WinToastStringWrapper(_In_ const std::wstring& stringRef) noexcept
	{
		HRESULT hr = DllImporter::WindowsCreateStringReference(stringRef.c_str(),
		                                                       static_cast<UINT32>(stringRef.length()), &_header,
		                                                       &_hstring);
		if (FAILED(hr))
		{
			RaiseException(static_cast<DWORD>(STATUS_INVALID_PARAMETER), EXCEPTION_NONCONTINUABLE, 0, nullptr);
		}
	}

	~WinToastStringWrapper()
	{
		DllImporter::WindowsDeleteString(_hstring);
	}

	inline HSTRING Get() const noexcept
	{
		return _hstring;
	}

private:
	HSTRING _hstring;
	HSTRING_HEADER _header;
};

class InternalDateTime : public IReference<DateTime>
{
public:
	static INT64 Now()
	{
		FILETIME now;
		GetSystemTimeAsFileTime(&now);
		return ((((INT64)now.dwHighDateTime) << 32) | now.dwLowDateTime);
	}

	InternalDateTime(DateTime dateTime) : _dateTime(dateTime)
	{
	}

	InternalDateTime(INT64 millisecondsFromNow)
	{
		_dateTime.UniversalTime = Now() + millisecondsFromNow * 10000;
	}

	virtual ~InternalDateTime() = default;

	operator INT64()
	{
		return _dateTime.UniversalTime;
	}

	HRESULT STDMETHODCALLTYPE get_Value(DateTime* dateTime) override
	{
		*dateTime = _dateTime;
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(const IID& riid, void** ppvObject) override
	{
		if (!ppvObject)
		{
			return E_POINTER;
		}
		if (riid == __uuidof(IUnknown) || riid == __uuidof(IReference<DateTime>))
		{
			*ppvObject = static_cast<IUnknown*>(static_cast<IReference<DateTime>*>(this));
			return S_OK;
		}
		return E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		return 1;
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return 2;
	}

	HRESULT STDMETHODCALLTYPE GetIids(ULONG*, IID**) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING*) override
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel*) override
	{
		return E_NOTIMPL;
	}

protected:
	DateTime _dateTime;
};

namespace Util
{
	typedef LONG NTSTATUS, *PNTSTATUS;
	using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

	inline RTL_OSVERSIONINFOW getRealOSVersion()
	{
		HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
		if (hMod)
		{
			auto fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
			if (fxPtr != nullptr)
			{
				RTL_OSVERSIONINFOW rovi = {0};
				rovi.dwOSVersionInfoSize = sizeof(rovi);
				if (STATUS_SUCCESS == fxPtr(&rovi))
				{
					return rovi;
				}
			}
		}
		RTL_OSVERSIONINFOW rovi = {0};
		return rovi;
	}

	inline HRESULT defaultExecutablePath(_In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH)
	{
		DWORD written = GetModuleFileNameExW(GetCurrentProcess(), nullptr, path, nSize);
		DEBUG_MSG("Default executable path: " << path);
		return (written > 0) ? S_OK : E_FAIL;
	}


	inline HRESULT defaultShellLinksDirectory(_In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH)
	{
		DWORD written = GetEnvironmentVariableW(L"APPDATA", path, nSize);
		HRESULT hr = written > 0 ? S_OK : E_INVALIDARG;
		if (SUCCEEDED(hr))
		{
			errno_t result = wcscat_s(path, nSize, DEFAULT_SHELL_LINKS_PATH);
			hr = (result == 0) ? S_OK : E_INVALIDARG;
			DEBUG_MSG("Default shell link path: " << path);
		}
		return hr;
	}

	inline HRESULT defaultShellLinkPath(const std::wstring& appname, _In_ WCHAR* path, _In_ DWORD nSize = MAX_PATH)
	{
		HRESULT hr = defaultShellLinksDirectory(path, nSize);
		if (SUCCEEDED(hr))
		{
			const std::wstring appLink(appname + DEFAULT_LINK_FORMAT);
			errno_t result = wcscat_s(path, nSize, appLink.c_str());
			hr = (result == 0) ? S_OK : E_INVALIDARG;
			DEBUG_MSG("Default shell link file path: " << path);
		}
		return hr;
	}


	inline PCWSTR AsString(ComPtr<IXmlDocument>& xmlDocument)
	{
		HSTRING xml;
		ComPtr<IXmlNodeSerializer> ser;
		HRESULT hr = xmlDocument.As<IXmlNodeSerializer>(&ser);
		hr = ser->GetXml(&xml);
		if (SUCCEEDED(hr))
			return DllImporter::WindowsGetStringRawBuffer(xml, nullptr);
		return nullptr;
	}

	inline PCWSTR AsString(HSTRING hstring)
	{
		return DllImporter::WindowsGetStringRawBuffer(hstring, nullptr);
	}

	inline HRESULT setNodeStringValue(const std::wstring& string, IXmlNode* node, IXmlDocument* xml)
	{
		ComPtr<IXmlText> textNode;
		HRESULT hr = xml->CreateTextNode(WinToastStringWrapper(string).Get(), &textNode);
		if (SUCCEEDED(hr))
		{
			ComPtr<IXmlNode> stringNode;
			hr = textNode.As(&stringNode);
			if (SUCCEEDED(hr))
			{
				ComPtr<IXmlNode> appendedChild;

				qDebug() << "setNodeStringValue: " << string;

				hr = node->AppendChild(stringNode.Get(), &appendedChild);
			}
		}
		return hr;
	}

	inline HRESULT addAttribute(_In_ IXmlDocument* xml, const std::wstring& name, IXmlNamedNodeMap* attributeMap)
	{
		ComPtr<ABI::Windows::Data::Xml::Dom::IXmlAttribute> srcAttribute;
		HRESULT hr = xml->CreateAttribute(WinToastStringWrapper(name).Get(), &srcAttribute);
		if (SUCCEEDED(hr))
		{
			ComPtr<IXmlNode> node;
			hr = srcAttribute.As(&node);
			if (SUCCEEDED(hr))
			{
				ComPtr<IXmlNode> pNode;
				hr = attributeMap->SetNamedItem(node.Get(), &pNode);
			}
		}
		return hr;
	}

	inline HRESULT createElement(_In_ IXmlDocument* xml, _In_ const std::wstring& root_node,
	                             _In_ const std::wstring& element_name,
	                             _In_ const std::vector<std::wstring>& attribute_names)
	{
		ComPtr<IXmlNodeList> rootList;
		HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(root_node).Get(), &rootList);
		if (SUCCEEDED(hr))
		{
			ComPtr<IXmlNode> root;
			hr = rootList->Item(0, &root);
			if (SUCCEEDED(hr))
			{
				ComPtr<ABI::Windows::Data::Xml::Dom::IXmlElement> audioElement;
				hr = xml->CreateElement(WinToastStringWrapper(element_name).Get(), &audioElement);
				if (SUCCEEDED(hr))
				{
					ComPtr<IXmlNode> audioNodeTmp;
					hr = audioElement.As(&audioNodeTmp);
					if (SUCCEEDED(hr))
					{
						ComPtr<IXmlNode> audioNode;
						hr = root->AppendChild(audioNodeTmp.Get(), &audioNode);
						if (SUCCEEDED(hr))
						{
							ComPtr<IXmlNamedNodeMap> attributes;
							hr = audioNode->get_Attributes(&attributes);
							if (SUCCEEDED(hr))
							{
								for (const auto& it : attribute_names)
								{
									hr = addAttribute(xml, it, attributes.Get());
								}
							}
						}
					}
				}
			}
		}
		return hr;
	}
}


QWinToastTemplate::QWinToastTemplate(WinToastTemplateType type) :
	_type(type)
{
	static constexpr std::size_t TextFieldCount[] = { 1, 2, 2, 3, 1, 2, 2, 3 };
	_textFields = QVector<QString>(TextFieldCount[type]);
}

QWinToastTemplate::~QWinToastTemplate()
{
	_textFields.clear();
}

void QWinToastTemplate::setFirstLine(const QString& text)
{
	setTextField(text, QWinToastTemplate::FirstLine);
}

void QWinToastTemplate::setSecondLine(const QString& text)
{
	setTextField(text, QWinToastTemplate::SecondLine);
}

void QWinToastTemplate::setThirdLine(const QString& text)
{
	setTextField(text, QWinToastTemplate::ThirdLine);
}

void QWinToastTemplate::setTextField(const QString& text, TextField pos)
{
	const auto position = static_cast<std::size_t>(pos);
	assert(position < _textFields.size());
	_textFields[position] = text;
}

void QWinToastTemplate::setAttributionText(const QString& attributionText)
{
	_attributionText = attributionText;
}

void QWinToastTemplate::setImagePath(const QString& imgPath)
{
	_imagePath = imgPath;
}

void QWinToastTemplate::setAudioPath(QWinToastTemplate::AudioSystemFile audio)
{
	static const QHash<AudioSystemFile, QString> Files = {
		{AudioSystemFile::DefaultSound, "ms-winsoundevent:Notification.Default"},
		{AudioSystemFile::IM, "ms-winsoundevent:Notification.IM"},
		{AudioSystemFile::Mail, "ms-winsoundevent:Notification.Mail"},
		{AudioSystemFile::Reminder, "ms-winsoundevent:Notification.Reminder"},
		{AudioSystemFile::SMS, "ms-winsoundevent:Notification.SMS"},
		{AudioSystemFile::Alarm, "ms - winsoundevent:Notification.Looping.Alarm"},
		{AudioSystemFile::Alarm2, "ms-winsoundevent:Notification.Looping.Alarm2"},
		{AudioSystemFile::Alarm3, "ms-winsoundevent:Notification.Looping.Alarm3"},
		{AudioSystemFile::Alarm4, "ms-winsoundevent:Notification.Looping.Alarm4"},
		{AudioSystemFile::Alarm5, "ms-winsoundevent:Notification.Looping.Alarm5"},
		{AudioSystemFile::Alarm6, "ms-winsoundevent:Notification.Looping.Alarm6"},
		{AudioSystemFile::Alarm7, "ms-winsoundevent:Notification.Looping.Alarm7"},
		{AudioSystemFile::Alarm8, "ms-winsoundevent:Notification.Looping.Alarm8"},
		{AudioSystemFile::Alarm9, "ms-winsoundevent:Notification.Looping.Alarm9"},
		{AudioSystemFile::Alarm10, "ms-winsoundevent:Notification.Looping.Alarm10"},
		{AudioSystemFile::Call, "ms-winsoundevent:Notification.Looping.Call"},
		{AudioSystemFile::Call1, "ms-winsoundevent:Notification.Looping.Call1"},
		{AudioSystemFile::Call2, "ms-winsoundevent:Notification.Looping.Call2"},
		{AudioSystemFile::Call3, "ms-winsoundevent:Notification.Looping.Call3"},
		{AudioSystemFile::Call4, "ms-winsoundevent:Notification.Looping.Call4"},
		{AudioSystemFile::Call5, "ms-winsoundevent:Notification.Looping.Call5"},
		{AudioSystemFile::Call6, "ms-winsoundevent:Notification.Looping.Call6"},
		{AudioSystemFile::Call7, "ms-winsoundevent:Notification.Looping.Call7"},
		{AudioSystemFile::Call8, "ms-winsoundevent:Notification.Looping.Call8"},
		{AudioSystemFile::Call9, "ms-winsoundevent:Notification.Looping.Call9"},
		{AudioSystemFile::Call10, "ms-winsoundevent:Notification.Looping.Call10"},
	};
	const auto iter = Files.find(audio);
	assert(iter != Files.end());
	_audioPath = iter.value();
}

void QWinToastTemplate::setAudioPath(const QString& audioPath)
{
	_audioPath = audioPath;
}

void QWinToastTemplate::setAudioOption(QWinToastTemplate::AudioOption audioOption)
{
	_audioOption = audioOption;
}

void QWinToastTemplate::setDuration(Duration duration)
{
	_duration = duration;
}

void QWinToastTemplate::setExpiration(INT64 millsecondsFromNow)
{
	_expiration = expiration();
}

void QWinToastTemplate::setScenario(Scenario scenario)
{
	switch (scenario) {
	case Scenario::Default: _scenario = "Default"; break;
	case Scenario::Alarm: _scenario = "Alarm"; break;
	case Scenario::IncomingCall: _scenario = "IncomingCall"; break;
	case Scenario::Reminder: _scenario = "Reminder"; break;
	}
}

void QWinToastTemplate::addAction(const QString& label)
{
	_actions.push_back(label);
}

std::size_t QWinToastTemplate::textFieldsCount() const
{
	return _textFields.size();
}

std::size_t QWinToastTemplate::actionsCount() const
{
	return _actions.size();
}

bool QWinToastTemplate::hasImage() const
{
	return _type < QWinToastTemplate::Text01;
}

const QVector<QString>& QWinToastTemplate::textFields() const
{
	return _textFields;
}

const QString& QWinToastTemplate::textField(TextField pos) const
{
	const auto position = static_cast<std::size_t>(pos);
	assert(position < _textFields.size());
	return _textFields[position];
}

const QString& QWinToastTemplate::actionLabel(std::size_t pos) const
{
	assert(pos < _actions.size());
	return _actions[pos];
}

const QString& QWinToastTemplate::imagePath() const
{
	return _imagePath;
}

const QString& QWinToastTemplate::audioPath() const
{
	return _audioPath;
}

const QString& QWinToastTemplate::attributionText() const
{
	return _attributionText;
}

const QString& QWinToastTemplate::scenario() const
{
	return _scenario;
}

INT64 QWinToastTemplate::expiration() const
{
	return _expiration;
}

QWinToastTemplate::WinToastTemplateType QWinToastTemplate::type() const
{
	return _type;
}

QWinToastTemplate::AudioOption QWinToastTemplate::audioOption() const
{
	return _audioOption;
}

QWinToastTemplate::Duration QWinToastTemplate::duration() const
{
	return _duration;
}

QWinToast* QWinToast::instance()
{
	static QWinToast instance;
	return &instance;
}

QWinToast::QWinToast(QObject* parent) :
	QObject(parent),
	_isInitialized(false),
	_hasCoInitialized(false)
{
	if (!isCompatible())
	{
		DEBUG_MSG(L"Warning: Your system is not compatible with this library ");
	}
}

QWinToast::~QWinToast()
{
	if(_hasCoInitialized)
	{
		CoUninitialize();
	}
}

void QWinToast::setAppName(const QString& appName)
{
	_appName = appName;
}

void QWinToast::setAppUserModelID(const QString& aumi)
{
	_aumi = aumi;
	qDebug() << "Default App User Model Id: " << _aumi;
}

void QWinToast::setShortcutPolicy(ShortcutPolicy policy)
{
	_shortcutPolicy = policy;
}

bool QWinToast::isCompatible()
{
	DllImporter::initialize();
	return !((DllImporter::SetCurrentProcessExplicitAppUserModelID == nullptr)
		|| (DllImporter::PropVariantToString == nullptr)
		|| (DllImporter::RoGetActivationFactory == nullptr)
		|| (DllImporter::WindowsCreateStringReference == nullptr)
		|| (DllImporter::WindowsDeleteString == nullptr));
}

bool QWinToast::isSupportingModernFeatures()
{
	constexpr auto MinimumSupportedVersion = 6;
	return Util::getRealOSVersion().dwMajorVersion > MinimumSupportedVersion;
}

QString QWinToast::configureAUMI(const QString& companyName, const QString& product, const QString& subProduct, const QString& versionInformation)
{
	QString aumi = companyName;
	aumi += product;
	if(!subProduct.isEmpty())
	{
		aumi += subProduct;
		if (!versionInformation.isEmpty())
			aumi += versionInformation;
	}

	if(aumi.toStdWString().length() > SCHAR_MAX)
	{
		qDebug() << "Error: max size allowed for AUMI: 128 characters.";
	}

	return aumi;
}

const QString& QWinToast::strerror(QWinToastError error)
{
	static const QHash<QWinToastError, QString> Labels = {
		{QWinToastError::NoError, "No error. The process was executed correctly"},
		{QWinToastError::NotInitialized, "The library has not been initialized"},
		{QWinToastError::SystemNotSupported, "The OS does not support WinToast"},
		{QWinToastError::ShellLinkNotCreated, "The library was not able to create a Shell Link for the app"},
		{QWinToastError::InvalidAppUserModelID, "The AUMI is not a valid one"},
		{QWinToastError::InvalidParameters, "The parameters used to configure the library are not valid normally because an invalid AUMI or App Name"},
		{QWinToastError::NotDisplayed, "The toast was created correctly but WinToast was not able to display the toast"},
		{QWinToastError::UnknownError, "Unknown error"}
	};

	const auto iter = Labels.find(error);
	assert(iter != Labels.end());
	return iter.value();
}

enum QWinToast::ShortcutResult QWinToast::createShortcut() {
	if (_aumi.isEmpty() || _appName.isEmpty()) {
		DEBUG_MSG(L"Error: App User Model Id or Appname is empty!");
		return SHORTCUT_MISSING_PARAMETERS;
	}

	if (!isCompatible()) {
		DEBUG_MSG(L"Your OS is not compatible with this library! =(");
		return SHORTCUT_INCOMPATIBLE_OS;
	}

	if (!_hasCoInitialized) {
		HRESULT initHr = CoInitializeEx(nullptr, COINIT::COINIT_MULTITHREADED);
		if (initHr != RPC_E_CHANGED_MODE) {
			if (FAILED(initHr) && initHr != S_FALSE) {
				DEBUG_MSG(L"Error on COM library initialization!");
				return SHORTCUT_COM_INIT_FAILURE;
			}
			else {
				_hasCoInitialized = true;
			}
		}
	}

	bool wasChanged;
	HRESULT hr = validateShellLinkHelper(wasChanged);
	if (SUCCEEDED(hr))
		return wasChanged ? SHORTCUT_WAS_CHANGED : SHORTCUT_UNCHANGED;

	hr = createShellLinkHelper();
	return SUCCEEDED(hr) ? SHORTCUT_WAS_CREATED : SHORTCUT_CREATE_FAILED;
}

bool QWinToast::initialize(_Out_opt_ QWinToastError* error) {
	_isInitialized = false;
	setError(error, QWinToastError::NoError);

	if (!isCompatible()) {
		setError(error, QWinToastError::SystemNotSupported);
		DEBUG_MSG(L"Error: system not supported.");
		return false;
	}


	if (_aumi.isEmpty() || _appName.isEmpty()) {
		setError(error, QWinToastError::InvalidParameters);
		DEBUG_MSG(L"Error while initializing, did you set up a valid AUMI and App name?");
		return false;
	}

	if (_shortcutPolicy != SHORTCUT_POLICY_IGNORE) {
		if (createShortcut() < 0) {
			setError(error, QWinToastError::ShellLinkNotCreated);
			DEBUG_MSG(L"Error while attaching the AUMI to the current proccess =(");
			return false;
		}
	}

	if (FAILED(DllImporter::SetCurrentProcessExplicitAppUserModelID(_aumi.toStdWString().c_str()))) {
		setError(error, QWinToastError::InvalidAppUserModelID);
		DEBUG_MSG(L"Error while attaching the AUMI to the current proccess =(");
		return false;
	}

	_isInitialized = true;
	return _isInitialized;
}

bool QWinToast::isInitialized() const {
	return _isInitialized;
}

const QString& QWinToast::appName() const {
	return _appName;
}

const QString& QWinToast::appUserModelId() const {
	return _aumi;
}

HRESULT	QWinToast::validateShellLinkHelper(_Out_ bool& wasChanged) {
	WCHAR	path[MAX_PATH] = { L'\0' };
	Util::defaultShellLinkPath(_appName.toStdWString(), path);
	// Check if the file exist
	DWORD attr = GetFileAttributesW(path);
	if (attr >= 0xFFFFFFF) {
		DEBUG_MSG("Error, shell link not found. Try to create a new one in: " << path);
		return E_FAIL;
	}

	// Let's load the file as shell link to validate.
	// - Create a shell link
	// - Create a persistant file
	// - Load the path as data for the persistant file
	// - Read the property AUMI and validate with the current
	// - Review if AUMI is equal.
	ComPtr<IShellLink> shellLink;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
	if (SUCCEEDED(hr)) {
		ComPtr<IPersistFile> persistFile;
		hr = shellLink.As(&persistFile);
		if (SUCCEEDED(hr)) {
			hr = persistFile->Load(path, STGM_READWRITE);
			if (SUCCEEDED(hr)) {
				ComPtr<IPropertyStore> propertyStore;
				hr = shellLink.As(&propertyStore);
				if (SUCCEEDED(hr)) {
					PROPVARIANT appIdPropVar;
					hr = propertyStore->GetValue(PKEY_AppUserModel_ID, &appIdPropVar);
					if (SUCCEEDED(hr)) {
						WCHAR AUMI[MAX_PATH];
						hr = DllImporter::PropVariantToString(appIdPropVar, AUMI, MAX_PATH);
						wasChanged = false;
						if (FAILED(hr) || _aumi != AUMI) {
							if (_shortcutPolicy == SHORTCUT_POLICY_REQUIRE_CREATE) {
								// AUMI Changed for the same app, let's update the current value! =)
								wasChanged = true;
								PropVariantClear(&appIdPropVar);
								hr = InitPropVariantFromString(_aumi.toStdWString().c_str(), &appIdPropVar);
								if (SUCCEEDED(hr)) {
									hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
									if (SUCCEEDED(hr)) {
										hr = propertyStore->Commit();
										if (SUCCEEDED(hr) && SUCCEEDED(persistFile->IsDirty())) {
											hr = persistFile->Save(path, TRUE);
										}
									}
								}
							}
							else {
								// Not allowed to touch the shortcut to fix the AUMI
								hr = E_FAIL;
							}
						}
						PropVariantClear(&appIdPropVar);
					}
				}
			}
		}
	}
	return hr;
}


HRESULT	QWinToast::createShellLinkHelper() {
	if (_shortcutPolicy != SHORTCUT_POLICY_REQUIRE_CREATE) {
		return E_FAIL;
	}

	WCHAR   exePath[MAX_PATH]{ L'\0' };
	WCHAR	slPath[MAX_PATH]{ L'\0' };
	Util::defaultShellLinkPath(_appName.toStdWString(), slPath);
	Util::defaultExecutablePath(exePath);
	ComPtr<IShellLinkW> shellLink;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
	if (SUCCEEDED(hr)) {
		hr = shellLink->SetPath(exePath);
		if (SUCCEEDED(hr)) {
			hr = shellLink->SetArguments(L"");
			if (SUCCEEDED(hr)) {
				hr = shellLink->SetWorkingDirectory(exePath);
				if (SUCCEEDED(hr)) {
					ComPtr<IPropertyStore> propertyStore;
					hr = shellLink.As(&propertyStore);
					if (SUCCEEDED(hr)) {
						PROPVARIANT appIdPropVar;
						hr = InitPropVariantFromString(_aumi.toStdWString().c_str(), &appIdPropVar);
						if (SUCCEEDED(hr)) {
							hr = propertyStore->SetValue(PKEY_AppUserModel_ID, appIdPropVar);
							if (SUCCEEDED(hr)) {
								hr = propertyStore->Commit();
								if (SUCCEEDED(hr)) {
									ComPtr<IPersistFile> persistFile;
									hr = shellLink.As(&persistFile);
									if (SUCCEEDED(hr)) {
										hr = persistFile->Save(slPath, TRUE);
									}
								}
							}
							PropVariantClear(&appIdPropVar);
						}
					}
				}
			}
		}
	}
	return hr;
}

INT64 QWinToast::showToast(_In_ const QWinToastTemplate& toast, _Out_ QWinToastError* error) {
	setError(error, QWinToastError::NoError);
	INT64 id = -1;
	if (!isInitialized()) {
		setError(error, QWinToastError::NotInitialized);
		DEBUG_MSG("Error when launching the toast. WinToast is not initialized.");
		return id;
	}

	ComPtr<IToastNotificationManagerStatics> notificationManager;
	HRESULT hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
	if (SUCCEEDED(hr)) {
		ComPtr<IToastNotifier> notifier;
		hr = notificationManager->CreateToastNotifierWithId(WinToastStringWrapper(_aumi.toStdWString()).Get(), &notifier);
		if (SUCCEEDED(hr)) {
			ComPtr<IToastNotificationFactory> notificationFactory;
			hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), &notificationFactory);
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlDocument> xmlDocument;
				HRESULT hr = notificationManager->GetTemplateContent(ToastTemplateType(toast.type()), &xmlDocument);
				if (SUCCEEDED(hr)) {
					for (UINT32 i = 0, fieldsCount = static_cast<UINT32>(toast.textFieldsCount()); i < fieldsCount && SUCCEEDED(hr); i++) {

						qDebug() << "hr = setTextFieldHelper(xmlDocument.Get(), toast.textField(QWinToastTemplate::TextField(i)), i);" << toast.textField(QWinToastTemplate::TextField(i)) << i;
						qDebug() << toast.type();
						hr = setTextFieldHelper(xmlDocument.Get(), toast.textField(QWinToastTemplate::TextField(i)), i);
						//qDebug() << toast.textField(0) << toast.textField(1) << toast.textField(2);
						//hr = setTextFieldHelper(xmlDocument.Get(), toast.textField(QWinToastTemplate::TextField(0)), 0);
						//hr = setTextFieldHelper(xmlDocument.Get(), toast.textField(QWinToastTemplate::TextField(1)), 1);
					}

					// Modern feature are supported Windows > Windows 10
					if (SUCCEEDED(hr) && isSupportingModernFeatures()) {

						// Note that we do this *after* using toast.textFieldsCount() to
						// iterate/fill the template's text fields, since we're adding yet another text field.
						if (SUCCEEDED(hr)
							&& !toast.attributionText().isEmpty()) {
							hr = setAttributionTextFieldHelper(xmlDocument.Get(), toast.attributionText());
						}

						std::array<WCHAR, 12> buf;
						for (std::size_t i = 0, actionsCount = toast.actionsCount(); i < actionsCount && SUCCEEDED(hr); i++) {
							_snwprintf_s(buf.data(), buf.size(), _TRUNCATE, L"%zd", i);
							hr = addActionHelper(xmlDocument.Get(), toast.actionLabel(i), QString::fromWCharArray(buf.data()));
						}

						if (SUCCEEDED(hr)) {
							hr = (toast.audioPath().isEmpty() && toast.audioOption() == QWinToastTemplate::AudioOption::Default)
								? hr : setAudioFieldHelper(xmlDocument.Get(), toast.audioPath(), toast.audioOption());
						}

						if (SUCCEEDED(hr) && toast.duration() != QWinToastTemplate::Duration::System) {
							hr = addDurationHelper(xmlDocument.Get(),
								(toast.duration() == QWinToastTemplate::Duration::Short) ? "short" : "long");
						}

						if (SUCCEEDED(hr)) {
							hr = addScenarioHelper(xmlDocument.Get(), toast.scenario());
						}

					}
					else {
						DEBUG_MSG("Modern features (Actions/Sounds/Attributes) not supported in this os version");
					}

					if (SUCCEEDED(hr)) {
						hr = toast.hasImage() ? setImageFieldHelper(xmlDocument.Get(), toast.imagePath()) : hr;
						if (SUCCEEDED(hr)) {
							ComPtr<IToastNotification> notification;
							hr = notificationFactory->CreateToastNotification(xmlDocument.Get(), &notification);
							if (SUCCEEDED(hr)) {
								INT64 expiration = 0, relativeExpiration = toast.expiration();
								if (relativeExpiration > 0) {
									InternalDateTime expirationDateTime(relativeExpiration);
									expiration = expirationDateTime;
									hr = notification->put_ExpirationTime(&expirationDateTime);
								}

								if (SUCCEEDED(hr)) {
									hr = handleEventHandlers(notification.Get(), expiration);
									if (FAILED(hr)) {
										setError(error, QWinToastError::InvalidHandler);
									}
								}

								if (SUCCEEDED(hr)) {
									GUID guid;
									hr = CoCreateGuid(&guid);
									if (SUCCEEDED(hr)) {
										id = guid.Data1;
										_buffer[id] = notification;
										DEBUG_MSG("xml: " << Util::AsString(xmlDocument));
										hr = notifier->Show(notification.Get());
										if (FAILED(hr)) {
											setError(error, QWinToastError::NotDisplayed);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return FAILED(hr) ? -1 : id;
}

ComPtr<IToastNotifier> QWinToast::notifier(_In_ bool* succeded) const {
	ComPtr<IToastNotificationManagerStatics> notificationManager;
	ComPtr<IToastNotifier> notifier;
	HRESULT hr = DllImporter::Wrap_GetActivationFactory(WinToastStringWrapper(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), &notificationManager);
	if (SUCCEEDED(hr)) {
		hr = notificationManager->CreateToastNotifierWithId(WinToastStringWrapper(_aumi.toStdWString()).Get(), &notifier);
	}
	*succeded = SUCCEEDED(hr);
	return notifier;
}

bool QWinToast::hideToast(_In_ INT64 id) {
	if (!isInitialized()) {
		DEBUG_MSG("Error when hiding the toast. WinToast is not initialized.");
		return false;
	}

	if (_buffer.find(id) != _buffer.end()) {
		auto succeded = false;
		auto notify = notifier(&succeded);
		if (succeded) {
			auto result = notify->Hide(_buffer[id].Get());
			_buffer.erase(id);
			return SUCCEEDED(result);
		}
	}
	return false;
}

void QWinToast::clear() {
	auto succeded = false;
	auto notify = notifier(&succeded);
	if (succeded) {
		auto end = _buffer.end();
		for (auto it = _buffer.begin(); it != end; ++it) {
			notify->Hide(it->second.Get());
		}
		_buffer.clear();
	}
}

//
// Available as of Windows 10 Anniversary Update
// Ref: https://docs.microsoft.com/en-us/windows/uwp/design/shell/tiles-and-notifications/adaptive-interactive-toasts
//
// NOTE: This will add a new text field, so be aware when iterating over
//       the toast's text fields or getting a count of them.
//
HRESULT QWinToast::setAttributionTextFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& text) {
	Util::createElement(xml, L"binding", L"text", { L"placement" });
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"text").Get(), &nodeList);
	if (SUCCEEDED(hr)) {
		UINT32 nodeListLength;
		hr = nodeList->get_Length(&nodeListLength);
		if (SUCCEEDED(hr)) {
			for (UINT32 i = 0; i < nodeListLength; i++) {
				ComPtr<IXmlNode> textNode;
				hr = nodeList->Item(i, &textNode);
				if (SUCCEEDED(hr)) {
					ComPtr<IXmlNamedNodeMap> attributes;
					hr = textNode->get_Attributes(&attributes);
					if (SUCCEEDED(hr)) {
						ComPtr<IXmlNode> editedNode;
						if (SUCCEEDED(hr)) {
							hr = attributes->GetNamedItem(WinToastStringWrapper(L"placement").Get(), &editedNode);
							if (FAILED(hr) || !editedNode) {
								continue;
							}
							hr = Util::setNodeStringValue(L"attribution", editedNode.Get(), xml);
							if (SUCCEEDED(hr)) {
								return setTextFieldHelper(xml, text, i);
							}
						}
					}
				}
			}
		}
	}
	return hr;
}

HRESULT QWinToast::addDurationHelper(_In_ IXmlDocument* xml, _In_ const QString& duration) {
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"toast").Get(), &nodeList);
	if (SUCCEEDED(hr)) {
		UINT32 length;
		hr = nodeList->get_Length(&length);
		if (SUCCEEDED(hr)) {
			ComPtr<IXmlNode> toastNode;
			hr = nodeList->Item(0, &toastNode);
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlElement> toastElement;
				hr = toastNode.As(&toastElement);
				if (SUCCEEDED(hr)) {
					hr = toastElement->SetAttribute(WinToastStringWrapper(L"duration").Get(),
						WinToastStringWrapper(duration.toStdWString()).Get());
				}
			}
		}
	}
	return hr;
}

HRESULT QWinToast::addScenarioHelper(_In_ IXmlDocument* xml, _In_ const QString& scenario) {
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"toast").Get(), &nodeList);
	if (SUCCEEDED(hr)) {
		UINT32 length;
		hr = nodeList->get_Length(&length);
		if (SUCCEEDED(hr)) {
			ComPtr<IXmlNode> toastNode;
			hr = nodeList->Item(0, &toastNode);
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlElement> toastElement;
				hr = toastNode.As(&toastElement);
				if (SUCCEEDED(hr)) {
					hr = toastElement->SetAttribute(WinToastStringWrapper(L"scenario").Get(),
						WinToastStringWrapper(scenario.toStdWString()).Get());
				}
			}
		}
	}
	return hr;
}

HRESULT QWinToast::setTextFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& text, _In_ UINT32 pos) {
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"text").Get(), &nodeList);

	UINT32 ret;
	nodeList.Get()->get_Length(&ret);
	qDebug() << "ret: " << ret;

	if (SUCCEEDED(hr)) {
		ComPtr<IXmlNode> node;
		hr = nodeList->Item(pos, &node);
		if (!node)
			qDebug() << text << pos << "node is empty";
		if (SUCCEEDED(hr)) {
			hr = Util::setNodeStringValue(text.toStdWString(), node.Get(), xml);
		}
	}
	return hr;
}


HRESULT QWinToast::setImageFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& path) {
	assert(path.size() < MAX_PATH);

	wchar_t imagePath[MAX_PATH] = L"file:///";
	HRESULT hr = StringCchCatW(imagePath, MAX_PATH, path.toStdWString().c_str());
	if (SUCCEEDED(hr)) {
		ComPtr<IXmlNodeList> nodeList;
		HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"image").Get(), &nodeList);
		if (SUCCEEDED(hr)) {
			ComPtr<IXmlNode> node;
			hr = nodeList->Item(0, &node);
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlNamedNodeMap> attributes;
				hr = node->get_Attributes(&attributes);
				if (SUCCEEDED(hr)) {
					ComPtr<IXmlNode> editedNode;
					hr = attributes->GetNamedItem(WinToastStringWrapper(L"src").Get(), &editedNode);
					if (SUCCEEDED(hr)) {
						Util::setNodeStringValue(imagePath, editedNode.Get(), xml);
					}
				}
			}
		}
	}
	return hr;
}

HRESULT QWinToast::setAudioFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& path, _In_opt_ QWinToastTemplate::AudioOption option) {
	std::vector<std::wstring> attrs;
	if (!path.isEmpty()) attrs.push_back(L"src");
	if (option == QWinToastTemplate::AudioOption::Loop) attrs.push_back(L"loop");
	if (option == QWinToastTemplate::AudioOption::Silent) attrs.push_back(L"silent");
	Util::createElement(xml, L"toast", L"audio", attrs);

	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"audio").Get(), &nodeList);
	if (SUCCEEDED(hr)) {
		ComPtr<IXmlNode> node;
		hr = nodeList->Item(0, &node);
		if (SUCCEEDED(hr)) {
			ComPtr<IXmlNamedNodeMap> attributes;
			hr = node->get_Attributes(&attributes);
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlNode> editedNode;
				if (!path.isEmpty()) {
					if (SUCCEEDED(hr)) {
						hr = attributes->GetNamedItem(WinToastStringWrapper(L"src").Get(), &editedNode);
						if (SUCCEEDED(hr)) {
							hr = Util::setNodeStringValue(path.toStdWString(), editedNode.Get(), xml);
						}
					}
				}

				if (SUCCEEDED(hr)) {
					switch (option) {
					case QWinToastTemplate::AudioOption::Loop:
						hr = attributes->GetNamedItem(WinToastStringWrapper(L"loop").Get(), &editedNode);
						if (SUCCEEDED(hr)) {
							hr = Util::setNodeStringValue(L"true", editedNode.Get(), xml);
						}
						break;
					case QWinToastTemplate::AudioOption::Silent:
						hr = attributes->GetNamedItem(WinToastStringWrapper(L"silent").Get(), &editedNode);
						if (SUCCEEDED(hr)) {
							hr = Util::setNodeStringValue(L"true", editedNode.Get(), xml);
						}
					default:
						break;
					}
				}
			}
		}
	}
	return hr;
}

HRESULT QWinToast::addActionHelper(_In_ IXmlDocument* xml, _In_ const QString& content, _In_ const QString& arguments) {
	ComPtr<IXmlNodeList> nodeList;
	HRESULT hr = xml->GetElementsByTagName(WinToastStringWrapper(L"actions").Get(), &nodeList);
	if (SUCCEEDED(hr)) {
		UINT32 length;
		hr = nodeList->get_Length(&length);
		if (SUCCEEDED(hr)) {
			ComPtr<IXmlNode> actionsNode;
			if (length > 0) {
				hr = nodeList->Item(0, &actionsNode);
			}
			else {
				hr = xml->GetElementsByTagName(WinToastStringWrapper(L"toast").Get(), &nodeList);
				if (SUCCEEDED(hr)) {
					hr = nodeList->get_Length(&length);
					if (SUCCEEDED(hr)) {
						ComPtr<IXmlNode> toastNode;
						hr = nodeList->Item(0, &toastNode);
						if (SUCCEEDED(hr)) {
							ComPtr<IXmlElement> toastElement;
							hr = toastNode.As(&toastElement);
							if (SUCCEEDED(hr))
								hr = toastElement->SetAttribute(WinToastStringWrapper(L"template").Get(), WinToastStringWrapper(L"ToastGeneric").Get());
							if (SUCCEEDED(hr))
								hr = toastElement->SetAttribute(WinToastStringWrapper(L"duration").Get(), WinToastStringWrapper(L"long").Get());
							if (SUCCEEDED(hr)) {
								ComPtr<IXmlElement> actionsElement;
								hr = xml->CreateElement(WinToastStringWrapper(L"actions").Get(), &actionsElement);
								if (SUCCEEDED(hr)) {
									hr = actionsElement.As(&actionsNode);
									if (SUCCEEDED(hr)) {
										ComPtr<IXmlNode> appendedChild;
										hr = toastNode->AppendChild(actionsNode.Get(), &appendedChild);
									}
								}
							}
						}
					}
				}
			}
			if (SUCCEEDED(hr)) {
				ComPtr<IXmlElement> actionElement;
				hr = xml->CreateElement(WinToastStringWrapper(L"action").Get(), &actionElement);
				if (SUCCEEDED(hr))
					hr = actionElement->SetAttribute(WinToastStringWrapper(L"content").Get(), WinToastStringWrapper(content.toStdWString()).Get());
				if (SUCCEEDED(hr))
					hr = actionElement->SetAttribute(WinToastStringWrapper(L"arguments").Get(), WinToastStringWrapper(arguments.toStdWString()).Get());
				if (SUCCEEDED(hr)) {
					ComPtr<IXmlNode> actionNode;
					hr = actionElement.As(&actionNode);
					if (SUCCEEDED(hr)) {
						ComPtr<IXmlNode> appendedChild;
						hr = actionsNode->AppendChild(actionNode.Get(), &appendedChild);
					}
				}
			}
		}
	}
	return hr;
}

void QWinToast::setError(_Out_opt_ QWinToastError* error, _In_ QWinToastError value) {
	if (error) {
		*error = value;
	}
}

HRESULT QWinToast::handleEventHandlers(IToastNotification* notification, INT64 expirationTime)
{
	EventRegistrationToken activatedToken, dismissedToken, failedToken;
	HRESULT hr = notification->add_Activated(
		Callback<Implements<RuntimeClassFlags<ClassicCom>,
		ITypedEventHandler<ToastNotification*, IInspectable*>>>(
			[this](IToastNotification*, IInspectable* inspectable)
			{
				IToastActivatedEventArgs* activatedEventArgs;
				HRESULT hr = inspectable->QueryInterface(&activatedEventArgs);
				if (SUCCEEDED(hr))
				{
					HSTRING argumentsHandle;
					hr = activatedEventArgs->get_Arguments(&argumentsHandle);
					if (SUCCEEDED(hr))
					{
						PCWSTR arguments = Util::AsString(argumentsHandle);
						if (arguments && *arguments)
						{
							emit toastActivated(static_cast<int>(wcstol(arguments, nullptr, 10)));
							qDebug() << "emit toastActivated(static_cast<int>(wcstol(arguments, nullptr, 10)));";
							//eventHandler->toastActivated(static_cast<int>(wcstol(arguments, nullptr, 10)));
							return S_OK;
						}
					}
				}
				emit toastActivated();
				qDebug() << "emit toastActivated();";
				//eventHandler->toastActivated();
				return S_OK;
			}).Get(), &activatedToken);

	if (SUCCEEDED(hr))
	{
		hr = notification->add_Dismissed(Callback<Implements<RuntimeClassFlags<ClassicCom>,
			ITypedEventHandler<
			ToastNotification*, ToastDismissedEventArgs*>>>(
				[this, expirationTime](
					IToastNotification*, IToastDismissedEventArgs* e)
				{
					ToastDismissalReason reason;
					if (SUCCEEDED(e->get_Reason(&reason)))
					{
						if (reason == ToastDismissalReason_UserCanceled &&
							expirationTime && InternalDateTime::Now() >=
							expirationTime)
							reason = ToastDismissalReason_TimedOut;
						emit toastDismissed(
							static_cast<WinToastDismissalReason>(
								reason));
						qDebug() << "emit toastDismissed";
						//eventHandler->toastDismissed(
						//	static_cast<IWinToastHandler::WinToastDismissalReason>(
						//		reason));
					}
					return S_OK;
				}).Get(), &dismissedToken);
		if (SUCCEEDED(hr))
		{
			hr = notification->add_Failed(Callback<Implements<RuntimeClassFlags<ClassicCom>,
				ITypedEventHandler<
				ToastNotification*, ToastFailedEventArgs*>>>(
					[this](IToastNotification*, IToastFailedEventArgs*)
					{
						emit toastFailed();
						//eventHandler->toastFailed();
						return S_OK;
					}).Get(), &failedToken);
		}
	}
	return hr;
}
