#ifndef QWINTOAST
#define QWINTOAST

#include <QObject>
#include <QtCore>
#include <Windows.h>
#include <sdkddkver.h>
#include <WinUser.h>
#include <ShObjIdl.h>
#include <wrl/implements.h>
#include <wrl/event.h>
#include <windows.ui.notifications.h>
#include <strsafe.h>
#include <Psapi.h>
#include <ShlObj.h>
#include <roapi.h>
#include <propvarutil.h>
#include <functiondiscoverykeys.h>
#include <iostream>
#include <winstring.h>
#include <string.h>
#include <vector>
#include <map>
using namespace Microsoft::WRL;
using namespace ABI::Windows::Data::Xml::Dom;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::UI::Notifications;
using namespace Windows::Foundation;


class QWinToastTemplate
{
public:
    enum class Scenario
    {
        Default,
        Alarm,
        IncomingCall,
        Reminder
    };

    enum Duration
    {
        System,
        Short,
        Long
    };

    enum AudioOption
    {
        Default = 0,
        Silent,
        Loop
    };

    enum TextField
    {
        FirstLine = 0,
        SecondLine = 1,
        ThirdLine = 2
    };

    enum WinToastTemplateType
    {
        ImageAndText01 = ToastTemplateType::ToastTemplateType_ToastImageAndText01,
        ImageAndText02 = ToastTemplateType::ToastTemplateType_ToastImageAndText02,
        ImageAndText03 = ToastTemplateType::ToastTemplateType_ToastImageAndText03,
        ImageAndText04 = ToastTemplateType::ToastTemplateType_ToastImageAndText04,
        Text01 = ToastTemplateType::ToastTemplateType_ToastText01,
        Text02 = ToastTemplateType::ToastTemplateType_ToastText02,
        Text03 = ToastTemplateType::ToastTemplateType_ToastText03,
        Text04 = ToastTemplateType::ToastTemplateType_ToastText04
    };

    enum AudioSystemFile
    {
        DefaultSound,
        IM,
        Mail,
        Reminder,
        SMS,
        Alarm,
        Alarm2,
        Alarm3,
        Alarm4,
        Alarm5,
        Alarm6,
        Alarm7,
        Alarm8,
        Alarm9,
        Alarm10,
        Call,
        Call1,
        Call2,
        Call3,
        Call4,
        Call5,
        Call6,
        Call7,
        Call8,
        Call9,
        Call10
    };

    QWinToastTemplate(_In_ WinToastTemplateType type = WinToastTemplateType::ImageAndText02);
    ~QWinToastTemplate();

    void setFirstLine(_In_ const QString& text);
    void setSecondLine(_In_ const QString& text);
    void setThirdLine(_In_ const QString& text);
    void setTextField(_In_ const QString& text, _In_ TextField pos);
    void setAttributionText(_In_ const QString& attributionText);
    void setImagePath(_In_ const QString& imgPath);
    void setAudioPath(_In_ QWinToastTemplate::AudioSystemFile audio);
    void setAudioPath(_In_ const QString& audioPath);
    void setAudioOption(_In_ QWinToastTemplate::AudioOption audioOption);
    void setDuration(_In_ Duration duration);
    void setExpiration(_In_ INT64 millsecondsFromNow);
    void setScenario(_In_ Scenario scenario);
    void addAction(_In_ const QString& label);

    std::size_t textFieldsCount() const;
    std::size_t actionsCount() const;
    bool hasImage() const;
    const QVector<QString>& textFields() const;
    const QString& textField(_In_ TextField pos) const;
    const QString& actionLabel(_In_ std::size_t pos) const;
    const QString& imagePath() const;
    const QString& audioPath() const;
    const QString& attributionText() const;
    const QString& scenario() const;
    INT64 expiration() const;
    WinToastTemplateType type() const;
    QWinToastTemplate::AudioOption audioOption() const;
    Duration duration() const;

private:
    QVector<QString> _textFields{};
    QVector<QString> _actions{};
    QString _imagePath{};
    QString _audioPath{};
    QString _attributionText{};
    QString _scenario{ "Default" };
    INT64 _expiration{ 0 };
    AudioOption _audioOption{ QWinToastTemplate::AudioOption::Default };
    WinToastTemplateType _type{ WinToastTemplateType::Text01 };
    Duration _duration{ Duration::System };
};

