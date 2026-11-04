#include "ContentRegistry.hpp"
#include "assets/AssetManager.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>

namespace ecosphere {
namespace {
void validateId(std::string_view id) {
    if (id.empty() || id.size() > 128 || !std::all_of(id.begin(),id.end(),[](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    })) throw std::invalid_argument("Content ID requires 1..128 ASCII letters, digits, '.', '_' or '-'");
}
Aabb modelBounds(const Model& model) {
    Aabb bounds{glm::vec3(std::numeric_limits<float>::infinity()),glm::vec3(-std::numeric_limits<float>::infinity())};
    model.forEach([&](std::size_t mesh, const glm::mat4& world) {
        for (const auto& vertex : model.meshes.at(mesh).vertices) {
            const auto p = glm::vec3(world*glm::vec4(vertex.position,1));
            bounds.minimum = glm::min(bounds.minimum,p);
            bounds.maximum = glm::max(bounds.maximum,p);
        }
    });
    return bounds;
}
Pose sample(const std::optional<AnimationDescriptor>& animation, const AnimationPlayer& player) {
    if (!animation) return {};
    return std::visit([&](const auto& source) -> Pose {
        using Source = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<Source,KeyframeTrack>) return source.sample(player.time());
        else if constexpr (std::is_same_v<Source,SimpleAnimation>) return source.sample(player.elapsed());
        else return sampleProcedural(source,player.elapsed());
    },animation->source);
}
struct Entity {
    EntityDescriptor descriptor;
    std::shared_ptr<const Model> resource, model;
    AnimationPlayer player;
    Pose animation;
    Entity(const EntityDescriptor& d, std::shared_ptr<const Model> source)
        : descriptor(d), resource(std::move(source)), model(resource),
          player(d.animation ? d.animation->duration : 1, d.animation ? d.animation->repeat : true) {
        if (d.animation) player.setSpeed(d.animation->speed);
        animation = sample(d.animation,player);
        (void)normalMatrix(d.transform.matrix()*animation.matrix());
        if (!d.visual) return;
        validateMaterial(d.visual->material);
        for (int c=0; c<4; ++c)
            if (!std::isfinite(d.visual->tint[c]) || d.visual->tint[c] < 0 || d.visual->tint[c] > 1)
                throw std::invalid_argument("Content tint requires finite linear RGBA in [0,1]");
        // shortcut: la variante visual copia el modelo CPU, usar overrides por instancia si el perfil lo exige.
        auto variant = std::make_shared<Model>(*resource);
        for (auto& mesh : variant->meshes) {
            mesh.color *= d.visual->tint;
            mesh.material = d.visual->material;
        }
        model = std::move(variant);
    }
};
struct ModuleContent {
    std::map<std::string,Entity> entities;
    InteractionSystem interaction;
    std::optional<LightingSettings> lighting;
    bool playing = true;
};
void setModulePlaying(ModuleContent& module, bool playing) {
    for (auto& item : module.entities) item.second.player.setPlaying(playing);
    module.playing = playing;
}
void updateContent(ModuleContent& module, double delta, bool restart) {
    struct Pending { Entity* entity; AnimationPlayer player; Pose pose; };
    std::vector<Pending> pending;
    auto nextInteraction = module.interaction;
    for (auto& item : module.entities) {
        auto& entity = item.second;
        auto player = entity.player;
        if (restart) player.restart(); else player.update(delta);
        const auto pose = sample(entity.descriptor.animation,player);
        if (entity.descriptor.interaction) {
            nextInteraction.object(item.first).animation = pose;
            (void)nextInteraction.world(item.first);
        } else (void)normalMatrix(entity.descriptor.transform.matrix()*pose.matrix());
        pending.push_back({&entity,player,pose});
    }
    // Publicar solo después de evaluar/validar toda la instalación.
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
    std::shared_ptr<const Model> load(const std::string& relative) {
        const auto path = assets.resolve(relative);
        const auto found = resources.find(path);
        if (found != resources.end()) if (auto model = found->second.lock()) return model;
        auto model = assets.loadModel(relative);
        assets.clear(); // El registro comparte referencias débiles; las entidades/snapshots poseen el recurso.
        resources[path] = model;
        return model;
    }
    AssetManager assets;
    std::map<std::filesystem::path,std::weak_ptr<const Model>> resources;
    std::map<std::string,ModuleContent> modules;
};

class RegisteredContent final : public ContentServices {
public:
    RegisteredContent(std::shared_ptr<ContentRegistry::State> state, std::string owner)
        : state_(std::move(state)), owner_(std::move(owner)) {
        if (!state_->modules.try_emplace(owner_).second)
            throw std::invalid_argument("Content session already active: " + owner_);
    }
    ~RegisteredContent() override {
        state_->modules.erase(owner_);
        for (auto it=state_->resources.begin(); it!=state_->resources.end(); ) {
            if (it->second.expired()) it=state_->resources.erase(it); else ++it;
        }
    }
    void add(const EntityDescriptor& d) override {
        validateId(d.id);
        auto& content = module();
        if (content.entities.count(d.id)) throw std::invalid_argument("Duplicate content ID: " + owner_ + "/" + d.id);
        validatePose(d.transform);
        try {
            Entity entity(d,state_->load(d.model));
            auto nextInteraction = content.interaction;
            if (d.interaction) {
                InteractiveObject object;
                object.id = d.id; object.base = d.transform; object.animation = entity.animation;
                object.bounds = d.interaction->bounds ? *d.interaction->bounds : modelBounds(*entity.resource);
                nextInteraction.add(std::move(object));
                (void)nextInteraction.world(d.id);
            }
            content.entities.emplace(d.id,std::move(entity));
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
        } else (void)normalMatrix(transform.matrix()*entity.animation.matrix());
        entity.descriptor.transform = transform;
    }
    void manipulate(const std::string& id, glm::vec3 translation, glm::quat rotation, float scale) override {
        if (!module().entities.at(id).descriptor.interaction)
            throw std::invalid_argument("Entity is not interactive: " + id);
        auto next = module().interaction;
        next.select(id); next.manipulate(translation,rotation,scale);
        (void)next.world(id);
        module().interaction = std::move(next);
    }
    void setPlaying(bool playing) override {
        setModulePlaying(module(),playing);
    }
    void restart() override { updateContent(module(),0,true); }
    void configureLighting(const LightingSettings& lighting) override {
        validateLighting(lighting,glm::vec3(0));
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
    return std::make_unique<RegisteredContent>(state_,std::string(owner));
}
std::vector<ContentPlacement> ContentRegistry::placements() const {
    std::vector<ContentPlacement> result;
    for (const auto& module : state_->modules) for (const auto& item : module.second.entities) {
        const auto& entity = item.second;
        const auto world = entity.descriptor.interaction ? module.second.interaction.world(item.first) :
            entity.descriptor.transform.matrix()*entity.animation.matrix();
        result.push_back({module.first,item.first,entity.resource,entity.model,world});
    }
    return result;
}
void ContentRegistry::update(double delta) {
    validateSeconds(delta);
    for (auto& module : state_->modules) updateContent(module.second,delta,false);
}
bool ContentRegistry::playing(const std::string& owner) const { return state_->modules.at(owner).playing; }
void ContentRegistry::setPlaying(const std::string& owner, bool playing) { setModulePlaying(state_->modules.at(owner),playing); }
void ContentRegistry::restart(const std::string& owner) { updateContent(state_->modules.at(owner),0,true); }
std::optional<LightingSettings> ContentRegistry::lighting(const std::string& owner) const {
    return state_->modules.at(owner).lighting;
}
InteractionSystem& ContentRegistry::interactions(const std::string& owner) {
    return state_->modules.at(owner).interaction;
}
}
