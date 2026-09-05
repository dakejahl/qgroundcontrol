#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

/// Metadata-driven layout document. Unknown interchange fields survive edits and saves.
class OsdLayout : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by OsdController")
    Q_PROPERTY(QVariantMap display READ display NOTIFY changed)
    Q_PROPERTY(QVariantMap canvas READ canvas NOTIFY changed)
    Q_PROPERTY(QVariantList elements READ elements NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(QStringList errors READ errors NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)

public:
    explicit OsdLayout(QObject* parent = nullptr);

    static bool validateMetadata(const QJsonObject& metadata, QString& error);
    static QStringList validateLayout(const QJsonObject& display, const QJsonObject& document);
    static QString writablePath(const QString& uri);

    void load(const QJsonObject& display, const QJsonObject& document);
    void markSaved();
    void replaceDocument(const QJsonObject& document);
    void setLocked(bool locked);

    QJsonObject document() const { return _document; }

    QJsonObject displayObject() const { return _display; }

    QVariantMap display() const { return _display.toVariantMap(); }

    QVariantMap canvas() const;
    QVariantList elements() const;

    bool dirty() const { return _document != _baseline; }

    bool locked() const { return _locked; }

    QStringList errors() const { return validateLayout(_display, _document); }

    Q_INVOKABLE void selectCanvas(const QString& id);
    Q_INVOKABLE int addElement(const QString& id);
    Q_INVOKABLE void removeElement(int index);
    Q_INVOKABLE void updateElement(int index, const QVariantMap& values);
    Q_INVOKABLE void moveElement(int index, int x, int y, bool snap);
    Q_INVOKABLE void revert();
    Q_INVOKABLE void loadDefaults();

signals:
    void changed();

private:
    QJsonObject _display;
    QJsonObject _document;
    QJsonObject _baseline;
    bool _locked = false;
};
