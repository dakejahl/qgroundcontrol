#include "OsdController.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>
#include <QtCore/QUrl>

#include <limits>

#include "FTPManager.h"
#include "Fact.h"
#include "ParameterManager.h"
#include "Vehicle.h"

namespace {
QString localPath(const QString& file)
{
    const QUrl url(file);
    return url.isLocalFile() ? url.toLocalFile() : file;
}
}  // namespace

OsdController::OsdController(Vehicle* vehicle, QObject* parent) : QObject(parent), _vehicle(vehicle), _layout(this)
{
    _reloadTimer.setSingleShot(true);
    _reloadTimer.setInterval(15000);
    connect(&_reloadTimer, &QTimer::timeout, this, [this]() {
        _finish(tr("File uploaded, but the reload parameter was not acknowledged. Retry saving to request reload."));
    });
    connect(&_layout, &OsdLayout::changed, this, &OsdController::stateChanged);
    if (!vehicle) {
        _updateLock();
        return;
    }
    connect(vehicle->ftpManager(), &FTPManager::downloadComplete, this, &OsdController::_downloadComplete);
    connect(vehicle->ftpManager(), &FTPManager::uploadComplete, this, &OsdController::_uploadComplete);
    connect(vehicle->ftpManager(), &FTPManager::commandProgress, this, [this](float progress) {
        if (busy()) {
            _progress = progress;
            emit stateChanged();
        }
    });
    connect(vehicle, &Vehicle::armedChanged, this, [this](bool armed) {
        if (armed && (_operation == Operation::UploadLayout || _operation == Operation::UploadFirmware)) {
            cancel();
        }
        _updateLock();
        emit stateChanged();
    });
    connect(vehicle->parameterManager(), &ParameterManager::parametersReadyChanged, this, &OsdController::stateChanged);
}

bool OsdController::setMetadata(const QJsonObject& metadata, QString& error)
{
    if (!OsdLayout::validateMetadata(metadata, error)) {
        return false;
    }
    if (_displays == metadata["displays"].toArray() && available()) {
        return true;
    }
    if (busy() || _layout.dirty()) {
        error = tr("Cannot replace OSD metadata while editing or transferring a layout.");
        return false;
    }
    _displays = metadata["displays"].toArray();
    _displayIndex = -1;
    _ready = false;
    _layout.load({}, {});
    _updateLock();
    emit metadataChanged();
    emit stateChanged();
    return true;
}

QJsonObject OsdController::_display() const
{
    return _displayIndex < 0 || _displayIndex >= _displays.size() ? QJsonObject{} : _displays[_displayIndex].toObject();
}

Fact* OsdController::_parameter(const QString& name) const
{
    if (!_vehicle || name.isEmpty()) {
        return nullptr;
    }
    ParameterManager* parameters = _vehicle->parameterManager();
    if (!parameters->parametersReady() || !parameters->parameterExists(MAV_COMP_ID_AUTOPILOT1, name)) {
        return nullptr;
    }
    return parameters->getParameter(MAV_COMP_ID_AUTOPILOT1, name);
}

Fact* OsdController::enableFact() const
{
    return _parameter(_display()["enable"].toObject()["param"].toString());
}

bool OsdController::canSave() const
{
    return _vehicle && !_vehicle->armed() && _ready && !busy() && _layout.errors().isEmpty() &&
           _parameter(_display()["reload"].toObject()["param"].toString());
}

void OsdController::_updateLock()
{
    _layout.setLocked(!_vehicle || _vehicle->armed() || busy() || !_ready);
}

void OsdController::selectDisplay(int index)
{
    if (busy() || index < 0 || index >= _displays.size()) {
        return;
    }
    if (_layout.dirty()) {
        _finish(tr("Save or revert your changes before switching displays."));
        return;
    }
    _displayIndex = index;
    _ready = false;
    _layout.load(_display(), {});
    fetch();
}

void OsdController::fetch()
{
    if (!_vehicle || busy() || _display().isEmpty()) {
        return;
    }
    if (_layout.dirty()) {
        _finish(tr("Save or revert your changes before fetching the layout."));
        return;
    }
    if (!_temporaryDir.isValid()) {
        _finish(tr("Cannot create a temporary directory for OSD files."));
        return;
    }
    _ready = false;
    _operation = Operation::Download;
    _progress = 0;
    _status = tr("Fetching layout…");
    _updateLock();
    emit stateChanged();
    if (!_vehicle->ftpManager()->download(MAV_COMP_ID_AUTOPILOT1, _display()["layout"].toObject()["uri"].toString(),
                                          _temporaryDir.path(), QStringLiteral("layout.json"))) {
        _finish(tr("Cannot start OSD download. Another file transfer may be active."));
    }
}

void OsdController::_downloadComplete(const QString& file, const QString& error)
{
    if (_operation != Operation::Download) {
        return;
    }
    // FTPManager normalizes PX4's ENOENT and MAVFTP FileNotFound to the same error suffix.
    if (error.endsWith(MavlinkFTP::errorCodeToString(MavlinkFTP::kErrFailFileNotFound))) {
        _layout.load(_display(), _display()["defaults"].toObject());
        _ready = true;
        _finish(tr("No saved layout. Showing the driver's compiled defaults."));
        return;
    }
    if (!error.isEmpty()) {
        _finish(tr("Layout download failed: %1").arg(error));
        return;
    }
    QFile input(file);
    if (!input.open(QIODevice::ReadOnly)) {
        _finish(input.errorString());
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (!document.isObject() || document.object()["version"].toInt() != 1 ||
        document.object()["display"] != _display()["id"]) {
        _layout.load(_display(), document.object());
        _ready = true;
        _finish(
            tr("The downloaded layout is invalid or belongs to a different display. Load Defaults or import a valid "
               "layout."));
        return;
    }
    _layout.load(_display(), document.object());
    _ready = true;
    _finish(tr("Layout downloaded."));
}

void OsdController::save()
{
    if (!canSave()) {
        return;
    }
    const QString path = _temporaryDir.filePath(QStringLiteral("upload.json"));
    QSaveFile output(path);
    const QByteArray bytes = QJsonDocument(_layout.document()).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        _finish(tr("Cannot write the temporary layout: %1").arg(output.errorString()));
        return;
    }
    _upload(path, false);
}

