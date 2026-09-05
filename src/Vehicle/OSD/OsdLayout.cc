#include "OsdLayout.h"

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>

#include "JsonSchemaValidator.h"

namespace {
QJsonObject findById(const QJsonArray& array, const QString& id)
{
    for (const QJsonValue& value : array) {
        if (value.toObject()["id"].toString() == id) {
            return value.toObject();
        }
    }
    return {};
}
}  // namespace

OsdLayout::OsdLayout(QObject* parent) : QObject(parent) {}

QString OsdLayout::writablePath(const QString& uri)
{
    if (!uri.startsWith(QStringLiteral("mftp://fs/microsd/osd/"))) {
        return {};
    }
    const QString path = uri.mid(6);
    if (QDir::cleanPath(path) != path || path.endsWith('/') || path.contains('\\') || path.mid(16).contains('/') ||
        path.contains(QChar::Null)) {
        return {};
    }
    return path;
}

bool OsdLayout::validateMetadata(const QJsonObject& metadata, QString& error)
{
    if (metadata["version"].toInt() != 1) {
        error = tr("Unsupported OSD metadata version.");
        return false;
    }
    if (!JsonSchemaValidator::validate(QJsonDocument(metadata), QStringLiteral(":/json/osd/osd.schema.json"), error)) {
        return false;
    }
    error.clear();
    QSet<QString> displayIds;
    QSet<QString> paths;
    for (const QJsonValue& value : metadata["displays"].toArray()) {
        const QJsonObject display = value.toObject();
        const QString id = display["id"].toString();
        const QString path = writablePath(display["layout"].toObject()["uri"].toString());
        if (id.size() > 15 || displayIds.contains(id) || path.isEmpty() || paths.contains(path)) {
            error = tr("Duplicate display or invalid OSD layout path: %1").arg(id);
            return false;
        }
        displayIds.insert(id);
        paths.insert(path);
        if (display["features"].toArray().contains("firmware") &&
            writablePath(display["firmware"].toObject()["uri"].toString()).isEmpty()) {
            error = tr("Invalid OSD firmware path: %1").arg(id);
            return false;
        }
        for (const QString& key : {QStringLiteral("canvases"), QStringLiteral("elements")}) {
            QSet<QString> ids;
            for (const QJsonValue& entry : display[key].toArray()) {
                const QJsonObject object = entry.toObject();
                const QString entryId = object["id"].toString();
                if (ids.contains(entryId) || (key == "canvases" && entryId.size() > 15)) {
                    error = tr("Duplicate OSD catalog identifier: %1").arg(entryId);
                    return false;
                }
                ids.insert(entryId);
                if (key == "canvases" && display["model"] == "pixel" &&
                    (object["width"].toInt() < 1 || object["height"].toInt() < 1 || object["width"].toInt() > 32767 ||
                     object["height"].toInt() > 32767)) {
                    error = tr("Invalid pixel canvas: %1").arg(entryId);
                    return false;
                }
            }
        }
        const QStringList errors = validateLayout(display, display["defaults"].toObject());
        if (!errors.isEmpty()) {
            error = errors.join('\n');
            return false;
        }
    }
    return true;
}

