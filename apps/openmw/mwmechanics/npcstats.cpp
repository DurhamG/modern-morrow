#include "npcstats.hpp"

#include <algorithm>
#include <cassert>
#include <format>

#include <components/esm/attr.hpp>
#include <components/esm3/loadclas.hpp>
#include <components/esm3/loadfact.hpp>
#include <components/esm3/loadgmst.hpp>
#include <components/esm3/npcstats.hpp>
#include <components/fallback/fallback.hpp>

#include <MyGUI_TextIterator.h>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

MWMechanics::NpcStats::NpcStats()
    : mDisposition(0)
    , mCrimeDispositionModifier(0)
    , mReputation(0)
    , mCrimeId(-1)
    , mBounty(0)
    , mWerewolfKills(0)
    , mLevelProgress(0)
    , mTimeToStartDrowning(-1.0) // set breath to special value, it will be replaced during actor update
    , mIsWerewolf(false)
{
    mSpecIncreases.resize(3, 0);
    for (const ESM::Skill& skill : MWBase::Environment::get().getESMStore()->get<ESM::Skill>())
        mSkills.emplace(skill.mId, SkillValue{});
}

int MWMechanics::NpcStats::getBaseDisposition() const
{
    return mDisposition;
}

void MWMechanics::NpcStats::setBaseDisposition(int disposition)
{
    mDisposition = disposition;
}

int MWMechanics::NpcStats::getCrimeDispositionModifier() const
{
    return mCrimeDispositionModifier;
}

void MWMechanics::NpcStats::setCrimeDispositionModifier(int value)
{
    mCrimeDispositionModifier = value;
}

void MWMechanics::NpcStats::modCrimeDispositionModifier(int value)
{
    mCrimeDispositionModifier += value;
}

const MWMechanics::SkillValue& MWMechanics::NpcStats::getSkill(ESM::RefId id) const
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    return it->second;
}

MWMechanics::SkillValue& MWMechanics::NpcStats::getSkill(ESM::RefId id)
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    return it->second;
}

void MWMechanics::NpcStats::setSkill(ESM::RefId id, const MWMechanics::SkillValue& value)
{
    auto it = mSkills.find(id);
    if (it == mSkills.end())
        throw std::runtime_error("skill not found");
    it->second = value;
}

const std::map<ESM::RefId, int>& MWMechanics::NpcStats::getFactionRanks() const
{
    return mFactionRank;
}

int MWMechanics::NpcStats::getFactionRank(const ESM::RefId& faction) const
{
    auto it = mFactionRank.find(faction);
    if (it != mFactionRank.end())
        return it->second;

    return -1;
}

void MWMechanics::NpcStats::joinFaction(const ESM::RefId& faction)
{
    auto it = mFactionRank.find(faction);
    if (it == mFactionRank.end())
        mFactionRank[faction] = 0;
}

void MWMechanics::NpcStats::setFactionRank(const ESM::RefId& faction, int newRank)
{
    auto it = mFactionRank.find(faction);
    if (it != mFactionRank.end())
    {
        const ESM::Faction* factionPtr = MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(faction);
        if (newRank < 0)
        {
            mFactionRank.erase(it);
            mExpelled.erase(faction);
        }
        else if (newRank < static_cast<int>(factionPtr->mData.mRankData.size()))
            do
                it->second = newRank;
            // Does the new rank exist?
            while (newRank > 0 && factionPtr->mRanks[newRank--].empty());
    }
}

bool MWMechanics::NpcStats::getExpelled(const ESM::RefId& factionID) const
{
    return mExpelled.find(factionID) != mExpelled.end();
}

void MWMechanics::NpcStats::expell(const ESM::RefId& factionID, bool printMessage)
{
    if (mExpelled.find(factionID) == mExpelled.end())
    {
        mExpelled.insert(factionID);
        if (!printMessage)
            return;

        std::string message = "#{sExpelledMessage}";
        message += MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(factionID)->mName;
        MWBase::Environment::get().getWindowManager()->messageBox(message);
    }
}

