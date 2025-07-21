#include "multitargetupdaterv2.h"


MultiTargetUpdaterv2::MultiTargetUpdaterv2(QWidget* parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	listWidgetAlive = true;
	// Impostazione di tableWidget
	ui.tableWidget->setColumnCount(4);
	QStringList headers;
	headers << "NOME" << "FIRMWARE\nINSTALLATO" << "FIRMWARE DA\nINSTALLARE" << "AZIONE";
	ui.tableWidget->setHorizontalHeaderLabels(headers);
	// Deve adattarsi al contenuto di ogni colonna anche riducendo la dimensione del testo contenuto
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	ui.tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	ui.tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui.tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui.tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
	// Devo attaccare i segnali per la gestione dei pulsanti di azione


	enterBlTimer = new QTimer(this);
	netconfTransfTimer = new QTimer(this);
	downloadInstalledFwTimer = new QTimer(this);

	connect(ui.btn_enterBootloader, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_enterBlMode);
	connect(ui.btn_downloadInstaledFws, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonclicked_checkInstalledFw);
	connect(ui.btn_downloadSystemList, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_downloadSystemList);
	connect(ui.btn_updateFwInfo, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_downloadFwforCurrentSystem);
	connect(ui.btn_loadFromlocal, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_loadFromLocalFolder);
	connect(ui.btn_startApplication, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_startApplication);
	connect(enterBlTimer, &QTimer::timeout, this, &MultiTargetUpdaterv2::retryWinSCPProcess);
	connect(netconfTransfTimer, &QTimer::timeout, this, &MultiTargetUpdaterv2::retryWinSCPProcess);
	connect(downloadInstalledFwTimer, &QTimer::timeout, this, &MultiTargetUpdaterv2::retryWinSCPProcess);
	connect(ui.lw_logList, &QObject::destroyed, this, [this]() {
		listWidgetAlive = false;
	});
	// Creo il logger per la mia applicazione
	log = new logger(mainLogDirPath);
	if (log->logFileCreated) {
		connect(ui.lw_logList, &LogListWidget::itemAdded, log, &logger::log);
		connect(ui.btn_exportLog, &QPushButton::clicked, this, &MultiTargetUpdaterv2::on_pushButtonClicked_exportLog);
		connect(this, &MultiTargetUpdaterv2::exportlog_signal, log, &logger::exportlog);
		connect(log, &logger::printLogMessage, this, &MultiTargetUpdaterv2::printLogMessage);
	}

}

MultiTargetUpdaterv2::~MultiTargetUpdaterv2()
{
	// Qui devo fermare eventuali timer attivi e processi in esecuzione
	if (enterBlTimer->isActive()) {
		enterBlTimer->stop();
	}
	if (netconfTransfTimer->isActive()) {
		netconfTransfTimer->stop();
	}
	if (downloadInstalledFwTimer->isActive()) {
		downloadInstalledFwTimer->stop();
	}
	// E rimuovo tutti i file temporanei creati
	QDir tempDir(exeAppPath + "/data/temp/");
	if (tempDir.exists()) {
		QString tempPath = tempDir.path();
		QStringList directories = tempDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
		for (const QString& dirName : directories) {
			QDir subDir(tempPath + "/" + dirName);
			if (subDir.exists()) {
				subDir.removeRecursively();
			}
		}
	}

	// Rimuovo tutti i file txt creati 
	QDir txtDir(QCoreApplication::applicationDirPath());
	if (txtDir.exists()) {
		QStringList txtFiles = txtDir.entryList(QStringList() << "*.txt", QDir::Files);
		for (const QString& fileName : txtFiles) {
			QFile::remove(txtDir.filePath(fileName));
		}
	}
}

/**
* @function writeScripFile
* @brief Scrive uno script WinSCP in file specificato.
* @param script stringa contente il contenuto dello script da scrivere
* @param path stringa contenente il percorso del file in cui scrivere lo script.
* @return true se il file è stato scritto correttamente, false altrimenti.
* @details Questa funzione apre un file in modalità scrittura e scrive il contenuto dello script in esso.
* @see QFile, QTestStrem 
* @note Assicurarsi che il percorso del file sia valido e che l'applicazione abbia i permessi di scrittura nella directory specificata.
*/
bool MultiTargetUpdaterv2::writeScriptFile(const QString& script, const QString& path)
{
	QFile file(path);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QTextStream out(&file);
		out << script;
		file.close();
		qDebug() << "Script file written successfully.";
		return true;
	}
	else
	{
		qDebug() << "Failed to open script file for writing.";
		return false;
	}
}

/*
 * @slot on_pushButtonClicked_enterBlMode
 * @brief Gestisce il click del pulsante per entrare in modalità bootloader.
 * @param 
 * @retunr void
 * @details Questa funzione pulisce la vista dei log, verifica se il pulsante è selezionato e chiama la funzione per entrare in modalità bootloader.
 * Bisogna tenere conto che i dispositivi hanno una piccola finestra temporale dopo l'accensione in cui sono responsivi a questo comando prima di avviare il processo di boot.
 * @see enterBootloader()
 */
void MultiTargetUpdaterv2::on_pushButtonClicked_enterBlMode()
{
	cleanLogsView();

	if (ui.btn_enterBootloader->isChecked()) {
		enteringBl = true;
		// Entro nel loop di connessione che attiva la modalità bootloader del dispositivo
		enterBootloader();
		ui.btn_enterBootloader->setChecked(true);

		if (listWidgetAlive) ui.lw_logList->addItem("Entering bootloader mode...");
		// qDebug() << "Modalità connessione abilitata.";
	}
	else {
		enteringBl = false;
		ui.btn_enterBootloader->setChecked(false);
		if (listWidgetAlive) ui.lw_logList->addItem("Entering bootloader manualy stopped.");
		// qDebug() << "Modalità connessione disabilitata.";
	}
}

/*
 * @funztion enterBootloader
 * @brief crea un processo WinSCP per connettersi al dispositivo e attivare la modalità bootloader.
 * Crea le stringhe per lo script e il log (winscp), crea lo script  e lo scrive su file di testo.
 * Crea e avvia il processo WinSCP con i parametri necessari e connette i metodi per gestire la fine processo e gli errori.
 * Se il processo termina con successo, imposta isConnected a true e chiama la funzione per scaricare il file di configurazione di rete, altrimenti gestisce il retry.
 * @param void
 * @return void
 * @details Questa funzione è responsabile della connessione al dispositivo tramite WinSCP e dell'attivazione della modalità bootloader di quest'ultimo.
 * @see writeScriptFile(), onWinSCPErrorOccurred(), retryWinSCPProcess()
 */
void MultiTargetUpdaterv2::enterBootloader()
{
	isConnected = false;

	QString scriptFileName = "winscp_script.txt";
	QString scriptFilePath = exeAppPath + "/" + scriptFileName;

	QString logFileName = "winscp_connection_log.txt"; // Nome specifico per il log di connessione
	QString logFilePath = exeAppPath + "/" + logFileName;

	QString script;
	script = QString("open ftp://%1:%2@%3:%4/\n"
		"close\n"
		"exit\n"
	).arg(
		userName,
		password,
		hostName,
		port
	);

	if (!writeScriptFile(script, scriptFilePath)) {
		if (listWidgetAlive) ui.lw_logList->addItem(QString("Failed to write script file %1 for WinSCP.").arg(scriptFilePath));
		return;
	}

	QProcess* enterBlProcess = new QProcess(this);

	// Connetti i segnali agli slot
	connect(enterBlProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
			// Criterio di successo: processo terminato normalmente e codice di uscita 0
			if (!listWidgetAlive) 
				return;

			if (exitStatus == QProcess::NormalExit && exitCode == 0) {
				isConnected = true;
				retryCount = 0;
				if (listWidgetAlive) ui.lw_logList->addItem("WinSCP process finished successfully. Device in bootloader mode");
				downLoadNetConfig();
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("Failed to enter bootloader mode. Code: " + QString::number(exitCode) + " Status: " + QString::number(static_cast<int>(exitStatus)));

				if (!isConnected && enteringBl && retryCount < MAX_RETRIES)
				{
					retryCount++;
					enterBlTimer->start();
					if (listWidgetAlive) ui.lw_logList->addItem("Attempt to reconnect" + QString::number(retryCount) + " in " + QString::number(RETRY_INTERVAL_MS) + "ms due to start-up error...");
				}
				else if (enteringBl && retryCount >= MAX_RETRIES)
				{
					ui.btn_enterBootloader->setChecked(false);
					retryCount = 0;
					if (listWidgetAlive) ui.lw_logList->addItem("Maximum number of reconnection attempts reached. Connection failed.");
				}
				else if (!enterBlTimer)
				{
					ui.btn_enterBootloader->setChecked(false);
					retryCount = 0;
					if (listWidgetAlive) ui.lw_logList->addItem("Entering bootloader mode manually disabled.");
				}
			}
			enterBlProcess->deleteLater();
		});

	connect(enterBlProcess, &QProcess::errorOccurred,
		this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	if (enterBlTimer->isActive()) {
		enterBlTimer->stop();
	}

	// Avvia il processo WinSCP
	if (listWidgetAlive) ui.lw_logList->addItem("WinSCP process started");
	enterBlProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath);

}


/*
 * @slot onWinSCPErrorOccurred
 * @brief Gestisce gli errori andando a individuarne il tipo al termine del processo. Imposta a false isConnected e ready.
 * @param error QProcess::ProcessError che indica il tipo di errore che si è verificato.
 * @return void
 * @details Questa funzione viene chiamata quando si verifica un errore durante l'esecuzione del processo WinSCP.
 * @see QProcess::ProcessError, QProcess::errorOccurred
 */
void MultiTargetUpdaterv2::onWinSCPErrorOccurred(QProcess::ProcessError error)
{
	if (listWidgetAlive) {
		ui.lw_logList->addItem("Errore di avvio di WinSCP: " + error);
	}

	switch (error) {
	case QProcess::FailedToStart: ui.lw_logList->addItem("Errore: WinSCP.exe non trovato. Controlla il percorso. Percorso corrente " + QCoreApplication::applicationFilePath()); break;
	case QProcess::Crashed: qDebug() << "Errore: WinSCP è crashato."; break;
	case QProcess::Timedout: qDebug() << "Errore: Timeout nell'avvio di WinSCP."; break;
	case QProcess::ReadError: qDebug() << "Errore: Errore di lettura dal processo."; break;
	case QProcess::WriteError: qDebug() << "Errore: Errore di scrittura al processo."; break;
	case QProcess::UnknownError: qDebug() << "Errore: Errore sconosciuto."; break;
	default: break;
	}
	isConnected = false;
	ready = false;

}

/**
 * @slot retryWinSCPProcess
 * @brief Gestisce il retry di connessione allo scadere del timer.
 * Questa funzione viene chiamata quando il QTimer 'retryTimer' scade,
 * indicando che è il momento di tentare nuovamente il processo WinSCP.
 * Incrementa il contatore dei tentativi e richiama la funzione in base al timer che scatta.
 * @param void
 * @return void
 * @details Questa funzione è responsabile della gestione deella riesecuzione dei alcuni processi WinSCP in caso di errore o timeot.
 * @see enterBootLoader(), downLoadNetCOnfig(), checkInstalledfs(), retryCount, MAX_RETRIES, RETRY_INTERVAL_MS
 */