void OsdController::_upload(const QString& file, bool firmware)
{
    if (!_vehicle || _vehicle->armed() || busy() || !_parameter(_display()["reload"].toObject()["param"].toString())) {
        return;
    }
    const QString uri = _display()[firmware ? "firmware" : "layout"].toObject()["uri"].toString();
    if (OsdLayout::writablePath(uri).isEmpty()) {
        _finish(tr("Invalid OSD upload destination."));
        return;
    }
    _savingLayout = !firmware;
    _operation = firmware ? Operation::UploadFirmware : Operation::UploadLayout;
    _progress = 0;
    _status = firmware ? tr("Uploading board firmware…") : tr("Uploading layout…");
    _updateLock();
    emit stateChanged();
    if (!_vehicle->ftpManager()->upload(MAV_COMP_ID_AUTOPILOT1, uri, file, true)) {
        _finish(tr("Cannot start OSD upload. Another file transfer may be active."));
    }
}

void OsdController::_uploadComplete(const QString& file, const QString& error)
{
    Q_UNUSED(file)
    if (_operation != Operation::UploadLayout && _operation != Operation::UploadFirmware) {
        return;
    }
    if (!error.isEmpty()) {
        _finish(tr("OSD upload failed: %1").arg(error));
        return;
    }
    Fact* reload = _parameter(_display()["reload"].toObject()["param"].toString());
    if (!_vehicle || _vehicle->armed() || !reload) {
        _finish(tr("File uploaded, but reload is unavailable. Disarm and retry saving."));
        return;
    }
    const qint64 generation = reload->rawValue().toLongLong();
    const int next = generation >= std::numeric_limits<int>::max() ? 0 : static_cast<int>(generation + 1);
    _operation = Operation::Reload;
    _status = tr("File uploaded. Requesting reload…");
    _reloadConnection = connect(reload, &Fact::vehicleUpdated, this, [this, next](const QVariant& value) {
        if (_operation != Operation::Reload || value.toInt() != next) {
            return;
        }
        if (_savingLayout) {
            _layout.markSaved();
        }
        _finish(_savingLayout
                    ? tr("Layout saved; reload requested. The driver does not report whether it applied the layout.")
                    : tr("Image uploaded; update requested. The driver does not report board update progress."));
    });
    _reloadTimer.start();
    reload->setRawValue(next);
    emit stateChanged();
}

void OsdController::_finish(const QString& status)
{
    _reloadTimer.stop();
    disconnect(_reloadConnection);
    _operation = Operation::None;
    _status = status;
    _updateLock();
    emit stateChanged();
}

void OsdController::cancel()
{
    if (!_vehicle) {
        return;
    }
    if (_operation == Operation::Download) {
        _vehicle->ftpManager()->cancelDownload();
    } else if (_operation == Operation::UploadLayout || _operation == Operation::UploadFirmware) {
        _vehicle->ftpManager()->cancelUpload();
    } else if (_operation == Operation::Reload) {
        _finish(tr("Stopped waiting for reload acknowledgement. The file was uploaded."));
    }
}

void OsdController::importLayout(const QString& file)
{
    if (_layout.locked()) {
        return;
    }
    QFile input(localPath(file));
    if (!input.open(QIODevice::ReadOnly)) {
        _finish(input.errorString());
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll());
    const QStringList errors = OsdLayout::validateLayout(_display(), document.object());
    if (!document.isObject() || !errors.isEmpty()) {
        _finish(tr("Cannot import layout: %1").arg(errors.join('\n')));
        return;
    }
    // Keep the vehicle baseline so Revert still restores the last fetched/saved document.
    _layout.replaceDocument(document.object());
    _finish(tr("Layout imported. Save to send it to the vehicle."));
}

void OsdController::exportLayout(const QString& file)
{
    if (!_ready) {
        return;
    }
    QSaveFile output(localPath(file));
    const QByteArray bytes = QJsonDocument(_layout.document()).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        _status = output.errorString();
    } else {
        _status = tr("Layout exported.");
    }
    emit stateChanged();
}

void OsdController::updateFirmware(const QString& file)
{
    if (!_vehicle || _vehicle->armed() || busy() || !_display()["features"].toArray().contains("firmware")) {
        return;
    }
    QFile input(localPath(file));
    if (!input.open(QIODevice::ReadOnly) || input.size() < 8 || input.size() > 2 * 1024 * 1024) {
        _finish(tr("Cannot read the OSD firmware image."));
        return;
    }
    // RP2350 picobin start marker; reject unrelated files before transferring them.
    if (!input.read(4096).contains(QByteArray::fromHex("d3deffff"))) {
        _finish(tr("The file is not an RP2350 OSD firmware image."));
        return;
    }
    _upload(localPath(file), true);
}