void MWMechanics::NpcStats::clearExpelled(const ESM::RefId& factionID)
{
    mExpelled.erase(factionID);
}

bool MWMechanics::NpcStats::isInFaction(const ESM::RefId& faction) const
{
    return (mFactionRank.find(faction) != mFactionRank.end());
}

int MWMechanics::NpcStats::getFactionReputation(const ESM::RefId& faction) const
{
    auto iter = mFactionReputation.find(faction);

    if (iter == mFactionReputation.end())
        return 0;

    return iter->second;
}

void MWMechanics::NpcStats::setFactionReputation(const ESM::RefId& faction, int value)
{
    mFactionReputation[faction] = value;
}

namespace
{
    float getTypeFactor(ESM::RefId id, const ESM::Class& npcClass)
    {
        const auto& gmst = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
        for (const auto& skill : npcClass.mData.mMinorSkills)
        {
            if (skill == id)
                return gmst.find("fMinorSkillBonus")->mValue.getFloat();
        }
        for (const auto& skill : npcClass.mData.mMajorSkills)
        {
            if (skill == id)
                return gmst.find("fMajorSkillBonus")->mValue.getFloat();
        }
        return gmst.find("fMiscSkillBonus")->mValue.getFloat();
    }
}

float MWMechanics::NpcStats::getSkillProgressRequirement(ESM::RefId id, const ESM::Class& npcClass) const
{
    float progressRequirement = 1.f + getSkill(id).getBase();

    const MWWorld::Store<ESM::GameSetting>& gmst = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
    const ESM::Skill* skill = MWBase::Environment::get().getESMStore()->get<ESM::Skill>().find(id);

    const float typeFactor = getTypeFactor(id, npcClass);

    progressRequirement *= typeFactor;

    if (typeFactor <= 0)
        throw std::runtime_error("invalid skill type factor");

    float specialisationFactor = 1;

    if (skill->mData.mSpecialization == npcClass.mData.mSpecialization)
    {
        specialisationFactor = gmst.find("fSpecialSkillBonus")->mValue.getFloat();

        if (specialisationFactor <= 0)
            throw std::runtime_error("invalid skill specialisation factor");
    }
    progressRequirement *= specialisationFactor;

    return progressRequirement;
}

int MWMechanics::NpcStats::getLevelProgress() const
{
    return mLevelProgress;
}

void MWMechanics::NpcStats::setLevelProgress(int progress)
{
    mLevelProgress = progress;
}

bool MWMechanics::NpcStats::isLevelUpReady() const
{
    const MWWorld::Store<ESM::GameSetting>& gmst = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();
    return mLevelProgress >= gmst.find("iLevelUpTotal")->mValue.getInteger();
}

int MWMechanics::NpcStats::getLevelUpAttributeCount() const
{
    constexpr int maxAttributes = 3;
    int count = 0;
    for (const ESM::Attribute& attribute : MWBase::Environment::get().getESMStore()->get<ESM::Attribute>())
    {
        if (getLevelUpAttributeIncrease(attribute.mId) > 0)
            ++count;
    }
    return std::min(maxAttributes, count);
}

int MWMechanics::NpcStats::getLevelUpAttributeIncrease(ESM::RefId attribute) const
{
    const float base = getAttribute(attribute).getBase();
    if (base >= 100)
        return 0;
    return std::max(1, std::min(getLevelupAttributeMultiplier(attribute), static_cast<int>(100 - base)));
}

