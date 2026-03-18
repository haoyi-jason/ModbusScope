#ifndef INTERNALPARAMDIALOG_H
#define INTERNALPARAMDIALOG_H

#include <QDialog>
#include "models/connectiontypes.h"

namespace Ui {
class InternalParamDialog;
}

class ModbusPoll;

class InternalParamDialog : public QDialog
{
    Q_OBJECT
public:
    explicit InternalParamDialog(ModbusPoll* pModbusPoll, QWidget* parent = nullptr);
    ~InternalParamDialog();

private slots:
    void onReadClicked();
    void onWriteClicked();
    void onModeChanged();

    void onReadDone(bool success, const QString& errorMsg, quint16 word1, quint16 word2);
    void onWriteDone(bool success, const QString& errorMsg, quint16 word1, quint16 word2);

    void onWriteKeyClicked();
    void onWriteKeyDone(bool success, const QString& errorMsg);

private:
    void setButtonsEnabled(bool enabled);
    bool is32Bit() const;
    ConnectionTypes::connectionId_t selectedConnId() const;
    static quint16 parseHexOrDec(const QString& text, bool* ok = nullptr);

    Ui::InternalParamDialog* ui;
    ModbusPoll* _pModbusPoll;
};

#endif // INTERNALPARAMDIALOG_H
