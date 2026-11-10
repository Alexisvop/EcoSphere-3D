#pragma once
#include <ecosphere/sdk/IModule.hpp>
#include <ecosphere/sdk/ModuleServices.hpp>
#include <utility>

namespace ecosphere {
// módulo mínimo con un solo objeto de ejemplo
class StubModule final : public IModule {
public:
    StubModule(std::string_view id, glm::vec3 color, EntityDescriptor example)
        : id_(id), color_(color), example_(std::move(example)) {}

    std::string_view id() const noexcept override { return id_; }

    void initialize(ModuleServices& services) override {
        services_ = &services;
        services_->log(id_);
        services_->log(services_->content() ? "initialize content" : "initialize stub");
        services_->setClearColor(color_);
        if (auto* content = services_->content()) {
            content->add(example_);
            LightingSettings lighting;
            lighting.legacyUnlit = false;
            content->configureLighting(lighting);
        }
    }
    void update(double) override {}

    void shutdown() noexcept override {
        if (!services_) return;
        services_->log(id_);
        services_->log(services_->content() ? "shutdown content" : "shutdown stub");
        services_ = nullptr;
    }

private:
    std::string_view id_;
    glm::vec3 color_;
    EntityDescriptor example_;
    ModuleServices* services_ = nullptr;
};
}