std::string_view MWMechanics::NpcStats::getLevelUpClassImage() const
{
    const int combatIncreases = getSkillIncreasesForSpecialization(ESM::Class::Specialization::Combat);
    const int magicIncreases = getSkillIncreasesForSpecialization(ESM::Class::Specialization::Magic);
    const int stealthIncreases = getSkillIncreasesForSpecialization(ESM::Class::Specialization::Stealth);

    std::string_view ret = "acrobat";

    int total = combatIncreases + magicIncreases + stealthIncreases;
    if (total == 0)
        return ret;

    int combatFraction = static_cast<int>(static_cast<float>(combatIncreases) / total * 10.f);
    int magicFraction = static_cast<int>(static_cast<float>(magicIncreases) / total * 10.f);
    int stealthFraction = static_cast<int>(static_cast<float>(stealthIncreases) / total * 10.f);

    if (combatFraction > 7)
        ret = "warrior";
    else if (magicFraction > 7)
        ret = "mage";
    else if (stealthFraction > 7)
        ret = "thief";

    switch (combatFraction)
    {
        case 7:
            ret = "warrior";
            break;
        case 6:
            if (stealthFraction == 1)
                ret = "barbarian";
            else if (stealthFraction == 3)
                ret = "crusader";
            else
                ret = "knight";
            break;
        case 5:
            if (stealthFraction == 3)
                ret = "scout";
            else
                ret = "archer";
            break;
        case 4:
            ret = "rogue";
            break;
        default:
            break;
    }

    switch (magicFraction)
    {
        case 7:
            ret = "mage";
            break;
        case 6:
            if (combatFraction == 2)
                ret = "sorcerer";
            else if (combatIncreases == 3)
                ret = "healer";
            else
                ret = "battlemage";
            break;
        case 5:
            ret = "witchhunter";
            break;
        case 4:
            ret = "spellsword";
            // In vanilla there's also code for "nightblade", however it seems to be unreachable.
            break;
        default:
            break;
    }

    switch (stealthFraction)
    {
        case 7:
            ret = "thief";
            break;
        case 6:
            if (magicFraction == 1)
                ret = "agent";
            else if (magicIncreases == 3)
                ret = "assassin";
            else
                ret = "acrobat";
            break;
        case 5:
            if (magicIncreases == 3)
                ret = "monk";
            else
                ret = "pilgrim";
            break;
        case 3:
            if (magicFraction == 3)
                ret = "bard";
            break;
        default:
            break;
    }

    return ret;
}

std::string_view MWMechanics::NpcStats::getLevelUpDescription() const
{
    const std::string_view description = Fallback::Map::getString(std::format("Level_Up_Level{}", getLevel() + 1));
    if (description.empty())
        return Fallback::Map::getString("Level_Up_Default");
    return description;
}

void MWMechanics::NpcStats::levelUp(const std::vector<ESM::RefId>& attributes)
{
    for (const ESM::RefId& id : attributes)
    {
        AttributeValue attribute = getAttribute(id);
        attribute.setBase(std::min(100.f, attribute.getBase() + getLevelupAttributeMultiplier(id)));
        setAttribute(id, attribute);
    }

    const MWWorld::Store<ESM::GameSetting>& gmst = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>();

    mLevelProgress -= gmst.find("iLevelUpTotal")->mValue.getInteger();
    mLevelProgress = std::max(0, mLevelProgress); // might be necessary when levelup was invoked via console

    mSkillIncreases.clear();

    const float endurance = getAttribute(ESM::Attribute::Endurance).getBase();

    // "When you gain a level, in addition to increasing three primary attributes, your Health
    // will automatically increase by 10% of your Endurance attribute. If you increased Endurance this level,
    // the Health increase is calculated from the increased Endurance"
    // Note: we should add bonus Health points to current level too.
    float healthGain = endurance * gmst.find("fLevelUpHealthEndMult")->mValue.getFloat();
    MWMechanics::DynamicStat<float> health(getHealth());
    health.setBase(getHealth().getBase() + healthGain);
    health.setCurrent(std::max(1.f, getHealth().getCurrent() + healthGain));
    setHealth(health);

    setLevel(getLevel() + 1);
}

