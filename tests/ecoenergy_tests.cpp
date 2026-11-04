#include "modules/EcoCycle/EcoCycle.hpp"
#include "modules/EcoEnergy/EcoEnergy.hpp"
#include "modules/AquaBio/AquaBio.hpp"
#include "world/Plaza.hpp"
#include "content/ContentRegistry.hpp"
#include "assets/AssetManager.hpp"
#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/modules/Stubs.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F f) {
    try { f(); } catch (const std::exception&) { return; }
    throw std::runtime_error("Expected rejection");
}

std::string animationFamilyName(const std::optional<ecosphere::AnimationDescriptor>& anim) {
    if (!anim) return "none";
    return std::visit([](const auto& src) -> std::string {
        using T = std::decay_t<decltype(src)>;
        if constexpr (std::is_same_v<T, ecosphere::SimpleAnimation>) return "simple";
        else if constexpr (std::is_same_v<T, ecosphere::KeyframeTrack>) return "keyframe";
        else return "procedural";
    }, anim->source);
}

std::string instanceCsv() {
    std::ostringstream out;
    out << std::setprecision(9);
    out << "instance_id,owner,model_id,glb_file,x,y,z,qw,qx,qy,qz,sx,sy,sz,interactive,animation_family\n";
    for (const auto& e : ecosphere::makeEcoEnergyInstances()) {
        const auto& p = e.transform;
        const auto q = glm::normalize(p.orientation);
        out << e.id << ",ecoenergy," << e.id.substr(0, 10) << ",assets/installations/" << e.model << ','
            << p.position.x << ',' << p.position.y << ',' << p.position.z << ','
            << q.w << ',' << q.x << ',' << q.y << ',' << q.z << ','
            << p.scale.x << ',' << p.scale.y << ',' << p.scale.z << ','
            << (e.interaction ? "true" : "false") << ',' << animationFamilyName(e.animation) << '\n';
    }
    return out.str();
}

struct Host final : ecosphere::ModuleServices {
    ecosphere::ContentRegistry& registry;
    explicit Host(ecosphere::ContentRegistry& r) : registry(r) {}
    void log(std::string_view) noexcept override {}
    void setClearColor(glm::vec3) noexcept override {}
    std::unique_ptr<ecosphere::ContentServices> openContent(std::string_view owner) override {
        return registry.open(owner);
    }
};
}

