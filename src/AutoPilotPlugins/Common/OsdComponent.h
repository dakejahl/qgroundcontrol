#pragma once

#include "VehicleComponent.h"

class OsdController;

class OsdComponent : public VehicleComponent
{
    Q_OBJECT
    Q_MOC_INCLUDE("OsdController.h")
    Q_PROPERTY(OsdController* osdController READ osdController CONSTANT)

public:
    OsdComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent = nullptr);
    OsdController* osdController() const;

    QStringList setupCompleteChangedTriggerList() const override { return {}; }

    QString name() const override { return tr("OSD"); }

    QString description() const override { return tr("Configure on-screen display layouts."); }

    QString iconResource() const override { return QStringLiteral("/qmlimages/CameraComponentIcon.png"); }

    bool requiresSetup() const override { return false; }

    bool setupComplete() const override { return true; }

    QUrl setupSource() const override
    {
        return QUrl(QStringLiteral("qrc:/qml/QGroundControl/AutoPilotPlugins/Common/OsdComponent.qml"));
    }

    QUrl summaryQmlSource() const override { return {}; }
};