class QWinToast: public QObject
{
    Q_OBJECT
public:
    enum WinToastDismissalReason {
        UserCanceled = ToastDismissalReason::ToastDismissalReason_UserCanceled,
        ApplicationHidden = ToastDismissalReason::ToastDismissalReason_ApplicationHidden,
        TimedOut = ToastDismissalReason::ToastDismissalReason_TimedOut
    };

    enum QWinToastError
    {
        NoError = 0,
        NotInitialized,
        SystemNotSupported,
        ShellLinkNotCreated,
        InvalidAppUserModelID,
        InvalidParameters,
        InvalidHandler,
        NotDisplayed,
        UnknownError
    };

    enum ShortcutResult
    {
        SHORTCUT_UNCHANGED = 0,
        SHORTCUT_WAS_CHANGED = 1,
        SHORTCUT_WAS_CREATED = 2,

        SHORTCUT_MISSING_PARAMETERS = -1,
        SHORTCUT_INCOMPATIBLE_OS = -2,
        SHORTCUT_COM_INIT_FAILURE = -3,
        SHORTCUT_CREATE_FAILED = -4
    };

    enum ShortcutPolicy
    {
        /* Don't check, create, or modify a shortcut. */
        SHORTCUT_POLICY_IGNORE = 0,
        /* Require a shortcut with matching AUMI, don't create or modify an existing one. */
        SHORTCUT_POLICY_REQUIRE_NO_CREATE = 1,
        /* Require a shortcut with matching AUMI, create if missing, modify if not matching.
         * This is the default.
         *
        **/
        SHORTCUT_POLICY_REQUIRE_CREATE = 2,
    };

    QWinToast(QObject* parent = 0);
    virtual ~QWinToast();
    static QWinToast* instance();
    static bool isCompatible();
    static bool isSupportingModernFeatures();
    static QString configureAUMI(_In_ const QString& companyName,
                                 _In_ const QString& product,
                                 _In_ const QString& subProduct,
                                 _In_ const QString& versionInformation);
    static const QString& strerror(_In_ QWinToastError error);
    virtual bool initialize(_Out_opt_ QWinToastError* error = nullptr);
    virtual bool isInitialized() const;
    virtual bool hideToast(_In_ INT64 id);
    virtual INT64 showToast(_In_ const QWinToastTemplate& toast, _Out_opt_ QWinToastError* error = nullptr);
    virtual void clear();
    virtual enum ShortcutResult createShortcut();

    const QString& appName() const;
    const QString& appUserModelId() const;
    void setAppUserModelID(_In_ const QString& aumi);
    void setAppName(_In_ const QString& appName);
    void setShortcutPolicy(_In_ ShortcutPolicy policy);

signals:
    void toastActivated();
    void toastActivated(int actionIndex);
    void toastDismissed(WinToastDismissalReason state);
    void toastFailed();

protected:
    bool _isInitialized{ false };
    bool _hasCoInitialized{ false };
    ShortcutPolicy _shortcutPolicy{ SHORTCUT_POLICY_REQUIRE_CREATE };
    QString _appName{};
    QString _aumi{};
    std::map<INT64, ComPtr<IToastNotification>> _buffer{};

    HRESULT validateShellLinkHelper(_Out_ bool& wasChanged);
    HRESULT createShellLinkHelper();
    HRESULT setImageFieldHelper(_In_ IXmlDocument *xml, _In_ const QString& path);
    HRESULT setAudioFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& path, _In_opt_ QWinToastTemplate::AudioOption option = QWinToastTemplate::AudioOption::Default);
    HRESULT setTextFieldHelper(_In_ IXmlDocument* xml, _In_ const QString& text, _In_ UINT32 POS);
    HRESULT setAttributionTextFieldHelper(_In_ IXmlDocument *xml, _In_ const QString& text);
    HRESULT addActionHelper(_In_ IXmlDocument* xml, _In_ const QString& action, _In_ const QString& arguments);
    HRESULT addDurationHelper(_In_ IXmlDocument* xml, _In_ const QString& duration);
    HRESULT addScenarioHelper(_In_ IXmlDocument* xml, _In_ const QString& scenario);
    ComPtr<IToastNotifier> notifier(_In_ bool* succeded) const;
    void setError(_Out_opt_ QWinToastError* error, _In_ QWinToastError value);
    HRESULT handleEventHandlers(_In_ IToastNotification* notification, _In_ INT64 expirationTime);

};


#endif // QWINTOAST