void MultiTargetUpdaterv2::retryWinSCPProcess() {
	// Devo capire quale timer ha chiamato questa funzione
	if (enterBlTimer->isActive()) {
		enterBlTimer->stop();
		enterBootloader();
	}
	else if (netconfTransfTimer->isActive()) {
		netconfTransfTimer->stop(); // Ferma il timer per questo tentativo
		downLoadNetConfig();
	}
	else if (downloadInstalledFwTimer->isActive()) {
		downloadInstalledFwTimer->stop();
		checkInstalledfw();
	}
}

/**
 * @function downLoadNetConfig
 * @brief Questa funzione gestisce il download del file di configurazione di rete del dispositivo attraverso un processo WinSCP.
 * @param void
 * @return void
 * @details Questa funzione crea uno script per WinSCP che si connette al dispositivo e scarica il file di configurazione di rete 'netconf.bin'.
 * @see writeScriptFile(), readNetConfig(), onWinSCPErrorOccurred(), retryWinSCPProcess()
 */
void MultiTargetUpdaterv2::downLoadNetConfig()
{
	QString localDestinationPath = QDir::toNativeSeparators(exeAppPath + "/data/temp/netconfig/");
	QString localFilePath = localDestinationPath + "netconf.bin";

	QDir dir(localDestinationPath);
	if (!dir.exists()) {
		if (dir.mkpath(localDestinationPath)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Local temp folder created.");
		}
		else {
			if (listWidgetAlive) ui.lw_logList->addItem("Impossible to create local temp folder. Process terminated");
			return;
		}
	}

	QString script;
	script = QString(
		"open ftp://%1:%2@%3:%4/\n"
		"get /netconf.bin \"%5\"\n"
		"close\n"
		"exit\n"
	).arg(
		userName,
		password,
		hostName,
		port,
		QDir::toNativeSeparators(localFilePath) // Assicura il percorso corretto per Windows
	);

	QString scriptFilePath = exeAppPath + "/netconfig_transfer_script.txt";

	QString transferLogFilePath = exeAppPath + "/netconfig_transfer_log.txt";

	if (!writeScriptFile(script, scriptFilePath)) {
		if (listWidgetAlive) ui.lw_logList->addItem(QString("Failed to write script file %1 for WinSCP.").arg(scriptFilePath));
		ui.btn_enterBootloader->setDisabled(false);
		return;
	}

	QProcess* netconfigTransfer = new QProcess(this);
	// Connetti i segnali agli slot
	connect(netconfigTransfer, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
			if (exitStatus == QProcess::NormalExit && exitCode == 0) {
				retryCount = 0;
				ui.btn_enterBootloader->setChecked(false); // Riabilita il pulsante
				if (QFile::exists(localDestinationPath)) {
					if (listWidgetAlive) ui.lw_logList->addItem("File 'netconf.bin' downloaded successfully to: " + localDestinationPath);
					readNetConfig(localFilePath);
				}
				else {
					if (listWidgetAlive) ui.lw_logList->addItem("File 'netconf.bin' not found after download.");
				}
			}
			else {

				ui.lw_logList->addItem(tr("Trasferimento file 'netconf.bin' fallito o output non atteso."));

				if (enteringBl && retryCount < MAX_RETRIES) {
					retryCount++;
					if (listWidgetAlive) ui.lw_logList->addItem("Attempt to reconnect" + QString::number(retryCount) + " in " + QString::number(RETRY_INTERVAL_MS) + "ms due to start-up error...");
					netconfTransfTimer->start(RETRY_INTERVAL_MS);
				}
				else if (enteringBl && retryCount >= MAX_RETRIES) {
					retryCount = 0;
					if (listWidgetAlive) ui.lw_logList->addItem("Maximum number of reconnection attempts reached. Connection failed.");
					ui.btn_enterBootloader->setChecked(false); // Riabilita il pulsante
				}
				else if (!enteringBl) { // !modeConnect
					retryCount = 0;
					if (listWidgetAlive) ui.lw_logList->addItem("Entering bootloader mode manually disabled.");
					ui.btn_enterBootloader->setChecked(false);
				}
			}
		});

	connect(netconfigTransfer, &QProcess::errorOccurred,
		this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	if (netconfTransfTimer->isActive()) {
		netconfTransfTimer->stop();
	}

	netconfigTransfer->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + transferLogFilePath);

}

/**
* @function readNetConfig
* @brief Legge il file di configurazione di rete 'netconf.bin' e valida i dati letti.
* @param netConfPath Il percorso del file di configurazione di rete da leggere.
* @return void
* @details Questa funzione apre il file di configurazione di rete, legge i dati in una struttura NetConf e verifica la validità dei dati letti confrontando il CRC e altri campi.
* @see NetConf, QFile, QDataStream, logger
*/
void MultiTargetUpdaterv2::readNetConfig(const QString& netConfPath) {

	NetConf tempConf;
	uint32_t calculatedCRC = 0;

	QFile file(netConfPath);
	if (!file.open(QIODevice::ReadOnly)) {
		if (listWidgetAlive) ui.lw_logList->addItem("Impossible to open file netconfig.bin at the specified path " + file.errorString());
		return;
	}
	QDataStream reader(&file);
	reader.setByteOrder(QDataStream::LittleEndian);
	reader.setFloatingPointPrecision(QDataStream::SinglePrecision); // Buona pratica

	try {
		// Leggi i membri della struttura
		reader >> tempConf.Magic;

		for (int i = 0; i < 4; i++)
			reader >> tempConf.val_IP_ADDR[i];

		for (int i = 0; i < 4; i++)
			reader >> tempConf.val_NETMASK_ADDR[i];

		for (int i = 0; i < 4; i++)
			reader >> tempConf.val_GW_ADDR[i];

		for (int i = 0; i < 6; i++)
			reader >> tempConf.val_MAC_ADDR[i];

		reader >> tempConf.m2_1;
		reader >> tempConf.m2_2;
		reader >> tempConf.CRC;

		// Calcolo del CRC
		// Ritorno al primo byte del file
		file.seek(0);
		calculatedCRC = 1234;
		// Controlla che la lunghezza corisponda a 28 - 4
		int byteToReadForCrc = sizeof(NetConf) - sizeof(uint32_t);

		for (int i = 0; i < byteToReadForCrc; i++) {
			uint8_t dato;
			reader >> dato;
			calculatedCRC += (static_cast<uint32_t>(i) * 2) + (static_cast<uint32_t>(dato) * dato);
		}

		file.close();

	}
	catch (const std::exception& ex) {
		if (listWidgetAlive) ui.lw_logList->addItem(tr("Error during file reading: ") + QString::fromStdString(ex.what()));
		file.close();
		return;
	}
	catch (...) {
		if (listWidgetAlive) ui.lw_logList->addItem(tr("Unknown error during file reading."));
		file.close();
		return;
	}

	// Validazione della configurazione
	if ((calculatedCRC == tempConf.CRC) &&
		(tempConf.Magic == 0x734AB337) &&
		(tempConf.m2_1 == static_cast<uint8_t>(~tempConf.val_MAC_ADDR[0])) &&
		(tempConf.m2_2 == static_cast<uint8_t>(~tempConf.val_IP_ADDR[1])))
	{
		// Se tutto è valido, copia la configurazione
		// Assumendo che 'Configurazione' sia un membro della classe
		this->Configurazione = tempConf;
		if (listWidgetAlive) ui.lw_logList->addItem("Net Configuration readed successfully.");
		// Potresti voler visualizzare i valori letti per debug
		//qDebug() << "Magic:" << QString::number(tempConf.Magic, 16);
		ui.lb_IPAddress->setText(QString("%1.%2.%3.%4")
			.arg(tempConf.val_IP_ADDR[0])
			.arg(tempConf.val_IP_ADDR[1])
			.arg(tempConf.val_IP_ADDR[2])
			.arg(tempConf.val_IP_ADDR[3]));

		ui.lb_MacAddress->setText(QString("%1:%2:%3:%4:%5:%6")
			.arg(tempConf.val_MAC_ADDR[0], 2, 16, QChar('0'))
			.arg(tempConf.val_MAC_ADDR[1], 2, 16, QChar('0'))
			.arg(tempConf.val_MAC_ADDR[2], 2, 16, QChar('0'))
			.arg(tempConf.val_MAC_ADDR[3], 2, 16, QChar('0'))
			.arg(tempConf.val_MAC_ADDR[4], 2, 16, QChar('0'))
			.arg(tempConf.val_MAC_ADDR[5], 2, 16, QChar('0')));
		;
		ui.lb_NetMask->setText(QString("%1.%2.%3.%4")
			.arg(tempConf.val_NETMASK_ADDR[0])
			.arg(tempConf.val_NETMASK_ADDR[1])
			.arg(tempConf.val_NETMASK_ADDR[2])
			.arg(tempConf.val_NETMASK_ADDR[3]));

		ui.lb_gwAddress->setText(QString("%1.%2.%3.%4")
			.arg(tempConf.val_GW_ADDR[0])
			.arg(tempConf.val_GW_ADDR[1])
			.arg(tempConf.val_GW_ADDR[2])
			.arg(tempConf.val_GW_ADDR[3]));

		Configurazione.m2_1 = static_cast<quint8>(~this->Configurazione.val_MAC_ADDR[0]);
		Configurazione.m2_2 = static_cast<quint8>(~this->Configurazione.val_IP_ADDR[1]);

	}
	else {
		if (listWidgetAlive) ui.lw_logList->addItem("Invalid configuration file. CRC or magic number mismatch.");
	}
}

/**
* @slot on_pushButtonclicked_checkInstalledFw
 * @brief Gestisce il click del pulsante per controllare i firmware installati sui dispositivi.
 * @param void
 * @return void
 * @details Questa funzione pulisce la vista dei log, verifica se il dispositivo è connesso e chiama la funzione per controllare i firmware installati.
 * Se il dispositivo non è in modalità bootloader, informa l'utente di farlo prima.
 * @see checkInstalledfw(), cleanLogsView()
 */
void MultiTargetUpdaterv2::on_pushButtonclicked_checkInstalledFw() {

	cleanLogsView();

	if (isConnected) {
		if (listWidgetAlive) ui.lw_logList->addItem("Reading target device fws info...");
		checkInstalledfw();
	}
	else {
		if (listWidgetAlive) ui.lw_logList->addItem("Device not in bootloader mode. Please enter bootloader mode first.");
	}
}

