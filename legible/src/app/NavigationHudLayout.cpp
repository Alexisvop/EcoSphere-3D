#include "NavigationHudLayout.hpp"
#include <algorithm>
#include <array>
#include <sstream>

namespace ecosphere {
namespace {
// fuente de 5x7 píxeles, cada número es una fila (bit 4 = columna izquierda)
struct Glyph {
    char character;
    std::array<unsigned, 7> rows;
};

const Glyph glyphs[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {31, 4, 4, 4, 4, 4, 31}},      {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 27, 17}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {':', {0, 4, 4, 0, 4, 4, 0}},        {'|', {4, 4, 4, 4, 4, 4, 4}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
};

// caracteres sin dibujo quedan en blanco
std::array<unsigned, 7> glyph(char character) {
    for (const auto& entry : glyphs) {
        if (entry.character == character) return entry.rows;
    }
    return {};
}

void quad(Mesh& mesh, float x, float y, float w, float h) {
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    for (const auto& corner : {glm::vec3(x, y, 0), glm::vec3(x + w, y, 0), glm::vec3(x + w, y + h, 0),
                               glm::vec3(x, y + h, 0)})
        mesh.vertices.push_back({corner, {0, 0, 1}, {0, 0}});
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
}

// parte un párrafo en renglones de hasta "columns" letras
std::vector<std::string> wrapParagraph(const std::string& paragraph, std::size_t columns) {
    std::istringstream words(paragraph);
    std::string word;
    std::string row;
    std::vector<std::string> wrapped;
    while (words >> word) {
        while (word.size() > columns) {
            if (!row.empty()) {
                wrapped.push_back(row);
                row.clear();
            }
            wrapped.push_back(word.substr(0, columns));
            word.erase(0, columns);
        }
        if (!row.empty() && row.size() + 1 + word.size() > columns) {
            wrapped.push_back(row);
            row.clear();
        }
        if (!row.empty()) row += ' ';
        row += word;
    }
    if (!row.empty()) wrapped.push_back(row);
    return wrapped;
}
}

NavigationHudLayout layoutNavigationHud(const std::string& message, int width, int height) {
    NavigationHudLayout result;
    if (width < 160 || height < 96 || message.empty()) return result;
    result.pixel = width < 800 ? 1.5f : 2.f;
    const float padding = 8;
    const float advance = 6 * result.pixel;
    const float lineHeight = 9 * result.pixel;
    const float usableWidth = std::min(float(width) - 20, 1100.f) - 2 * padding;
    const auto columns = static_cast<std::size_t>(usableWidth / advance);
    const auto maxRows = static_cast<std::size_t>((height / 3.f - 2 * padding) / lineHeight);

    // un párrafo que ya no cabe completo se corta entero
    std::istringstream paragraphs(message);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph)) {
        const auto wrapped = wrapParagraph(paragraph, columns);
        if (result.lines.size() + wrapped.size() > maxRows) {
            result.truncated = true;
            break;
        }
        result.lines.insert(result.lines.end(), wrapped.begin(), wrapped.end());
    }
    if (result.lines.empty()) return result;

    std::size_t longest = 0;
    for (const auto& line : result.lines) longest = std::max(longest, line.size());
    result.width = 2 * padding + longest * advance;
    result.height = 2 * padding + result.lines.size() * lineHeight;
    result.y = height - 10 - result.height;
    return result;
}

std::shared_ptr<const Model> makeNavigationHudModel(const NavigationHudLayout& layout) {
    auto model = std::make_shared<Model>();

    // fondo oscuro un poco atrás del texto
    Mesh panel;
    panel.material.unlit = true;
    panel.color = {.008f, .025f, .035f, 1};
    quad(panel, 0, 0, layout.width, layout.height);
    for (auto& vertex : panel.vertices) vertex.position.z = -.1f;
    model->meshes.push_back(std::move(panel));

    // un cuadrito por cada píxel encendido de cada letra
    Mesh letters;
    letters.material.unlit = true;
    letters.color = {.93f, .98f, .9f, 1};
    for (std::size_t row = 0; row < layout.lines.size(); ++row) {
        for (std::size_t column = 0; column < layout.lines[row].size(); ++column) {
            const auto pixels = glyph(layout.lines[row][column]);
            for (unsigned y = 0; y < 7; ++y) {
                for (unsigned x = 0; x < 5; ++x) {
                    if (!(pixels[y] & (1u << (4 - x)))) continue;
                    quad(letters, 8 + (column * 6 + x) * layout.pixel,
                         layout.height - 8 - (row * 9 + y + 1) * layout.pixel, layout.pixel, layout.pixel);
                }
            }
        }
    }
    model->meshes.push_back(std::move(letters));

    ModelNode node;
    node.meshes = {0, 1};
    model->nodes.push_back(std::move(node));
    return model;
}
}
