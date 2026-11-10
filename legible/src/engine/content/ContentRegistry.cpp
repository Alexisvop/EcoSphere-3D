#include "ContentRegistry.hpp"
#include "assets/AssetManager.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>

namespace ecosphere {
namespace {
bool isIdCharacter(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.';
}

void validateId(std::string_view id) {
    if (id.empty() || id.size() > 128 || !std::all_of(id.begin(), id.end(), isIdCharacter)) {
        throw std::invalid_argument("Content ID requires 1..128 ASCII letters, digits, '.', '_' or '-'");
    }
}

// caja que envuelve todos los vértices del modelo ya transformados
Aabb modelBounds(const Model& model) {
    const float infinity = std::numeric_limits<float>::infinity();
    Aabb bounds{glm::vec3(infinity), glm::vec3(-infinity)};
    model.forEach([&](std::size_t mesh, const glm::mat4& world) {
        for (const auto& vertex : model.meshes.at(mesh).vertices) {
            const auto point = glm::vec3(world * glm::vec4(vertex.position, 1));
            bounds.minimum = glm::min(bounds.minimum, point);
            bounds.maximum = glm::max(bounds.maximum, point);
        }
    });
    return bounds;
}

// pose de la animación en el tiempo actual del reproductor
Pose samplePose(const std::optional<AnimationDescriptor>& animation, const AnimationPlayer& player) {
    if (!animation) return {};
    return std::visit([&](const auto& source) -> Pose {
        using Source = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<Source, KeyframeTrack>) {
            return source.sample(player.time());
        } else if constexpr (std::is_same_v<Source, SimpleAnimation>) {
            return source.sample(player.elapsed());
        } else {
            return sampleProcedural(source, player.elapsed());
        }
    }, animation->source);
}

void validateTint(const glm::vec4& tint) {
    for (int c = 0; c < 4; ++c) {
        if (!std::isfinite(tint[c]) || tint[c] < 0 || tint[c] > 1) {
            throw std::invalid_argument("Content tint requires finite linear RGBA in [0,1]");
        }
    }
}

struct Entity {
    EntityDescriptor descriptor;
    std::shared_ptr<const Model> resource, model;
    AnimationPlayer player;
    Pose animation;

    Entity(const EntityDescriptor& source, std::shared_ptr<const Model> loaded)
        : descriptor(source), resource(std::move(loaded)), model(resource),
          player(source.animation ? source.animation->duration : 1,
                 source.animation ? source.animation->repeat : true) {
        if (source.animation) player.setSpeed(source.animation->speed);
        animation = samplePose(source.animation, player);
        (void)normalMatrix(source.transform.matrix() * animation.matrix());
        if (!source.visual) return;
        validateMaterial(source.visual->material);
        validateTint(source.visual->tint);
        // copia del modelo con el tinte y el material de la entidad
        auto variant = std::make_shared<Model>(*resource);
        for (auto& mesh : variant->meshes) {
            mesh.color *= source.visual->tint;
            mesh.material = source.visual->material;
        }
        model = std::move(variant);
    }
};

struct ModuleContent {
    std::map<std::string, Entity> entities;
    InteractionSystem interaction;
    std::optional<LightingSettings> lighting;
    bool playing = true;
};

void setModulePlaying(ModuleContent& module, bool playing) {
    for (auto& item : module.entities) item.second.player.setPlaying(playing);
    module.playing = playing;
}

// avanza las animaciones, primero calcula todo y al final lo guarda
void updateContent(ModuleContent& module, double delta, bool restart) {
    struct Pending {
        Entity* entity;
        AnimationPlayer player;
        Pose pose;
    };
    std::vector<Pending> pending;
    auto nextInteraction = module.interaction;
    for (auto& [id, entity] : module.entities) {
        auto player = entity.player;
        if (restart) {
            player.restart();
        } else {
            player.update(delta);
        }
        const auto pose = samplePose(entity.descriptor.animation, player);
        if (entity.descriptor.interaction) {
            nextInteraction.object(id).animation = pose;
            (void)nextInteraction.world(id);
        } else {
            (void)normalMatrix(entity.descriptor.transform.matrix() * pose.matrix());
        }
        pending.push_back({&entity, player, pose});
    }
    for (auto& change : pending) {
        change.entity->player = change.player;
        change.entity->animation = change.pose;
    }
    module.interaction = std::move(nextInteraction);
    if (restart) module.playing = true;
}
}

struct ContentRegistry::State {
    explicit State(const std::filesystem::path& root) : assets(root) {}

    // reutiliza el modelo si alguna entidad todavía lo tiene cargado
    std::shared_ptr<const Model> load(const std::string& relative) {
        const auto path = assets.resolve(relative);
        const auto found = resources.find(path);
        if (found != resources.end()) {
            if (auto model = found->second.lock()) return model;
        }
        auto model = assets.loadModel(relative);
        assets.clear();
        resources[path] = model;
        return model;
    }

