local content = require('openmw.content')

local function testApparatusContent()
    local apparatuses = content.apparatuses.records
    apparatuses.OMW_Generated_Apparatus = {
        name = 'Generated apparatus',
        model = 'meshes/a/generated.nif',
        icon = 'icons/a/generated.dds',
        mwscript = 'generated_script',
        type = content.apparatuses.TYPE.Alembic,
        quality = 2.5,
        weight = 4,
        value = 300,
    }
    local apparatus = apparatuses.OMW_Generated_Apparatus
    apparatus.name = 'Modified apparatus'
    apparatus.model = 'meshes/a/modified.nif'
    apparatus.icon = 'icons/a/modified.dds'
    apparatus.mwscript = nil
    apparatus.type = content.apparatuses.TYPE.Calcinator
    apparatus.quality = 10
    apparatus.weight = 5.5
    apparatus.value = 450

    apparatuses.OMW_Templated_Apparatus = { template = apparatus, name = 'Templated apparatus' }
    apparatuses.OMW_Copied_Apparatus = apparatus
    apparatuses.OMW_Temporary_Apparatus = { type = content.apparatuses.TYPE.Retort }
    apparatuses.OMW_Temporary_Apparatus = nil

    local ok = pcall(function()
        apparatuses.OMW_Invalid_Apparatus = { type = 4 }
    end)
    assert(not ok, 'Invalid apparatus type should be rejected during creation')
    ok = pcall(function()
        apparatus.type = -1
    end)
    assert(not ok, 'Invalid apparatus type should be rejected during mutation')
end

return {
    engineHandlers = {
        onContentFilesLoaded = function()
            content.statics.records.OMW_Generated_Static = { model = 'meshes/generatedonload.nif' }
            testApparatusContent()
            local fireDamage = content.magicEffects.records.firedamage
            fireDamage.baseCost = 5
            fireDamage.onSelf = true
            fireDamage.onTouch = true
            fireDamage.onTarget = true
            fireDamage.allowsSpellmaking = true
            fireDamage.allowsEnchanting = true
            for _, id in ipairs({ 'gold_001', 'gold_005', 'gold_010', 'gold_025', 'gold_100' }) do
                content.miscs.records[id] = { name = 'Gold', value = 1 }
            end
            content.repairs.records.OMW_Test_RepairTool = { name = 'Test hammer', maxCondition = 10, quality = 1 }
            content.miscs.records.OMW_Test_SoulGem = { name = 'Test soul gem', value = 10 }
            content.miscs.records.misc_soulgem_omw_test = { name = 'Test soul gem with its usual id', value = 10 }
            content.apparatuses.records.OMW_Test_Mortar = {
                name = 'Test mortar',
                type = content.apparatuses.TYPE.MortarPestle,
                quality = 1,
            }
            -- Races and a class for the character creation tests
            content.spells.records.omw_chargen_power = { name = 'Chargen power', type = content.spells.TYPE.Power }
            content.races.records.omw_chargen_race = {
                template = content.races.records.defaultrace,
                name = 'Chargen race',
                isPlayable = true,
                spells = { 'omw_chargen_power' },
            }
            content.races.records.omw_unplayable_race = {
                template = content.races.records.defaultrace,
                isPlayable = false,
            }
            content.classes.records.omw_chargen_class = {
                template = content.classes.records.defaultclass,
                name = 'Chargen class',
                isPlayable = true,
            }
        end
    }
}
