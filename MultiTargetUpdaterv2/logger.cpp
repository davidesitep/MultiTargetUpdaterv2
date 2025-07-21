#include "logger.h"

logger::logger(const QString logDirPath) {

	logFilePath = logDirPath + "log.txt";
	// Se non esiste creo la cartella per i log file
	QDir dir(logDirPath);
	if (!dir.exists()) {
		if (!dir.mkpath(logDirPath)) {
			return;
		}
	}
	// Crea un file di log al percorso filename
	if (QFile::exists(logFilePath)) {
		logFile = new QFile(logFilePath);
		logFileCreated = true;
		return;
	}
	else {
		logFile = new QFile(logFilePath);
		if (!logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
			qDebug() << "Could not open log file for writing:" << logFilePath;
			return;
		}
		logFile->close();
	}

	logFileCreated = true;
}

logger::~logger() {
	// Il file di log viene chiuso automaticamente quando l'oggetto logger viene distrutto
}

/**
* @brief log - Scrive un messaggio di log nel file di log con un timestamp
* @param message - il messaggio da scrivere nel file di log
* @return void
* @details Questa funzione apre il file di log in modalità append, scrive il messaggio con un timestamp e poi chiude il file.
* Se il file non può essere aperto, emette un segnale di errore.
*/
void logger::log(const QString message) {

	QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
	QString logMessage = "[" + timestamp + "] - " + message;

	// Scrive il messaggio nel file di log
	if (!logFile->open(QIODevice::Append | QIODevice::Text)) {
		emit printLogMessage(("Could not open log file for appending: " + logFilePath));
		return;
	}
	else {
		QTextStream out(logFile);
		out << logMessage << "\n";
		logFile->close();
	}
}

void logger::stop() {

}

/**
* @brief Esporta il il file di testo di log in una directory specificata
* @param destinationPath - il percorso alla directory di destinazione di esportazione del file di log
* @return void
* @details Questa funzione copia il file di log esistente nella directory di destinazione specificata.
* Se il file di log non esiste, emette un segnale di errore. Se la copia ha successo, emette un messaggio di successo.*
*/
void logger::exportlog(const QString destinationPath) {
	
	QString destPath = QDir::toNativeSeparators(destinationPath + "/");

	if (!logFile->exists()) {
		emit printLogMessage(("Log file does not exist: " + logFilePath));
		return;
	}
	else {
		if (!QFile::copy(logFilePath, destPath + "log.txt")) {
						emit printLogMessage(("Failed to copy log file to destination: " + destPath));
			return;
		}
		else {
			emit printLogMessage(("Log file successfully copied to: " + destPath));
		}
	}
}