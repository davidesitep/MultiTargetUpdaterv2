#pragma once

#include <QtWidgets/QMainWindow>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression> // Replace QRegex with QRegularExpression
#include "ui_multitargetupdaterv2.h"
#include "logger.h"
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qcryptographichash.h>
#include <qxmlstream.h>
#include <QStandardPaths>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/aes.h>

const int VERSION_SIZE = 64;
const int DATA_SIZE = 16;
const int ORA_SIZE = 16;

/**
* @brief MultiTargetUpdaterv2 è una classe che gestisce l'aggiornamento di più dispositivi tramite un 'interfaccia grafica.
* La classe permette di connettersi ai dispositivi, scaricare le configurazioni, verificare le versioni del firmware installato 
* e disponibile, e gestire l'aggiornamento del firmware.
* Utilizza WinSCP per la gestione dei file remoti e OpenSSL per la crittografia e decrittografia dei dati.
* La classe include anche funzionalità per l'interazione con l'utente tramite un interfaccia gradica.
* @note Questa classe è progettata in ambiente Qt e utilizza le librerie OpenSSl per la criptografia e il client WinSCP per l'interazione
* con i dispositivi remoti e con il server FTP contenente i firmware.
* @author Topolino
* @date 2025-07-07
* @version 1.0
*/

class MultiTargetUpdaterv2 : public QMainWindow
{
    Q_OBJECT

public:
    MultiTargetUpdaterv2(QWidget* parent = nullptr);
    ~MultiTargetUpdaterv2();



private:
    Ui::MultiTargetUpdaterv2Class ui;
    logger* log;
    bool isConnected = false;
    bool enteringBl = false;
    bool ready = false;
    bool updateProcess = false;
    QString exeAppPath = QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QString mainLogDirPath = exeAppPath + "/log/";
    QTimer* enterBlTimer;
    QTimer* netconfTransfTimer;
    QTimer* downloadInstalledFwTimer;
    int retryCount;
    const int MAX_RETRIES = 9;
    const int RETRY_INTERVAL_MS = 1200; // 1 secondo
    const QString hostName = "192.168.1.80"; // Indirizzo IP del dispositivo
    const QString userName = "SitepAdministrator";
    const QString password = "SecureUpdate2015";
    const QString port = "7356";

    const QString updateHostName = "192.168.1.19"; // Indirizzo IP del dispositivo
    const QString updateUserName = "SitepUpdatePack";
    const QString updatePassword = "SecureUpdate2016";
    const QString updatePort = "21";
    const QString packType = "*";

    const unsigned char rawKeyBytes[32] = { 1, 253, 5, 60, 52, 91, 193, 133, 193, 121, 221, 164, 57, 128, 91, 91, 19, 39, 111, 197, 125, 98, 89, 48, 97, 154, 83, 187, 222, 167, 171, 74 }; // La tua chiave AES (es. 16, 24 o 32 byte)

    const unsigned char rawIvBytes[16] = { 10, 51, 235, 120, 122, 120, 80, 248, 13, 182, 196, 212, 176, 46, 23, 85 }; // Il tuo IV (sempre 16 byte per AES)

    // Nuovo: Percorsi per il file da scaricare
    const QString remoteNetconfPath = "/netconf.bin"; // Percorso remoto del file
    const QString localNetconfPath = "netconf.bin";   // Nome del file locale
    QString listFilesLogPath;
    bool listWidgetAlive;

#pragma pack(push, 1) // Assicura che la struttura non abbia padding tra i membri
    struct NetConf {
        uint32_t Magic;
        uint8_t val_IP_ADDR[4];
        uint8_t val_NETMASK_ADDR[4];
        uint8_t val_GW_ADDR[4];
        uint8_t val_MAC_ADDR[6];
        uint8_t m2_1;
        uint8_t m2_2;
        uint32_t CRC;
    };
#pragma pack(pop)

    NetConf Configurazione;

    struct swInfo {
        QString version;
        QString data;
        QString ora;
        QString deviceName;
        QString fileLocation;
    };
    QList<swInfo> installedFwInfos;
    QList<swInfo> downloadedFwInfos;

    struct remoteProject {
        QString updateHostName;
        QString updateUserName;
        QString updatePassword;
        QString configurationName;
        QString port;
    };
    QList<remoteProject> remoteProjects;

    struct deviceFwInfo {
        QString deviceName;
        QString installedVersion;
        QString installedData;
        QString installedOra;
        QString availableVersion;
        QString availableData;
        QString availableOra;
		QString fileLocation;
        bool updated = false; // Nuovo campo per tenere traccia dello stato di aggiornamento
    };
    QList<deviceFwInfo> deviceFwInfos; // Lista dei firmware installati sui dispositivi


    void enterBootloader();
    bool writeScriptFile(const QString& script, const QString& path);
    void downLoadNetConfig();
    void readNetConfig(const QString& netConfPath);
    void checkInstalledfw();
    void readFwVersions(const QString& dataPath);
    int readFwVersion(QString filePath, swInfo& info);
    void updateFwTableWidget();
    void compareSwInfos(QList<swInfo>& existingInfos, QList<swInfo> newInfos);
    void downloadFwList();
    void readConfList(QString confListPath);
    void updateAvailableSystemList();
    QString DecryptString(const QString& src);
    void handleOpenSSLError(const QString& context);
    void downloadRemoteFw();
    QString hash_generator(QString algorithm, QString filepath);
    void unzipFolder(QString localTempFolder);
    void readLocalFwVersion(const QString& filesPath);
    void updateFirmware(QString deviceName, QString path);
    void checkUpdate(QString updateFileName, QString binFolder);
    void cleanLogsView();

signals:
    void enableActionButton(QString deviceName);
    void exportlog_signal(const QString destinationDriPath);


private slots:
    void on_pushButtonClicked_enterBlMode();
    void onWinSCPErrorOccurred(QProcess::ProcessError error);
    void retryWinSCPProcess(); // Slot per il timer di ritentativo
    void on_pushButtonclicked_checkInstalledFw();
    void on_pushButtonClicked_downloadSystemList();
    void on_pushButtonClicked_downloadFwforCurrentSystem();
    void on_pushButtonClicked_loadFromLocalFolder();
    void on_pushButtonClicked_startApplication();
    void on_pushButtonClicked_exportLog();
	void printLogMessage(const QString& message);

};

