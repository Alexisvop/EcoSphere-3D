#pragma once
#include <ecosphere/sdk/Content.hpp>
#include <array>

namespace ecosphere::ecocycle {
enum class Model { platform=1, gateway, walkway, kiosk, sorting, feeder, conveyor, roller, sensor, gate,
    magnetic, press, ram, bale, shredder, rotor, washer, extruder, spool, glassCrusher, glassBin, compost, workbench, chair };
struct Settings {
    glm::vec3 clearColor{.04f,.14f,.06f};
    // Metros, +Y arriba, -Z norte; índice CYCLE menos uno. Reservas previas al modelado.
    std::array<glm::vec3,24> positions{{
        {0.000f,0.000f,-1.000f},
        {0.000f,0.010f,18.000f},
        {0.000f,0.040f,12.000f},
        {-7.500f,0.010f,14.000f},
        {-16.000f,0.010f,9.000f},
        {-16.000f,0.010f,1.000f},
        {-15.000f,0.010f,-8.000f},
        {-15.000f,1.310f,-6.000f},
        {-16.000f,2.610f,1.000f},
        {-16.000f,1.810f,-0.500f},
        {-15.000f,1.660f,-10.000f},
        {-16.000f,0.010f,-18.000f},
        {-16.000f,2.710f,-18.000f},
        {-7.500f,0.010f,-18.000f},
        {-7.000f,0.010f,-8.000f},
        {-7.000f,1.510f,-8.000f},
        {14.000f,0.010f,-18.000f},
        {14.000f,0.010f,-8.000f},
        {7.500f,0.010f,-18.000f},
        {7.500f,0.010f,-1.000f},
        {16.000f,0.010f,1.000f},
        {16.000f,0.010f,20.000f},
        {16.000f,0.010f,12.000f},
        {7.500f,0.010f,12.000f}
    }};
    std::array<glm::vec3,24> scale=[] {std::array<glm::vec3,24> v;v.fill(glm::vec3(1));return v;}();
    std::array<float,24> yawDegrees{};
    std::array<bool,24> visible=[] {std::array<bool,24> v;v.fill(true);return v;}();
    // Cuatro tramos centrales 3.8x10; cuatro ramales 2.8x18 m útiles.
    float mainPathWidth=3.8f, secondaryPathWidth=2.8f;
    std::array<glm::vec3,7> extraPaths{{
        {0.000f,0.040f,2.000f},
        {0.000f,0.040f,-8.000f},
        {0.000f,0.040f,-18.000f},
        {-9.000f,0.040f,5.000f},
        {9.000f,0.040f,5.000f},
        {-9.000f,0.040f,-12.000f},
        {9.000f,0.040f,-12.000f}
    }};
    std::array<glm::vec3,1> extraKiosks{{{6.500f,0.010f,18.000f}}};
    std::array<glm::vec3,2> extraBales{{{-9.500f,0.010f,-20.500f},{-6.000f,0.010f,-20.500f}}};
    std::array<glm::vec3,2> extraSpools{{{6.000f,0.010f,-20.500f},{9.000f,0.010f,-20.500f}}};
    std::array<glm::vec3,2> extraChairs{{{7.000f,0.010f,14.500f},{10.000f,0.010f,14.500f}}};
    glm::vec3 cameraPosition{0,1.7f,13};
    float cameraYaw=-90, cameraPitch=-4;
    // Recorrido instrumental por zonas; no representa la velocidad del visitante.
    std::array<glm::vec3,12> tourPositions{{
        {0,1.7f,13},{-10,1.7f,5},{0,1.7f,5},{0,1.7f,-12},{-10,1.7f,-12},{0,1.7f,-12},
        {0,1.7f,-18},{10,1.7f,-12},{0,1.7f,-12},{0,1.7f,5},{10,1.7f,5},{12,1.7f,16}}};
    std::array<glm::vec3,12> tourTargets{{
        {-7,1.5f,14},{-16,1.5f,9},{-16,1.5f,1},{-15,1.5f,-8},{-16,1.5f,-18},{-7,1.5f,-8},
        {7.5f,1,-18},{14,1.5f,-18},{14,1.5f,-8},{7.5f,1.5f,-1},{16,1,1},{16,1.2f,12}}};
    float rollerSpeedDeg=60, magnetSpeedDeg=22, rotorSpeedDeg=75, compostSpeedDeg=18;
    double gateDurationSeconds=8, pressDurationSeconds=10;
    float gateOpeningDeg=55, pressTravelMeters=.75f;
    float feederAmplitudeMeters=.055f, sensorAmplitudeDegrees=24;
    double feederFrequencyHz=2.2, sensorFrequencyHz=.22;
    LightingSettings lighting=[] {
        LightingSettings s;s.legacyUnlit=false;s.ambient=.24f;
        s.directional={true,{-.4f,-1,-.3f},{1,.95f,.82f},.9f};
        s.point={true,{0,5,-4},{.68f,1,.68f},1.1f};s.attenuation={1,.07f,.018f};
        s.environment.intensity=.25f;s.environment.fresnelPower=5;return s;
    }();
};
inline const Settings settings;
}