void MWMechanics::NpcStats::updateHealth()
{
    const float endurance = getAttribute(ESM::Attribute::Endurance).getBase();
    const float strength = getAttribute(ESM::Attribute::Strength).getBase();

    setHealth(floor(0.5f * (strength + endurance)));
}

int MWMechanics::NpcStats::getLevelupAttributeMultiplier(ESM::RefId attribute) const
{
    const auto it = mSkillIncreases.find(attribute);
    if (it == mSkillIncreases.end())
        return 1;
    const int num = std::clamp(it->second, 0, 10);
    if (num == 0)
        return 1;

    // iLevelUp01Mult - iLevelUp10Mult
    const std::string id = std::format("iLevelUp{:0>2}Mult", num);
    if (const ESM::GameSetting* gmst = MWBase::Environment::get().getESMStore()->get<ESM::GameSetting>().search(id))
        return gmst->mValue.getInteger();

    return 1;
}

int MWMechanics::NpcStats::getSkillIncreasesForAttribute(ESM::RefId attribute) const
{
    auto it = mSkillIncreases.find(attribute);
    if (it == mSkillIncreases.end())
        return 0;
    return it->second;
}

void MWMechanics::NpcStats::setSkillIncreasesForAttribute(ESM::RefId attribute, int increases)
{
    if (increases == 0)
        mSkillIncreases.erase(attribute);
    else
        mSkillIncreases[attribute] = increases;
}

int MWMechanics::NpcStats::getSkillIncreasesForSpecialization(ESM::Class::Specialization spec) const
{
    return mSpecIncreases[spec];
}

void MWMechanics::NpcStats::setSkillIncreasesForSpecialization(ESM::Class::Specialization spec, int increases)
{
    assert(spec >= 0 && spec < 3);
    mSpecIncreases[spec] = increases;
}

void MWMechanics::NpcStats::flagAsUsed(const ESM::RefId& id)
{
    mUsedIds.insert(id);
}

bool MWMechanics::NpcStats::hasBeenUsed(const ESM::RefId& id) const
{
    return mUsedIds.find(id) != mUsedIds.end();
}

int MWMechanics::NpcStats::getBounty() const
{
    return mBounty;
}

void MWMechanics::NpcStats::setBounty(int bounty)
{
    mBounty = bounty;
}

int MWMechanics::NpcStats::getReputation() const
{
    return mReputation;
}

void MWMechanics::NpcStats::setReputation(int reputation)
{
    // Reputation is capped in original engine
    mReputation = std::clamp(reputation, 0, 255);
}

int MWMechanics::NpcStats::getCrimeId() const
{
    return mCrimeId;
}

void MWMechanics::NpcStats::setCrimeId(int id)
{
    mCrimeId = id;
}

bool MWMechanics::NpcStats::hasSkillsForRank(const ESM::RefId& factionId, int rank) const
{
    const ESM::Faction& faction = *MWBase::Environment::get().getESMStore()->get<ESM::Faction>().find(factionId);

    const ESM::RankData& rankData = faction.mData.mRankData.at(rank);

    std::vector<int> skills;

    for (const ESM::RefId& id : faction.mData.mSkills)
    {
        if (!id.empty())
            skills.push_back(static_cast<int>(getSkill(id).getBase()));
    }

    if (skills.empty())
        return true;

    std::sort(skills.begin(), skills.end());

    std::vector<int>::const_reverse_iterator iter = skills.rbegin();

    if (*iter < rankData.mPrimarySkill)
        return false;

    if (skills.size() < 2)
        return true;

    iter++;
    if (*iter < rankData.mFavouredSkill)
        return false;

    if (skills.size() < 3)
        return true;

    iter++;
    if (*iter < rankData.mFavouredSkill)
        return false;

    return true;
}

bool MWMechanics::NpcStats::isWerewolf() const
{
    return mIsWerewolf;
}

