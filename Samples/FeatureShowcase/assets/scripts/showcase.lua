local rotator = nil
local fire = nil
local elapsed = 0.0

-- Drives two scene entities so the showcase exercises Lua transforms every frame.
return {
    OnStart = function(entity)
        rotator = scene.Find("Rotator")
        fire = scene.Find("Fire")
        elapsed = 0.0
    end,
    OnUpdate = function(entity, deltaTime)
        elapsed = elapsed + deltaTime
        if rotator ~= nil then
            rotator:SetRotation(0.0, elapsed * 35.0, 0.0)
        end
        if fire ~= nil then
            fire:SetPosition(math.sin(elapsed * 1.1) * 2.6, 0.35, 2.8 + math.cos(elapsed * 0.7) * 0.8)
        end
    end
}
