#ifndef WRITEREGISTERDIALOG_H
#define WRITEREGISTERDIALOG_H

#include <QDialog>
#include "models/connectiontypes.h"

namespace Ui {
class WriteRegisterDialog;
}

class ModbusPoll;

class WriteRegisterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WriteRegisterDialog(ModbusPoll* pModbusPoll, QWidget* parent = nullptr);
    ~WriteRegisterDialog();

private slots:
    void onWriteClicked();
    void onWriteDone(bool success, const QString& errorMessage);

private:
    Ui::WriteRegisterDialog* ui;
    ModbusPoll* _pModbusPoll;
};

#endif // WRITEREGISTERDIALOG_H
