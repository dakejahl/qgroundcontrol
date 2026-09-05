#pragma once

#include "UnitTest.h"

class OsdLayoutTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _metadataFromPx4();
    void _invalidMetadata_data();
    void _invalidMetadata();
    void _invalidLayouts_data();
    void _invalidLayouts();
    void _editsRoundTripAndRevert();
    void _moveAndClamp_data();
    void _moveAndClamp();
};
