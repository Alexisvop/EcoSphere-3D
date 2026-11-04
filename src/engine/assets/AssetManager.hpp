#pragma once
#include "Model.hpp"
#include <filesystem>
#include <map>
#include <memory>

namespace ecosphere {
// CPU solamente. La raíz absoluta se fija al construir, independiente del cwd posterior.
class AssetManager final {
public:
    explicit AssetManager(const std::filesystem::path& root);
    std::filesystem::path resolve(const std::filesystem::path& relative) const;
    std::shared_ptr<const Model> loadModel(const std::filesystem::path& relative);
    // shortcut: sin recarga automática de archivos editados; clear y volver a cargar al reemplazar assets.
    void clear() noexcept { models_.clear(); }
private:
    std::filesystem::path root_;
    std::map<std::filesystem::path, std::shared_ptr<const Model>> models_;
};
}
