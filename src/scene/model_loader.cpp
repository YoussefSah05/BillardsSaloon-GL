#include "scene/model_loader.h"

#include <cgltf.h>
#include <meshoptimizer.h>
#include <stb_image.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <functional>
#include <map>
#include <stdexcept>

namespace BilliardsSaloon
{
    namespace
    {
        struct GltfFile
        {
            cgltf_data* data {nullptr};
            ~GltfFile()
            {
                if (data != nullptr)
                {
                    cgltf_free(data);
                }
            }
        };

        std::shared_ptr<const ModelTexture> decodeImage(const cgltf_image* image, const std::filesystem::path& folder)
        {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = nullptr;
            if (image->buffer_view != nullptr)
            {
                const cgltf_buffer_view* view = image->buffer_view;
                const auto* bytes = static_cast<const stbi_uc*>(view->buffer->data) + view->offset;
                pixels = stbi_load_from_memory(bytes, static_cast<int>(view->size), &width, &height, &channels, 4);
            }
            else if (image->uri != nullptr)
            {
                std::string uri = image->uri;
                cgltf_decode_uri(uri.data());
                uri.resize(std::char_traits<char>::length(uri.c_str()));
                pixels = stbi_load((folder / uri).string().c_str(), &width, &height, &channels, 4);
            }
            if (pixels == nullptr)
            {
                return nullptr;
            }
            auto texture = std::make_shared<ModelTexture>();
            texture->width = width;
            texture->height = height;
            texture->rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
            stbi_image_free(pixels);
            return texture;
        }

        // Reads `count` float vectors of `components` from an accessor.
        std::vector<float> readFloats(const cgltf_accessor* accessor, std::size_t components)
        {
            std::vector<float> values(accessor->count * components);
            for (cgltf_size i = 0; i < accessor->count; ++i)
            {
                cgltf_accessor_read_float(accessor, i, values.data() + i * components, components);
            }
            return values;
        }

        MeshData readPrimitive(const cgltf_primitive& primitive, const glm::mat4& world)
        {
            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* uvs = nullptr;
            for (cgltf_size a = 0; a < primitive.attributes_count; ++a)
            {
                const cgltf_attribute& attribute = primitive.attributes[a];
                if ((attribute.type == cgltf_attribute_type_position) && (attribute.index == 0)) positions = attribute.data;
                if ((attribute.type == cgltf_attribute_type_normal) && (attribute.index == 0)) normals = attribute.data;
                if ((attribute.type == cgltf_attribute_type_texcoord) && (attribute.index == 0)) uvs = attribute.data;
            }
            MeshData mesh;
            if (positions == nullptr)
            {
                return mesh;
            }

            const std::vector<float> p = readFloats(positions, 3);
            const std::vector<float> n = (normals != nullptr) ? readFloats(normals, 3) : std::vector<float>{};
            const std::vector<float> t = (uvs != nullptr) ? readFloats(uvs, 2) : std::vector<float>{};
            const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(world));

            mesh.vertices.resize(positions->count);
            for (std::size_t i = 0; i < positions->count; ++i)
            {
                MeshVertex& v = mesh.vertices[i];
                v.position = glm::vec3(world * glm::vec4(p[i * 3], p[i * 3 + 1], p[i * 3 + 2], 1.0f));
                if (!n.empty())
                {
                    v.normal = glm::normalize(normalMatrix * glm::vec3(n[i * 3], n[i * 3 + 1], n[i * 3 + 2]));
                }
                if (!t.empty())
                {
                    v.uv = glm::vec2(t[i * 2], t[i * 2 + 1]);
                }
            }

            if (primitive.indices != nullptr)
            {
                mesh.indices.resize(primitive.indices->count);
                for (cgltf_size i = 0; i < primitive.indices->count; ++i)
                {
                    mesh.indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
                }
            }
            else
            {
                for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(positions->count); ++i)
                {
                    mesh.indices.push_back(i);
                }
            }