/**
* @function checkInstalledfw
* @brief Controlla i firmware installati sul dispositivo e li scarica nella cartella temporanea /data/temp/installedFw/.
* @param void
* @return void
* @details Questa funzione crea uno script per WinSCP che si connette al dispositivo e scaricare le informazioni sui firmware installati sul dispositivo e sui dispositivi CAN e COM collegati.
* Usa un funzione definata inline per gestire il processo di download e la lettura dei file scaricati.
* @see writeScriptFile(), readFwVersions(), compareSwInfos(), updateFwTableWidget(), onWinSCPErrorOccurred(), retryWinSCPProcess()
*/
void MultiTargetUpdaterv2::checkInstalledfw() {

	//dove sono sicuro che riesca a scaricare i file
	QString localDataFolder = exeAppPath + "/data/temp/installedFw/"; // Usa la cartella 'data' nella directory dell'app
	localDataFolder = QDir::toNativeSeparators(localDataFolder);

	// Verifico che la directory esista, altrimenti la creo
	QDir dataDir(localDataFolder);
	if (!dataDir.exists()) {
		if (!dataDir.mkpath(localDataFolder)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to create the directory: " + dataDir.path());
			return;
		}
	}

	// Se presenti rimuovo i file precedenti
	// listo i file presenti e li rimuovo
	QStringList existingFiles = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
	foreach(const QString & fileName, existingFiles) {
		QFile::remove(dataDir.filePath(fileName));
	}

	QString script;
	script = QString("open ftp://%1:%2@%3:%4/\n"
		"option batch continue\n"
		"lcd \"%5\"\n" //cambia la directory localDataFolder
		"get /!spifi/infospifi.bin\n"
		"get /!nand/infonand.bin\n"
		"get /!flash/infoflash.bin\n"
		"get ""/!CAN_DEV_02/infocan02f.bin\n"
		"get ""/!CAN_DEV_03/infocan03f.bin\n"
		"get ""/!CAN_DEV_04/infocan04f.bin\n"
		"get ""/!CAN_DEV_05/infocan05f.bin\n"
		"get ""/!CAN_DEV_06/infocan06f.bin\n"
		"get ""/!CAN_DEV_07/infocan07f.bin\n"
		"get ""/!CAN_DEV_08/infocan08f.bin\n"
		"get ""/!CAN_DEV_09/infocan09f.bin\n"
		"get ""/!CAN_DEV_10/infocan10f.bin\n"
		"get ""/!CAN_DEV_11/infocan11f.bin\n"
		"get ""/!CAN_DEV_12/infocan12f.bin\n"
		"get ""/!CAN_DEV_13/infocan13f.bin\n"
		"get ""/!CAN_DEV_14/infocan14f.bin\n"
		"get ""/!CAN_DEV_15/infocan15f.bin\n"
		"get ""/!COM_DEV_00/infocom00f.bin\n"
		"get ""/!COM_DEV_01/infocom01f.bin\n"
		"get ""/!COM_DEV_02/infocom02f.bin\n"
		"close\n"
		"exit\n"
	).arg(
		userName,
		password,
		hostName,
		port,
		QDir::toNativeSeparators(localDataFolder) // Assicura il percorso corretto per Windows
	);

	// Salvataggio su file temporaneo
	QString scriptFilePath = exeAppPath + "/check_installedFw_script.txt";
	QString logInfoFilePath = exeAppPath + "/check_installedFw_log.txt";

	QFile scriptFile(scriptFilePath);
	if (!writeScriptFile(script, scriptFilePath)) {
		if (listWidgetAlive) ui.lw_logList->addItem("Failed to write script file.");
		return;
	}

	QProcess* checkInstalledFwProcess = new QProcess(this);

	connect(checkInstalledFwProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {

			bool foundFiles = false;

			if (exitCode == 0 && exitStatus == QProcess::NormalExit) {

				if (listWidgetAlive) ui.lw_logList->addItem("Fw information download completed successfullt.");

				// Update the tableWidget with the downloaded information
				readFwVersions(localDataFolder);
				compareSwInfos(installedFwInfos, downloadedFwInfos);
				updateFwTableWidget();
			}
			else if (exitCode == 1 and exitStatus == QProcess::NormalExit) {

				// Controllo se è stato scaricato qualcosa in localDataFolder
				QStringList files = QDir(localDataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
				if (!files.isEmpty())
					foundFiles = true;

				if (foundFiles) {
					if (listWidgetAlive) ui.lw_logList->addItem("Fw information download completed successfull, but some files are missing.");
					// Update the tableWidget with the downloaded information
					readFwVersions(localDataFolder);
					compareSwInfos(installedFwInfos, downloadedFwInfos);
					updateFwTableWidget();

				}
				else {
					if (listWidgetAlive) ui.lw_logList->addItem("No file founded. Download error.");
				}
			}
			else {

				if (listWidgetAlive) ui.lw_logList->addItem(tr("Fw information download failed."));

				if (retryCount < MAX_RETRIES) {
					retryCount++;
					if (listWidgetAlive) ui.lw_logList->addItem("Attempt to reconnect" + QString::number(retryCount) + " in " + QString::number(RETRY_INTERVAL_MS) + "ms due to start-up error...");
					netconfTransfTimer->start(RETRY_INTERVAL_MS);
				}
				else if (retryCount >= MAX_RETRIES) {
					qDebug() << "Raggiunto il numero massimo di tentativi di ritrasferimento. Trasferimento fallito.";
					ui.btn_enterBootloader->setDisabled(false); // Riabilita il pulsante
				}
				else {
					ui.lw_logList->addItem("Max number of tries reached, process stopped.");
					ui.btn_enterBootloader->setDisabled(false);
				}
			}
			checkInstalledFwProcess->deleteLater(); // Pulizia del processo
		});

	connect(checkInstalledFwProcess, &QProcess::errorOccurred, this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	if (downloadInstalledFwTimer->isActive()) {
		downloadInstalledFwTimer->stop(); // Ferma il timer per questo tentativo
	}

	checkInstalledFwProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logInfoFilePath);

}

/* 
 * @funztion readFwVersions
 * @brief Legge le versioni e le informazioni SW dai file scaricati e li memorizza in una QList.
 * @param dataFolder Il percorso della cartella contenente i file con le informazioni SW scaricati.
 * @return void
 * @details Questa funzione legge i file presenti nella cartella specificata, estrae le informazioni sulle versioni SW e le memorizza in una QList di strutture swInfo.
 * @see swInfo, readFwVersion(), installedFwInfos
 */
void MultiTargetUpdaterv2::readFwVersions(const QString& dataFolder) {

	// Leggi le versioni SW dai file scaricati
	QDir dataDir(dataFolder);
	if (!dataDir.exists()) {
		if (listWidgetAlive) ui.lw_logList->addItem("The installedFw directory does not exist.");
		return;
	}

	QStringList filenames = dataDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
	if (filenames.isEmpty()) {
		if (listWidgetAlive) ui.lw_logList->addItem("No file found in the installedFw directory.");
		return;
	}
	else {

		// Pulisco la QList
		installedFwInfos.clear();

		foreach(const QString & filename, filenames) {
			swInfo info;
			qDebug() << "- " << filename;
			switch (readFwVersion(dataFolder + filename, info)) {

			case 0:
				info.fileLocation = dataFolder + filename;
				info.deviceName = filename;
				info.deviceName.remove("info");
				info.deviceName.remove(".bin");
				installedFwInfos.append(info);
				if (listWidgetAlive) ui.lw_logList->addItem("Fw version correctly readed from file: " + filename);
				break;
			case 1:
				if (listWidgetAlive) ui.lw_logList->addItem("File " + filename + "not present.");
				break;
			case 2:
				if (listWidgetAlive) ui.lw_logList->addItem("Impossible open file: " + filename);
				break;
			}
		}
	}
}

/**
 * @function readFwVersion
 * @brief Legge le informazioni sulla versione del firmware da un file specificato e le memorizza in una struttura swInfo.
 * @param filePath Il percorso del file da cui leggere le informazioni sulla versione del firmware.
 * @param info Riferimento alla struttura swInfo in cui memerizzare le informazioni lette.
 * @return int Restituisce 0 se la lettura è avvenuta con successo, 1 se il file non esiste, 2 se si è verificato un errore nell'apertura del file.
 * @details Questa funzione apre un file binario, legge le informazioni sulla versione, i dati e l'ora, e le memorizza nella struttura swInfo passata come parametro.
 * @see swInfo, QFile, QDataStream, VERSION_SIZE, DATA_SIZE, ORA_SIZE
 */
int MultiTargetUpdaterv2::readFwVersion(QString filePath, swInfo& info) {

	char version_buffer[VERSION_SIZE + 1];
	char data_buffer[DATA_SIZE + 1];
	char ora_buffer[ORA_SIZE + 1];

	if (QFile::exists(filePath)) {

		QFile file(filePath);
		if (file.open(QIODevice::ReadOnly)) {

			QDataStream reader(&file);
			reader.setByteOrder(QDataStream::LittleEndian);
			reader.setFloatingPointPrecision(QDataStream::SinglePrecision);
			file.read(version_buffer, VERSION_SIZE);
			file.seek(VERSION_SIZE);
			file.read(data_buffer, DATA_SIZE);
			file.seek(VERSION_SIZE + DATA_SIZE);
			file.read(ora_buffer, ORA_SIZE);
			file.close();

			version_buffer[VERSION_SIZE] = '\0';
			data_buffer[DATA_SIZE] = '\0';
			ora_buffer[ORA_SIZE] = '\0';
			info.version = QString::fromLatin1(version_buffer);
			info.data = QString::fromLatin1(data_buffer);
			info.ora = QString::fromLatin1(ora_buffer);
			return 0;
		}
		else {
			// Nell'eventualità che non si riesca ad aprire il file
			return 2;
		}
	}
	else {
		// Nell'eventualità che non venga trovato il file
		return 1;
	}
}

/**
 * @function compareSwInfos
 * @brief Confronta le informazioni dei software presenti sul dispositivo con le informazioni dei software scaricati e aggiorna la lista deviceFwInfos 
 * contenente le informazioni appaiate di software installati e da installare.
 * @param existingInfos Lista delle informazioni software esistenti.
 * @param newInfos Lista delle nuove informazioni software da confrontare.
 * @return void
 * @details Questa funzione confronta le informazioni software esistenti con le nuove informazioni e aggiorna la lista deviceFwInfos con le versioni installate e disponibili.
 * Se un dispositivo non ha una versione installata, viene indicato come "Non installato".
 * @see deviceFwInfo, swInfo, deviceFwInfos
 */
void MultiTargetUpdaterv2::compareSwInfos(QList<swInfo>& existingInfos, QList<swInfo> newInfos) {

	// Pulisco la lista prima di riempirla nuovamente.
	deviceFwInfos.clear();

	if (newInfos.isEmpty()) {

		for (const swInfo& existingInfo : existingInfos) {
			deviceFwInfo newDeviceFwInfo;
			newDeviceFwInfo.deviceName = existingInfo.deviceName;
			newDeviceFwInfo.installedVersion = existingInfo.version;
			newDeviceFwInfo.installedData = existingInfo.data;
			newDeviceFwInfo.installedOra = existingInfo.ora;
			newDeviceFwInfo.availableVersion = "Non installato"; // Se non trovato, metti "Non installato"
			newDeviceFwInfo.availableData = "N/A"; // Non disponibile
			newDeviceFwInfo.availableOra = "N/A"; // Non disponibile
			deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos
		}
	}
	if (existingInfos.isEmpty()) {
		for (const swInfo& newInfo : newInfos) {
			deviceFwInfo newDeviceFwInfo;
			newDeviceFwInfo.deviceName = newInfo.deviceName;
			newDeviceFwInfo.installedVersion = "Non installato"; // Se non trovato, metti "Non installato"
			newDeviceFwInfo.installedData = "N/A"; // Non disponibile
			newDeviceFwInfo.installedOra = "N/A"; // Non disponibile
			newDeviceFwInfo.availableVersion = newInfo.version;
			newDeviceFwInfo.availableData = newInfo.data;
			newDeviceFwInfo.availableOra = newInfo.ora;
			newDeviceFwInfo.fileLocation = newInfo.fileLocation; // Aggiungi il percorso del file
			deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos
		}
		return;
	}
	else if (existingInfos.count() > newInfos.count() && !newInfos.isEmpty()) {
		for (const swInfo& existingInfo : existingInfos) {
			bool found = false;
			for (const swInfo& newInfo : newInfos) {
				// Se il newInfos.deviceName è contenuto in existingInfos.deviceName
				if (newInfo.deviceName.contains(existingInfo.deviceName, Qt::CaseInsensitive)) {
					// Assegno le informazioni di versione, data e ora a deviceFwInfos
					deviceFwInfo newDeviceFwInfo;
					newDeviceFwInfo.deviceName = existingInfo.deviceName;
					newDeviceFwInfo.installedVersion = existingInfo.version;
					newDeviceFwInfo.installedData = existingInfo.data;
					newDeviceFwInfo.installedOra = existingInfo.ora;
					newDeviceFwInfo.availableVersion = newInfo.version;
					newDeviceFwInfo.availableData = newInfo.data;
					newDeviceFwInfo.availableOra = newInfo.ora;
					newDeviceFwInfo.fileLocation = newInfo.fileLocation; // Aggiungi il percorso del file
					found = true;
					deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos
					break; // Esci dal ciclo interno se trovi una corrispondenza
				}
			}
			if (!found) {
				deviceFwInfo newDeviceFwInfo;
				newDeviceFwInfo.deviceName = existingInfo.deviceName;
				newDeviceFwInfo.installedVersion = existingInfo.version;
				newDeviceFwInfo.installedData = existingInfo.data;
				newDeviceFwInfo.installedOra = existingInfo.ora;
				newDeviceFwInfo.availableVersion = "Non installato"; // Se non trovato, metti "Non installato"
				newDeviceFwInfo.availableData = "N/A"; // Non disponibile
				newDeviceFwInfo.availableOra = "N/A"; // Non disponibile
				deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos
			}
		}
	}
	else {
		for (const swInfo& newInfo : newInfos) {

			bool found = false;

			for (swInfo& existingInfo : existingInfos) {

				// Se il newInfos.deviceName è contenuto in existingInfos.deviceName
				if (existingInfo.deviceName.contains(newInfo.deviceName, Qt::CaseInsensitive)) {

					// Assegno le informazioni di versione, data e ora a deviceFwInfos
					deviceFwInfo newDeviceFwInfo;
					newDeviceFwInfo.deviceName = newInfo.deviceName;
					newDeviceFwInfo.installedVersion = existingInfo.version;
					newDeviceFwInfo.installedData = existingInfo.data;
					newDeviceFwInfo.installedOra = existingInfo.ora;
					newDeviceFwInfo.availableVersion = newInfo.version;
					newDeviceFwInfo.availableData = newInfo.data;
					newDeviceFwInfo.availableOra = newInfo.ora;
					newDeviceFwInfo.fileLocation = newInfo.fileLocation; // Aggiungi il percorso del file
					found = true;
					deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos
					break; // Esci dal ciclo interno se trovi una corrispondenza

				}
			}
			if (!found) {

				deviceFwInfo newDeviceFwInfo;
				newDeviceFwInfo.deviceName = newInfo.deviceName;
				newDeviceFwInfo.installedVersion = "Non installato"; // Se non trovato, metti "Non installato"
				newDeviceFwInfo.installedData = "N/A"; // Non disponibile
				newDeviceFwInfo.installedOra = "N/A"; // Non disponibile
				newDeviceFwInfo.availableVersion = newInfo.version;
				newDeviceFwInfo.availableData = newInfo.data;
				newDeviceFwInfo.availableOra = newInfo.ora;
				deviceFwInfos.append(newDeviceFwInfo); // Aggiungi l'informazione alla lista deviceFwInfos

			}
		}
	}
}


/**
 * @function updateFwTableWidget
 * @brief Aggiorna la tabella dei firmware installati e disponibili.
 * @param void
 * @return void
 * @details Questa funzione pulisce la tabella esistente, imposta le intestazioni delle colonne e aggiunge le informazioni sui firmware installati e disponibili per ogni dispositivo
 * @see deviceFwInfo, QTableWidget, QTableWidgetItem
 */
void MultiTargetUpdaterv2::updateFwTableWidget() {

	// Pulisce la tabella prima di aggiungere nuovi dati
	ui.tableWidget->setRowCount(0);
	ui.tableWidget->setColumnCount(4);
	// Aggiunge le intestazioni delle colonne
	QStringList headers;
	headers << "NOME" << "FIRMWARE\nINSTALLATO" << "FIRMWARE DA\nINSTALLARE" << "AZIONE";
	ui.tableWidget->setHorizontalHeaderLabels(headers);

	// Aggiunge le informazioni sui firmware installati e scaricati
	for (const deviceFwInfo& info : deviceFwInfos) {
		int row = ui.tableWidget->rowCount();
		ui.tableWidget->insertRow(row);
		ui.tableWidget->setItem(row, 0, new QTableWidgetItem(info.deviceName));
		ui.tableWidget->setItem(row, 1, new QTableWidgetItem(info.installedVersion + " " + info.installedData + " " + info.installedOra));
		// Uso un widget composito per mostrare il fw nella label e il pulsante per l'azione
		QWidget* updateFwWidget = new QWidget;
		QHBoxLayout* layout = new QHBoxLayout(updateFwWidget);
		layout->setContentsMargins(0, 0, 0, 0); // Rimuove i margini del layout
		layout->setSpacing(2); // Rimuove lo spazio tra gli elementi del layout

		QLabel* fwLabel = new QLabel(QString("%1 %2 %3")
			.arg(info.availableVersion)
			.arg(info.availableData)
			.arg(info.availableOra));
		fwLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // Allinea il testo a sinistra e al centro verticalmente
		fwLabel->setWordWrap(true); // Abilita l'andata a capo del testo se necessario
		layout->addWidget(fwLabel);
		QPushButton* browseButton = new QPushButton("...");
		browseButton->setObjectName("browseButton"); // Imposta un nome per il pulsante
		browseButton->setFixedWidth(20); // Imposta una larghezza fissa per il pulsante
		layout->addWidget(browseButton); // Aggiunge il pulsante al layout

		//layout->addStretch(); // Aggiunge uno spazio flessibile tra la label e il pulsante
		ui.tableWidget->setCellWidget(row, 2, updateFwWidget); // Imposta il widget composito nella cella

		//ui.tableWidget->setItem(row, 2, new QTableWidgetItem(info.availableVersion + " " + info.availableData + " " + info.availableOra));

		QPushButton* actionButton = new QPushButton("Update");
		actionButton->setCheckable(true);
		if (!info.deviceName.contains("IndicatorApp")) {
			ui.tableWidget->setCellWidget(row, 3, actionButton);
			if (info.installedVersion == info.availableVersion
				|| info.availableVersion.isEmpty())
			{
				actionButton->setEnabled(false);
				actionButton->setText("Aggiornato");
			}
		}
		else {
			// inserisco vuoto se il dispositivo è l'IndicatorApp
			ui.tableWidget->setItem(row, 3, new QTableWidgetItem(""));
		}

		/*
		* @brief Connetti il pulsante browseButton per selezionare un file firmware locale da caricare
		* @details Questa connessione lambda viene attivata quando l'utente clicca sul pulsante browseButton per selezionare un file firmware locale da caricare.
		* @see readLocalFwVersion(), compareSwInfos(), updateFwTableWidget()
		*/
		connect(browseButton, &QPushButton::clicked, this, [=]() {

			qDebug() << "Browse button clicked for device: " << info.deviceName;
			QString filePath = QFileDialog::getOpenFileName(this, "Select Firmware File", QDir::homePath(), "Firmware Files (*.bin)");
			if (!filePath.isEmpty()) {
				qDebug() << "Selected file: " << filePath;
				QFile localFile(filePath);
				swInfo newInfo;
				// Sostituisco il file corrispondente nella directory locale con il file a filePath 
				QString localDataFolder = exeAppPath + "/data/temp/remotedata/newfirmware/NewData/"; // Cartella temporanea per i file scaricati
				localDataFolder = QDir::toNativeSeparators(localDataFolder); // Assicura il percorso corretto per Windows
				QString localFilePath = localDataFolder + info.deviceName + ".bin"; // Percorso del file binario da aggiornare
				QString fileName = QFileInfo(filePath).fileName();
				// Rimuovo il file locale se esiste già	
				if (QDir(localDataFolder).exists()) {

					if (QFile::exists(localFilePath)) {
						
						// Copio il file a filePath dentro il file al percorso localFilePath
						if (fileName.contains("IndicatorApp")) {

							// Creo i path di IndicatorAppl.bin e IndicatorAppu.bin
							QString _indicatorApplBinPath = filePath.left(QDir::toNativeSeparators(filePath).lastIndexOf(QDir::separator())) + "\\IndicatorAppl.bin";
							QString _indicatorAppuBinPath = filePath.left(QDir::toNativeSeparators(filePath).lastIndexOf(QDir::separator())) + "\\IndicatorAppu.bin";
							QString indicatorApplBinPath = localDataFolder + "IndicatorAppl.bin";
							QString indicatorAppuBinPath = localDataFolder + "IndicatorAppu.bin";

							QFile indicatorApplBin(QDir::toNativeSeparators(_indicatorApplBinPath));
							QFile indicatorAppuBin(QDir::toNativeSeparators(_indicatorAppuBinPath));

							QFile::remove(localFilePath);
							if (!localFile.copy(localFilePath)) {
								if (listWidgetAlive) ui.lw_logList->addItem("Error copying file to local directory: " + localDataFolder);
								return;
							}
							QFile::remove(indicatorApplBinPath);
							if (!indicatorApplBin.copy(indicatorApplBinPath)) {
								if (listWidgetAlive) ui.lw_logList->addItem("Error copying IndicatorAppl.bin to local directory: " + localDataFolder);
								return;
							}
							QFile::remove(indicatorAppuBinPath);
							if (!indicatorAppuBin.copy(indicatorAppuBinPath)) {
								if (listWidgetAlive) ui.lw_logList->addItem("Error copying IndicatorAppu.bin to local directory: " + localDataFolder);
								return;
							}
						} 
						else {
							QFile::remove(localFilePath);
							if (!localFile.copy(localFilePath)) {
								QString error = localFile.errorString();
								ui.lw_logList->addItem("Errore nella copia: " + error + " - Destinazione: " + localFilePath);
								return;
							}
						}

					}
										
					else {
						// salvo il file a filepath nella cartella locale al percorso localFilePath
						if (!localFile.copy(localFilePath)) {
							if (listWidgetAlive) ui.lw_logList->addItem("Error copying file to local directory: " + localDataFolder);
							return;
						}
					}
				}
				else {
					if (!QDir().mkpath(localDataFolder)) {
						if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to create the local data folder: " + localDataFolder);
						return;
					}
					if (!localFile.copy(localFilePath)) {
						if (listWidgetAlive) ui.lw_logList->addItem("Error copying file to local directory: " + localDataFolder);
						return;
					}
				}


				readLocalFwVersion(localDataFolder); // Legge la versione del firmware locale
				compareSwInfos(installedFwInfos, downloadedFwInfos);
				updateFwTableWidget();
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("No file selected for device: " + info.deviceName);
			}

		});

		/*
		* @brief Connetti il pulsante actionButton per avviare l'aggiornamento del firmware
		* @details Questa connessione lambda viene attivata quando l'utente clicca sul pulsante actionButton per avviare l'aggiornamento del firmware del dispositivo.
		* @see updateFirmware()
		*/
		connect(actionButton, &QPushButton::clicked, this, [=]() { // Connetti il pulsante all'azione di aggiornamento

			QPushButton* btn_sender = qobject_cast<QPushButton*> (sender());

			if (!updateProcess) {

				if (listWidgetAlive) ui.lw_logList->addItem("Update remote device: " + info.deviceName);
				updateProcess = true;
				btn_sender->setChecked(true);
				btn_sender->setText("Updating");
				updateFirmware(info.deviceName, info.fileLocation);
			}
			});

		/*
		* @brief Connetti il segnale enableActionButton per abilitare il pulsante di azione dopo l'aggiornamento 
		* @details Questa connessione lambda viene attivata quando il segnale enableActionButton viene emesso in seguito al termine del processo id aggiornamento,
		* per abilitare il pulsante di azione del dispositivo specificato.
		* @see enableActionButton, updateProcess
		*/
		connect(this, &MultiTargetUpdaterv2::enableActionButton, this, [=](QString deviceName) {

			for (int i = 0; i < ui.tableWidget->rowCount(); i++) {
				QWidget* cellWidget = ui.tableWidget->cellWidget(i, 3);
				if (!cellWidget)
					continue;
			
				QTableWidgetItem* item = ui.tableWidget->item(i, 0);
				if (item && item->text() == deviceName) {
					QPushButton* actionButton = qobject_cast<QPushButton*>(ui.tableWidget->cellWidget(i, 3));
					if (actionButton) {
						actionButton->setChecked(false);
						actionButton->setText("Update");
						updateProcess = false;
						break; // Esci dal ciclo dopo aver trovato il bottone giusto
					}
				}
			}
		});
	}
}

/*
 * @brief Gestisce il click del pulsante per scaricare la lista dei sistemi remoti.
 * @see downloadFwList()
 */
void MultiTargetUpdaterv2::on_pushButtonClicked_downloadSystemList() {

	cleanLogsView();
	downloadFwList();
}

/**
 * @function downloadFwList
 * @brief Scarica la lista dei firmware disponibili da un server remoto utilizzando WinSCP.
 * @param void
 * @return void
 * @details Questa funzione crea uno script per WinSCP che si connette al server FTP e scarica il file ConfigList.xml contenente le informazioni sui firmware disponibili.
 * Dopo il download, chiama la funzione readConfList per leggere il file XML e popolare la lista dei progetti remoti.
 * @see writeScriptFile(), readConfList(), onWinSCPErrorOccurred()
 */ 
void MultiTargetUpdaterv2::downloadFwList() {

	QString remoteDataFolder = exeAppPath + "/data/temp/remotedata/"; // Cartella temporanea per i file scaricati
	remoteDataFolder = QDir::toNativeSeparators(remoteDataFolder); // Assicura il percorso corretto per Windows
	QString remoteConfListPath = remoteDataFolder + "ConfigList.xml";

	if (!QDir(remoteDataFolder).exists()) {
		if (!QDir().mkpath(remoteDataFolder)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to create the remote data folder: " + remoteDataFolder);
			return;
		}
	}

	QString scriptFilePath = exeAppPath + "/download_conflist_script.txt";
	QString logFilePath = exeAppPath + "/download_conflist_log.txt";

	QString script;
	script = QString(
		"open ftp://%1:%2@%3:%4/\n"
		"option batch continue\n"
		"get /ConfigList.xml \"%5\"\n"
		"close\n"
		"exit\n").arg(
			updateUserName,
			updatePassword,
			updateHostName,
			updatePort,
			QDir::toNativeSeparators(remoteConfListPath)
		);

	QFile scriptFile(scriptFilePath);
	if (!writeScriptFile(script, scriptFilePath)) {
		if (listWidgetAlive) ui.lw_logList->addItem("Failed to write script file.");
		return;
	}

	QProcess* downloadSystemListProcess = new QProcess(this);

	/*
	 * @brief Connetti il processo di download della lista dei sistemi remoti al segnale finished e gestisce il risultato del download.
	 * @details Quando il processo di download termina, questa connessione lambda viene attivata per verificare il codice di uscita e lo stato del processo.
	 * @see readConfList()
	 */
	connect(downloadSystemListProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {

			if (exitCode == 0) {
				if (listWidgetAlive) ui.lw_logList->addItem("ConfList.xml download successfully completed.");
				readConfList(remoteConfListPath);
			}
			else if (exitCode == 1 and exitStatus == QProcess::NormalExit) {

				QStringList files = QDir(remoteDataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
				if (!files.isEmpty()) {
					if (listWidgetAlive) ui.lw_logList->addItem("ConfList.xml download with error. Continue with other checks.");
					readConfList(remoteConfListPath);
				}
				else {
					if (listWidgetAlive) ui.lw_logList->addItem("Error no file downloaded.");
				}
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("General download error.");
			}
			downloadSystemListProcess->deleteLater(); // Pulizia del processo
		});

	connect(downloadSystemListProcess, &QProcess::errorOccurred, this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	downloadSystemListProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath);
}


/**
 * @function readConfList
 * @brief Legge la lista di configurazione da ConfList.xml
 * @param confListDirPath Il percorso della directory contenente il file ConfList.xml
 * @return void
 * @details Questa funzione apre il file ConfList.xml, decripta e legge le informazioni sui progetti remoti, e le memorizza in una Qlist di remoteProject.
 * @see remoteProject, QXmlStramReader, DecryptString(), updateAvailableSysstemList()
 */
void MultiTargetUpdaterv2::readConfList(QString confListDirPath) {

	remoteProject tempProject; // Struttura temporanea per i dati del progetto

	QFile confFile(confListDirPath);
	if (!confFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (listWidgetAlive) ui.lw_logList->addItem("Impossible to open ConfList.xml at: " + confListDirPath);
		return;
	}

	QXmlStreamReader xml(&confFile);
	while (!xml.atEnd() && !xml.hasError()) {
		xml.readNext();
		if (xml.isStartElement() && xml.name().toString() == "TabConfig") {
			QString id, hostName, userName, userPassword, ftpPort, nomeConf, pack;
			while (!(xml.isEndElement() && xml.name().toString() == "TabConfig") && !xml.atEnd()) {
				xml.readNext();
				if (xml.isStartElement()) {
					QString tag = xml.name().toString();
					QString value = xml.readElementText();
					if (tag == "ID") id = value;
					else if (tag == "UpdateHostName") tempProject.updateHostName = DecryptString(value.toUtf8());
					else if (tag == "UpdateUserName") tempProject.updateUserName = DecryptString(value);
					else if (tag == "UpdateUserPassword") tempProject.updatePassword = DecryptString(value);
					else if (tag == "UpdateFtpPort") tempProject.port = value;
					else if (tag == "NomeConfigurazione") tempProject.configurationName = (value);
					//else if (tag == "PACK") pack = value;
				}
			}
			// Esempio: stampa i dati letti
			remoteProjects.append(tempProject); // Aggiungi il progetto alla lista
			//qDebug() << "ID:" << id << "Host:" << hostName << "User:" << userName << "Pwd:" << userPassword << "Nome conf:" << nomeConf;

		}
	}
	if (xml.hasError()) {
		if (listWidgetAlive) ui.lw_logList->addItem("Error during xml file reading: " + xml.errorString());
	}

	confFile.close();

	// Aggiorno la combo box
	updateAvailableSystemList();
}

/**
 * @function updateAvailableSystemList
 * @brief Aggiorna la lista dei progetti disponibili nella combo box.
 * @param void
 * @return void
 * @details Questa funzione pulisce la combo box e aggiunge i nomi dei progetti remoti disponibili.
 * Se non ci sono progetti, la combo box rimane vuota.
 * @see remoteProjects, ui.cb_projectsList
 */
void MultiTargetUpdaterv2::updateAvailableSystemList() {
	// Aggiorna la lista dei progetti nella combo box
	ui.cb_projectsList->clear();
	for (const auto& project : remoteProjects) {
		ui.cb_projectsList->addItem(project.configurationName);
	}
	if (!remoteProjects.isEmpty()) {
		ui.cb_projectsList->setCurrentIndex(0); // Se ci sono progetti, seleziona il primo
	}
}

/**
* @function handleOpenSSLError
* @brief Gestisce gli errori di OpenSSL.
* @param context Il contesto in cui si verifica l'errore.
* @return void
* @details Questa funzione intercetta gli errori di OpenSSl e li stampa nell'output di debug.
* @see ERR_get_error(), ERR_error_string()
*/
void MultiTargetUpdaterv2::handleOpenSSLError(const QString& context) {
	unsigned long errCode;
	while ((errCode = ERR_get_error())) {
		char* errStr = ERR_error_string(errCode, nullptr);
		qDebug() << "OpenSSL Error in" << context << ":" << errStr;
	}
}

/**
* @function DecryptString
* @brief Decripta una stringa cifrata in Base64 utilizzando AES-256-CBC:
* @param src La stringa cifrata da decodificare.
* @return QString() la stringa decifrata fino a quel punto di esecuzioine dell'algoritmo e solo in caso di errore.
* @return resultString stringa decifrata senza padding e spazi bianchi.
* @details Questa funzione utilizza OpenSSL per cifrare una stringa cifrata in Base64.
* @see EVP_CIPHER_CTX_new(), EVP_DecryptInit_ex(), EVP_DecryptUpdate(), EVP_DecryptFinal_ex(), QByteArray::fromBase64()
*/
QString MultiTargetUpdaterv2::DecryptString(const QString& src) {

	QByteArray keyb(reinterpret_cast<const char*>(rawKeyBytes), sizeof(rawKeyBytes)); // Converti in QByteArray
	QByteArray ivb(reinterpret_cast<const char*>(rawIvBytes), sizeof(rawIvBytes)); // Converti in QByteArray

	// 1. Preparazione dell'input cifrato (Base64)
	QByteArray toDecrypt;
	QString srcProcessed = src;

	int resto = srcProcessed.length() % 4; // Base64 è multiplo di 4
	if (resto != 0) {
		// Se la stringa Base64 non è multiplo di 4, aggiungi '='
		srcProcessed += QByteArray(4 - resto, '=');
		//qDebug() << "Aggiunto padding '=' alla stringa Base64 per renderla valida.";
	}

	toDecrypt = QByteArray::fromBase64(srcProcessed.toLatin1()); // Converti in Latin1 (ASCII) per Base64

	if (toDecrypt.isEmpty() && !srcProcessed.isEmpty()) {
		if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to decode Base64 string. It would be not valid one.");
		return QString();
	}

	// 2. Decifratura effettiva con OpenSSL EVP
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		handleOpenSSLError("EVP_CIPHER_CTX_new");
		return QString();
	}

	// Inizializza il contesto per la decifratura AES-256-CBC (o AES-128-CBC a seconda della chiave)
	// Scegli il cipher in base alla lunghezza della chiave:
	const EVP_CIPHER* cipher = nullptr;
	if (keyb.length() == 16) { // 128 bit
		cipher = EVP_aes_128_cbc();
	}
	else if (keyb.length() == 24) { // 192 bit
		cipher = EVP_aes_192_cbc();
	}
	else if (keyb.length() == 32) { // 256 bit
		cipher = EVP_aes_256_cbc();
	}
	else {
		qDebug() << "Errore: Lunghezza chiave AES non supportata:" << keyb.length() << "byte.";
		EVP_CIPHER_CTX_free(ctx);
		return QString();
	}

	// EVP_DecryptInit_ex(ctx, cipher, NULL, keyb.constData(), ivb.constData());
	// Inizializza il contesto per la decifratura.
	// Il terzo parametro è il motore (engine), NULL per il default.
	// Il padding PKCS7 è abilitato di default con EVP_CIPHER_CTX_set_padding(ctx, 1);
	if (1 != EVP_DecryptInit_ex(ctx, cipher, nullptr,
		reinterpret_cast<const unsigned char*>(keyb.constData()),
		reinterpret_cast<const unsigned char*>(ivb.constData()))) {
		handleOpenSSLError("EVP_DecryptInit_ex");
		EVP_CIPHER_CTX_free(ctx);
		return QString();
	}

	// Buffer per i dati decifrati. La dimensione massima è la dimensione dei dati cifrati.
	// OpenSSL gestirà il padding PKCS7.
	QByteArray decryptedData(toDecrypt.length() + AES_BLOCK_SIZE, '\0'); // Aumenta leggermente per sicurezza
	int len = 0; // Lunghezza dei dati decifrati in questo passaggio
	int final_len = 0; // Lunghezza totale dei dati decifrati

	// Decifra il blocco principale
	if (1 != EVP_DecryptUpdate(ctx,
		reinterpret_cast<unsigned char*>(decryptedData.data()), &len,
		reinterpret_cast<const unsigned char*>(toDecrypt.constData()), toDecrypt.length())) {
		handleOpenSSLError("EVP_DecryptUpdate");
		EVP_CIPHER_CTX_free(ctx);
		return QString();
	}
	final_len = len;

	// Finalizza la decifratura (rimuove il padding PKCS7)
	if (1 != EVP_DecryptFinal_ex(ctx,
		reinterpret_cast<unsigned char*>(decryptedData.data() + len), &len)) {
		handleOpenSSLError("EVP_DecryptFinal_ex");
		EVP_CIPHER_CTX_free(ctx);
		return QString();
	}
	final_len += len;

	// Rilascia il contesto
	EVP_CIPHER_CTX_free(ctx);

	// Ridimensiona il QByteArray alla lunghezza effettiva dei dati decifrati
	decryptedData.resize(final_len);

	QString resultString = QString::fromLatin1(decryptedData);
	int nullPos = resultString.indexOf(QChar('\0'));
	if (nullPos != -1) {
		resultString.chop(resultString.length() - nullPos);
	}

	return resultString.trimmed(); // trimmed() rimuove spazi bianchi e caratteri di controllo all'inizio/fine

}

/*
* @slot on_pushButtonClicked_downloadFwforCurrentSystem
* @brief Gestisce il click del pulsante per scaricare la configurazione.
* @param void
* @return void
* @details Questa funzione viene chiamata quando l'utente clicca sul pulsante per scaricare la configurazione.
* Avvia il processo di download della configurazione dal server FTP.
* @see downloadRemoteFw()
*/
void MultiTargetUpdaterv2::on_pushButtonClicked_downloadFwforCurrentSystem() {

	cleanLogsView();

	// Scarica la configurazione selezionata
	if (ui.cb_projectsList->count() == 0) {
		if (listWidgetAlive) ui.lw_logList->addItem("Please download System list before.");
		return;
	}
	else {
		downloadRemoteFw();
	}
}


/*
 * @function downloadRemoteFw
 * @brief Scarica la configurazione (serie di file.bin appartenenti al dispositivo scelto) dal server FTP.
 * @param void
 * @return void
 * @details Questa funzione crea uno script per WinSCP che si connette al server FTP e scarica i file UpdateData.zip e UpdateData.md5.
 * Se il file md5 è presente, verifica l'integrità del file zip scaricato e decomprime il file zip nella cartella temporanea.
 * @see writeScriptFile(), unzipFolder(), hash_generator()
 */
void MultiTargetUpdaterv2::downloadRemoteFw() {

	QString script;

	int remoteProjectID = ui.cb_projectsList->currentIndex();

	QString localdataFolder = exeAppPath + "/data/temp/remotedata/newfirmware/"; // Cartella temporanea per i file scaricati
	localdataFolder = QDir::toNativeSeparators(localdataFolder); // Assicura il percorso corretto per Windows

	// Se presenti file precedenti, li rimuove
	if (QFile::exists(localdataFolder + "UpdateData.zip")) {
		QFile::remove(localdataFolder + "UpdateData.zip");
	}
	if (QFile::exists(localdataFolder + "UpdateData.md5")) {
		QFile::remove(localdataFolder + "UpdateData.md5");
	}
	if (QFile::exists(localdataFolder + "NewData")) {
		QStringList files = QDir(localdataFolder + "NewData/").entryList(QDir::Files | QDir::NoDotAndDotDot);
		foreach(const QString & file, files) {
			QFile::remove(localdataFolder + "NewData/" + file);
		}
		// Rimuove la cartella NewData se esiste, per evitare conflitti con i nuovi dati scaricati
		if (QDir(localdataFolder + "NewData/").exists()) {
			QDir(localdataFolder + "NewData/").removeRecursively();
		}
	}

	// Assicurati che le cartelle esistano
	if (!QDir(localdataFolder).exists()) {
		if (!QDir().mkpath(localdataFolder)) {
			ui.lw_logList->addItem("Error: impossible to create the remote data folder: " + localdataFolder);
			return;
		}
	}

	// Imposta il percorso del file di script e del log
	QString scriptFilePath = exeAppPath + "/download_script.txt";
	QString logFilePath = exeAppPath + "/download_log.txt";

	// Crea il contenuto dello script per WinSCP
	if (remoteProjectID != -1) {
		script = QString(
			"open ftp://%1:%2@%3:%4/\n"
			"option batch continue\n"
			"get /UpdateData.zip \"%5\"\n"
			"get /UpdateData.md5 \"%6\"\n"
			"close\n"
			"exit\n"
		).arg(
			remoteProjects.at(remoteProjectID).updateUserName,
			remoteProjects.at(remoteProjectID).updatePassword,
			updateHostName,
			remoteProjects.at(remoteProjectID).port,
			QDir::toNativeSeparators(localdataFolder + "UpdateData.zip"),
			QDir::toNativeSeparators(localdataFolder + "UpdateData.md5")
		);
	}
	else {
		if (listWidgetAlive) ui.lw_logList->addItem("Please select a valid project from the list.");
		return;
	}

	// Scrive il contenuto dello script su un file
	QFile scriptFile(script);
	if (!writeScriptFile(script, scriptFilePath)) {
		qDebug() << "Failed to write script file.";
		return;
	}

	QProcess* downloadProcess = new QProcess(this);

	/*
	 * @brief Connetti il segnale finished del processo per gestire il completamento del download.
	 * @details Questa connessione lambda viene attivata quando il processo di download termina.
	 * Se è andata a buon fine, verifica l'integrità del file scaricato e decomprime il file zip nella cartella temporanea.
	 * @see unzipFolder(), hash_generator()
	 */
	connect(downloadProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {

			// Gestisce il completamento del processo di download
			if (exitCode == 0) {

				if (listWidgetAlive) ui.lw_logList->addItem("Device firmware list successfully downloaded.");

				// Controlla che ci sia il file MD5 nella cartella temporanea che significa che il download è andato a buon fine
				QStringList files = QDir(localdataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
				if (files.isEmpty()) {
					if (listWidgetAlive) ui.lw_logList->addItem("Error: no files downloaded. Please check the connection and try again.");
					return;
				}
				else {

					QString Md5Hash;
					QFile md5File(localdataFolder + "UpdateData.md5");
					if (md5File.open(QIODevice::ReadWrite | QIODevice::Text)) {
						QTextStream in(&md5File);
						Md5Hash = in.readLine().trimmed(); // Legge la prima riga e rimuove spazi bianchi
						md5File.close();
					}
					else {
						if (listWidgetAlive) ui.lw_logList->addItem("Error reading md5 file: " + md5File.errorString());
						return;
					}

					// Se l'hash coincide  con quello calcolato, decomprimo il file zip
					if (QFile::exists(localdataFolder + "UpdateData.zip")) {
						if (!Md5Hash.isEmpty() && (Md5Hash == hash_generator("md5", localdataFolder + "UpdateData.zip")))
						{
							unzipFolder(localdataFolder);
							if (listWidgetAlive) ui.lw_logList->addItem("Firmware directory correctly decompressed.");
						}
					}
					else {
						if (listWidgetAlive) ui.lw_logList->addItem("Error: zip file not found in the temporary folder: " + localdataFolder + "UpdateData.zip");
					}
					
				}
			}

			else if (exitCode == 1 and exitStatus == QProcess::NormalExit) {

				if (listWidgetAlive) ui.lw_logList->addItem("Device firmware list downloaded. Check fo error.");

				// Controlla che ci sia il file MD5 nella cartella temporanea che significa che il download è andato a buon fine
				QStringList files = QDir(localdataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
				if (files.isEmpty()) {
					if (listWidgetAlive) ui.lw_logList->addItem("Error: no files downloaded. Please check the connection and try again.");
					return;
				}
				else {

					QString Md5Hash;
					QFile md5File(localdataFolder + "UpdateData.md5");
					if (md5File.open(QIODevice::ReadWrite | QIODevice::Text)) {
						QTextStream in(&md5File);
						Md5Hash = in.readLine().trimmed(); // Legge la prima riga e rimuove spazi bianchi
						md5File.close();
					}
					else {
						if (listWidgetAlive) ui.lw_logList->addItem("Error reading md5 file: " + md5File.errorString());
						return;
					}

					// Se l'hash coincide  con quello calcolato, decomprimo il file zip
					if (QFile::exists(localdataFolder + "UpdateData.zip")) {
						if (!Md5Hash.isEmpty() && (Md5Hash == hash_generator("md5", localdataFolder + "UpdateData.zip")))
						{
							unzipFolder(localdataFolder);
							if (listWidgetAlive) ui.lw_logList->addItem("Firmware directory correctly decompressed.");
						}
					}
					else {
						if (listWidgetAlive) ui.lw_logList->addItem("Error: zip file not found in the temporary folder: " + localdataFolder + "UpdateData.zip");
					}
				}
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("Error: download process failed with exit code " + QString::number(exitCode) + ".");
			}

			QFile::remove(scriptFilePath);
			downloadProcess->deleteLater(); // Pulizia del processo
		});

	connect(downloadProcess, &QProcess::errorOccurred, this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	downloadProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath); // << "/loglevel=2" << "/log=" + logFilePath);
}

/* 
 * @funtion writeScriptFile
 * @brief Genera un hash MD5 per il file specificato. *
 * Questa funzione calcola l'hash MD5 di un file specificato dal percorso `filepath`.
 * Utilizza la classe `QCryptographicHash` per generare l'hash.
 * @param algorithm Algoritmo di hash da utilizzare (attualmente non utilizzato).
 * @param filepath Percorso del file di cui calcolare l'hash.
 * @return L'hash MD5 del file come stringa esadecimale.
 * @detail Se il file non può essere aperto, restituisce una stringa vuota.
 * @see QCryptographicHash
 */
QString MultiTargetUpdaterv2::hash_generator(QString algorithm, QString filepath) {

	Q_UNUSED(algorithm);
	QFile file(filepath);
	if (!file.open(QIODevice::ReadWrite)) return QString();
	QCryptographicHash hash(QCryptographicHash::Md5);
	hash.addData(&file);
	file.close();
	return hash.result().toHex();
}

/* 
 * @function unzipFolder 
 * @brief Decomprime la cartella di contenente i file scaricati.
 * Questa funzione viene chiamata quando il download della configurazione avviene correttamente.
 * @param localTempFolder Il percorso della cartella contenente i file scaricati.
 * @return void
 * @details Questa funzione utilizza un processo esterno per decomprimere il file ZIP scaricato nella cartella temporanea.
 * @see writeScriptFile(), unzip_script.bat, readLocalFwVersion()
 */
void MultiTargetUpdaterv2::unzipFolder(QString localTempFolder) {

	localTempFolder = QDir::toNativeSeparators(localTempFolder); // Assicura il percorso corretto per Windows

	QDir tempDir(localTempFolder);
	if (!tempDir.exists()) {
		if (listWidgetAlive) ui.lw_logList->addItem("Temporary directory does not exists: " + localTempFolder);
		return;
	}

	QString scriptPath = QCoreApplication::applicationDirPath() + "/unzip_script.bat";

	QStringList arguments;
	arguments << localTempFolder + "UpdateData.zip";
	arguments << localTempFolder + "NewData";
	arguments << QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/7z.exe");
	ui.lw_logList->addItem("Argomento 1: " + arguments.at(0));
	ui.lw_logList->addItem("Argomento 2: " + arguments.at(1));
	ui.lw_logList->addItem("Argomento 3: " + arguments.at(2));
	QProcess* unzipProcess = new QProcess(this);

	/*
	* @brief Connette il segnale di fine processo per gestire il completamento dell'operazione
	* @details Questa connessione lambda viene attivata quando il processo di decompressione termina. Se avvenuto con successo, chiama la funzione readLocalFwVersion
	* per leggere la versione dei firmware scaricati.
	* @see readLocalFwVersion()
	*/
	connect(unzipProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {

			ui.lw_logList->addItem("Porco dio: " + exitCode);

			if (exitStatus == QProcess::NormalExit && exitCode == 0) {

				QString newDataFolder = localTempFolder + "NewData/"; // Cartella temporanea per i file scaricati	
				newDataFolder = QDir::toNativeSeparators(newDataFolder); // Assicura il percorso corretto per Windows

				QFile indicatorFile(newDataFolder + "IndicatorAppu.bin");

				if (indicatorFile.exists()) {
					// Se il file esiste, crea una copia rinominatqa flash.bin

					QString newFilePath = newDataFolder + "flash.bin";
					if (indicatorFile.copy(newFilePath)) {
						if (listWidgetAlive) ui.lw_logList->addItem("IndicatorAppu.bin renominated in flash.bin.");
					}
					else {
						if (listWidgetAlive) ui.lw_logList->addItem("Error during copy in IndicatorAppu.bin " + indicatorFile.errorString());
						return;
					}
				}
				else {
					if (listWidgetAlive) ui.lw_logList->addItem("File IndicatorAppu.bin does not exist.");
					return;
				}
				if (listWidgetAlive) ui.lw_logList->addItem("ZIP extraction completed successfully.");
				readLocalFwVersion(newDataFolder);
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("Error during ZIP decompression: " + exitCode);
			}
			unzipProcess->deleteLater(); // Pulizia del processo
		});
	connect(unzipProcess, &QProcess::errorOccurred, this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);
	//ui.lw_logList->addItem("Dio porco, Porco dio, porco, madonn aputtana schifosa!");
	unzipProcess->start(scriptPath, arguments);

}

/*
*
 * @function readLocalFwVersion
 * @brief Legge le informazioni sui firmware installati e scaricati dalla cartella temporanea.
 * @param filesPath Il percorso della cartella contenente i file scaricati.
 * @return void
 * @details Questa funzione legge i file binari nella cartella specificata, estrae le informazioni sulla versione, data e ora,
 * e le memorizza in una lista di strutture swInfo. Poi confronta le informazioni con quelle installate e aggiorna la tabella.
 * @see swInfo, installedFwInfos, downloadedFwInfos, compareSwInfos(), updateFwTableWidget()
 */
void MultiTargetUpdaterv2::readLocalFwVersion(const QString& filesPath) {

	QStringList filenames;

	char version_buffer[VERSION_SIZE + 1];
	char data_buffer[DATA_SIZE + 1];
	char ora_buffer[ORA_SIZE + 1];

	// Controlla se la cartella non esiste
	if (!QDir(filesPath).exists()) {
		if (listWidgetAlive) ui.lw_logList->addItem("Error: temporary folder does not exist: " + filesPath);
		//return;
	}
	// Controlla se ci sono file nella cartella
	if (QDir(filesPath).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
		if (listWidgetAlive) ui.lw_logList->addItem("Error: no files found in the temporary folder: " + filesPath);
		//return;
	}
	else {
		// Riempio una lista con i file presenti nella cartella
		filenames = QDir(filesPath).entryList(QDir::Files | QDir::NoDotAndDotDot);
	}
	downloadedFwInfos.clear();
	// Legge le informazioni SW dai file scaricati e li inserisce nella lista downloadedFwInfos
	for (const QString& filename : filenames) {
		swInfo info;
		// Apro il file binario al path localDataFolder + filename
		QFile file(filesPath + filename);
		if (file.open(QIODevice::ReadOnly)) {
			QByteArray buffer;
			QByteArray overlayBuffer;
			uint64_t offset = 0;
			QDataStream reader(&file);
			qDebug() << "Dimensione file: " << file.size();
			reader.setByteOrder(QDataStream::LittleEndian);
			reader.setFloatingPointPrecision(QDataStream::SinglePrecision);

			while (!file.atEnd()) {
				buffer = file.read(65536);
				QByteArray combinedBuffer = overlayBuffer + buffer; // Combina il buffer precedente con quello corrente	

				int index = combinedBuffer.indexOf(QByteArray::fromHex("53573A2D")); // Cerca la sequenza 53 57 3A 2D
				if (index != -1) {
					offset = offset - overlayBuffer.size() + index; // Restituisce l'offset della sequenza trovata
					break;
				}
				else {
					if (combinedBuffer.size() > 65536) {
						// Se il buffer combinato supera i 65536 byte, mantieni solo gli ultimi 65536 byte
						overlayBuffer = combinedBuffer.right(65536);
					}
					else
						overlayBuffer = combinedBuffer; // Mantieni il buffer corrente se non trovi la sequenza
				}

				offset += buffer.size(); // Aggiorna l'offset con la dimensione del buffer corrente
			}
			file.seek(offset);
			file.read(version_buffer, 64);
			file.seek(offset + 64);
			file.read(data_buffer, 16);
			file.seek(offset + 64 + 16);
			file.read(ora_buffer, 16);
			file.close();


			version_buffer[64] = '\0';
			data_buffer[16] = '\0';
			ora_buffer[16] = '\0';

			info.version = QString::fromLatin1(version_buffer);
			info.data = QString::fromLatin1(data_buffer);
			info.ora = QString::fromLatin1(ora_buffer);
			info.deviceName = filename;
			info.fileLocation = filesPath + filename;
			info.deviceName.remove(".bin");
			downloadedFwInfos.append(info); // Aggiungo l'informazione alla lista deviceFwList
		}
	}

	compareSwInfos(installedFwInfos, downloadedFwInfos);
	updateFwTableWidget(); // Aggiorna la tabella con le informazioni sui firmware installati e scaricati

}

/*
* @function on_pushButtonClicked_loadFromLocalFolder
* @brief Gestisce il click del pulsante per caricare i file da una cartella locale.
* @param void
* @return void
* @details Questa funzione viene chiamata quando l'utente clicca sul pulsante per caricare i file da una cartella locale.
* @see readLocalFwVersion(), compareSwInfos(), updateFwTableWidget()
*/
void MultiTargetUpdaterv2::on_pushButtonClicked_loadFromLocalFolder() {

	cleanLogsView();

	QString localDataFolder = exeAppPath + "/data/temp/remotedata/newfirmware/loadedData/"; // Cartella temporanea per i file scaricati

	QString loadDataFolder = QFileDialog::getExistingDirectory(this, "Select Local Data Folder");
	if (loadDataFolder.isEmpty()) {
		if (listWidgetAlive) ui.lw_logList->addItem("No folder selected.");
		return;
	}
	loadDataFolder = QDir::toNativeSeparators(loadDataFolder); // Assicura il percorso corretto per Windows
	// Creo la cartella se non esiste
	if (!QDir(localDataFolder).exists()) {
		if (!QDir().mkpath(localDataFolder)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to create the local data folder: " + localDataFolder);
			return;
		}
	}
	else {
		// Se ci sono file nella cartella, li rimuove
		QStringList files = QDir(localDataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
		foreach(const QString & file, files) {
			QFile::remove(localDataFolder + "/" + file);
		}
		// salva dentro localDataFolder i file presenti in loadDataFolder
		QStringList loadedFiles = QDir(loadDataFolder).entryList(QDir::Files | QDir::NoDotAndDotDot);
		foreach(const QString & file, loadedFiles) {
			QFile::copy(loadDataFolder + "/" + file, localDataFolder + "/" + file);
		}
	}
	readLocalFwVersion(localDataFolder); // Legge la versione del firmware locale
	compareSwInfos(installedFwInfos, downloadedFwInfos);
	updateFwTableWidget(); // Aggiorna la tabella con le informazioni sui firmware installati e scaricati
}

/*
 * @function updateFirmware 
 * @brief Aggiorna il firmware del dispositivo selezionato.
 * @param swInfos Lista delle informazioni SW installate.
 * @param deviceFwList Lista delle informazioni SW scaricate.
 * @return void
 * @details Questa funzione crea uno script per WinSCP che si connette al server FTP e carica i file binari necessari per l'aggiornamento del firmware.
 * Il processo di creazione dello script di caricamento dipende dal tipo di target (flash, nand, spifi, CAN, COM o altro).
 * @see writeScriptFile(), enableActionButton(), checkUpdate()
 */

void MultiTargetUpdaterv2::updateFirmware(QString deviceName, QString updateFilePath) {


	// Directory per i file di update
	QString updateFileDirPath = exeAppPath + "/data/temp/remotedata/updateFiles/";
	QDir updateFileDir(updateFileDirPath);
	if (!updateFileDir.exists()) {
		// crea la directory se non esiste
		if (!updateFileDir.mkpath(updateFileDirPath)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Error: impossible to create the update files directory: " + updateFileDirPath);
			return;
		}
	}

	QString scriptFilePath = exeAppPath + "/fwUpdate_script.txt"; // Percorso del file di script
	QString logFilePath = exeAppPath + "/fwUpdate_log.txt"; // Percorso del file di log
	// Taglio il percorso del file di aggiornamento per ottenere il percorso della cartella
	QString fileFolder = updateFilePath.left(updateFilePath.lastIndexOf(QDir::separator())) + "\\"; // Cartella del file di aggiornamento
	QString destinationFolder;
	destinationFolder = QString("/!%1/")
		.arg(
			deviceName
		); // Cartella di destinazione sul server FTP

	// Inizializzo il file di scrittura di test
	QString testFile;
	testFile = QString("%1update%2.bin")
		.arg(
			updateFileDirPath,
			deviceName.toLower()
		);

	// Creo il file
	QFile updateFile(QDir::toNativeSeparators(testFile));
	if (!updateFile.open(QIODevice::WriteOnly)) {
		if (listWidgetAlive) ui.lw_logList->addItem("Error opening update file: " + updateFile.errorString());
		emit enableActionButton(deviceName);
		return;
	}
	updateFile.close();

	QFileInfo updateFileInfo(testFile);

	QString writeScript;
	QString remoteUpdateFileName;

	if (deviceName.contains("flash")) {

		remoteUpdateFileName = updateFileInfo.fileName().toLower();

		writeScript = QString(
			"open ftp://%1:%2@%3:%4/\n"
			"option batch continue\n"
			"put \"%5\" %6IndicatorAppl.bin\n"
			"put \"%7\" %6IndicatorAppu.bin\n" // Carica il file binario con nome IndicatorAppu.bin e con nome deviceName.bin
			"put \"%8\" %9\n" // Carica il file binario con nome deviceName.bin
			"close\n"
			"exit\n"
		).arg(
			userName,
			password,
			hostName,
			port,
			fileFolder + "IndicatorAppl.bin", // Percorso locale del file binario da caricare
			destinationFolder, // Percorso remoto dove caricare il file binario sul server FTP
			fileFolder + "IndicatorAppu.bin",
			QDir::toNativeSeparators(updateFile.fileName()),
			remoteUpdateFileName // Rimuove il percorso locale e mantiene solo il nome del file
		);
	}
	else if (deviceName.contains("nand", Qt::CaseInsensitive) || deviceName.contains("spifi", Qt::CaseInsensitive)) {

		remoteUpdateFileName = updateFileInfo.fileName();

		writeScript = QString(
			"open ftp://%1:%2@%3:%4/\n"
			"option batch continue\n"
			"put \"%5\" %6.bin\n"
			"put \"%7\" %8\n"
			"close\n"
			"exit\n"
		).arg(
			userName,
			password,
			hostName,
			port,
			updateFilePath, // Percorso locale del file binario da caricare"
			destinationFolder + deviceName, // Percorso remoto dove caricare il file binario sul server FTP
			QDir::toNativeSeparators(updateFile.fileName()),
			remoteUpdateFileName
		);
	}
	else {
		// Rinomina il file di aggiornamento in update.bin se non è un file flash, nand o spifi
		QString renamedFile = updateFileDirPath + "update.bin";
		if (QFile::exists(renamedFile)) {
			QFile::remove(renamedFile);
		}
		if (!QFile::rename(testFile, renamedFile)) {
			qWarning() << "Errore";
		}
		else {
			qDebug() << "File rinominato in:" << renamedFile;
		}
		qDebug() << "Filename: " << QDir::toNativeSeparators(updateFile.fileName());

		QRegularExpression re("([a-zA-Z]+)(\\d+)");

		destinationFolder = QString(
			"/!%1_DEV_%2/"
		).arg(
			re.match(deviceName).captured(1).toUpper(),
			re.match(deviceName).captured(2)
		);
		remoteUpdateFileName = destinationFolder + "update.bin";
		writeScript = QString(
			"open ftp://%1:%2@%3:%4/\n"
			"option batch continue\n"
			"put \"%5\" %6\n"
			"put \"%7\" %8\n"
			"close\n"
			"exit\n"
		).arg(
			userName,
			password,
			hostName,
			port,
			updateFilePath, // Percorso locale del file binario da caricare"
			destinationFolder + "flash.bin", // Percorso remoto dove caricare il file binario sul server FTP
			QDir::toNativeSeparators(renamedFile),
			remoteUpdateFileName
		);
	}

	// Scrive il contenuto dello script su un file
	QFile scriptFile(exeAppPath + "/fwUpdate_script.txt");
	if (!writeScriptFile(writeScript, scriptFile.fileName())) {
		qDebug() << "Failed to write script file.";
		return;
	}

	QProcess* fwUpdateProcess = new QProcess(this);
	// Connetti il segnale finished del processo per gestire il completamento dell'aggiornamento

	/*
	* @brief Connetti il segnale finished del processo per gestire il completamento dell'aggiornamento
	* se avvenuto con successo, chiama la funzione checkUpdate per verificare l'aggiornamento del firmware.
	* @see checkUpdate()
	*/
	connect(fwUpdateProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {

			if (exitStatus == QProcess::NormalExit && exitCode == 0) {

				if (listWidgetAlive) ui.lw_logList->addItem("Firmware update process finished successfully.");
				checkUpdate(remoteUpdateFileName, remoteUpdateFileName);

			}
			else {

				if (listWidgetAlive) ui.lw_logList->addItem("Error during firmware update process code: " + QString::number(exitCode));

			}
			QFile::remove(scriptFilePath); // Remove the script after execution
			fwUpdateProcess->deleteLater(); // Clean up the process
			updateProcess = false;
			emit enableActionButton(deviceName);

		});

	connect(fwUpdateProcess, &QProcess::errorOccurred, this, &MultiTargetUpdaterv2::onWinSCPErrorOccurred);

	fwUpdateProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath);
}

/*
* @function checkUpdate
* @brief Controlla l'aggiornamento del firmwaresu dipsositivo selezionato.
* @param updateFileName Il nome del file di aggiornamento da controllare.
* @param binFolder La cartella in cui si trova il file di aggiornamento
* @return void
* @details Questa funzione crea uno script per WinSCP che si connette al server FTP e scarica il file di aggiornamento. 
* Questa è un'operazione fittizia replicata dal software originale, solo per perdere tempo.
* @see writeScriptFile()
*/
void MultiTargetUpdaterv2::checkUpdate(QString updateFileName, QString binFolder) {
	QString scriptFilePath = exeAppPath + "/updateCheck_script.txt"; // Percorso del file di script
	QString logFilePath = exeAppPath + "/updateCheck_log.txt"; // Percorso del file di log
	QString checkFilePath = binFolder + "dummy.bin";
	// Ora si crea un nuovo script per lanciare un nuovo processo in lettura 
	QString readScript;
	readScript = QString(
		"open ftp://%1:%2@%3:%4/\n"
		"option batch continue\n"
		"get /%5 \"%6\"\n"
		"get /%5 \"%6\"\n"
		"get /%5 \"%6\"\n"
		"get /%5 \"%6\"\n"
		"get /%5 \"%6\"\n"
		"close\n"
		"exit\n"
	).arg(
		userName,
		password,
		hostName,
		port,
		updateFileName, // Percorso remoto dove caricare il file binario sul server FTP
		checkFilePath // Percorso locale del file binario da scaricare
	);

	QFile scriptFile(scriptFilePath);
	if (!writeScriptFile(readScript, scriptFile.fileName())) {
		qDebug() << "Failed to write script file.";
		return;
	}

	QProcess* loadingCheckProcess = new QProcess(this);
	// Connetti il segnale finished del processo per gestire il completamento del caricamento
	connect(loadingCheckProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
			if (exitStatus == QProcess::NormalExit && exitCode == 0) {

				if (listWidgetAlive) ui.lw_logList->addItem("Loading check process finished successfully.");
				// Handle successful loading check logic here
			}
			else {
				if (listWidgetAlive) ui.lw_logList->addItem("Loading check process failed with exit code: " + exitCode);
				// Handle failure logic here
			}
			QFile::remove(scriptFilePath); // Remove the script after execution
			loadingCheckProcess->deleteLater(); // Clean up the process
			// Cerca se il file temp esiste o qualsiasi altro file che contiene il prefisso update e li rimuove

		});

	loadingCheckProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath);
}

/*
 * @slot on_pushButtonClicked_startApplication
 * @brief Gestisce il click del pulsante per avviare l'applicazione sul dispositivo.
 * @param void
 * @return void
 * @details Questa funzione viene chiamata quando l'utente clicca sul pulsante per avviare il processo di boot del dispositivo.
 * Crea un file di script per WinSCP che carica il file boot.bin sul dispositivo questo permette di avviare il dispositivo.
 * @see writeScriptFile()
 */ 
void MultiTargetUpdaterv2::on_pushButtonClicked_startApplication() {

	cleanLogsView();

	if (isConnected) {

		QString scriptFilePath = exeAppPath + "/startApp_script.txt"; // Percorso del file di script
		QString logFilePath = exeAppPath + "/startApp_log.txt"; // Percorso del file di log

		// Creo un file boot.bin nella cartella del progetto corrente
		QString currentFolder = exeAppPath;
		QString bootFilePath = currentFolder + "/boot.bin";
		bootFilePath = QDir::toNativeSeparators(bootFilePath);
		QFile bootFile(bootFilePath);
		if (!bootFile.open(QIODevice::WriteOnly)) {
			if (listWidgetAlive) ui.lw_logList->addItem("Error opening boot file: " + bootFile.errorString());
			return;
		}
		bootFile.close();
		// Creo il file di script FTP per avviare l'applicazione
		QString startAppScript;
		startAppScript = QString(
			"open ftp://%1:%2@%3:%4/\n"
			"option batch continue\n"
			"put \"%5\" /boot.bin\n"
			"close\n"
			"exit\n"
		).arg(
			userName,
			password,
			hostName,
			port,
			QDir::toNativeSeparators(bootFilePath) // Percorso locale del file binario da caricare
		);
		QFile scriptFile(scriptFilePath);
		if (!writeScriptFile(startAppScript, scriptFile.fileName())) {
			qDebug() << "Failed to write script file.";
			return;
		}

		QProcess* startAppProcess = new QProcess(this);
		// Connetti il segnale finished del processo per gestire il completamento del caricamento
		connect(startAppProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
			this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
				if (exitStatus == QProcess::NormalExit && exitCode == 0) {

					if (listWidgetAlive) ui.lw_logList->addItem("Applicationin starting mode.");
					// Handle successful loading check logic here
				}
				else {
					if (listWidgetAlive) ui.lw_logList->addItem("Fail to start application: " + exitCode);
					// Handle failure logic here
				}
				QFile::remove(startAppScript); // Remove the script after execution
				startAppProcess->deleteLater(); // Clean up the process
				// Cerca se il file temp esiste o qualsiasi altro file che contiene il prefisso update e li rimuove

			});

		startAppProcess->start("winscp", QStringList() << "/script=" + scriptFilePath << "/loglevel=2" << "/log=" + logFilePath);
	}
	else {
		if (listWidgetAlive) ui.lw_logList->addItem("Please connect to the device before starting the application.");
		return;
	}
}

