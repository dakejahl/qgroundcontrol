#pragma once

#include "CompInfo.h"

class CompInfoOsd : public CompInfo
{
    Q_OBJECT

public:
    // Proposed COMP_METADATA_TYPE_OSD in ARK's dialect; no new MAVLink message or dialect build is needed.
    static constexpr COMP_METADATA_TYPE METADATA_TYPE = static_cast<COMP_METADATA_TYPE>(6);

    CompInfoOsd(uint8_t componentId, Vehicle* ownerVehicle, QObject* parent = nullptr);
    void setJson(const QString& file) override;
};
