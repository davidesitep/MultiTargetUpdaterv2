#pragma once  
/*  
* logger.h  
* This file is part of MultiTargetUpdaterv2  
* Esegue il log degli eventi del programma  
*/  
#include <QObject>  
#include <QString>  
#include <QFile>  
#include <QDebug>  
#include <QTextStream>  
#include <QDir>

// Aggiunta di un controllo per evitare problemi con la macro VCR102  
#ifndef LOGGER_H  
#define LOGGER_H  

/**
* @class logger
* @brief Classe per la gestione del logging degli eventi del programma.
*/

class logger : 
    public QObject  
{  
    Q_OBJECT  

public:   
    // Inizializza il logger  
    logger(const QString logDirPath);  
    ~logger();  

    bool logFileCreated;
    // Ferma il logger e chiude il file di log  
    void stop();  

private:  
    QFile* logFile; 
    QString logFilePath;
    

public slots:  
    // Registra un messaggio di log  
    void log(const QString message); 
	void exportlog(const QString destinationPath);

signals:
    void printLogMessage(const QString& message);
};  

#endif // LOGGER_H
