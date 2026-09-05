#pragma once

#include "VehicleTest.h"

class OsdControllerTest : public VehicleTest
{
    Q_OBJECT

private slots:
    void _discoveryAndSetupPage();
    void _saveReloadRoundTrip_data();
    void _saveReloadRoundTrip();
    void _failedTransferDoesNotReload();
    void _downloadFailureDoesNotUseDefaults();
    void _invalidLayoutCannotBeSaved();
    void _armedAndNullGuards();
    void _editorControls();
};
