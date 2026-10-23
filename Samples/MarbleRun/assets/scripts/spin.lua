-- Aster sample behavior: rotate the entity around its Y axis.
return {
    OnUpdate = function(entity, deltaTime)
        local x, y, z = entity:GetRotation()
        entity:SetRotation(x, y + 45.0 * deltaTime, z)
    end
}