int main(int argc, char** argv) {
    try {
        using namespace ecosphere;
        if (argc == 3 && std::string(argv[1]) == "--write-instances") {
            std::ofstream out(argv[2]);
            out.exceptions(std::ios::badbit | std::ios::failbit);
            out << instanceCsv();
            std::cout << "EcoEnergy instance CSV generated from EcoEnergySettings\n";
            return 0;
        }
        require(argc == 5, "Expected staged assets, instances CSV, import report and scratch");
        const std::filesystem::path root = argv[1], scratch = argv[4];
        std::ifstream instanceFile(argv[2]);
        const std::string recorded((std::istreambuf_iterator<char>(instanceFile)), {});
        require(recorded == instanceCsv(), "EcoEnergy instance CSV differs from actual configuration; regenerate explicitly");

        std::ofstream report(argv[3]);
        report.exceptions(std::ios::badbit | std::ios::failbit);
        report << "model_id,triangles,meshes,textures,width_m,height_m,depth_m\n";

        AssetManager assets(root);
        std::set<std::set<std::array<int, 3>>> shapes;
        std::size_t totalTriangles = 0;
        for (int i = 1; i <= 28; ++i) {
            const auto id = ecoEnergyModelId(static_cast<ecoenergy::Model>(i));
            const auto path = "ecoenergy/models/" + id + ".glb";
            require(std::filesystem::is_regular_file(root / path), "Missing EcoEnergy GLB " + id);
            const auto model = assets.loadModel(path);
            require(!model->meshes.empty() && !model->nodes.empty(), "Empty EcoEnergy model " + id);
            std::size_t tris = 0;
            std::vector<glm::vec3> points;
            glm::vec3 lo(std::numeric_limits<float>::infinity()), hi(-std::numeric_limits<float>::infinity());
            model->forEach([&](std::size_t meshIndex, const glm::mat4& world) {
                (void)normalMatrix(world);
                const auto& mesh = model->meshes.at(meshIndex);
                validateMaterial(mesh.material);
                require(!mesh.vertices.empty(), "Empty primitive " + id);
                for (const auto& v : mesh.vertices) {
                    require(finiteVector(v.position) && finiteVector(v.normal), "Nonfinite geometry " + id);
                    require(std::abs(glm::length(v.normal) - 1.0f) < 0.01f, "Invalid normal " + id);
                    const auto p = glm::vec3(world * glm::vec4(v.position, 1.0f));
                    points.push_back(p);
                    lo = glm::min(lo, p);
                    hi = glm::max(hi, p);
                }
                for (auto index : mesh.indices) require(index < mesh.vertices.size(), "Invalid index " + id);
                const auto n = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
                require(n % 3 == 0, "Nontriangle geometry " + id);
                tris += n / 3;
                for (std::size_t t = 0; t < n; t += 3) {
                    const auto& a = mesh.vertices.at(mesh.indices.empty() ? t : mesh.indices[t]);
                    const auto& b = mesh.vertices.at(mesh.indices.empty() ? t + 1 : mesh.indices[t + 1]);
                    const auto& c = mesh.vertices.at(mesh.indices.empty() ? t + 2 : mesh.indices[t + 2]);
                    const auto normal = glm::cross(b.position - a.position, c.position - a.position);
                    require(glm::length(normal) > 1e-10f, "Degenerate triangle " + id);
                    require(glm::dot(normal, a.normal + b.normal + c.normal) > 0.0f, "Incoherent winding/normal " + id);
                }
            });
            require(tris > 0, "No rendered triangles " + id);
            totalTriangles += tris;
            const auto size = hi - lo;
            require(glm::all(glm::greaterThan(size, glm::vec3(0.0f))), "Invalid dimensions " + id);
            std::set<std::array<int, 3>> shape;
            for (const auto& p : points) {
                const auto v = (p - lo) / size;
                shape.insert({int(std::lround(v.x * 10000.0f)), int(std::lround(v.y * 10000.0f)), int(std::lround(v.z * 10000.0f))});
            }
            require(shapes.insert(std::move(shape)).second, "Duplicate geometry after position/scale normalization " + id);
            report << id << ',' << tris << ',' << model->meshes.size() << ',' << model->textures.size() << ','
                   << size.x << ',' << size.y << ',' << size.z << '\n';
            require(assets.loadModel("./" + path) == model, "Canonical EcoEnergy resource not reused");
        }
        report.close();
        assets.clear();

        rejects([] { ecoEnergyModelId(static_cast<ecoenergy::Model>(0)); });
        rejects([] { ecoEnergyModelId(static_cast<ecoenergy::Model>(29)); });

        const auto entities = makeEcoEnergyInstances();
        std::set<std::string> ids, models;
        std::size_t interactiveCount = 0, simpleCount = 0, keyframeCount = 0, proceduralCount = 0;
        for (const auto& e : entities) {
            require(ids.insert(e.id).second, "Duplicate EcoEnergy instance ID: " + e.id);
            models.insert(e.model);
            (void)normalMatrix(e.transform.matrix());
            interactiveCount += e.interaction.has_value();
            const auto fam = animationFamilyName(e.animation);
            simpleCount += fam == "simple";
            keyframeCount += fam == "keyframe";
            proceduralCount += fam == "procedural";
        }
        require(models.size() == 28 && entities.size() == 60, "EcoEnergy requires 28 unique models across 60 instances");
        require(interactiveCount == 6, "EcoEnergy requires 6 interactive objects");
        require(simpleCount == 4 && keyframeCount == 2 && proceduralCount == 2,
                "EcoEnergy requires 4 simple, 2 keyframe and 2 procedural animations");

        // Rechazo de parámetros inválidos en EcoEnergySettings.
        auto bad = ecoenergy::settings; bad.solarTiltDegrees = 89.5f; rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.solarPanelScale.x = 0.0f; rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.secondaryTurbineScale = -1.0f; rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.mainRotorSpeedDeg = std::numeric_limits<float>::quiet_NaN(); rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.trackerDurationSeconds = 0.0; rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.chargeIndicatorAmplitudeMeters = 2.0f; rejects([&] { makeEcoEnergyInstances(bad); });
        bad = ecoenergy::settings; bad.visible[2] = false;
        require(makeEcoEnergyInstances(bad).size() + 7 == entities.size(), "Walkway visibility toggle failed");

        // Independencia de FPS: 30 FPS vs 144 FPS vs un solo paso de 3.0 s.
        {
            ContentRegistry reg30(root), reg144(root), regOne(root);
            Host h30(reg30), h144(reg144), hOne(regOne);
            ModuleSession s30(makeEcoEnergy(), h30), s144(makeEcoEnergy(), h144), sOne(makeEcoEnergy(), hOne);
            for (int step = 0; step < 90; ++step) reg30.update(3.0 / 90.0);
            for (int step = 0; step < 432; ++step) reg144.update(3.0 / 432.0);
            regOne.update(3.0);
            const auto p30 = reg30.placements(), p144 = reg144.placements(), pOne = regOne.placements();
            require(p30.size() == p144.size() && p30.size() == pOne.size(), "FPS test placement count mismatch");
            for (std::size_t idx = 0; idx < p30.size(); ++idx) {
                for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) {
                    require(std::abs(p30[idx].world[c][r] - p144[idx].world[c][r]) < 1e-4f,
                            "30 vs 144 FPS drift on " + p30[idx].id);
                    require(std::abs(p30[idx].world[c][r] - pOne[idx].world[c][r]) < 1e-4f,
                            "Stepped vs single-step drift on " + p30[idx].id);
                }
            }
        }

        // Registro SDK v1.0, las 6 interacciones reales, pausa/reinicio, coexistencia y ciclo de vida.
        ContentRegistry registry(root);
        Host host(registry);
        std::weak_ptr<const Model> retained;
        std::vector<ContentPlacement> snapshot;
        {
            ModuleSession energy(makeEcoEnergy(), host);
            require(registry.placements().size() == entities.size()+3, "EcoEnergy registration count mismatch");
            require(registry.lighting("ecoenergy").has_value() && !registry.lighting("ecoenergy")->legacyUnlit,
                    "EcoEnergy warm Blinn-Phong lighting not configured");

            auto& inter = registry.interactions("ecoenergy");
            require(inter.objects().size() == 6, "Expected 6 registered interactions");
            require(inter.objects().front().id == "ENERGY-005-panel-adjustable",
                    "Adjustable solar panel must be first interactive object for front ray-pick");

            const std::array<const char*, 6> expectedInteractive{{
                "ENERGY-005-panel-adjustable", "ENERGY-019-blade-demo", "ENERGY-011-battery-module",
                "ENERGY-023-generator-coil", "ENERGY-027-charge-connector", "ENERGY-026-ev-interactive"
            }};
            for (const char* id : expectedInteractive) {
                require(inter.select(id), std::string("Cannot select ") + id);
                const auto beforeWorld = inter.world(id);
                inter.manipulate({0.35f, 0.15f, -0.25f}, glm::angleAxis(glm::radians(20.0f), glm::vec3{0.0f, 1.0f, 0.0f}), 1.1f);
                const auto movedWorld = inter.world(id);
                require(beforeWorld != movedWorld, std::string("TRS manipulation failed on ") + id);
                inter.take(Ray{{0.0f, 1.6f, 4.0f}, {0.0f, 0.0f, -1.0f}}, 2.5f);
                require(inter.holding(), std::string("Take failed on ") + id);
                inter.follow(Ray{{0.5f, 1.6f, 3.2f}, {0.0f, 0.0f, -1.0f}});
                require(inter.world(id) != movedWorld, std::string("Camera carry failed on ") + id);
                inter.drop();
                require(!inter.holding(), std::string("Drop failed on ") + id);
            }
            inter.deselect();

            ModuleSession hub(makePlaza(), host), water(makeAquaBio(), host), cycle(makeEcoCycle(), host);
            require(registry.placements().size() == entities.size() + 90 + makeAquaBioInstances().size() + makeEcoCycleInstances().size()+12, "Four-session coexistence failed");

            snapshot = registry.placements();
            std::map<std::string, const Model*> shared;
            for (const auto& p : snapshot) if (p.owner == "ecoenergy") {
                require(p.resource.get() == p.model.get(), "Default EcoEnergy duplicated shared CPU/GPU model");
                const auto modelId = p.id.substr(0, 10);
                const auto found = shared.find(modelId);
                if (found != shared.end()) require(found->second == p.resource.get(), "Repeated EcoEnergy instance reloaded its GLB");
                shared[modelId] = p.resource.get();
                retained = p.resource;
            }
            require(shared.size() == 31, "EcoEnergy requires 28 original plus 3 shared GLBs");
            registry.update(0.25);
            energy.update(0.25);
            rejects([&] { registry.open("ecoenergy"); });
        }
        require(registry.placements().empty() && !retained.expired(), "Snapshot must keep shared Model alive");
        snapshot.clear();
        require(retained.expired(), "Last EcoEnergy Model reference did not release on shutdown");

        // Recuperación ante inicialización parcial fallida, GLB corrupto y escape de ruta.
        std::filesystem::create_directories(scratch / "ecoenergy/models");
        for (const auto* id : {"ENERGY-001", "ENERGY-005"})
            std::filesystem::copy_file(root / "ecoenergy/models" / (std::string(id) + ".glb"),
                                       scratch / "ecoenergy/models" / (std::string(id) + ".glb"),
                                       std::filesystem::copy_options::overwrite_existing);
        ContentRegistry missing(scratch);
        Host brokenHost(missing);
        rejects([&] { ModuleSession partial(makeEcoEnergy(), brokenHost); });
        require(missing.placements().empty(), "Partial EcoEnergy init leaked content");
        {
            std::ofstream corrupt(scratch / "corrupt.glb", std::ios::binary);
            corrupt << "not a valid GLB";
        }
        auto broken = missing.open("corrupt");
        auto d = entities.front();
        d.model = "corrupt.glb";
        rejects([&] { broken->add(d); });
        d.model = "../escape.glb";
        rejects([&] { broken->add(d); });
        require(missing.placements().empty(), "Corrupt or escaped GLB registered");

        std::cout << "ECOENERGY CPU PASS: 28 distinct models; " << totalTriangles << " resource triangles; "
                  << entities.size() << " instances; 6 interactions; 4S/2K/2P FPS-independent animations; "
                  << "resource sharing, coexistence, lifetime cleanup and rollback\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
