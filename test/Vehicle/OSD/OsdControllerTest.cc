#include "OsdControllerTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QScopedPointer>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include "AutoPilotPlugin.h"
#include "ColoredSvgImageProvider.h"
#include "Fact.h"
#include "MockLinkFTP.h"
#include "OsdComponent.h"
#include "OsdController.h"
#include "ParameterManager.h"
#include "QGCCorePlugin.h"
#include "QGCPalette.h"
#include "Vehicle.h"

void OsdControllerTest::_discoveryAndSetupPage()
{
    auto* controller = vehicle()->osdController();
    QVERIFY(controller->available());
    QCOMPARE(controller->displays().size(), 3);
    bool found = false;
    for (const QVariant& component : vehicle()->autopilotPlugin()->vehicleComponents()) {
        auto* osd = qobject_cast<OsdComponent*>(component.value<VehicleComponent*>());
        if (osd) {
            found = true;
            QCOMPARE(osd->osdController(), controller);
            QVERIFY(!osd->requiresSetup());
        }
    }
    QVERIFY(found);
}

void OsdControllerTest::_saveReloadRoundTrip_data()
{
    QTest::addColumn<int>("displayIndex");
    QTest::newRow("ark-pixel") << 0;
    QTest::newRow("atxxxx-grid") << 1;
    QTest::newRow("msp-grid-disabled") << 2;
}

void OsdControllerTest::_saveReloadRoundTrip()
{
    QFETCH(int, displayIndex);
    auto* controller = vehicle()->osdController();
    controller->selectDisplay(displayIndex);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY2(controller->ready(), qPrintable(controller->status()));
    QVERIFY(controller->status().contains(QStringLiteral("defaults")));
    QVERIFY(controller->enableFact());
    auto* reload = vehicle()->parameterManager()->getParameter(1, QStringLiteral("OSD_LAYOUT_GEN"));
    const int generation = reload->rawValue().toInt();
    controller->layout()->moveElement(0, 36, 54, false);
    QVERIFY(controller->layout()->dirty());
    const QJsonObject expected = controller->layout()->document();
    const QString path =
        OsdLayout::writablePath(controller->layout()->displayObject()["layout"].toObject()["uri"].toString());
    QVERIFY(controller->canSave());
    controller->save();
    QVERIFY(controller->busy());
    QVERIFY(controller->layout()->locked());
    controller->layout()->removeElement(0);
    QCOMPARE(controller->layout()->document(), expected);
    controller->selectDisplay((displayIndex + 1) % 3);
    QCOMPARE(controller->displayIndex(), displayIndex);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY2(!controller->layout()->dirty(), qPrintable(controller->status()));
    QCOMPARE(reload->rawValue().toInt(), generation + 1);
    const QByteArray bytes = mockLink()->mockLinkFTP()->uploadedFileContents(path);
    QCOMPARE(QJsonDocument::fromJson(bytes).object(), expected);
    controller->fetch();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(controller->ready());
    QCOMPARE(controller->layout()->document(), expected);
    // A second save exercises CreateDirectory returning EEXIST.
    controller->save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QCOMPARE(reload->rawValue().toInt(), generation + 2);
}

