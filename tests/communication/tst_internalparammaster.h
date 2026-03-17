
#include <QObject>

#include "models/settingsmodel.h"
#include "testdevice.h"

#ifndef TST_INTERNALPARAMMASTER_H
#define TST_INTERNALPARAMMASTER_H

class TestInternalParamMaster : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void readParam16bitSuccess();
    void readParam32bitSuccess();
    void writeParam16bitSuccess();
    void writeParam32bitSuccess();

private:
    SettingsModel* _pSettingsModel;
    TestDevice* _pTestDevice;
};

#endif // TST_INTERNALPARAMMASTER_H