QStringList OsdLayout::validateLayout(const QJsonObject& display, const QJsonObject& document)
{
    QStringList errors;
    QString schemaError;
    if (!JsonSchemaValidator::validate(QJsonDocument(document), QStringLiteral(":/json/osd/osd_layout.schema.json"),
                                       schemaError)) {
        return {schemaError};
    }
    if (document["version"].toInt() != 1 || document["display"] != display["id"]) {
        errors << tr("Layout version or display does not match.");
    }
    const QJsonObject canvas = findById(display["canvases"].toArray(), document["canvas"].toString());
    if (canvas.isEmpty()) {
        errors << tr("Select a supported canvas.");
    }
    const bool grid = display["model"] == "grid";
    const QJsonArray elements = document["elements"].toArray();
    // Current PX4 JSON parser holds 48 entries; the pixel renderer holds 32 visible entries.
    if (elements.size() > 48) {
        errors << tr("The driver supports at most 48 layout elements.");
    }
    if (QJsonDocument(document).toJson(QJsonDocument::Compact).size() > 12 * 1024) {
        errors << tr("The driver supports layout files up to 12 KiB.");
    }
    QSet<QString> gridIds;
    int visibleElements = 0;
    for (const QJsonValue& value : elements) {
        const QJsonObject element = value.toObject();
        const QString id = element["id"].toString();
        const QJsonObject catalog = findById(display["elements"].toArray(), id);
        if (catalog.isEmpty()) {
            errors << tr("Unsupported element: %1").arg(id);
            continue;
        }
        if (grid && gridIds.contains(id)) {
            errors << tr("%1: grid displays draw only the first occurrence.").arg(id);
        }
        gridIds.insert(id);
        const bool pixel = element.contains("x");
        if (grid && (pixel || element.contains("bind"))) {
            errors << tr("%1 requires grid placement without a channel binding.").arg(id);
        }
        const int x = element[pixel ? "x" : "col"].toInt();
        const int y = element[pixel ? "y" : "row"].toInt();
        if (!canvas.isEmpty() && (x < 0 || y < 0 || x >= canvas[pixel ? "width" : "cols"].toInt() ||
                                  y >= canvas[pixel ? "height" : "rows"].toInt())) {
            errors << tr("%1 is outside the selected canvas.").arg(id);
        }
        const QJsonArray features = display["features"].toArray();
        if (element["blink"].toBool() && !features.contains("blink")) {
            errors << tr("%1: blinking is not supported.").arg(id);
        }
        if (element.contains("text") && (id != "LABEL" || !features.contains("label"))) {
            errors << tr("%1: label text is not supported.").arg(id);
        }
        if (element["variant"].toInt() >= qMax(1, catalog["variants"].toArray().size())) {
            errors << tr("%1: unsupported variant.").arg(id);
        }
        const QJsonArray profiles = element["profiles"].toArray();
        for (const QJsonValue& profile : profiles) {
            if (profile.toInt() > display["profiles"].toInt(1)) {
                errors << tr("%1: unsupported visibility profile.").arg(id);
            }
        }
        if (!grid) {
            QJsonObject bind = catalog["bind"].toObject();
            const QJsonObject overrides = element["bind"].toObject();
            for (auto it = overrides.begin(); it != overrides.end(); ++it) {
                bind[it.key()] = it.value();
            }
            const QString type = bind["type"].toString();
            const QString channelList = type == "text" ? QStringLiteral("texts") : QStringLiteral("channels");
            bool channelFound = false;
            for (const QJsonValue& channel : display[channelList].toArray()) {
                channelFound |= channel.toObject()["id"] == bind["channel"];
            }
            if ((type == "value" || type == "text") && !channelFound) {
                errors << tr("%1: select an advertised channel.").arg(id);
            }
        }
        if (element["visible"].toBool(true) && (profiles.isEmpty() || profiles.contains(1))) {
            ++visibleElements;
        }
    }
    if (!grid && visibleElements > 32) {
        errors << tr("The pixel renderer supports at most 32 visible elements.");
    }
    return errors;
}

void OsdLayout::load(const QJsonObject& display, const QJsonObject& document)
{
    _display = display;
    _document = document;
    _baseline = document;
    emit changed();
}

void OsdLayout::markSaved()
{
    _baseline = _document;
    emit changed();
}

void OsdLayout::replaceDocument(const QJsonObject& document)
{
    if (!_locked) {
        _document = document;
        emit changed();
    }
}

void OsdLayout::setLocked(bool locked)
{
    if (_locked != locked) {
        _locked = locked;
        emit changed();
    }
}

QVariantMap OsdLayout::canvas() const
{
    return findById(_display["canvases"].toArray(), _document["canvas"].toString()).toVariantMap();
}

