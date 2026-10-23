#include "Aster/Scene/SceneSerializer.h"

#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Aster
{
    namespace
    {
        void SetError(std::string* error, std::string message)
        {
            if (error != nullptr)
            {
                *error = std::move(message);
            }
        }

        YAML::Node Vec3Node(const glm::vec3& value)
        {
            YAML::Node node(YAML::NodeType::Sequence);
            node.push_back(value.x);
            node.push_back(value.y);
            node.push_back(value.z);
            return node;
        }

        YAML::Node Vec4Node(const glm::vec4& value)
        {
            YAML::Node node(YAML::NodeType::Sequence);
            node.push_back(value.x);
            node.push_back(value.y);
            node.push_back(value.z);
            node.push_back(value.w);
            return node;
        }

        glm::vec3 ReadVec3(const YAML::Node& node, const glm::vec3& fallback)
        {
            if (!node.IsDefined() || !node.IsSequence() || node.size() < 3)
            {
                return fallback;
            }
            return glm::vec3(node[0].as<float>(fallback.x), node[1].as<float>(fallback.y),
                node[2].as<float>(fallback.z));
        }

        glm::vec4 ReadVec4(const YAML::Node& node, const glm::vec4& fallback)
        {
            if (!node.IsDefined() || !node.IsSequence() || node.size() < 4)
            {
                return fallback;
            }
            return glm::vec4(node[0].as<float>(fallback.x), node[1].as<float>(fallback.y),
                node[2].as<float>(fallback.z), node[3].as<float>(fallback.w));
        }

        float ReadFloat(const YAML::Node& node, const char* key, float fallback)
        {
            const YAML::Node value = node[key];
            return value.IsDefined() ? value.as<float>(fallback) : fallback;
        }

        bool ReadBool(const YAML::Node& node, const char* key, bool fallback)
        {
            const YAML::Node value = node[key];
            return value.IsDefined() ? value.as<bool>(fallback) : fallback;
        }

        uint32 ReadUint(const YAML::Node& node, const char* key, uint32 fallback)
        {
            const YAML::Node value = node[key];
            return value.IsDefined() ? value.as<uint32>(fallback) : fallback;
        }

        std::string ReadString(const YAML::Node& node, const char* key, const std::string& fallback)
        {
            const YAML::Node value = node[key];
            return value.IsDefined() ? value.as<std::string>(fallback) : fallback;
        }

        std::string BodyTypeToString(BodyType type)
        {
            switch (type)
            {
                case BodyType::Static: return "static";
                case BodyType::Kinematic: return "kinematic";
                default: return "dynamic";
            }
        }

        BodyType BodyTypeFromString(const std::string& value)
        {
            if (value == "static") return BodyType::Static;
            if (value == "kinematic") return BodyType::Kinematic;
            return BodyType::Dynamic;
        }

        YAML::Node SerializeMaterial(const Material& material)
        {
            YAML::Node node;
            node["name"] = material.Name;
            node["base_color"] = Vec4Node(material.BaseColor);
            node["metallic"] = material.Metallic;
            node["roughness"] = material.Roughness;
            node["emissive"] = Vec3Node(material.Emissive);
            node["emissive_strength"] = material.EmissiveStrength;
            node["opacity"] = material.Opacity;
            node["alpha_cutoff"] = material.AlphaCutoff;
            node["double_sided"] = material.DoubleSided;
            node["alpha_mask"] = material.AlphaMask;
            node["alpha_blend"] = material.AlphaBlend;
            if (material.AlbedoMap.IsValid() && !material.AlbedoMap.Path.empty())
            {
                node["albedo"] = material.AlbedoMap.Path;
            }
            if (material.NormalMap.IsValid() && !material.NormalMap.Path.empty())
            {
                node["normal"] = material.NormalMap.Path;
            }
            if (material.MetallicRoughnessMap.IsValid() && !material.MetallicRoughnessMap.Path.empty())
            {
                node["metallic_roughness"] = material.MetallicRoughnessMap.Path;
            }
            if (material.EmissiveMap.IsValid() && !material.EmissiveMap.Path.empty())
            {
                node["emissive_map"] = material.EmissiveMap.Path;
            }
            if (material.OcclusionMap.IsValid() && !material.OcclusionMap.Path.empty())
            {
                node["occlusion"] = material.OcclusionMap.Path;
            }
            return node;
        }

        Material DeserializeMaterial(const YAML::Node& node, AssetManager* assets)
        {
            Material material;
            material.Name = ReadString(node, "name", "Material");
            material.BaseColor = ReadVec4(node["base_color"], material.BaseColor);
            material.Metallic = ReadFloat(node, "metallic", material.Metallic);
            material.Roughness = ReadFloat(node, "roughness", material.Roughness);
            material.Emissive = ReadVec3(node["emissive"], material.Emissive);
            material.EmissiveStrength = ReadFloat(node, "emissive_strength", material.EmissiveStrength);
            material.Opacity = ReadFloat(node, "opacity", material.Opacity);
            material.AlphaCutoff = ReadFloat(node, "alpha_cutoff", material.AlphaCutoff);
            material.DoubleSided = ReadBool(node, "double_sided", material.DoubleSided);
            material.AlphaMask = ReadBool(node, "alpha_mask", material.AlphaMask);
            material.AlphaBlend = ReadBool(node, "alpha_blend", material.AlphaBlend);

            if (assets != nullptr)
            {
                const std::pair<const char*, AssetRef<Texture> Material::*> maps[] = {
                    { "albedo", &Material::AlbedoMap },
                    { "normal", &Material::NormalMap },
                    { "metallic_roughness", &Material::MetallicRoughnessMap },
                    { "emissive_map", &Material::EmissiveMap },
                    { "occlusion", &Material::OcclusionMap },
                };
                for (const auto& [key, member] : maps)
                {
                    if (!node[key].IsDefined())
                    {
                        continue;
                    }
                    std::string error;
                    const AssetRef<Texture> texture = assets->LoadTexture(node[key].as<std::string>(), &error);
                    if (!texture.IsValid())
                    {
                        ASTER_LOG_WARN("SceneSerializer", "Could not load texture '%s': %s",
                            node[key].as<std::string>().c_str(), error.c_str());
                        continue;
                    }
                    material.*member = texture;
                    if (texture.IsValid() && (std::string(key) == "normal" || std::string(key) == "metallic_roughness" ||
                            std::string(key) == "occlusion"))
                    {
                        texture->SRGB = false;
                    }
                }
            }
            return material;
        }

        YAML::Node SerializeEntity(const Scene& scene, Entity entity)
        {
            const Registry& registry = scene.GetRegistry();
            YAML::Node node;
            if (registry.Has<IDComponent>(entity))
            {
                node["id"] = registry.Get<IDComponent>(entity).Id.ToString();
            }
            if (registry.Has<NameComponent>(entity))
            {
                node["name"] = registry.Get<NameComponent>(entity).Name;
            }
            const Entity parent = scene.GetParent(entity);
            if (parent != NullEntity && registry.Has<IDComponent>(parent))
            {
                node["parent"] = registry.Get<IDComponent>(parent).Id.ToString();
            }

            if (registry.Has<TransformComponent>(entity))
            {
                const Transform& transform = registry.Get<TransformComponent>(entity).Local;
                YAML::Node transformNode;
                transformNode["translation"] = Vec3Node(transform.Translation);
                transformNode["rotation"] = Vec3Node(transform.Rotation);
                transformNode["scale"] = Vec3Node(transform.Scale);
                node["transform"] = transformNode;
            }

            if (registry.Has<CameraComponent>(entity))
            {
                const CameraComponent& camera = registry.Get<CameraComponent>(entity);
                YAML::Node cameraNode;
                cameraNode["primary"] = camera.Primary;
                cameraNode["perspective"] = camera.Perspective;
                cameraNode["fov"] = camera.FieldOfView;
                cameraNode["near"] = camera.NearPlane;
                cameraNode["far"] = camera.FarPlane;
                cameraNode["ortho_size"] = camera.OrthographicSize;
                cameraNode["exposure"] = camera.Exposure;
                node["camera"] = cameraNode;
            }

            if (registry.Has<DirectionalLightComponent>(entity))
            {
                const auto& light = registry.Get<DirectionalLightComponent>(entity);
                YAML::Node lightNode;
                lightNode["color"] = Vec3Node(light.Color);
                lightNode["intensity"] = light.Intensity;
                lightNode["cast_shadows"] = light.CastShadows;
                lightNode["shadow_bias"] = light.ShadowBias;
                lightNode["shadow_normal_bias"] = light.ShadowNormalBias;
                node["directional_light"] = lightNode;
            }

            if (registry.Has<PointLightComponent>(entity))
            {
                const auto& light = registry.Get<PointLightComponent>(entity);
                YAML::Node lightNode;
                lightNode["color"] = Vec3Node(light.Color);
                lightNode["intensity"] = light.Intensity;
                lightNode["range"] = light.Range;
                lightNode["cast_shadows"] = light.CastShadows;
                node["point_light"] = lightNode;
            }

            if (registry.Has<SpotLightComponent>(entity))
            {
                const auto& light = registry.Get<SpotLightComponent>(entity);
                YAML::Node lightNode;
                lightNode["color"] = Vec3Node(light.Color);
                lightNode["intensity"] = light.Intensity;
                lightNode["range"] = light.Range;
                lightNode["inner_angle"] = light.InnerAngle;
                lightNode["outer_angle"] = light.OuterAngle;
                lightNode["cast_shadows"] = light.CastShadows;
                node["spot_light"] = lightNode;
            }

            if (registry.Has<MeshRendererComponent>(entity))
            {
                const auto& renderer = registry.Get<MeshRendererComponent>(entity);
                YAML::Node meshNode;
                if (renderer.Mesh.IsValid())
                {
                    meshNode["asset"] = renderer.Mesh.Path;
                }
                meshNode["cast_shadows"] = renderer.CastShadows;
                meshNode["receive_shadows"] = renderer.ReceiveShadows;
                meshNode["visible"] = renderer.Visible;
                if (!renderer.Materials.empty())
                {
                    YAML::Node materials(YAML::NodeType::Sequence);
                    for (const Material& material : renderer.Materials)
                    {
                        materials.push_back(SerializeMaterial(material));
                    }
                    meshNode["materials"] = materials;
                }
                node["mesh"] = meshNode;
            }

            if (registry.Has<RigidBodyComponent>(entity))
            {
                const auto& body = registry.Get<RigidBodyComponent>(entity);
                YAML::Node bodyNode;
                bodyNode["type"] = BodyTypeToString(body.Type);
                bodyNode["mass"] = body.Mass;
                bodyNode["restitution"] = body.Restitution;
                bodyNode["friction"] = body.Friction;
                bodyNode["gravity"] = body.UseGravity;
                bodyNode["linear_velocity"] = Vec3Node(body.LinearVelocity);
                bodyNode["angular_velocity"] = Vec3Node(body.AngularVelocity);
                bodyNode["linear_damping"] = body.LinearDamping;
                bodyNode["angular_damping"] = body.AngularDamping;
                node["rigidbody"] = bodyNode;
            }

            if (registry.Has<BoxColliderComponent>(entity))
            {
                const auto& collider = registry.Get<BoxColliderComponent>(entity);
                YAML::Node colliderNode;
                colliderNode["half_extents"] = Vec3Node(collider.HalfExtents);
                colliderNode["center"] = Vec3Node(collider.Center);
                colliderNode["trigger"] = collider.IsTrigger;
                node["box_collider"] = colliderNode;
            }

            if (registry.Has<SphereColliderComponent>(entity))
            {
                const auto& collider = registry.Get<SphereColliderComponent>(entity);
                YAML::Node colliderNode;
                colliderNode["radius"] = collider.Radius;
                colliderNode["center"] = Vec3Node(collider.Center);
                colliderNode["trigger"] = collider.IsTrigger;
                node["sphere_collider"] = colliderNode;
            }

            if (registry.Has<ScriptComponent>(entity))
            {
                const auto& scripts = registry.Get<ScriptComponent>(entity);
                YAML::Node scriptsNode(YAML::NodeType::Sequence);
                for (const ScriptEntry& script : scripts.Scripts)
                {
                    YAML::Node entry;
                    entry["path"] = script.Path;
                    entry["enabled"] = script.Enabled;
                    scriptsNode.push_back(entry);
                }
                node["scripts"] = scriptsNode;
            }

            if (registry.Has<AudioSourceComponent>(entity))
            {
                const auto& audio = registry.Get<AudioSourceComponent>(entity);
                YAML::Node audioNode;
                if (audio.Clip.IsValid())
                {
                    audioNode["clip"] = audio.Clip.Path;
                }
                audioNode["play_on_start"] = audio.PlayOnStart;
                audioNode["loop"] = audio.Loop;
                audioNode["spatial"] = audio.Spatial;
                audioNode["volume"] = audio.Volume;
                audioNode["pitch"] = audio.Pitch;
                audioNode["min_distance"] = audio.MinDistance;
                audioNode["max_distance"] = audio.MaxDistance;
                node["audio_source"] = audioNode;
            }

            if (registry.Has<AudioListenerComponent>(entity))
            {
                node["audio_listener"] = YAML::Node(registry.Get<AudioListenerComponent>(entity).Primary);
            }

            return node;
        }

        void LoadEntityComponents(Scene& scene, Entity entity, const YAML::Node& node, AssetManager* assets)
        {
            Registry& registry = scene.GetRegistry();
            if (node["transform"].IsDefined())
            {
                const YAML::Node transformNode = node["transform"];
                Transform& transform = registry.Get<TransformComponent>(entity).Local;
                transform.Translation = ReadVec3(transformNode["translation"], transform.Translation);
                transform.Rotation = ReadVec3(transformNode["rotation"], transform.Rotation);
                transform.Scale = ReadVec3(transformNode["scale"], transform.Scale);
            }

            if (node["camera"].IsDefined())
            {
                const YAML::Node cameraNode = node["camera"];
                CameraComponent camera;
                camera.Primary = ReadBool(cameraNode, "primary", camera.Primary);
                camera.Perspective = ReadBool(cameraNode, "perspective", camera.Perspective);
                camera.FieldOfView = ReadFloat(cameraNode, "fov", camera.FieldOfView);
                camera.NearPlane = ReadFloat(cameraNode, "near", camera.NearPlane);
                camera.FarPlane = ReadFloat(cameraNode, "far", camera.FarPlane);
                camera.OrthographicSize = ReadFloat(cameraNode, "ortho_size", camera.OrthographicSize);
                camera.Exposure = ReadFloat(cameraNode, "exposure", camera.Exposure);
                registry.Emplace<CameraComponent>(entity, camera);
            }

            if (node["directional_light"].IsDefined())
            {
                const YAML::Node lightNode = node["directional_light"];
                DirectionalLightComponent light;
                light.Color = ReadVec3(lightNode["color"], light.Color);
                light.Intensity = ReadFloat(lightNode, "intensity", light.Intensity);
                light.CastShadows = ReadBool(lightNode, "cast_shadows", light.CastShadows);
                light.ShadowBias = ReadFloat(lightNode, "shadow_bias", light.ShadowBias);
                light.ShadowNormalBias = ReadFloat(lightNode, "shadow_normal_bias", light.ShadowNormalBias);
                registry.Emplace<DirectionalLightComponent>(entity, light);
            }

            if (node["point_light"].IsDefined())
            {
                const YAML::Node lightNode = node["point_light"];
                PointLightComponent light;
                light.Color = ReadVec3(lightNode["color"], light.Color);
                light.Intensity = ReadFloat(lightNode, "intensity", light.Intensity);
                light.Range = ReadFloat(lightNode, "range", light.Range);
                light.CastShadows = ReadBool(lightNode, "cast_shadows", light.CastShadows);
                registry.Emplace<PointLightComponent>(entity, light);
            }

            if (node["spot_light"].IsDefined())
            {
                const YAML::Node lightNode = node["spot_light"];
                SpotLightComponent light;
                light.Color = ReadVec3(lightNode["color"], light.Color);
                light.Intensity = ReadFloat(lightNode, "intensity", light.Intensity);
                light.Range = ReadFloat(lightNode, "range", light.Range);
                light.InnerAngle = ReadFloat(lightNode, "inner_angle", light.InnerAngle);
                light.OuterAngle = ReadFloat(lightNode, "outer_angle", light.OuterAngle);
                light.CastShadows = ReadBool(lightNode, "cast_shadows", light.CastShadows);
                registry.Emplace<SpotLightComponent>(entity, light);
            }

            if (node["mesh"].IsDefined())
            {
                const YAML::Node meshNode = node["mesh"];
                MeshRendererComponent renderer;
                renderer.CastShadows = ReadBool(meshNode, "cast_shadows", renderer.CastShadows);
                renderer.ReceiveShadows = ReadBool(meshNode, "receive_shadows", renderer.ReceiveShadows);
                renderer.Visible = ReadBool(meshNode, "visible", renderer.Visible);
                if (assets != nullptr && meshNode["asset"].IsDefined())
                {
                    std::string error;
                    renderer.Mesh = assets->LoadMesh(meshNode["asset"].as<std::string>(), &error);
                    if (!renderer.Mesh.IsValid())
                    {
                        ASTER_LOG_WARN("SceneSerializer", "Could not load mesh '%s': %s",
                            meshNode["asset"].as<std::string>().c_str(), error.c_str());
                    }
                }

                if (meshNode["materials"].IsDefined() && meshNode["materials"].IsSequence())
                {
                    for (const YAML::Node& materialNode : meshNode["materials"])
                    {
                        renderer.Materials.push_back(DeserializeMaterial(materialNode, assets));
                    }
                }
                else if (renderer.Mesh.IsValid())
                {
                    renderer.Materials = renderer.Mesh->Materials;
                }
                registry.Emplace<MeshRendererComponent>(entity, std::move(renderer));
            }

            if (node["rigidbody"].IsDefined())
            {
                const YAML::Node bodyNode = node["rigidbody"];
                RigidBodyComponent body;
                body.Type = BodyTypeFromString(ReadString(bodyNode, "type", "dynamic"));
                body.Mass = ReadFloat(bodyNode, "mass", body.Mass);
                body.Restitution = ReadFloat(bodyNode, "restitution", body.Restitution);
                body.Friction = ReadFloat(bodyNode, "friction", body.Friction);
                body.UseGravity = ReadBool(bodyNode, "gravity", body.UseGravity);
                body.LinearVelocity = ReadVec3(bodyNode["linear_velocity"], body.LinearVelocity);
                body.AngularVelocity = ReadVec3(bodyNode["angular_velocity"], body.AngularVelocity);
                body.LinearDamping = ReadFloat(bodyNode, "linear_damping", body.LinearDamping);
                body.AngularDamping = ReadFloat(bodyNode, "angular_damping", body.AngularDamping);
                registry.Emplace<RigidBodyComponent>(entity, body);
            }

            if (node["box_collider"].IsDefined())
            {
                const YAML::Node colliderNode = node["box_collider"];
                BoxColliderComponent collider;
                collider.HalfExtents = ReadVec3(colliderNode["half_extents"], collider.HalfExtents);
                collider.Center = ReadVec3(colliderNode["center"], collider.Center);
                collider.IsTrigger = ReadBool(colliderNode, "trigger", collider.IsTrigger);
                registry.Emplace<BoxColliderComponent>(entity, collider);
            }

            if (node["sphere_collider"].IsDefined())
            {
                const YAML::Node colliderNode = node["sphere_collider"];
                SphereColliderComponent collider;
                collider.Radius = ReadFloat(colliderNode, "radius", collider.Radius);
                collider.Center = ReadVec3(colliderNode["center"], collider.Center);
                collider.IsTrigger = ReadBool(colliderNode, "trigger", collider.IsTrigger);
                registry.Emplace<SphereColliderComponent>(entity, collider);
            }

            if (node["scripts"].IsDefined() && node["scripts"].IsSequence())
            {
                ScriptComponent scripts;
                for (const YAML::Node& scriptNode : node["scripts"])
                {
                    ScriptEntry entry;
                    entry.Path = ReadString(scriptNode, "path", "");
                    entry.Enabled = ReadBool(scriptNode, "enabled", true);
                    if (!entry.Path.empty())
                    {
                        scripts.Scripts.push_back(std::move(entry));
                    }
                }
                registry.Emplace<ScriptComponent>(entity, std::move(scripts));
            }

            if (node["audio_source"].IsDefined())
            {
                const YAML::Node audioNode = node["audio_source"];
                AudioSourceComponent audio;
                audio.PlayOnStart = ReadBool(audioNode, "play_on_start", audio.PlayOnStart);
                audio.Loop = ReadBool(audioNode, "loop", audio.Loop);
                audio.Spatial = ReadBool(audioNode, "spatial", audio.Spatial);
                audio.Volume = ReadFloat(audioNode, "volume", audio.Volume);
                audio.Pitch = ReadFloat(audioNode, "pitch", audio.Pitch);
                audio.MinDistance = ReadFloat(audioNode, "min_distance", audio.MinDistance);
                audio.MaxDistance = ReadFloat(audioNode, "max_distance", audio.MaxDistance);
                if (assets != nullptr && audioNode["clip"].IsDefined())
                {
                    std::string error;
                    audio.Clip = assets->LoadSound(audioNode["clip"].as<std::string>(), &error);
                    if (!audio.Clip.IsValid())
                    {
                        ASTER_LOG_WARN("SceneSerializer", "Could not load sound '%s': %s",
                            audioNode["clip"].as<std::string>().c_str(), error.c_str());
                    }
                }
                registry.Emplace<AudioSourceComponent>(entity, std::move(audio));
            }

            if (node["audio_listener"].IsDefined())
            {
                AudioListenerComponent listener;
                listener.Primary = node["audio_listener"].as<bool>(true);
                registry.Emplace<AudioListenerComponent>(entity, listener);
            }
        }
    }

    bool SceneSerializer::Save(const Scene& scene, const std::string& path, std::string* error)
    {
        const std::string contents = Serialize(scene);
        const std::string directory = FileSystem::Normalize(FileSystem::GetDirectory(path));
        if (!directory.empty() && !FileSystem::CreateDirectories(directory))
        {
            SetError(error, "Could not create scene directory: " + directory);
            return false;
        }
        if (!FileSystem::WriteTextFile(path, contents))
        {
            SetError(error, "Could not write scene file: " + path);
            return false;
        }
        return true;
    }

    std::string SceneSerializer::Serialize(const Scene& scene)
    {
        YAML::Node root;
        root["format"] = "aster-scene";
        root["version"] = 1;
        root["name"] = scene.GetSettings().Name;

        const SceneSettings& settings = scene.GetSettings();
        YAML::Node settingsNode;
        settingsNode["exposure"] = settings.Exposure;
        settingsNode["ambient_intensity"] = settings.AmbientIntensity;
        settingsNode["ambient_color"] = Vec3Node(settings.AmbientColor);
        settingsNode["shadows"] = settings.EnableShadows;
        settingsNode["shadow_strength"] = settings.ShadowStrength;
        settingsNode["shadow_map_size"] = settings.ShadowMapSize;
        settingsNode["ssao"] = settings.EnableSsao;
        settingsNode["ssao_radius"] = settings.SsaoRadius;
        settingsNode["ssao_intensity"] = settings.SsaoIntensity;
        settingsNode["ssao_bias"] = settings.SsaoBias;
        settingsNode["bloom"] = settings.EnableBloom;
        settingsNode["bloom_threshold"] = settings.BloomThreshold;
        settingsNode["bloom_intensity"] = settings.BloomIntensity;
        settingsNode["fxaa"] = settings.EnableFxaa;
        settingsNode["skybox"] = settings.ShowSkybox;
        settingsNode["environment"] = settings.EnvironmentPath;
        settingsNode["environment_intensity"] = settings.EnvironmentIntensity;
        settingsNode["clear_color"] = Vec3Node(settings.ClearColor);
        settingsNode["vsync"] = settings.VSync;
        root["settings"] = settingsNode;

        YAML::Node entities(YAML::NodeType::Sequence);
        for (Entity entity : scene.GetEntities())
        {
            entities.push_back(SerializeEntity(scene, entity));
        }
        root["entities"] = entities;

        YAML::Emitter emitter;
        emitter.SetFloatPrecision(9);
        emitter << root;
        if (!emitter.good())
        {
            return "";
        }
        return std::string(emitter.c_str()) + "\n";
    }

    bool SceneSerializer::Load(Scene& scene, const std::string& path, AssetManager* assets, std::string* error)
    {
        const std::optional<std::string> contents = FileSystem::ReadTextFile(path);
        if (!contents.has_value())
        {
            SetError(error, "Could not read scene file: " + path);
            return false;
        }
        return LoadFromString(scene, *contents, assets, error);
    }

    bool SceneSerializer::LoadFromString(Scene& scene, std::string_view yaml, AssetManager* assets,
        std::string* error)
    {
        try
        {
            const YAML::Node root = YAML::Load(std::string(yaml));
            if (!root.IsMap())
            {
                SetError(error, "Scene YAML root must be a map");
                return false;
            }

            if (root["format"].IsDefined() && root["format"].as<std::string>() != "aster-scene")
            {
                SetError(error, "Unsupported scene format: " + root["format"].as<std::string>());
                return false;
            }
            const int version = root["version"].IsDefined() ? root["version"].as<int>(1) : 1;
            if (version < 1 || version > 1)
            {
                SetError(error, "Unsupported scene version: " + std::to_string(version));
                return false;
            }

            const std::string sceneName = ReadString(root, "name", "Untitled");
            Scene loaded(sceneName);
            SceneSettings& settings = loaded.GetSettings();
            settings.Name = sceneName;

            if (root["settings"].IsDefined())
            {
                const YAML::Node settingsNode = root["settings"];
                settings.Exposure = ReadFloat(settingsNode, "exposure", settings.Exposure);
                settings.AmbientIntensity = ReadFloat(settingsNode, "ambient_intensity", settings.AmbientIntensity);
                settings.AmbientColor = ReadVec3(settingsNode["ambient_color"], settings.AmbientColor);
                settings.EnableShadows = ReadBool(settingsNode, "shadows", settings.EnableShadows);
                settings.ShadowStrength = ReadFloat(settingsNode, "shadow_strength", settings.ShadowStrength);
                settings.ShadowMapSize = ReadUint(settingsNode, "shadow_map_size", settings.ShadowMapSize);
                settings.EnableSsao = ReadBool(settingsNode, "ssao", settings.EnableSsao);
                settings.SsaoRadius = ReadFloat(settingsNode, "ssao_radius", settings.SsaoRadius);
                settings.SsaoIntensity = ReadFloat(settingsNode, "ssao_intensity", settings.SsaoIntensity);
                settings.SsaoBias = ReadFloat(settingsNode, "ssao_bias", settings.SsaoBias);
                settings.EnableBloom = ReadBool(settingsNode, "bloom", settings.EnableBloom);
                settings.BloomThreshold = ReadFloat(settingsNode, "bloom_threshold", settings.BloomThreshold);
                settings.BloomIntensity = ReadFloat(settingsNode, "bloom_intensity", settings.BloomIntensity);
                settings.EnableFxaa = ReadBool(settingsNode, "fxaa", settings.EnableFxaa);
                settings.ShowSkybox = ReadBool(settingsNode, "skybox", settings.ShowSkybox);
                settings.EnvironmentPath = ReadString(settingsNode, "environment", settings.EnvironmentPath);
                settings.EnvironmentIntensity = ReadFloat(settingsNode, "environment_intensity",
                    settings.EnvironmentIntensity);
                settings.ClearColor = ReadVec3(settingsNode["clear_color"], settings.ClearColor);
                settings.VSync = ReadBool(settingsNode, "vsync", settings.VSync);
            }

            std::vector<std::pair<UUID, UUID>> parentLinks;
            if (root["entities"].IsDefined() && root["entities"].IsSequence())
            {
                for (const YAML::Node& entityNode : root["entities"])
                {
                    UUID id = UUID::Generate();
                    if (entityNode["id"].IsDefined())
                    {
                        const std::optional<UUID> parsed = UUID::FromString(entityNode["id"].as<std::string>());
                        if (parsed.has_value())
                        {
                            id = *parsed;
                        }
                        else
                        {
                            ASTER_LOG_WARN("SceneSerializer", "Invalid entity UUID '%s'; generating a new one",
                                entityNode["id"].as<std::string>().c_str());
                        }
                    }

                    const std::string name = ReadString(entityNode, "name", "Entity");
                    const Entity entity = loaded.CreateEntity(id, name);
                    LoadEntityComponents(loaded, entity, entityNode, assets);

                    if (entityNode["parent"].IsDefined())
                    {
                        const std::optional<UUID> parentId =
                            UUID::FromString(entityNode["parent"].as<std::string>());
                        if (parentId.has_value())
                        {
                            parentLinks.emplace_back(id, *parentId);
                        }
                    }
                }
            }

            for (const auto& [childId, parentId] : parentLinks)
            {
                const Entity child = loaded.FindByUUID(childId);
                const Entity parent = loaded.FindByUUID(parentId);
                if (child != NullEntity && parent != NullEntity)
                {
                    loaded.SetParent(child, parent);
                }
                else
                {
                    ASTER_LOG_WARN("SceneSerializer", "Entity parent reference could not be resolved");
                }
            }

            scene = std::move(loaded);
            return true;
        }
        catch (const YAML::Exception& exception)
        {
            SetError(error, std::string("Failed to parse scene YAML: ") + exception.what());
            return false;
        }
    }
}
