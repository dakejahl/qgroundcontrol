#include "OsdComponent.h"

#include "Vehicle.h"

OsdComponent::OsdComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent)
    : VehicleComponent(vehicle, autopilot, AutoPilotPlugin::UnknownVehicleComponent, parent)
{}

OsdController* OsdComponent::osdController() const
{
    return _vehicle ? _vehicle->osdController() : nullptr;
}