void MWMechanics::NpcStats::setWerewolf(bool set)
{
    if (mIsWerewolf == set)
        return;

    if (set != false)
    {
        mWerewolfKills = 0;
    }
    mIsWerewolf = set;
}

int MWMechanics::NpcStats::getWerewolfKills() const
{
    return mWerewolfKills;
}

void MWMechanics::NpcStats::addWerewolfKill()
{
    ++mWerewolfKills;
}

float MWMechanics::NpcStats::getTimeToStartDrowning() const
{
    return mTimeToStartDrowning;
}

void MWMechanics::NpcStats::setTimeToStartDrowning(float time)
{
    mTimeToStartDrowning = time;
}

void MWMechanics::NpcStats::writeState(ESM::CreatureStats& state) const
{
    CreatureStats::writeState(state);
}

void MWMechanics::NpcStats::writeState(ESM::NpcStats& state) const
{
    for (std::map<ESM::RefId, int>::const_iterator iter(mFactionRank.begin()); iter != mFactionRank.end(); ++iter)
        state.mFactions[iter->first].mRank = iter->second;

    state.mDisposition = mDisposition;
    state.mCrimeDispositionModifier = mCrimeDispositionModifier;

    for (const auto& [id, value] : mSkills)
        value.writeState(state.mSkills[id]);

    state.mIsWerewolf = mIsWerewolf;

    state.mCrimeId = mCrimeId;

    state.mBounty = mBounty;

    for (auto iter(mExpelled.begin()); iter != mExpelled.end(); ++iter)
        state.mFactions[*iter].mExpelled = true;

    for (auto iter(mFactionReputation.begin()); iter != mFactionReputation.end(); ++iter)
        state.mFactions[iter->first].mReputation = iter->second;

    state.mReputation = mReputation;
    state.mWerewolfKills = mWerewolfKills;
    state.mLevelProgress = mLevelProgress;

    state.mSkillIncrease = mSkillIncreases;

    for (size_t i = 0; i < state.mSpecIncreases.size(); ++i)
        state.mSpecIncreases[i] = mSpecIncreases[i];

    std::copy(mUsedIds.begin(), mUsedIds.end(), std::back_inserter(state.mUsedIds));

    state.mTimeToStartDrowning = mTimeToStartDrowning;
}
void MWMechanics::NpcStats::readState(const ESM::CreatureStats& state)
{
    CreatureStats::readState(state);
}

void MWMechanics::NpcStats::readState(const ESM::NpcStats& state)
{
    const MWWorld::ESMStore& store = *MWBase::Environment::get().getESMStore();

    for (auto iter(state.mFactions.begin()); iter != state.mFactions.end(); ++iter)
        if (store.get<ESM::Faction>().search(iter->first))
        {
            if (iter->second.mExpelled)
                mExpelled.insert(iter->first);

            if (iter->second.mRank >= 0)
                mFactionRank[iter->first] = iter->second.mRank;

            if (iter->second.mReputation)
                mFactionReputation[iter->first] = iter->second.mReputation;
        }

    mDisposition = state.mDisposition;
    mCrimeDispositionModifier = state.mCrimeDispositionModifier;

    for (const auto& [id, value] : state.mSkills)
        mSkills[id].readState(value);

    mIsWerewolf = state.mIsWerewolf;

    mCrimeId = state.mCrimeId;
    mBounty = state.mBounty;
    mReputation = state.mReputation;
    mWerewolfKills = state.mWerewolfKills;
    mLevelProgress = state.mLevelProgress;

    mSkillIncreases = state.mSkillIncrease;

    for (size_t i = 0; i < state.mSpecIncreases.size(); ++i)
        mSpecIncreases[i] = state.mSpecIncreases[i];

    for (auto iter(state.mUsedIds.begin()); iter != state.mUsedIds.end(); ++iter)
        if (store.find(*iter))
            mUsedIds.insert(*iter);

    mTimeToStartDrowning = state.mTimeToStartDrowning;
}
