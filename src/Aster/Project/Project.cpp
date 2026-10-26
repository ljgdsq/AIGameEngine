#include "Aster/Project/Project.h"

#include "Aster/Assets/Primitives.h"
#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"
#include "Aster/Scene/SceneSerializer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <optional>
#include <vector>

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

        Entity AddMeshEntity(Scene& scene, AssetManager& assets, const std::string& name,
            const std::string& meshPath, const glm::vec3& position, const glm::vec3& scale,
            const Material& material, std::string* error)
        {
            AssetRef<MeshAsset> mesh = assets.LoadMesh(meshPath, error);
            if (!mesh.IsValid())
            {
                return NullEntity;
            }

            const Entity entity = scene.CreateEntity(name);
            TransformComponent& transform = scene.GetRegistry().Get<TransformComponent>(entity);
            transform.Local.Translation = position;
            transform.Local.Scale = scale;

            MeshRendererComponent renderer;
            renderer.Mesh = mesh;
            renderer.Materials.push_back(material);
            scene.GetRegistry().Emplace<MeshRendererComponent>(entity, std::move(renderer));
            return entity;
        }

        bool CopySharedAsset(const std::filesystem::path& relativePath, const std::filesystem::path& destination,
            std::string* error)
        {
            const std::filesystem::path source = std::filesystem::path(ASTER_ASSET_DIR) / relativePath;
            const std::optional<std::vector<uint8>> contents = FileSystem::ReadBinaryFile(source);
            if (!contents.has_value())
            {
                SetError(error, "Could not read shared asset: " + FileSystem::Normalize(source));
                return false;
            }
            if (!FileSystem::WriteBinaryFile(destination, contents->data(), contents->size()))
            {
                SetError(error, "Could not write project asset: " + FileSystem::Normalize(destination));
                return false;
            }
            return true;
        }
    }

    Project::Project() = default;

    bool Project::Create(const std::filesystem::path& root, const std::string& name, std::string* error)
    {
        const std::filesystem::path absoluteRoot = FileSystem::Absolute(root);
        const std::filesystem::path projectFile = absoluteRoot / "project.aster.yaml";
        if (FileSystem::Exists(projectFile))
        {
            SetError(error, "A project already exists at " + FileSystem::Normalize(absoluteRoot));
            return false;
        }

        m_Info = {};
        m_Info.Name = name.empty() ? "Untitled Project" : name;
        m_Info.RootPath = absoluteRoot;
        m_Info.DefaultScene = "scenes/main.aster.yaml";

        const std::filesystem::path directories[] = {
            absoluteRoot,
            absoluteRoot / "assets",
            absoluteRoot / "assets" / "models",
            absoluteRoot / "assets" / "textures",
            absoluteRoot / "assets" / "environments",
            absoluteRoot / "assets" / "scripts",
            absoluteRoot / "assets" / "sounds",
            absoluteRoot / "scenes",
            absoluteRoot / "prefabs"
        };
        for (const auto& directory : directories)
        {
            if (!FileSystem::CreateDirectories(directory))
            {
                SetError(error, "Could not create project directory: " + FileSystem::Normalize(directory));
                return false;
            }
        }

        if (!Primitives::WriteDefaultPrimitives(absoluteRoot / "assets" / "models", error))
        {
            return false;
        }
        if (!Primitives::WriteCheckerTexture(absoluteRoot / "assets" / "textures" / "checker.png", 128, error))
        {
            return false;
        }
        if (!Primitives::WriteStudioEnvironment(absoluteRoot / "assets" / "environments" / "studio.hdr", 512,
                256, error))
        {
            return false;
        }
        if (!Primitives::WriteToneWav(absoluteRoot / "assets" / "sounds" / "ambient.wav", 330.0f, 0.6f, 44100,
                error))
        {
            return false;
        }
        if (!CopySharedAsset("textures/container2.png",
                absoluteRoot / "assets" / "textures" / "container2.png", error))
        {
            return false;
        }

        const std::string spinScript = R"(-- Aster sample behavior: rotate the entity around its Y axis.
return {
    OnUpdate = function(entity, deltaTime)
        local x, y, z = entity:GetRotation()
        entity:SetRotation(x, y + 45.0 * deltaTime, z)
    end
}
)";
        if (!FileSystem::WriteTextFile(absoluteRoot / "assets" / "scripts" / "spin.lua", spinScript))
        {
            SetError(error, "Could not write the default Lua script");
            return false;
        }

        m_Assets.SetRootPath(absoluteRoot);
        m_CurrentScenePath = m_Info.DefaultScene;
        m_Open = true;
        if (!CreateDefaultScene(error))
        {
            m_Open = false;
            return false;
        }
        if (!Save(error))
        {
            m_Open = false;
            return false;
        }

        ASTER_LOG_INFO("Project", "Created project '%s' at %s", m_Info.Name.c_str(),
            FileSystem::Normalize(absoluteRoot).c_str());
        return true;
    }

    bool Project::CreateGameDemo(const std::filesystem::path& root, const std::string& name,
        std::string* error)
    {
        if (!Create(root, name, error))
        {
            return false;
        }
        if (!CreateMarbleDemoScene(error))
        {
            return false;
        }
        if (!SaveSceneAs("scenes/marble_run.aster.yaml", error))
        {
            return false;
        }
        m_Info.DefaultScene = "scenes/marble_run.aster.yaml";
        return WriteProjectFile(error);
    }

    bool Project::Open(const std::filesystem::path& root, std::string* error)
    {
        const std::filesystem::path absoluteRoot = FileSystem::Absolute(root);
        const std::filesystem::path projectFile = absoluteRoot / "project.aster.yaml";
        if (!FileSystem::Exists(projectFile))
        {
            SetError(error, "No project.aster.yaml found at " + FileSystem::Normalize(absoluteRoot));
            return false;
        }

        ProjectInfo info;
        info.RootPath = absoluteRoot;
        if (!ReadProjectFile(projectFile, error))
        {
            return false;
        }
        info = m_Info;
        info.RootPath = absoluteRoot;

        m_Info = info;
        m_Assets.SetRootPath(absoluteRoot);
        m_Open = true;
        m_CurrentScenePath = m_Info.DefaultScene;
        if (!LoadScene(m_Info.DefaultScene, error))
        {
            m_Open = false;
            return false;
        }

        ASTER_LOG_INFO("Project", "Opened project '%s' at %s", m_Info.Name.c_str(),
            FileSystem::Normalize(absoluteRoot).c_str());
        return true;
    }

    void Project::Close()
    {
        m_Open = false;
        m_Assets.Clear();
        m_Assets.SetRootPath({});
        m_Scene.Clear();
        m_Info = {};
        m_CurrentScenePath.clear();
    }

    bool Project::Save(std::string* error)
    {
        if (!m_Open)
        {
            SetError(error, "No project is open");
            return false;
        }
        if (!WriteProjectFile(error))
        {
            return false;
        }
        return SaveScene(error);
    }

    bool Project::SaveScene(std::string* error)
    {
        if (!m_Open)
        {
            SetError(error, "No project is open");
            return false;
        }
        if (m_CurrentScenePath.empty())
        {
            return SaveSceneAs(m_Info.DefaultScene, error);
        }
        return SaveSceneAs(m_CurrentScenePath, error);
    }

    bool Project::SaveSceneAs(const std::string& relativePath, std::string* error)
    {
        if (!m_Open)
        {
            SetError(error, "No project is open");
            return false;
        }
        if (relativePath.empty())
        {
            SetError(error, "Scene path is empty");
            return false;
        }

        std::filesystem::path path = Resolve(relativePath).lexically_normal();
        if (!FileSystem::CreateDirectories(path.parent_path()))
        {
            SetError(error, "Could not create scene directory");
            return false;
        }
        if (!SceneSerializer::Save(m_Scene, FileSystem::Normalize(path), error))
        {
            return false;
        }
        m_CurrentScenePath = MakeRelative(path);
        ASTER_LOG_INFO("Project", "Saved scene %s", m_CurrentScenePath.c_str());
        return true;
    }

    bool Project::LoadScene(const std::string& relativePath, std::string* error)
    {
        if (!m_Open)
        {
            SetError(error, "No project is open");
            return false;
        }

        const std::filesystem::path path = Resolve(relativePath);
        m_Scene.Clear();
        if (!SceneSerializer::Load(m_Scene, FileSystem::Normalize(path), &m_Assets, error))
        {
            return false;
        }
        m_CurrentScenePath = relativePath;
        return true;
    }

    bool Project::NewScene(std::string* error)
    {
        if (!m_Open)
        {
            SetError(error, "No project is open");
            return false;
        }
        if (!CreateDefaultScene(error))
        {
            return false;
        }
        m_CurrentScenePath = "scenes/untitled.aster.yaml";
        return true;
    }

    std::filesystem::path Project::Resolve(const std::string& relativePath) const
    {
        return FileSystem::Resolve(m_Info.RootPath, std::filesystem::path(relativePath));
    }

    std::string Project::MakeRelative(const std::filesystem::path& path) const
    {
        std::error_code code;
        const std::filesystem::path relative = std::filesystem::relative(path, m_Info.RootPath, code);
        if (code)
        {
            return FileSystem::Normalize(path);
        }
        return FileSystem::Normalize(relative);
    }

    bool Project::CreateMarbleDemoScene(std::string* error)
    {
        Scene scene("Marble Run");
        SceneSettings& settings = scene.GetSettings();
        settings.Name = "Marble Run";
        settings.AmbientIntensity = 0.18f;
        settings.AmbientColor = glm::vec3(0.44f, 0.52f, 0.68f);
        settings.ClearColor = glm::vec3(0.025f, 0.040f, 0.070f);
        settings.EnvironmentPath = "assets/environments/studio.hdr";
        settings.EnvironmentIntensity = 0.55f;
        settings.ShowSkybox = true;
        settings.EnableBloom = true;
        settings.BloomThreshold = 0.75f;
        settings.BloomIntensity = 0.45f;

        const Entity sun = scene.CreateEntity("Sun");
        scene.GetRegistry().Get<TransformComponent>(sun).Local.Rotation = glm::vec3(-48.0f, -32.0f, 0.0f);
        DirectionalLightComponent sunLight;
        sunLight.Color = glm::vec3(1.0f, 0.94f, 0.84f);
        sunLight.Intensity = 3.2f;
        scene.GetRegistry().Emplace<DirectionalLightComponent>(sun, sunLight);

        const Entity camera = scene.CreateEntity("Main Camera");
        TransformComponent& cameraTransform = scene.GetRegistry().Get<TransformComponent>(camera);
        cameraTransform.Local.Translation = glm::vec3(0.0f, 6.5f, 10.5f);
        cameraTransform.Local.Rotation = glm::vec3(-18.0f, 0.0f, 0.0f);
        CameraComponent cameraComponent;
        cameraComponent.Primary = true;
        cameraComponent.FieldOfView = 55.0f;
        scene.GetRegistry().Emplace<CameraComponent>(camera, cameraComponent);

        const Entity startLight = scene.CreateEntity("Start Light");
        TransformComponent& startLightTransform = scene.GetRegistry().Get<TransformComponent>(startLight);
        startLightTransform.Local.Translation = glm::vec3(-3.0f, 5.0f, 4.0f);
        PointLightComponent startPoint;
        startPoint.Color = glm::vec3(0.25f, 0.65f, 1.0f);
        startPoint.Intensity = 22.0f;
        startPoint.Range = 14.0f;
        scene.GetRegistry().Emplace<PointLightComponent>(startLight, startPoint);

        const Entity goalLight = scene.CreateEntity("Goal Light");
        TransformComponent& goalLightTransform = scene.GetRegistry().Get<TransformComponent>(goalLight);
        goalLightTransform.Local.Translation = glm::vec3(0.0f, 5.0f, -21.0f);
        PointLightComponent goalPoint;
        goalPoint.Color = glm::vec3(1.0f, 0.55f, 0.18f);
        goalPoint.Intensity = 26.0f;
        goalPoint.Range = 16.0f;
        scene.GetRegistry().Emplace<PointLightComponent>(goalLight, goalPoint);

        const auto addPlatform = [&](const std::string& platformName, const glm::vec3& position,
                                     const glm::vec3& scale, const glm::vec4& color) -> bool
        {
            Material material;
            material.Name = platformName + " Material";
            material.BaseColor = color;
            material.Metallic = 0.15f;
            material.Roughness = 0.55f;
            const Entity platform = AddMeshEntity(scene, m_Assets, platformName, "assets/models/cube.obj",
                position, scale, material, error);
            if (platform == NullEntity)
            {
                return false;
            }
            RigidBodyComponent body;
            body.Type = BodyType::Static;
            body.UseGravity = false;
            scene.GetRegistry().Emplace<RigidBodyComponent>(platform, body);
            BoxColliderComponent collider;
            collider.HalfExtents = glm::vec3(0.5f);
            scene.GetRegistry().Emplace<BoxColliderComponent>(platform, collider);
            return true;
        };

        if (!addPlatform("Start Pad", glm::vec3(0.0f, -0.25f, 2.0f), glm::vec3(5.0f, 0.5f, 5.0f),
                glm::vec4(0.18f, 0.36f, 0.62f, 1.0f)) ||
            !addPlatform("Bridge", glm::vec3(0.0f, -0.2f, -2.5f), glm::vec3(3.0f, 0.4f, 4.0f),
                glm::vec4(0.16f, 0.52f, 0.58f, 1.0f)) ||
            !addPlatform("Mid Platform", glm::vec3(0.0f, 0.4f, -7.5f), glm::vec3(4.0f, 0.5f, 4.0f),
                glm::vec4(0.25f, 0.34f, 0.58f, 1.0f)) ||
            !addPlatform("Right Step", glm::vec3(2.5f, 1.0f, -12.0f), glm::vec3(4.0f, 0.5f, 4.0f),
                glm::vec4(0.20f, 0.48f, 0.68f, 1.0f)) ||
            !addPlatform("Left Step", glm::vec3(-1.5f, 1.6f, -16.5f), glm::vec3(3.5f, 0.5f, 3.5f),
                glm::vec4(0.28f, 0.40f, 0.66f, 1.0f)) ||
            !addPlatform("Goal Terrace", glm::vec3(0.0f, 2.2f, -21.0f), glm::vec3(5.0f, 0.5f, 5.0f),
                glm::vec4(0.20f, 0.30f, 0.52f, 1.0f)))
        {
            return false;
        }

        if (!addPlatform("Start Pillar L", glm::vec3(-2.7f, 1.7f, 4.2f), glm::vec3(0.35f, 4.0f, 0.35f),
                glm::vec4(0.42f, 0.48f, 0.58f, 1.0f)) ||
            !addPlatform("Start Pillar R", glm::vec3(2.7f, 1.7f, 4.2f), glm::vec3(0.35f, 4.0f, 0.35f),
                glm::vec4(0.42f, 0.48f, 0.58f, 1.0f)))
        {
            return false;
        }

        Material marbleMaterial;
        marbleMaterial.Name = "Marble";
        marbleMaterial.BaseColor = glm::vec4(0.92f, 0.35f, 0.18f, 1.0f);
        marbleMaterial.Metallic = 0.75f;
        marbleMaterial.Roughness = 0.16f;
        const Entity marble = AddMeshEntity(scene, m_Assets, "Marble", "assets/models/sphere.obj",
            glm::vec3(0.0f, 1.0f, 2.5f), glm::vec3(0.8f), marbleMaterial, error);
        if (marble == NullEntity)
        {
            return false;
        }
        RigidBodyComponent marbleBody;
        marbleBody.Type = BodyType::Dynamic;
        marbleBody.Mass = 1.0f;
        marbleBody.Restitution = 0.22f;
        marbleBody.Friction = 0.85f;
        marbleBody.LinearDamping = 0.12f;
        scene.GetRegistry().Emplace<RigidBodyComponent>(marble, marbleBody);
        SphereColliderComponent marbleCollider;
        marbleCollider.Radius = 0.5f;
        scene.GetRegistry().Emplace<SphereColliderComponent>(marble, marbleCollider);

        const auto addCoin = [&](const std::string& coinName, const glm::vec3& position) -> bool
        {
            Material gold;
            gold.Name = "Coin Gold";
            gold.BaseColor = glm::vec4(1.0f, 0.72f, 0.18f, 1.0f);
            gold.Metallic = 0.9f;
            gold.Roughness = 0.18f;
            gold.Emissive = glm::vec3(1.0f, 0.62f, 0.10f);
            gold.EmissiveStrength = 3.0f;
            return AddMeshEntity(scene, m_Assets, coinName, "assets/models/sphere.obj", position,
                       glm::vec3(0.36f), gold, error) != NullEntity;
        };

        if (!addCoin("Coin 01", glm::vec3(0.0f, 1.3f, 1.0f)) ||
            !addCoin("Coin 02", glm::vec3(0.0f, 1.3f, -3.0f)) ||
            !addCoin("Coin 03", glm::vec3(0.0f, 1.9f, -7.5f)) ||
            !addCoin("Coin 04", glm::vec3(2.5f, 2.5f, -12.0f)) ||
            !addCoin("Coin 05", glm::vec3(-1.5f, 3.1f, -16.5f)) ||
            !addCoin("Coin 06", glm::vec3(0.0f, 3.7f, -21.0f)))
        {
            return false;
        }

        Material goalMaterial;
        goalMaterial.Name = "Goal";
        goalMaterial.BaseColor = glm::vec4(0.25f, 1.0f, 0.58f, 1.0f);
        goalMaterial.Metallic = 0.0f;
        goalMaterial.Roughness = 0.35f;
        goalMaterial.Emissive = glm::vec3(0.20f, 1.0f, 0.45f);
        goalMaterial.EmissiveStrength = 8.0f;
        const Entity goal = AddMeshEntity(scene, m_Assets, "Goal", "assets/models/sphere.obj",
            glm::vec3(0.0f, 3.4f, -21.0f), glm::vec3(1.1f), goalMaterial, error);
        if (goal == NullEntity)
        {
            return false;
        }
        PointLightComponent goalGlow;
        goalGlow.Color = glm::vec3(0.25f, 1.0f, 0.5f);
        goalGlow.Intensity = 20.0f;
        goalGlow.Range = 8.0f;
        scene.GetRegistry().Emplace<PointLightComponent>(goal, goalGlow);

        m_Scene.Clear();
        m_Scene.InstantiateScene(scene);
        m_Scene.GetSettings() = settings;
        return true;
    }

    bool Project::CreateDefaultScene(std::string* error)
    {
        Scene scene("Main");
        scene.GetSettings().Name = "Main";
        scene.GetSettings().AmbientIntensity = 0.2f;
        scene.GetSettings().AmbientColor = glm::vec3(0.5f, 0.58f, 0.7f);
        scene.GetSettings().ClearColor = glm::vec3(0.045f, 0.055f, 0.075f);
        scene.GetSettings().EnvironmentPath = "assets/environments/studio.hdr";
        scene.GetSettings().EnvironmentIntensity = 0.45f;
        scene.GetSettings().ShowSkybox = true;
        scene.GetSettings().EnableBloom = true;
        scene.GetSettings().BloomThreshold = 0.8f;
        scene.GetSettings().BloomIntensity = 0.35f;

        const Entity sun = scene.CreateEntity("Sun");
        TransformComponent& sunTransform = scene.GetRegistry().Get<TransformComponent>(sun);
        sunTransform.Local.Rotation = glm::vec3(-50.0f, -30.0f, 0.0f);
        DirectionalLightComponent sunLight;
        sunLight.Intensity = 3.5f;
        scene.GetRegistry().Emplace<DirectionalLightComponent>(sun, sunLight);

        const Entity cameraEntity = scene.CreateEntity("Main Camera");
        TransformComponent& cameraTransform = scene.GetRegistry().Get<TransformComponent>(cameraEntity);
        cameraTransform.Local.Translation = glm::vec3(7.0f, 4.5f, 9.0f);
        const glm::vec3 lookDirection = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) -
            cameraTransform.Local.Translation);
        cameraTransform.Local.Rotation = glm::vec3(glm::degrees(std::asin(lookDirection.y)),
            glm::degrees(std::atan2(-lookDirection.x, -lookDirection.z)), 0.0f);
        CameraComponent camera;
        camera.Primary = true;
        camera.FieldOfView = 55.0f;
        scene.GetRegistry().Emplace<CameraComponent>(cameraEntity, camera);

        const Entity spot = scene.CreateEntity("Spot Light");
        TransformComponent& spotTransform = scene.GetRegistry().Get<TransformComponent>(spot);
        spotTransform.Local.Translation = glm::vec3(0.0f, 5.5f, 2.0f);
        spotTransform.Local.Rotation = glm::vec3(-75.0f, 0.0f, 0.0f);
        SpotLightComponent spotLight;
        spotLight.Color = glm::vec3(0.55f, 0.72f, 1.0f);
        spotLight.Intensity = 6.0f;
        spotLight.Range = 12.0f;
        spotLight.InnerAngle = 20.0f;
        spotLight.OuterAngle = 38.0f;
        scene.GetRegistry().Emplace<SpotLightComponent>(spot, spotLight);

        const Entity fill = scene.CreateEntity("Fill Light");
        TransformComponent& fillTransform = scene.GetRegistry().Get<TransformComponent>(fill);
        fillTransform.Local.Translation = glm::vec3(4.0f, 4.0f, 6.0f);
        PointLightComponent fillLight;
        fillLight.Color = glm::vec3(1.0f, 0.88f, 0.72f);
        fillLight.Intensity = 18.0f;
        fillLight.Range = 18.0f;
        scene.GetRegistry().Emplace<PointLightComponent>(fill, fillLight);

        Material groundMaterial;
        groundMaterial.Name = "Grid";
        groundMaterial.BaseColor = glm::vec4(1.0f);
        groundMaterial.Roughness = 0.9f;
        std::string textureError;
        const AssetRef<Texture> checker = m_Assets.LoadTexture("assets/textures/checker.png", &textureError);
        if (checker.IsValid())
        {
            groundMaterial.AlbedoMap = checker;
        }
        else
        {
            ASTER_LOG_WARN("Project", "Could not load the sample checker texture: %s", textureError.c_str());
        }
        const Entity ground = AddMeshEntity(scene, m_Assets, "Ground", "assets/models/plane.obj",
            glm::vec3(0.0f), glm::vec3(24.0f, 1.0f, 24.0f), groundMaterial, error);
        if (ground != NullEntity)
        {
            RigidBodyComponent groundBody;
            groundBody.Type = BodyType::Static;
            groundBody.UseGravity = false;
            scene.GetRegistry().Emplace<RigidBodyComponent>(ground, groundBody);
            BoxColliderComponent groundCollider;
            groundCollider.HalfExtents = glm::vec3(0.5f, 0.05f, 0.5f);
            // The visual plane sits at y = 0; lower the thin collider so its top face matches it.
            groundCollider.Center = glm::vec3(0.0f, -0.05f, 0.0f);
            scene.GetRegistry().Emplace<BoxColliderComponent>(ground, groundCollider);
        }

        Material cubeMaterial;
        cubeMaterial.Name = "Cargo";
        cubeMaterial.BaseColor = glm::vec4(1.0f);
        cubeMaterial.Metallic = 0.1f;
        cubeMaterial.Roughness = 0.55f;
        std::string cubeTextureError;
        const AssetRef<Texture> containerTexture =
            m_Assets.LoadTexture("assets/textures/container2.png", &cubeTextureError);
        if (containerTexture.IsValid())
        {
            cubeMaterial.AlbedoMap = containerTexture;
        }
        else
        {
            ASTER_LOG_WARN("Project", "Could not load the sample cube texture: %s", cubeTextureError.c_str());
        }
        // A unit cube scaled to 0.75 has a half height of 0.375, so this places its bottom on y = 0.
        const Entity cube = AddMeshEntity(scene, m_Assets, "Cube", "assets/models/cube.obj",
            glm::vec3(0.0f, 0.375f, 0.0f), glm::vec3(0.75f), cubeMaterial, error);
        if (cube != NullEntity)
        {
            ScriptComponent scripts;
            scripts.Scripts.push_back(ScriptEntry{ "assets/scripts/spin.lua", true });
            scene.GetRegistry().Emplace<ScriptComponent>(cube, std::move(scripts));
        }

        Material sphereMaterial;
        sphereMaterial.Name = "Steel";
        sphereMaterial.BaseColor = glm::vec4(0.68f, 0.72f, 0.78f, 1.0f);
        sphereMaterial.Metallic = 0.85f;
        sphereMaterial.Roughness = 0.22f;
        const Entity sphere = AddMeshEntity(scene, m_Assets, "Sphere", "assets/models/sphere.obj",
            glm::vec3(2.2f, 1.0f, 0.0f), glm::vec3(1.0f), sphereMaterial, error);
        if (sphere != NullEntity)
        {
            RigidBodyComponent sphereBody;
            sphereBody.Type = BodyType::Dynamic;
            sphereBody.Mass = 1.0f;
            sphereBody.Restitution = 0.35f;
            scene.GetRegistry().Emplace<RigidBodyComponent>(sphere, sphereBody);
            SphereColliderComponent sphereCollider;
            sphereCollider.Radius = 0.5f;
            scene.GetRegistry().Emplace<SphereColliderComponent>(sphere, sphereCollider);
        }

        Material emissiveMaterial;
        emissiveMaterial.Name = "Emissive Orb";
        emissiveMaterial.BaseColor = glm::vec4(0.25f, 0.75f, 1.0f, 1.0f);
        emissiveMaterial.Metallic = 0.0f;
        emissiveMaterial.Roughness = 0.5f;
        emissiveMaterial.Emissive = glm::vec3(0.25f, 0.75f, 1.0f);
        emissiveMaterial.EmissiveStrength = 10.0f;
        const Entity orb = AddMeshEntity(scene, m_Assets, "Emissive Orb", "assets/models/sphere.obj",
            glm::vec3(-1.6f, 1.35f, 2.0f), glm::vec3(0.55f), emissiveMaterial, error);
        if (orb != NullEntity)
        {
            PointLightComponent orbLight;
            orbLight.Color = glm::vec3(0.25f, 0.75f, 1.0f);
            orbLight.Intensity = 2.0f;
            orbLight.Range = 5.0f;
            scene.GetRegistry().Emplace<PointLightComponent>(orb, orbLight);

            std::string audioError;
            AudioSourceComponent audio;
            audio.Clip = m_Assets.LoadSound("assets/sounds/ambient.wav", &audioError);
            audio.PlayOnStart = true;
            audio.Loop = true;
            audio.Spatial = false;
            audio.Volume = 0.15f;
            if (!audio.Clip.IsValid())
            {
                ASTER_LOG_WARN("Project", "Could not load the sample sound: %s", audioError.c_str());
            }
            scene.GetRegistry().Emplace<AudioSourceComponent>(orb, std::move(audio));

            ParticleEmitterComponent particles;
            particles.EmissionRate = 32.0f;
            particles.Lifetime = 1.2f;
            particles.StartSize = 0.12f;
            particles.EndSize = 0.0f;
            particles.StartColor = glm::vec4(0.25f, 0.75f, 1.0f, 0.9f);
            particles.EndColor = glm::vec4(0.05f, 0.2f, 0.45f, 0.0f);
            particles.Velocity = glm::vec3(0.0f, 0.7f, 0.0f);
            particles.VelocitySpread = 0.35f;
            particles.Gravity = glm::vec3(0.0f, 0.1f, 0.0f);
            particles.MaxParticles = 128;
            scene.GetRegistry().Emplace<ParticleEmitterComponent>(orb, std::move(particles));
        }

        m_Scene.Clear();
        m_Scene.InstantiateScene(scene);
        m_Scene.GetSettings() = scene.GetSettings();
        return true;
    }

    bool Project::WriteProjectFile(std::string* error)
    {
        YAML::Node node;
        node["format"] = "aster-project";
        node["version"] = 1;
        node["name"] = m_Info.Name;
        node["default_scene"] = m_Info.DefaultScene;

        YAML::Emitter emitter;
        emitter << node;
        if (!emitter.good())
        {
            SetError(error, "Could not serialize project settings");
            return false;
        }

        const std::filesystem::path path = m_Info.RootPath / "project.aster.yaml";
        if (!FileSystem::WriteTextFile(path, emitter.c_str()))
        {
            SetError(error, "Could not write " + FileSystem::Normalize(path));
            return false;
        }
        return true;
    }

    bool Project::ReadProjectFile(const std::filesystem::path& file, std::string* error)
    {
        const std::optional<std::string> contents = FileSystem::ReadTextFile(file);
        if (!contents.has_value())
        {
            SetError(error, "Could not read " + FileSystem::Normalize(file));
            return false;
        }

        try
        {
            const YAML::Node node = YAML::Load(*contents);
            if (!node["format"] || node["format"].as<std::string>() != "aster-project")
            {
                SetError(error, "Invalid project format");
                return false;
            }
            const int version = node["version"] ? node["version"].as<int>() : 1;
            if (version != 1)
            {
                SetError(error, "Unsupported project version: " + std::to_string(version));
                return false;
            }

            m_Info.Name = node["name"] ? node["name"].as<std::string>() : "Untitled Project";
            m_Info.DefaultScene = node["default_scene"] ? node["default_scene"].as<std::string>() :
                "scenes/main.aster.yaml";
            m_Info.RootPath = file.parent_path();
            return true;
        }
        catch (const std::exception& exception)
        {
            SetError(error, std::string("Could not parse project file: ") + exception.what());
            return false;
        }
    }
}
