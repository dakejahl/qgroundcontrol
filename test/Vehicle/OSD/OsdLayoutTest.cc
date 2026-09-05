#include "OsdLayoutTest.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include "OsdLayout.h"

namespace {
QJsonObject metadata()
{
    QFile file(QStringLiteral(":/MockLink/MockLink.Osd.MetaData.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject display(int index = 0)
{
    return metadata()["displays"].toArray()[index].toObject();
}
}  // namespace

void OsdLayoutTest::_metadataFromPx4()
{
    QString error;
    QVERIFY2(OsdLayout::validateMetadata(metadata(), error), qPrintable(error));
    QCOMPARE(metadata()["displays"].toArray().size(), 3);
    for (const QJsonValue& value : metadata()["displays"].toArray()) {
        const auto d = value.toObject();
        const QStringList errors = OsdLayout::validateLayout(d, d["defaults"].toObject());
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join('\n')));
    }
}

void OsdLayoutTest::_invalidMetadata_data()
{
    QTest::addColumn<QString>("mutation");
    for (const char* name : {"version", "empty", "duplicate-display", "duplicate-canvas", "zero-cell", "pixel-size",
                             "unsafe-path", "bad-default"}) {
        QTest::newRow(name) << QString::fromLatin1(name);
    }
}

void OsdLayoutTest::_invalidMetadata()
{
    QFETCH(QString, mutation);
    QJsonObject document = metadata();
    QJsonArray displays = document["displays"].toArray();
    QJsonObject d = displays[0].toObject();
    if (mutation == "version")
        document["version"] = 2;
    if (mutation == "empty")
        displays = {};
    if (mutation == "duplicate-display")
        displays.append(d);
    if (mutation == "duplicate-canvas") {
        auto canvases = d["canvases"].toArray();
        canvases.append(canvases[0]);
        d["canvases"] = canvases;
    }
    if (mutation == "zero-cell")
        d["cell"] = QJsonObject{{"width", 0}, {"height", 18}};
    if (mutation == "pixel-size") {
        auto canvases = d["canvases"].toArray();
        auto canvas = canvases[0].toObject();
        canvas.remove("width");
        canvases[0] = canvas;
        d["canvases"] = canvases;
    }
    if (mutation == "unsafe-path")
        d["layout"] = QJsonObject{{"uri", "mftp://fs/microsd/osd/../params"}};
    if (mutation == "bad-default") {
        auto layout = d["defaults"].toObject();
        layout["canvas"] = "unknown";
        d["defaults"] = layout;
    }
    if (!displays.isEmpty())
        displays[0] = d;
    document["displays"] = displays;
    QString error;
    QVERIFY(!OsdLayout::validateMetadata(document, error));
    QVERIFY(!error.isEmpty());
}

void OsdLayoutTest::_invalidLayouts_data()
{
    QTest::addColumn<QString>("mutation");
    for (const char* name : {"wrong-display", "wrong-version", "unknown-id", "outside", "negative", "fractional",
                             "missing-coordinate", "variant", "profile", "label-ascii", "channel", "too-many"}) {
        QTest::newRow(name) << QString::fromLatin1(name);
    }
}

void OsdLayoutTest::_invalidLayouts()
{
    QFETCH(QString, mutation);
    const QJsonObject d = display();
    QJsonObject document = d["defaults"].toObject();
    QJsonArray elements = document["elements"].toArray();
    QJsonObject element{{"id", "BATTERY_VOLTAGE"}, {"x", 12}, {"y", 18}};
    if (mutation == "wrong-display")
        document["display"] = "atxxxx";
    if (mutation == "wrong-version")
        document["version"] = 2;
    if (mutation == "unknown-id")
        element["id"] = "UNKNOWN";
    if (mutation == "outside")
        element["x"] = 360;
    if (mutation == "negative")
        element["y"] = -1;
    if (mutation == "fractional")
        element["x"] = 1.5;
    if (mutation == "missing-coordinate")
        element.remove("y");
    if (mutation == "variant")
        element["variant"] = 3;
    if (mutation == "profile")
        element["profiles"] = QJsonArray{2};
    if (mutation == "label-ascii") {
        element["id"] = "LABEL";
        element["text"] = QString::fromUtf8("é");
    }
    if (mutation == "channel")
        element["bind"] = QJsonObject{{"channel", 31}};
    elements[0] = element;
    if (mutation == "too-many")
        for (int i = 0; i < 49; ++i)
            elements.append(element);
    document["elements"] = elements;
    QVERIFY(!OsdLayout::validateLayout(d, document).isEmpty());
}

void OsdLayoutTest::_editsRoundTripAndRevert()
{
    const QJsonObject d = display();
    QJsonObject original = d["defaults"].toObject();
    original["betaflight"] = QJsonObject{{"alarms", QJsonArray{1, 2, 3}}};
    OsdLayout layout;
    layout.load(d, original);
    QVERIFY(!layout.dirty());
    layout.moveElement(0, 24, 36, false);
    QVERIFY(layout.dirty());
    QCOMPARE(layout.document()["betaflight"], original["betaflight"]);
    layout.revert();
    QCOMPARE(layout.document(), original);
    QVERIFY(!layout.dirty());
    const int index = layout.addElement(QStringLiteral("LABEL"));
    QVERIFY(index >= 0);
    layout.updateElement(index, {{"text", "ARK"}, {"blink", true}, {"profiles", QVariantList{1}}});
    layout.markSaved();
    const QJsonObject saved = layout.document();
    layout.removeElement(index);
    QVERIFY(layout.dirty());
    layout.revert();
    QCOMPARE(layout.document(), saved);
    layout.setLocked(true);
    layout.moveElement(0, 0, 0, false);
    layout.removeElement(0);
    layout.loadDefaults();
    QCOMPARE(layout.document(), saved);
    QCOMPARE(layout.addElement(QStringLiteral("LABEL")), -1);
}

void OsdLayoutTest::_moveAndClamp_data()
{
    QTest::addColumn<int>("displayIndex");
    QTest::addColumn<bool>("snap");
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");
    QTest::addColumn<int>("expectedX");
    QTest::addColumn<int>("expectedY");
    QTest::newRow("pixel") << 0 << false << 19 << 28 << 19 << 28;
    QTest::newRow("pixel-snapped") << 0 << true << 19 << 28 << 24 << 36;
    QTest::newRow("grid") << 1 << false << 19 << 28 << 24 << 36;
    QTest::newRow("negative") << 0 << false << -100 << -100 << 0 << 0;
    QTest::newRow("pixel-edge") << 0 << false << 10000 << 10000 << 359 << 233;
    QTest::newRow("grid-edge") << 1 << true << 10000 << 10000 << 348 << 216;
}

void OsdLayoutTest::_moveAndClamp()
{
    QFETCH(int, displayIndex);
    QFETCH(bool, snap);
    QFETCH(int, x);
    QFETCH(int, y);
    QFETCH(int, expectedX);
    QFETCH(int, expectedY);
    const QJsonObject d = display(displayIndex);
    OsdLayout layout;
    layout.load(d, d["defaults"].toObject());
    layout.moveElement(0, x, y, snap);
    QCOMPARE(layout.elements()[0].toMap()["left"].toInt(), expectedX);
    QCOMPARE(layout.elements()[0].toMap()["top"].toInt(), expectedY);
}

UT_REGISTER_TEST_LIGHTWEIGHT(OsdLayoutTest, TestLabel::Unit)
