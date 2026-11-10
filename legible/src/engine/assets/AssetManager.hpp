#pragma once
#include "Model.hpp"
#include <filesystem>
#include <map>
#include <memory>

namespace ecosphere {
// carga modelos GLB en memoria y los guarda en caché por ruta
class AssetManager final {
public:
    explicit AssetManager(const std::filesystem::path& root);
    std::filesystem::path resolve(const std::filesystem::path& relative) const;
    std::shared_ptr<const Model> loadModel(const std::filesystem::path& relative);
    void clear() noexcept { models_.clear(); }
private:
    std::filesystem::path root_;
    std::map<std::filesystem::path, std::shared_ptr<const Model>> models_;
};
}
