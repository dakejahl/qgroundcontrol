#include "CompInfoOsd.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>

#include "OsdController.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

QGC_LOGGING_CATEGORY(CompInfoOsdLog, "ComponentInformation.CompInfoOsd")

CompInfoOsd::CompInfoOsd(uint8_t componentId, Vehicle* ownerVehicle, QObject* parent)
    : CompInfo(METADATA_TYPE, componentId, ownerVehicle, parent)
{}

void CompInfoOsd::setJson(const QString& file)
{
    if (!vehicle || file.isEmpty()) {
        return;
    }
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) {
        qCWarning(CompInfoOsdLog) << "Cannot read OSD metadata:" << input.errorString();
        return;
    }
    QString error;
    if (!vehicle->osdController()->setMetadata(QJsonDocument::fromJson(input.readAll()).object(), error)) {
        qCWarning(CompInfoOsdLog) << "Invalid OSD metadata:" << error;
    }
}