            // A mirroring transform flips the winding; restore counter-clockwise.
            if (glm::determinant(glm::mat3(world)) < 0.0f)
            {
                for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
                {
                    std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
                }
            }
            return mesh;
        }

        void visit(const cgltf_node* node, const glm::mat4& parent,
                   const std::function<void(const cgltf_mesh&, const glm::mat4&)>& onMesh)
        {
            float local[16];
            cgltf_node_transform_local(node, local);
            const glm::mat4 world = parent * glm::make_mat4(local);
            if (node->mesh != nullptr)
            {
                onMesh(*node->mesh, world);
            }
            for (cgltf_size c = 0; c < node->children_count; ++c)
            {
                visit(node->children[c], world, onMesh);
            }
        }
    }

    MeshData simplifyMesh(const MeshData& mesh, float ratio, float error)
    {
        const std::size_t target = static_cast<std::size_t>(static_cast<float>(mesh.indices.size()) * ratio) / 3 * 3;
        // Exported meshes split vertices wherever normals or UVs differ, and
        // the simplifier will not collapse across such seams. Simplify a copy
        // welded by position only; at LOD distances the seams do not show.
        std::vector<std::uint32_t> welded(mesh.indices.size());
        meshopt_generateShadowIndexBuffer(welded.data(), mesh.indices.data(), mesh.indices.size(),
                                          &mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(glm::vec3), sizeof(MeshVertex));

        std::vector<std::uint32_t> indices(mesh.indices.size());
        float achieved = 0.0f;
        const std::size_t count = meshopt_simplify(indices.data(), welded.data(), welded.size(),
                                                   &mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(MeshVertex),
                                                   std::max<std::size_t>(target, 3), error, 0, &achieved);
        std::size_t kept = count;
        if (count > target + target / 2)
        {
            // Many small separate pieces (thin rods, screws) cannot collapse
            // within the error bound. Fall back to clustering vertices on a
            // grid, which ignores topology: rougher, but this is a distant level.
            kept = meshopt_simplifySloppy(indices.data(), welded.data(), welded.size(),
                                          &mesh.vertices[0].position.x, mesh.vertices.size(), sizeof(MeshVertex), nullptr,
                                          std::max<std::size_t>(target, 3), 1.0f, &achieved);
        }
        indices.resize(kept);

        // Keep only the vertices still used.
        MeshData simplified;
        std::vector<std::uint32_t> remap(mesh.vertices.size(), UINT32_MAX);
        for (const std::uint32_t index : indices)
        {
            if (remap[index] == UINT32_MAX)
            {
                remap[index] = static_cast<std::uint32_t>(simplified.vertices.size());
                simplified.vertices.push_back(mesh.vertices[index]);
            }
            simplified.indices.push_back(remap[index]);
        }
        return simplified;
    }

    ModelData loadModel(const std::filesystem::path& path, int lodLevels)
    {
        GltfFile file;
        cgltf_options options {};
        const std::string name = path.string();
        if ((cgltf_parse_file(&options, name.c_str(), &file.data) != cgltf_result_success) ||
            (cgltf_load_buffers(&options, file.data, name.c_str()) != cgltf_result_success))
        {
            throw std::runtime_error("Could not read model " + name);
        }

        ModelData model;
        model.name = path.stem().string();
        const std::filesystem::path folder = path.parent_path();

        // Images are decoded once, shared by every material that uses them.
        std::map<const cgltf_image*, std::shared_ptr<const ModelTexture>> images;
        const auto image = [&](const cgltf_texture_view& view) -> std::shared_ptr<const ModelTexture>
        {
            if ((view.texture == nullptr) || (view.texture->image == nullptr))
            {
                return nullptr;
            }
            auto found = images.find(view.texture->image);
            if (found == images.end())
            {
                found = images.emplace(view.texture->image, decodeImage(view.texture->image, folder)).first;
            }
            return found->second;
        };

        for (cgltf_size m = 0; m < file.data->materials_count; ++m)
        {
            const cgltf_material& source = file.data->materials[m];
            ModelMaterial material;
            if (source.has_pbr_metallic_roughness)
            {
                const cgltf_pbr_metallic_roughness& pbr = source.pbr_metallic_roughness;
                material.baseColor = glm::make_vec4(pbr.base_color_factor);
                material.metallic = pbr.metallic_factor;
                material.roughness = pbr.roughness_factor;
                material.baseColorMap = image(pbr.base_color_texture);
                material.ormMap = image(pbr.metallic_roughness_texture);
            }
            material.normalMap = image(source.normal_texture);
            material.alphaMask = source.alpha_mode == cgltf_alpha_mode_mask;
            material.doubleSided = source.double_sided != 0;
            // glTF exporters often pack occlusion into the metal-roughness
            // map's red channel: use it when both point at the same image.
            const cgltf_texture* occlusion = source.occlusion_texture.texture;
            const cgltf_texture* packed = source.pbr_metallic_roughness.metallic_roughness_texture.texture;
            material.occlusionInOrm = (occlusion != nullptr) && (packed != nullptr) && (occlusion->image == packed->image);
            model.materials.push_back(material);
        }
        if (model.materials.empty())
        {
            model.materials.emplace_back();
        }

        const auto onMesh = [&](const cgltf_mesh& mesh, const glm::mat4& world)
        {
            for (cgltf_size p = 0; p < mesh.primitives_count; ++p)
            {
                const cgltf_primitive& primitive = mesh.primitives[p];
                // Blended surfaces (clock glass) would need sorted
                // transparency; until the renderer has it they are left out.
                if ((primitive.type != cgltf_primitive_type_triangles) ||
                    ((primitive.material != nullptr) && (primitive.material->alpha_mode == cgltf_alpha_mode_blend)))
                {
                    continue;
                }
                MeshData data = readPrimitive(primitive, world);
                if (data.indices.empty())
                {
                    continue;
                }
                model.bounds.add(boundsOf(data));

                ModelPart part;
                part.material = (primitive.material != nullptr)
                    ? static_cast<std::size_t>(primitive.material - file.data->materials) : 0;
                part.lods.push_back(std::make_shared<const MeshData>(data));
                float ratio = 1.0f;
                for (int level = 1; level < lodLevels; ++level)
                {
                    ratio *= 0.35f;
                    part.lods.push_back(std::make_shared<const MeshData>(simplifyMesh(data, ratio, 0.02f * static_cast<float>(level))));
                }
                model.parts.push_back(std::move(part));
            }
        };

        const cgltf_scene* scene = (file.data->scene != nullptr) ? file.data->scene
                                 : ((file.data->scenes_count > 0) ? &file.data->scenes[0] : nullptr);
        if (scene != nullptr)
        {
            for (cgltf_size n = 0; n < scene->nodes_count; ++n)
            {
                visit(scene->nodes[n], glm::mat4(1.0f), onMesh);
            }
        }
        if (model.parts.empty())
        {
            throw std::runtime_error("Model " + name + " has no triangles");
        }
        return model;
    }
}
