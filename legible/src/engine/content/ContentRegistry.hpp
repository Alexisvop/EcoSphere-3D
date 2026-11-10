#pragma once
#include <ecosphere/sdk/Content.hpp>
#include "assets/Model.hpp"
#include "interaction/Interaction.hpp"
#include <filesystem>
#include <memory>
#include <vector>

namespace ecosphere {
// una entidad lista para dibujar: dueño, id, modelo y matriz de mundo
struct ContentPlacement {
    std::string owner, id;
    std::shared_ptr<const Model> resource, model;
    glm::mat4 world{1};
};

class RegisteredContent;

// guarda las entidades que agrega cada módulo
class ContentRegistry final {
public:
    explicit ContentRegistry(const std::filesystem::path& assetRoot);
    ContentRegistry(const ContentRegistry&) = delete;
    ContentRegistry& operator=(const ContentRegistry&) = delete;
    std::unique_ptr<ContentServices> open(std::string_view owner);
    std::vector<ContentPlacement> placements() const;
    void update(double deltaSeconds);
    std::optional<LightingSettings> lighting(const std::string& owner) const;
    InteractionSystem& interactions(const std::string& owner);
    bool playing(const std::string& owner) const;
    void setPlaying(const std::string& owner, bool playing);
    void restart(const std::string& owner);
private:
    friend class RegisteredContent;
    struct State;
    std::shared_ptr<State> state_;
};
}