/*
 * @cleanLogsView
 * @brief Pulisce la log view.
 * @param void
 * @return void
 * @details Questa funzione viene chiamata per pulire la log view quando si inizia un nuovo processo di aggiornamento o caricamento.
 */
void MultiTargetUpdaterv2::cleanLogsView() {
	// Pulisce la log view
	if (listWidgetAlive) ui.lw_logList->clear();
}

/*
 * @slot on_pushButtonClicked_exportLog
 * @brief Gestisce il click del pulsante per esportare i log.
 * @param void
 * @return void
 * @details Questa funzione viene chiamata quando l'utente clicca sul pulsante per esportare il file di log.
 */
void MultiTargetUpdaterv2::on_pushButtonClicked_exportLog() {

	QString filePath = QFileDialog::getExistingDirectory(this,
		"Select destination directory",
		QDir::homePath(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (!filePath.isEmpty()) {
		emit exportlog_signal(filePath);
	}
}

/*
 * @slot printLogMessage
 * @brief Stampa un messaggio di log nella log view.
 * @param message Il messaggio da stampare.
 * @return void
 * @see listWidgetAlive
 */
void MultiTargetUpdaterv2::printLogMessage(const QString& message) {

	if (listWidgetAlive) {
		ui.lw_logList->addItem(message);
	}
	else {
		qDebug() << "merda";
	}
}