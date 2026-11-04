#include <ecosphere/core/ModuleSession.hpp>
#include <ecosphere/modules/Stubs.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Exception, class Function>
void rejects(Function function) {
    try { function(); }
    catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
}
struct Services final : ecosphere::ModuleServices {
    std::vector<std::string> messages;
    glm::vec3 color{0.0f};
    void log(std::string_view text) noexcept override { messages.emplace_back(text); }
    void setClearColor(glm::vec3 value) noexcept override { color = value; }
};
struct State {
    int initialized = 0, closed = 0;
    double delta = -1;
    std::weak_ptr<int> resource;
};
struct Probe final : ecosphere::IModule {
    State& state;
    bool fail;
    std::shared_ptr<int> resource;
    Probe(State& value, bool shouldFail) : state(value), fail(shouldFail) {}
    std::string_view id() const noexcept override { return "probe"; }
    void initialize(ecosphere::ModuleServices&) override {
        ++state.initialized;
        resource = std::make_shared<int>(42);
        state.resource = resource;
        if (fail) throw std::runtime_error("simulated partial initialization");
    }
    void update(double delta) override { state.delta = delta; }
    void shutdown() noexcept override {
        resource.reset();
        ++state.closed;
    }
};
struct EmptyId final : ecosphere::IModule {
    std::string_view id() const noexcept override { return {}; }
    void initialize(ecosphere::ModuleServices&) override { throw std::logic_error("must not initialize"); }
    void update(double) override {}
    void shutdown() noexcept override {}
};
}

int main() {
    try {
        using Factory = std::unique_ptr<ecosphere::IModule> (*)();
        const std::array<Factory, 3> factories = {
            ecosphere::makeEcoEnergy, ecosphere::makeAquaBio, ecosphere::makeEcoCycle
        };
        const std::array<std::string_view, 3> ids = {"ecoenergy", "aquabio", "ecocycle"};
        const std::array<glm::vec3, 3> colors = {
            glm::vec3{0.12f, 0.10f, 0.03f}, {0.02f, 0.10f, 0.18f}, {0.04f, 0.14f, 0.06f}
        };
        for (std::size_t i = 0; i < factories.size(); ++i) {
            Services services;
            auto module = factories[i]();
            require(module->id() == ids[i], "Stable ID mismatch");
            {
                ecosphere::ModuleSession session(std::move(module), services);
                require(services.color == colors[i], "Module service not applied");
                session.update(0.0);
                session.update(1.0 / 60.0);
                require(services.messages.size() == 2, "Unexpected lifecycle events");
            }
            require(services.messages == std::vector<std::string>{
                std::string(ids[i]), "initialize stub", std::string(ids[i]), "shutdown stub"
            }, "Stub lifecycle mismatch");
            std::cout << ids[i] << ": initialize/update/shutdown PASS\n";
        }
        Services services;
        rejects<std::invalid_argument>([&] { ecosphere::ModuleSession session(nullptr, services); });
        rejects<std::invalid_argument>([&] {
            ecosphere::ModuleSession session(std::make_unique<EmptyId>(), services);
        });
        State state;
        {
            ecosphere::ModuleSession session(std::make_unique<Probe>(state, false), services);
            require(!state.resource.expired(), "Initialized module must own its resource");
            session.update(0.25);
            require(state.delta == 0.25, "Delta must be passed unchanged in seconds");
            for (double delta : {-1.0, std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()}) {
                rejects<std::invalid_argument>([&] { session.update(delta); });
            }
            require(state.delta == 0.25, "Invalid delta reached module");
        }
        require(state.initialized == 1 && state.closed == 1, "Normal shutdown must run once");
        require(state.resource.expired(), "Normal shutdown must release the resource");
        State failed;
        rejects<std::runtime_error>([&] {
            ecosphere::ModuleSession session(std::make_unique<Probe>(failed, true), services);
        });
        require(failed.initialized == 1 && failed.closed == 1, "Failed initialization must roll back once");
        require(failed.resource.expired(), "Failed initialization must not abandon resources");
        std::cout << "Contract validation and rollback PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
