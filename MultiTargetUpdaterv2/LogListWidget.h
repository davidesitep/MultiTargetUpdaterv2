#pragma once
#include <QListWidget>


/**
* @class LogListWidget
* @brief LogListWidget è una sottoclasse derivata da QListWidget che permette di aggiungere un segnale ogni volta che viene aggiunto un elemento al widget.
* Questo permette di segnalare ad un logger esterno gli eventi di log generati.
* Il metodo addItem è stato sovrascritto per emettere un segnale itemAdded con il testo dell'elemento aggiunto.
* @see Qusta classe è utilizzata da MultiTargetUpdaterv2 per segnalare gli aventi di log alla classe logger.
*/
class LogListWidget :
    public QListWidget
{
    Q_OBJECT
public:
    using QListWidget::QListWidget;

    void addItem(QListWidgetItem* item, const QString& text) {
        QListWidget::addItem(item);
        emit itemAdded(text);
    }

    void addItem(const QString& text) {
        auto* item = new QListWidgetItem(text);
        addItem(item, text);
    }

signals:
    void itemAdded(const QString& text);

};