QVariantList OsdLayout::elements() const
{
    QVariantList result;
    const QJsonObject cell = _display["cell"].toObject();
    for (const QJsonValue& value : _document["elements"].toArray()) {
        const QJsonObject element = value.toObject();
        QVariantMap item = element.toVariantMap();
        const QJsonObject catalog = findById(_display["elements"].toArray(), element["id"].toString());
        item["catalog"] = catalog.toVariantMap();
        item["left"] = element.contains("x") ? element["x"].toInt() : element["col"].toInt() * cell["width"].toInt();
        item["top"] = element.contains("y") ? element["y"].toInt() : element["row"].toInt() * cell["height"].toInt();
        item["shown"] = element["visible"].toBool(true);
        result.append(item);
    }
    return result;
}

void OsdLayout::selectCanvas(const QString& id)
{
    if (_locked || findById(_display["canvases"].toArray(), id).isEmpty() || _document["canvas"] == id) {
        return;
    }
    _document["canvas"] = id;
    emit changed();
}

int OsdLayout::addElement(const QString& id)
{
    const QJsonObject catalog = findById(_display["elements"].toArray(), id);
    QJsonArray elements = _document["elements"].toArray();
    if (_locked || catalog.isEmpty() || elements.size() >= 48) {
        return -1;
    }
    if (_display["model"] == "grid") {
        for (int i = 0; i < elements.size(); ++i) {
            if (elements[i].toObject()["id"] == id) {
                return i;
            }
        }
    }
    QJsonObject element{{"id", id}};
    const bool grid = _display["model"] == "grid";
    element[grid ? "col" : "x"] = 0;
    element[grid ? "row" : "y"] = 0;
    if (id == "LABEL") {
        element["text"] = QStringLiteral("LABEL");
    }
    elements.append(element);
    _document["elements"] = elements;
    emit changed();
    return elements.size() - 1;
}

void OsdLayout::removeElement(int index)
{
    QJsonArray elements = _document["elements"].toArray();
    if (_locked || index < 0 || index >= elements.size()) {
        return;
    }
    elements.removeAt(index);
    _document["elements"] = elements;
    emit changed();
}

void OsdLayout::updateElement(int index, const QVariantMap& values)
{
    QJsonArray elements = _document["elements"].toArray();
    if (_locked || index < 0 || index >= elements.size()) {
        return;
    }
    QJsonObject element = elements[index].toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (it.key() != "id") {
            element[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }
    if (elements[index] != element) {
        elements[index] = element;
        _document["elements"] = elements;
        emit changed();
    }
}

void OsdLayout::moveElement(int index, int x, int y, bool snap)
{
    QJsonArray elements = _document["elements"].toArray();
    if (_locked || index < 0 || index >= elements.size()) {
        return;
    }
    const QJsonObject cell = _display["cell"].toObject();
    const QVariantMap size = canvas();
    const int cellWidth = qMax(1, cell["width"].toInt());
    const int cellHeight = qMax(1, cell["height"].toInt());
    const bool grid = _display["model"] == "grid";
    QJsonObject element = elements[index].toObject();
    if (grid || snap) {
        x = qRound(double(x) / cellWidth) * cellWidth;
        y = qRound(double(y) / cellHeight) * cellHeight;
    }
    if (grid) {
        element.remove("x");
        element.remove("y");
        element["col"] = qBound(0, x / cellWidth, qMax(0, size["cols"].toInt() - 1));
        element["row"] = qBound(0, y / cellHeight, qMax(0, size["rows"].toInt() - 1));
    } else {
        element.remove("col");
        element.remove("row");
        element["x"] = qBound(0, x, qMax(0, size["width"].toInt() - 1));
        element["y"] = qBound(0, y, qMax(0, size["height"].toInt() - 1));
    }
    if (elements[index] != element) {
        elements[index] = element;
        _document["elements"] = elements;
        emit changed();
    }
}

void OsdLayout::revert()
{
    if (!_locked) {
        _document = _baseline;
        emit changed();
    }
}

void OsdLayout::loadDefaults()
{
    if (!_locked) {
        _document = _display["defaults"].toObject();
        emit changed();
    }
}
