#ifndef BATCHPARAMDIALOG_H
#define BATCHPARAMDIALOG_H

#include <QDialog>
#include "models/connectiontypes.h"
#include "dialogs/batchparamentry.h"

namespace Ui {
class BatchParamDialog;
}

class ModbusPoll;

class BatchParamDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BatchParamDialog(ModbusPoll* pModbusPoll, QWidget* parent = nullptr);
    ~BatchParamDialog();

    void setConnection(ConnectionTypes::connectionId_t connId, quint8 slaveId);

private slots:
    void onLoadCsvClicked();
    void onSaveCsvClicked();
    void onAddRowClicked();
    void onRemoveRowClicked();
    void onBatchReadClicked();
    void onBatchWriteClicked();
    void onStopClicked();

    void onBatchReadStepDone(bool success, const QString& errorMsg, quint16 word1, quint16 word2);
    void onBatchWriteStepDone(bool success, const QString& errorMsg, quint16 word1, quint16 word2);

private:
    void loadCsvToTable(const QString& filePath);
    void saveTableToCsv(const QString& filePath);

    void startNextRead();
    void startNextWrite();
    void finishBatch();
    void setBatchButtonsEnabled(bool enabled);

    static quint32 parseHexOrDec32(const QString& text, bool* ok = nullptr);
    static QString formatValue(quint32 value, bool is32Bit);

    Ui::BatchParamDialog* ui;
    ModbusPoll* _pModbusPoll;

    bool _batchActive{false};
    bool _batchIsWrite{false};
    bool _stopRequested{false};
    int  _currentBatchIndex{0};
    int  _totalBatchRows{0};
};

#endif // BATCHPARAMDIALOG_H
