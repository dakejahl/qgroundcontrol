#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QPointer>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtQmlIntegration/QtQmlIntegration>

#include "OsdLayout.h"

class Fact;
class Vehicle;

/// Uses the owning vehicle's FTP session and Facts; transfers survive closing the setup page.
class OsdController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Vehicle")
    Q_MOC_INCLUDE("Fact.h")
    Q_PROPERTY(QVariantList displays READ displays NOTIFY metadataChanged)
    Q_PROPERTY(OsdLayout* layout READ layout CONSTANT)
    Q_PROPERTY(int displayIndex READ displayIndex NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY metadataChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(Fact* enableFact READ enableFact NOTIFY stateChanged)

public:
    explicit OsdController(Vehicle* vehicle, QObject* parent = nullptr);
    bool setMetadata(const QJsonObject& metadata, QString& error);

    QVariantList displays() const { return _displays.toVariantList(); }

    OsdLayout* layout() { return &_layout; }

    int displayIndex() const { return _displayIndex; }

    bool available() const { return !_displays.isEmpty(); }

    bool busy() const { return _operation != Operation::None; }

    bool ready() const { return _ready; }

    bool canSave() const;

    QString status() const { return _status; }

    double progress() const { return _progress; }

    Fact* enableFact() const;

    Q_INVOKABLE void selectDisplay(int index);
    Q_INVOKABLE void fetch();
    Q_INVOKABLE void save();
    Q_INVOKABLE void importLayout(const QString& file);
    Q_INVOKABLE void exportLayout(const QString& file);
    Q_INVOKABLE void updateFirmware(const QString& file);
    Q_INVOKABLE void cancel();

signals:
    void metadataChanged();
    void stateChanged();

private:
    enum class Operation
    {
        None,
        Download,
        UploadLayout,
        UploadFirmware,
        Reload
    };
    Fact* _parameter(const QString& name) const;
    QJsonObject _display() const;
    void _downloadComplete(const QString& file, const QString& error);
    void _uploadComplete(const QString& file, const QString& error);
    void _upload(const QString& file, bool firmware);
    void _finish(const QString& status);
    void _updateLock();

    QPointer<Vehicle> _vehicle;
    OsdLayout _layout;
    QJsonArray _displays;
    QTemporaryDir _temporaryDir;
    QTimer _reloadTimer;
    QMetaObject::Connection _reloadConnection;
    int _displayIndex = -1;
    Operation _operation = Operation::None;
    bool _ready = false;
    bool _savingLayout = false;
    QString _status;
    double _progress = 0;
};
