#ifndef BATCHPARAMDIALOG_H
#define BATCHPARAMDIALOG_H

#include <QDialog>
#include "models/connectiontypes.h"
#include "dialogs/batchparamentry.h"
#include "util/modbusdatatype.h"

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
    void onHexDisplayToggled(bool checked);

private:
    void loadCsvToTable(const QString& filePath);
    void saveTableToCsv(const QString& filePath);

    void startNextRead();
    void startNextWrite();
    void finishBatch();
    void setBatchButtonsEnabled(bool enabled);

    ModbusDataType::Type rowType(int row) const;

    static quint32 parseHexOrDec32(const QString& text, bool* ok = nullptr);
    static quint32 parseDisplayedValue(const QString& text, ModbusDataType::Type type,
                                       bool currentHexMode, bool* ok = nullptr);
    static bool encodeToWords(const QString& text, ModbusDataType::Type type,
                              quint16& word1, quint16& word2);
    static QString formatValue(quint32 rawValue, ModbusDataType::Type type, bool hexMode);
    static QString formatAddress(quint16 addr, bool hexMode);

    static QString typeToLabel(ModbusDataType::Type type);
    static bool labelToType(const QString& label, ModbusDataType::Type& type);

    Ui::BatchParamDialog* ui;
    ModbusPoll* _pModbusPoll;

    bool _batchActive{false};
    bool _batchIsWrite{false};
    bool _stopRequested{false};
    int  _currentBatchIndex{0};
    int  _totalBatchRows{0};
};

#endif // BATCHPARAMDIALOG_H