    AssetManager assets;
    std::map<std::filesystem::path, std::weak_ptr<const Model>> resources;
    std::map<std::string, ModuleContent> modules;
};

// servicios de contenido de un módulo abierto
class RegisteredContent final : public ContentServices {
public:
    RegisteredContent(std::shared_ptr<ContentRegistry::State> state, std::string owner)
        : state_(std::move(state)), owner_(std::move(owner)) {
        if (!state_->modules.try_emplace(owner_).second) {
            throw std::invalid_argument("Content session already active: " + owner_);
        }
    }

    ~RegisteredContent() override {
        state_->modules.erase(owner_);
        auto& resources = state_->resources;
        for (auto it = resources.begin(); it != resources.end();) {
            if (it->second.expired()) {
                it = resources.erase(it);
            } else {
                ++it;
            }
        }
    }

    void add(const EntityDescriptor& descriptor) override {
        validateId(descriptor.id);
        auto& content = module();
        if (content.entities.count(descriptor.id)) {
            throw std::invalid_argument("Duplicate content ID: " + owner_ + "/" + descriptor.id);
        }
        validatePose(descriptor.transform);
        try {
            Entity entity(descriptor, state_->load(descriptor.model));
            auto nextInteraction = content.interaction;
            if (descriptor.interaction) {
                InteractiveObject object;
                object.id = descriptor.id;
                object.base = descriptor.transform;
                object.animation = entity.animation;
                const auto& customBounds = descriptor.interaction->bounds;
                object.bounds = customBounds ? *customBounds : modelBounds(*entity.resource);
                nextInteraction.add(std::move(object));
                (void)nextInteraction.world(descriptor.id);
            }
            content.entities.emplace(descriptor.id, std::move(entity));
            content.interaction = std::move(nextInteraction);
        } catch (...) {
            state_->assets.clear();
            throw;
        }
    }

    void setTransform(const std::string& id, const Pose& transform) override {
        validatePose(transform);
        auto& entity = module().entities.at(id);
        if (entity.descriptor.interaction) {
            auto next = module().interaction;
            next.object(id).base = transform;
            (void)next.world(id);
            module().interaction = std::move(next);
        } else {
            (void)normalMatrix(transform.matrix() * entity.animation.matrix());
        }
        entity.descriptor.transform = transform;
    }

    void manipulate(const std::string& id, glm::vec3 translation, glm::quat rotation, float scale) override {
        if (!module().entities.at(id).descriptor.interaction) {
            throw std::invalid_argument("Entity is not interactive: " + id);
        }
        auto next = module().interaction;
        next.select(id);
        next.manipulate(translation, rotation, scale);
        (void)next.world(id);
        module().interaction = std::move(next);
    }

    void setPlaying(bool playing) override { setModulePlaying(module(), playing); }

    void restart() override { updateContent(module(), 0, true); }

    void configureLighting(const LightingSettings& lighting) override {
        validateLighting(lighting, glm::vec3(0));
        module().lighting = lighting;
    }

private:
    ModuleContent& module() { return state_->modules.at(owner_); }
    std::shared_ptr<ContentRegistry::State> state_;
    std::string owner_;
};

ContentRegistry::ContentRegistry(const std::filesystem::path& root) : state_(std::make_shared<State>(root)) {}

std::unique_ptr<ContentServices> ContentRegistry::open(std::string_view owner) {
    validateId(owner);
    return std::make_unique<RegisteredContent>(state_, std::string(owner));
}

// lista de lo que hay que dibujar, con su matriz de mundo
std::vector<ContentPlacement> ContentRegistry::placements() const {
    std::vector<ContentPlacement> result;
    for (const auto& [owner, module] : state_->modules) {
        for (const auto& [id, entity] : module.entities) {
            const auto world = entity.descriptor.interaction
                ? module.interaction.world(id)
                : entity.descriptor.transform.matrix() * entity.animation.matrix();
            result.push_back({owner, id, entity.resource, entity.model, world});
        }
    }
    return result;
}

void ContentRegistry::update(double delta) {
    validateSeconds(delta);
    for (auto& module : state_->modules) updateContent(module.second, delta, false);
}

bool ContentRegistry::playing(const std::string& owner) const {
    return state_->modules.at(owner).playing;
}

void ContentRegistry::setPlaying(const std::string& owner, bool playing) {
    setModulePlaying(state_->modules.at(owner), playing);
}

void ContentRegistry::restart(const std::string& owner) {
    updateContent(state_->modules.at(owner), 0, true);
}

std::optional<LightingSettings> ContentRegistry::lighting(const std::string& owner) const {
    return state_->modules.at(owner).lighting;
}

InteractionSystem& ContentRegistry::interactions(const std::string& owner) {
    return state_->modules.at(owner).interaction;
}
}