void OsdControllerTest::_failedTransferDoesNotReload()
{
    auto* controller = vehicle()->osdController();
    controller->selectDisplay(0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(controller->ready());
    controller->layout()->moveElement(0, 36, 54, false);
    auto* reload = vehicle()->parameterManager()->getParameter(1, QStringLiteral("OSD_LAYOUT_GEN"));
    const QVariant generation = reload->rawValue();
    mockLink()->mockLinkFTP()->setErrorMode(MockLinkFTP::errModeNakResponse);
    controller->save();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    mockLink()->mockLinkFTP()->setErrorMode(MockLinkFTP::errModeNone);
    QVERIFY(controller->layout()->dirty());
    QCOMPARE(reload->rawValue(), generation);
    QVERIFY(mockLink()->mockLinkFTP()->uploadedFiles().isEmpty());
}

void OsdControllerTest::_downloadFailureDoesNotUseDefaults()
{
    auto* controller = vehicle()->osdController();
    mockLink()->mockLinkFTP()->setErrorMode(MockLinkFTP::errModeNakResponse);
    controller->selectDisplay(0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    mockLink()->mockLinkFTP()->setErrorMode(MockLinkFTP::errModeNone);
    QVERIFY(!controller->ready());
    QVERIFY(!controller->canSave());
    QVERIFY(controller->layout()->document().isEmpty());
}

void OsdControllerTest::_invalidLayoutCannotBeSaved()
{
    auto* controller = vehicle()->osdController();
    const QJsonObject d = QJsonObject::fromVariantMap(controller->displays()[0].toMap());
    QJsonObject invalid = d["defaults"].toObject();
    invalid["canvas"] = "unknown";
    mockLink()->mockLinkFTP()->setFileContents(OsdLayout::writablePath(d["layout"].toObject()["uri"].toString()),
                                               QJsonDocument(invalid).toJson());
    controller->selectDisplay(0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(controller->ready());
    QVERIFY(!controller->layout()->errors().isEmpty());
    QVERIFY(!controller->canSave());
    controller->save();
    QVERIFY(mockLink()->mockLinkFTP()->uploadedFiles().isEmpty());
    mockLink()->mockLinkFTP()->setFileContents(OsdLayout::writablePath(d["layout"].toObject()["uri"].toString()),
                                               "{broken");
    controller->fetch();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(!controller->canSave());
    controller->layout()->loadDefaults();
    QVERIFY(controller->canSave());
}

void OsdControllerTest::_armedAndNullGuards()
{
    OsdController offline(nullptr);
    offline.save();
    offline.fetch();
    offline.cancel();
    QVERIFY(!offline.canSave());
    auto* controller = vehicle()->osdController();
    controller->selectDisplay(0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    vehicle()->setArmed(true, false);
    QTRY_VERIFY_WITH_TIMEOUT(vehicle()->armed(), TestTimeout::longMs());
    QVERIFY(!controller->canSave());
    QVERIFY(controller->layout()->locked());
    controller->save();
    QVERIFY(!controller->busy());
    QVERIFY(mockLink()->mockLinkFTP()->uploadedFiles().isEmpty());
    vehicle()->setArmed(false, false);
    QTRY_VERIFY_WITH_TIMEOUT(!vehicle()->armed(), TestTimeout::longMs());
}

void OsdControllerTest::_editorControls()
{
    auto* controller = vehicle()->osdController();
    controller->selectDisplay(1);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(controller->ready());
    QQmlApplicationEngine* engine = QGCCorePlugin::instance()->createQmlApplicationEngine(this);
    engine->addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(engine,
                            QUrl(QStringLiteral("qrc:/qml/QGroundControl/AutoPilotPlugins/Common/OsdEditor.qml")));
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading, TestTimeout::mediumMs());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> editor(
        component.createWithInitialProperties({{"controller", QVariant::fromValue(controller)}, {"width", 1400}}));
    QVERIFY2(editor, qPrintable(component.errorString()));
    auto* item = qobject_cast<QQuickItem*>(editor.data());
    QVERIFY(item);
    QQuickWindow window;
    window.resize(1400, 900);
    window.setColor(QGCPalette().window());
    item->setParentItem(window.contentItem());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QObject* add = editor->findChild<QObject*>(QStringLiteral("osdAddButton"));
    QObject* canvas = editor->findChild<QObject*>(QStringLiteral("osdCanvas"));
    QVERIFY(add);
    QVERIFY(canvas);
    QObject* palette = editor->findChild<QObject*>(QStringLiteral("osdPalette"));
    QVERIFY(palette);
    const QVariantList catalog = controller->layout()->display().value("elements").toList();
    for (int i = 0; i < catalog.size(); ++i) {
        if (catalog[i].toMap().value("id") == "LABEL") {
            palette->setProperty("currentIndex", i);
        }
    }
    const int count = controller->layout()->elements().size();
    QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
    QCOMPARE(controller->layout()->elements().size(), count + 1);
    QCOMPARE(editor->property("selected").toInt(), count);
    auto* canvasItem = qobject_cast<QQuickItem*>(canvas);
    QVERIFY(canvasItem);
    canvasItem->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Right);
    QCOMPARE(controller->layout()->elements()[count].toMap()["left"].toInt(), 12);
    const double scale = canvasItem->width() / (30.0 * 12.0);
    const QPoint dragStart = canvasItem->mapToScene(QPointF(18 * scale, 9 * scale)).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(&window, dragStart + QPoint(qRound(12 * scale), 0));
    QTest::mouseMove(&window, dragStart + QPoint(qRound(24 * scale), 0));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragStart + QPoint(qRound(24 * scale), 0));
    const int movedX = controller->layout()->elements()[count].toMap()["left"].toInt();
    QVERIFY(movedX > 12);
    QCOMPARE(movedX % 12, 0);
    QTest::keyClick(&window, Qt::Key_Delete);
    QCOMPARE(controller->layout()->elements().size(), count);
    controller->selectDisplay(0);
    QTRY_VERIFY_WITH_TIMEOUT(!controller->busy(), TestTimeout::longMs());
    QVERIFY(controller->ready());
    const int valueIndex = controller->layout()->addElement(QStringLiteral("VALUE"));
    QVERIFY(valueIndex >= 0);
    editor->setProperty("selected", valueIndex);
    QVERIFY(!controller->canSave());
    controller->layout()->updateElement(valueIndex,
                                        {{"bind", QVariantMap{{"channel", 3}, {"format", "f1"}, {"suffix", "V"}}}});
    QVERIFY(controller->canSave());
    const QString screenshot = qEnvironmentVariable("QGC_OSD_SCREENSHOT");
    if (!screenshot.isEmpty()) {
        QVERIFY(window.grabWindow().save(screenshot));
    }
    editor.reset();
    QGCCorePlugin::instance()->destroyQmlApplicationEngine(engine);
}

UT_REGISTER_TEST(OsdControllerTest, TestLabel::Integration, TestLabel::Vehicle)
