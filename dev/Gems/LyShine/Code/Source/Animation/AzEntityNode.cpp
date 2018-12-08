/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "LyShine_precompiled.h"
#include "AzEntityNode.h"
#include "AnimSplineTrack.h"
#include "BoolTrack.h"
#include "ISystem.h"
#include "CompoundSplineTrack.h"
#include "UiAnimationSystem.h"
#include "PNoise3.h"
#include "AnimSequence.h"

#include <IAudioSystem.h>
#include <ILipSync.h>
#include <ICryAnimation.h>
#include <CryCharMorphParams.h>
#include <Cry_Camera.h>

// AI
#include <IAgent.h>
#include "IAIObject.h"
#include "IAIActor.h"
#include "IGameFramework.h"

#include <IEntityHelper.h>
#include "Components/IComponentEntityNode.h"
#include "Components/IComponentAudio.h"
#include "Components/IComponentPhysics.h"

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <LyShine/Bus/UiAnimateEntityBus.h>

#define s_nodeParamsInitialized s_nodeParamsInitializedEnt
#define s_nodeParams s_nodeParamsEnt
#define AddSupportedParam AddSupportedParamEnt

static const float TIMEJUMPED_TRANSITION_TIME = 1.0f;
static const float EPSILON = 0.01f;

static const char* s_VariablePrefixes[] =
{
    "n", "i", "b", "f", "s", "ei", "es",
    "shader", "clr", "color", "vector",
    "snd", "sound", "dialog", "tex", "texture",
    "obj", "object", "file", "aibehavior",
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
    "aicharacter",
#endif
    "aipfpropertieslist", "aientityclasses", "aiterritory",
    "aiwave", "text", "equip", "reverbpreset", "eaxpreset",
    "aianchor", "soclass", "soclasses", "sostate", "sostates"
    "sopattern", "soaction", "sohelper", "sonavhelper",
    "soanimhelper", "soevent", "sotemplate", "customaction",
    "gametoken", "seq_", "mission_", "seqid_", "lightanimation_"
};

//////////////////////////////////////////////////////////////////////////
namespace
{
    const char* kScriptTablePrefix = "ScriptTable:";

    bool s_nodeParamsInitialized = false;
    StaticInstance<std::vector<CUiAnimNode::SParamInfo>> s_nodeParams;

    void AddSupportedParam(std::vector<CUiAnimNode::SParamInfo>& nodeParams, const char* sName, int paramId, EUiAnimValue valueType, int flags = 0)
    {
        CUiAnimNode::SParamInfo param;
        param.name = sName;
        param.paramType = paramId;
        param.valueType = valueType;
        param.flags = (IUiAnimNode::ESupportedParamFlags)flags;
        nodeParams.push_back(param);
    }

    void NotifyEntityScript(const IEntity* pEntity, const char* funcName)
    {
        IScriptTable* pEntityScript = pEntity->GetScriptTable();
        if (pEntityScript && pEntityScript->HaveValue(funcName))
        {
            Script::CallMethod(pEntityScript, funcName);
        }
    }

    // Quat::IsEquivalent has numerical problems with very similar values
    bool CompareRotation(const Quat& q1, const Quat& q2, float epsilon)
    {
        return (fabs_tpl(q1.v.x - q2.v.x) <= epsilon)
               && (fabs_tpl(q1.v.y - q2.v.y) <= epsilon)
               && (fabs_tpl(q1.v.z - q2.v.z) <= epsilon)
               && (fabs_tpl(q1.w - q2.w) <= epsilon);
    }

    void NotifyEntityScript(const IEntity* pEntity, const char* funcName, const char* strParam)
    {
        IScriptTable* pEntityScript = pEntity->GetScriptTable();
        if (pEntityScript && pEntityScript->HaveValue(funcName))
        {
            Script::CallMethod(pEntityScript, funcName, strParam);
        }
    }
};

//////////////////////////////////////////////////////////////////////////
CUiAnimAzEntityNode::CUiAnimAzEntityNode(const int id)
    : CUiAnimNode(id)
{
    m_bWasTransRot = false;
    m_bInitialPhysicsStatus = false;

    m_pos(0, 0, 0);
    m_scale(1, 1, 1);
    m_rotate.SetIdentity();

    m_visible = true;

    m_time = 0.0f;

    m_lastEntityKey = -1;

    #ifdef CHECK_FOR_TOO_MANY_ONPROPERTY_SCRIPT_CALLS
    m_OnPropertyCalls = 0;
    #endif

    UiAnimNodeBus::Handler::BusConnect(this);

    CUiAnimAzEntityNode::Initialize();
}

//////////////////////////////////////////////////////////////////////////
CUiAnimAzEntityNode::CUiAnimAzEntityNode()
    : CUiAnimAzEntityNode(0)
{
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::Initialize()
{
    if (!s_nodeParamsInitialized)
    {
        s_nodeParamsInitialized = true;
        s_nodeParams.reserve(1);
        AddSupportedParam(s_nodeParams, "Component Field float", eUiAnimParamType_AzComponentField, eUiAnimValue_Float);
    }
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::AddTrack(IUiAnimTrack* pTrack)
{
    const CUiAnimParamType& paramType = pTrack->GetParameterType();

    CUiAnimNode::AddTrack(pTrack);
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::UpdateDynamicParams()
{
    m_entityScriptPropertiesParamInfos.clear();
    m_nameToScriptPropertyParamInfo.clear();

    // editor stores *all* properties of *every* entity used in an AnimAzEntityNode, including to-display names, full lua paths, string maps for fast access, etc.
    // in pure game mode we just need to store the properties that we know are going to be used in a track, so we can save a lot of memory.
    if (gEnv->IsEditor())
    {
        UpdateDynamicParams_Editor();
    }
    else
    {
        UpdateDynamicParams_PureGame();
    }
}


//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::UpdateDynamicParams_Editor()
{
}


//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::UpdateDynamicParams_PureGame()
{
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::FindScriptTableForParameterRec(IScriptTable* pScriptTable, const string& path, string& propertyName, SmartScriptTable& propertyScriptTable)
{
    size_t pos = path.find_first_of('/');
    if (pos == string::npos)
    {
        propertyName = path;
        propertyScriptTable = pScriptTable;
        return;
    }
    string tableName = path.Left(pos);
    pos++;
    string pathLeft = path.Right(path.size() - pos);

    ScriptAnyValue value;
    pScriptTable->GetValueAny(tableName.c_str(), value);
    assert(value.type == ANY_TTABLE);
    if (value.type == ANY_TTABLE)
    {
        FindScriptTableForParameterRec(value.table, pathLeft, propertyName, propertyScriptTable);
    }
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::FindDynamicPropertiesRec(IScriptTable* pScriptTable, const string& currentPath, unsigned int depth)
{
    // There might be infinite recursion in the tables, so we need to limit the max recursion depth
    const unsigned int kMaxRecursionDepth = 5;

    IScriptTable::Iterator iter = pScriptTable->BeginIteration();

    while (pScriptTable->MoveNext(iter))
    {
        if (!iter.sKey || iter.sKey[0] == '_')  // Skip properties that start with an underscore
        {
            continue;
        }

        SScriptPropertyParamInfo paramInfo;
        bool isUnknownTable = ObtainPropertyTypeInfo(iter.sKey, pScriptTable, paramInfo);

        if (isUnknownTable && depth < kMaxRecursionDepth)
        {
            FindDynamicPropertiesRec(paramInfo.scriptTable, currentPath + iter.sKey + "/", depth + 1);
            continue;
        }

        if (paramInfo.animNodeParamInfo.valueType != eUiAnimValue_Unknown)
        {
            AddPropertyToParamInfoMap(iter.sKey, currentPath, paramInfo);
        }
    }
    pScriptTable->EndIteration(iter);
}


//////////////////////////////////////////////////////////////////////////
// fills some fields in paramInfo with the appropriate value related to the property defined by pScriptTable and pKey.
// returns true if the property is a table that should be parsed, instead of an atomic type  (simple vectors are treated like atomic types)
bool CUiAnimAzEntityNode::ObtainPropertyTypeInfo(const char* pKey, IScriptTable* pScriptTable, SScriptPropertyParamInfo& paramInfo)
{
    ScriptAnyValue value;
    pScriptTable->GetValueAny(pKey, value);
    paramInfo.scriptTable = pScriptTable;
    paramInfo.isVectorTable = false;
    paramInfo.animNodeParamInfo.valueType = eUiAnimValue_Unknown;
    bool isUnknownTable = false;

    switch (value.type)
    {
    case ANY_TNUMBER:
    {
        const bool hasBoolPrefix = (strlen(pKey) > 1) && (pKey[0] == 'b')
            && (pKey[1] != tolower(pKey[1]));
        paramInfo.animNodeParamInfo.valueType = hasBoolPrefix ? eUiAnimValue_Bool : eUiAnimValue_Float;
    }
    break;
    case ANY_TVECTOR:
        paramInfo.animNodeParamInfo.valueType = eUiAnimValue_Vector;
        break;
    case ANY_TBOOLEAN:
        paramInfo.animNodeParamInfo.valueType = eUiAnimValue_Bool;
        break;
    case ANY_TTABLE:
        // Threat table as vector if it contains x, y & z
        paramInfo.scriptTable = value.table;
        if (value.table->HaveValue("x") && value.table->HaveValue("y") && value.table->HaveValue("z"))
        {
            paramInfo.animNodeParamInfo.valueType = eUiAnimValue_Vector;
            paramInfo.scriptTable = value.table;
            paramInfo.isVectorTable = true;
        }
        else
        {
            isUnknownTable = true;
        }
        break;
    }

    return isUnknownTable;
}


//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::AddPropertyToParamInfoMap(const char* pKey, const string& currentPath, SScriptPropertyParamInfo& paramInfo)
{
    // Strip variable name prefix
    const char* strippedName = pKey;
    while (*strippedName && (*strippedName == tolower(*strippedName) || *strippedName == '_'))
    {
        ++strippedName;
    }
    const size_t prefixLength = strippedName - pKey;

    // Check if stripped prefix is in list of valid variable prefixes
    bool foundPrefix = false;
    for (size_t i = 0; i < sizeof(s_VariablePrefixes) / sizeof(const char*); ++i)
    {
        if ((strlen(s_VariablePrefixes[i]) == prefixLength) &&
            (memcmp(pKey, s_VariablePrefixes[i], prefixLength) == 0))
        {
            foundPrefix = true;
            break;
        }
    }

    // Only use stripped name if prefix is in list, otherwise use full name
    strippedName = foundPrefix ? strippedName : pKey;

    // If it is a vector check if we need to create a color track
    if (paramInfo.animNodeParamInfo.valueType == eUiAnimValue_Vector)
    {
        if ((prefixLength == 3 && memcmp(pKey, "clr", 3) == 0)
            || (prefixLength == 5 && memcmp(pKey, "color", 5) == 0))
        {
            paramInfo.animNodeParamInfo.valueType = eUiAnimValue_RGB;
        }
    }

    paramInfo.variableName = pKey;
    paramInfo.displayName = currentPath + strippedName;
    paramInfo.animNodeParamInfo.name = &paramInfo.displayName[0];
    const string paramIdStr = kScriptTablePrefix + currentPath + pKey;
    paramInfo.animNodeParamInfo.paramType = CUiAnimParamType(paramIdStr);
    paramInfo.animNodeParamInfo.flags = eSupportedParamFlags_MultipleTracks;
    m_entityScriptPropertiesParamInfos.push_back(paramInfo);

    m_nameToScriptPropertyParamInfo[paramIdStr] = m_entityScriptPropertiesParamInfos.size() - 1;
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::CreateDefaultTracks()
{
    // Default tracks for Entities are controlled through the toolbar menu
    // in MannequinDialog.
}

//////////////////////////////////////////////////////////////////////////
CUiAnimAzEntityNode::~CUiAnimAzEntityNode()
{
    ReleaseSounds();

    UiAnimNodeBus::Handler::BusDisconnect();
}

//////////////////////////////////////////////////////////////////////////
unsigned int CUiAnimAzEntityNode::GetParamCount() const
{
    return CUiAnimAzEntityNode::GetParamCountStatic() + m_entityScriptPropertiesParamInfos.size();
}

//////////////////////////////////////////////////////////////////////////
CUiAnimParamType CUiAnimAzEntityNode::GetParamType(unsigned int nIndex) const
{
    SParamInfo info;

    if (!CUiAnimAzEntityNode::GetParamInfoStatic(nIndex, info))
    {
        const uint scriptParamsOffset = (uint)s_nodeParams.size();
        const uint end = (uint)s_nodeParams.size() + (uint)m_entityScriptPropertiesParamInfos.size();

        if (nIndex >= scriptParamsOffset && nIndex < end)
        {
            return m_entityScriptPropertiesParamInfos[nIndex - scriptParamsOffset].animNodeParamInfo.paramType;
        }

        return eUiAnimParamType_Invalid;
    }

    return info.paramType;
}

//////////////////////////////////////////////////////////////////////////
int CUiAnimAzEntityNode::GetParamCountStatic()
{
    return s_nodeParams.size();
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::GetParamInfoStatic(int nIndex, SParamInfo& info)
{
    if (nIndex >= 0 && nIndex < (int)s_nodeParams.size())
    {
        info = s_nodeParams[nIndex];
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::Reflect(AZ::SerializeContext* serializeContext)
{
    serializeContext->Class<CUiAnimAzEntityNode, CUiAnimNode>()
        ->Version(1)
        ->Field("Entity", &CUiAnimAzEntityNode::m_entityId);
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::GetParamInfoFromType(const CUiAnimParamType& paramId, SParamInfo& info) const
{
    for (int i = 0; i < (int)s_nodeParams.size(); i++)
    {
        if (s_nodeParams[i].paramType == paramId)
        {
            info = s_nodeParams[i];
            return true;
        }
    }

    for (size_t i = 0; i < m_entityScriptPropertiesParamInfos.size(); ++i)
    {
        if (m_entityScriptPropertiesParamInfos[i].animNodeParamInfo.paramType == paramId)
        {
            info = m_entityScriptPropertiesParamInfos[i].animNodeParamInfo;
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
const AZ::SerializeContext::ClassElement* CUiAnimAzEntityNode::ComputeOffsetFromElementName(
    const AZ::SerializeContext::ClassData* classData,
    IUiAnimTrack* pTrack,
    size_t baseOffset)
{
    const UiAnimParamData& paramData = pTrack->GetParamData();

    // find the data element in the class data that matches the name in the paramData
    AZ::Crc32 nameCrc = AZ_CRC(paramData.GetName());
    const AZ::SerializeContext::ClassElement* element = nullptr;
    for (const AZ::SerializeContext::ClassElement& classElement : classData->m_elements)
    {
        if (classElement.m_nameCrc == nameCrc)
        {
            element = &classElement;
            break;
        }
    }

    // if the name doesn't exist or is of the wrong type then the animation data
    // no longer matches the component definition. This could happen if the serialization format of
    // a component is changed. We don't want to assert in that case. Ideally we would have
    // some way of converting the animation data. We do not have that yet. So we will output a warning
    // and recover.
    if (!element || element->m_typeId != paramData.GetTypeId())
    {
        bool mismatch = true;

        if (element)
        {
            // Allow AZ::Vector2 types to be assigned Vec2 animation data and AZ::Color types
            // to be assiged AZ::Vector3 animation data
            if (((element->m_typeId == AZ::SerializeTypeInfo<AZ::Vector2>::GetUuid()) && (paramData.GetTypeId() == AZ::SerializeTypeInfo<Vec2>::GetUuid()))
                || (element->m_typeId == AZ::SerializeTypeInfo<AZ::Color>::GetUuid()) && (paramData.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector3>::GetUuid()))
            {
                mismatch = false;
            }
        }

        if (mismatch)
        {
            string warnMsg = "Data mismatch reading animation data for type ";
            warnMsg += classData->m_typeId.ToString<string>();
            warnMsg += ". The field \"";
            warnMsg += paramData.GetName();
            if (!element)
            {
                warnMsg += "\" cannot be found.";
            }
            else
            {
                warnMsg += "\" has a different type to that in the animation data.";
            }
            warnMsg += " This part of the animation data will be ignored.";
            CryWarning(VALIDATOR_MODULE_SHINE, VALIDATOR_WARNING, warnMsg.c_str());
            return nullptr;
        }
    }

    // Set the correct offset in the param data for the track
    UiAnimParamData newParamData(paramData.GetComponentId(), paramData.GetName(), element->m_typeId, baseOffset + element->m_offset);
    pTrack->SetParamData(newParamData);

    return element;
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::ComputeOffsetsFromElementNames()
{
    // Get the serialize context for the application
    AZ::SerializeContext* context = nullptr;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialization context found");

    // Get the AZ entity that this node is animating
    AZ::Entity* entity = nullptr;
    EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, m_entityId);
    if (!entity)
    {
        // This can happen, if a UI element is deleted we do not delete all AnimNodes
        // that reference it (which could be in multiple sequences). Instead we leave
        // them in the sequences and draw them in red and they have no effect.
        // If the canvas is saved like that and reloaded we come through here. The
        // AnimNode can't be made functional again at this point but is there so that
        // the user can see that their sequence was animating something that has now
        // been deleted.
        return;
    }

    // go through all its tracks and update the offsets
    for (int i = 0, num = (int)m_tracks.size(); i < num; i++)
    {
        IUiAnimTrack* pTrack = m_tracks[i].get();

        if (pTrack->GetParameterType() == eUiAnimParamType_AzComponentField)
        {
            // Get the class data for the component that this track is animating
            const UiAnimParamData& paramData = pTrack->GetParamData();
            AZ::Component* component = paramData.GetComponent(entity);
            const AZ::Uuid& classId = AZ::SerializeTypeInfo<AZ::Component>::GetUuid(component);
            const AZ::SerializeContext::ClassData* classData = context->FindClassData(classId);

            // update the offset for the field this track is animating
            const AZ::SerializeContext::ClassElement* element = ComputeOffsetFromElementName(classData, pTrack, 0);

            bool deleteTrack = false;
            if (element)
            {
                // the field is a valid field in the component, proceed with sub tracks if any
                size_t baseOffset = element->m_offset;
                const AZ::SerializeContext::ClassData* baseElementClassData = context->FindClassData(element->m_typeId);

                // Search the sub-tracks also if any.
                if (baseElementClassData && !baseElementClassData->m_elements.empty())
                {
                    for (int k = 0; k < pTrack->GetSubTrackCount(); ++k)
                    {
                        IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(k);

                        if (pSubTrack->GetParameterType() == eUiAnimParamType_AzComponentField)
                        {
                            // update the offset for this subtrack
                            if (!ComputeOffsetFromElementName(baseElementClassData, pSubTrack, baseOffset))
                            {
                                deleteTrack = true; // animation data is no longer valid
                            }
                        }
                    }
                }
            }
            else
            {
                deleteTrack = true; // animation data is no longer valid
            }

            if (deleteTrack)
            {
                // delete track and leave as nullptr to be cleaned up after loop
                m_tracks[i].reset();
            }
        }
    }

    // remove any null entries from m_tracks (only happens if we found invalid animation data)
    stl::find_and_erase_if(m_tracks, [](const AZStd::intrusive_ptr<IUiAnimTrack>& sp) { return !sp; });
}

//////////////////////////////////////////////////////////////////////////
const char* CUiAnimAzEntityNode::GetParamName(const CUiAnimParamType& param) const
{
    SParamInfo info;
    if (GetParamInfoFromType(param, info))
    {
        return info.name;
    }

    const char* pName = param.GetName();
    if (param.GetType() == eUiAnimParamType_ByString && pName && strncmp(pName, kScriptTablePrefix, strlen(kScriptTablePrefix)) == 0)
    {
        return pName + strlen(kScriptTablePrefix);
    }

    return "Unknown Entity Parameter";
}

//////////////////////////////////////////////////////////////////////////
const char* CUiAnimAzEntityNode::GetParamNameForTrack(const CUiAnimParamType& param, const IUiAnimTrack* track) const
{
    // for Az Component Fields we use the name from the ClassElement
    if (param == eUiAnimParamType_AzComponentField)
    {
        // if the edit context is available it would be better to use the EditContext to get
        // the name? If so then we should pass than in as the name when creating the track
        return track->GetParamData().GetName();
    }

    SParamInfo info;
    if (GetParamInfoFromType(param, info))
    {
        return info.name;
    }

    const char* pName = param.GetName();
    if (param.GetType() == eUiAnimParamType_ByString && pName && strncmp(pName, kScriptTablePrefix, strlen(kScriptTablePrefix)) == 0)
    {
        return pName + strlen(kScriptTablePrefix);
    }

    return "Unknown Entity Parameter";
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::StillUpdate()
{
    // used to handle LookAt
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::EnableEntityPhysics(bool bEnable)
{
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::Animate(SUiAnimContext& ec)
{
    if (!m_entityId.IsValid())
    {
        return;
    }

    AZ::Entity* entity = nullptr;
    EBUS_EVENT_RESULT(entity, AZ::ComponentApplicationBus, FindEntity, m_entityId);
    if (!entity)
    {
        // This can happen, if a UI element is deleted we do not delete all AnimNodes
        // that reference it (which could be in multiple sequences). Instead we leave
        // them in the sequences and draw them in red and they have no effect. If the
        // delete is undone they will go back to working.
        return;
    }


    int entityUpdateFlags = 0;
    bool bScaleModified = false;
    bool bApplyNoise = false;
    bool bScriptPropertyModified = false;
    bool bForceEntityActivation = false;

    IUiAnimTrack* pPosTrack = NULL;
    IUiAnimTrack* pRotTrack = NULL;
    IUiAnimTrack* sclTrack = NULL;

    int nAnimCharacterLayer = 0, iAnimationTrack = 0;
    size_t nNumAudioTracks = 0;
    int trackCount = NumTracks();
    for (int paramIndex = 0; paramIndex < trackCount; paramIndex++)
    {
        CUiAnimParamType paramType = m_tracks[paramIndex]->GetParameterType();
        IUiAnimTrack* pTrack = m_tracks[paramIndex].get();
        if ((pTrack->HasKeys() == false)
            || (pTrack->GetFlags() & IUiAnimTrack::eUiAnimTrackFlags_Disabled)
            || pTrack->IsMasked(ec.trackMask))
        {
            continue;
        }

        AZ_Assert(paramType.GetType() == eUiAnimParamType_AzComponentField, "Invalid param type");

        const UiAnimParamData& paramData = pTrack->GetParamData();

        AZ::Component* component = paramData.GetComponent(entity);

        if (!component)
        {
            continue;
        }

        void* elementData = reinterpret_cast<char*>(component) + paramData.GetOffset();

        if (paramData.GetTypeId() == AZ::SerializeTypeInfo<float>::GetUuid())
        {
            float* elementFloat = reinterpret_cast<float*>(elementData);

            float trackValue;
            pTrack->GetValue(ec.time, trackValue);

            *elementFloat = trackValue;
        }
        else if (paramData.GetTypeId() == AZ::SerializeTypeInfo<bool>::GetUuid())
        {
            bool* elementValue = reinterpret_cast<bool*>(elementData);

            bool trackValue;
            pTrack->GetValue(ec.time, trackValue);

            *elementValue = trackValue;
        }
        else if (paramData.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector2>::GetUuid())
        {
            AZ::Vector2* elementValue = reinterpret_cast<AZ::Vector2*>(elementData);

            AZ::Vector2 trackValue;
            pTrack->GetValue(ec.time, trackValue);

            *elementValue = trackValue;
        }
        else if (paramData.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector3>::GetUuid())
        {
            AZ::Vector3* elementValue = reinterpret_cast<AZ::Vector3*>(elementData);

            AZ::Vector3 trackValue;
            pTrack->GetValue(ec.time, trackValue);

            *elementValue = trackValue;
        }
        else if (paramData.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector4>::GetUuid())
        {
            AZ::Vector4* elementValue = reinterpret_cast<AZ::Vector4*>(elementData);

            AZ::Vector4 trackValue;
            pTrack->GetValue(ec.time, trackValue);

            *elementValue = trackValue;
        }
        else if (paramData.GetTypeId() == AZ::SerializeTypeInfo<AZ::Color>::GetUuid())
        {
            AZ::Color* elementValue = reinterpret_cast<AZ::Color*>(elementData);

            AZ::Color trackValue = AZ::Color::CreateOne(); // Initialize alpha
            pTrack->GetValue(ec.time, trackValue);

            *elementValue = trackValue;
        }
        else
        {
            // Animate the sub-tracks also if any.
            for (int k = 0; k < pTrack->GetSubTrackCount(); ++k)
            {
                IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(k);

                const UiAnimParamData& subTrackParamData = pSubTrack->GetParamData();

                if (pSubTrack->GetParameterType() == eUiAnimParamType_AzComponentField)
                {
                    elementData = reinterpret_cast<char*>(component) + subTrackParamData.GetOffset();

                    if (subTrackParamData.GetTypeId() == AZ::SerializeTypeInfo<float>::GetUuid())
                    {
                        float* elementFloat = reinterpret_cast<float*>(elementData);

                        float trackValue;
                        pSubTrack->GetValue(ec.time, trackValue);

                        *elementFloat = trackValue;
                    }
                    else if (subTrackParamData.GetTypeId() == AZ::SerializeTypeInfo<bool>::GetUuid())
                    {
                        bool* elementValue = reinterpret_cast<bool*>(elementData);

                        bool trackValue;
                        pSubTrack->GetValue(ec.time, trackValue);

                        *elementValue = trackValue;
                    }
                }
            }
        }
    }

    m_time = ec.time;

    if (m_pOwner)
    {
        m_bIgnoreSetParam = true; // Prevents feedback change of track.
        m_pOwner->OnNodeUiAnimated(this);
        m_bIgnoreSetParam = false;
    }

    EBUS_EVENT_ID(m_entityId, UiAnimateEntityBus, PropertyValuesChanged);
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::ReleaseSounds()
{
    // Audio: Stop all playing sounds
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::OnReset()
{
    m_lastEntityKey = -1;

    ReleaseSounds();
    UpdateDynamicParams();
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::OnResetHard()
{
    OnReset();
    if (m_pOwner)
    {
        m_pOwner->OnNodeReset(this);
    }
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::Activate(bool bActivate)
{
    CUiAnimNode::Activate(bActivate);
    if (bActivate)
    {
#ifdef CHECK_FOR_TOO_MANY_ONPROPERTY_SCRIPT_CALLS
        m_OnPropertyCalls = 0;
#endif
    }
    else
    {
#ifdef CHECK_FOR_TOO_MANY_ONPROPERTY_SCRIPT_CALLS
        IEntity* pEntity = GetEntity();
        if (m_OnPropertyCalls > 30) // arbitrary amount
        {
            CryWarning(VALIDATOR_MODULE_SHINE, VALIDATOR_ERROR, "Entity: %s. A UI animation has called lua function 'OnPropertyChange' too many (%d) times .This is a performance issue. Adding Some custom management in the entity lua code will fix the issue", pEntity ? pEntity->GetName() : "<UNKNOWN", m_OnPropertyCalls);
        }
#endif
    }
};

//////////////////////////////////////////////////////////////////////////
IUiAnimTrack* CUiAnimAzEntityNode::GetTrackForAzField(const UiAnimParamData& param) const
{
    for (int i = 0, num = (int)m_tracks.size(); i < num; i++)
    {
        IUiAnimTrack* pTrack = m_tracks[i].get();

        if (pTrack->GetParameterType() == eUiAnimParamType_AzComponentField)
        {
            if (pTrack->GetParamData() == param)
            {
                return pTrack;
            }
        }

        // Search the sub-tracks also if any.
        for (int k = 0; k < pTrack->GetSubTrackCount(); ++k)
        {
            IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(k);

            if (pSubTrack->GetParameterType() == eUiAnimParamType_AzComponentField)
            {
                if (pSubTrack->GetParamData() == param)
                {
                    return pSubTrack;
                }
            }
        }
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
IUiAnimTrack* CUiAnimAzEntityNode::CreateTrackForAzField(const UiAnimParamData& param)
{
    AZ::SerializeContext* context = nullptr;
    EBUS_EVENT_RESULT(context, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(context, "No serialization context found");

    IUiAnimTrack* pTrack = nullptr;

    const AZ::SerializeContext::ClassData* classData = context->FindClassData(param.GetTypeId());
    if (classData && !classData->m_elements.empty())
    {
        // this is a compound type, create a compound track

        // We only support compound tracks with 2, 3 or 4 subtracks
        int numElements = classData->m_elements.size();
        if (numElements < 2 || numElements > 4)
        {
            return nullptr;
        }

        EUiAnimValue valueType;
        switch (numElements)
        {
        case 2:
            valueType = eUiAnimValue_Vector2;
            break;
        case 3:
            valueType = eUiAnimValue_Vector3;
            break;
        case 4:
            valueType = eUiAnimValue_Vector4;
            break;
        }

        pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, valueType);

        pTrack->SetParamData(param);

        int numSubTracks = pTrack->GetSubTrackCount();
        int curSubTrack = 0;

        for (const AZ::SerializeContext::ClassElement& element : classData->m_elements)
        {
            if (element.m_typeId == AZ::SerializeTypeInfo<float>::GetUuid() && curSubTrack < numSubTracks)
            {
                IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(curSubTrack);
                pSubTrack->SetParameterType(eUiAnimParamType_AzComponentField);

                UiAnimParamData subTrackParam(param.GetComponentId(), element.m_name,
                    element.m_typeId, param.GetOffset() + element.m_offset);
                pSubTrack->SetParamData(subTrackParam);

                pTrack->SetSubTrackName(curSubTrack, element.m_name);
                curSubTrack++;
            }
        }

        for (int i = curSubTrack; i < numElements; ++i)
        {
            pTrack->SetSubTrackName(i, "_unused");  // only happens if some elements were not floats
        }
    }
    else if (param.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector2>::GetUuid())
    {
        pTrack = CreateVectorTrack(param, eUiAnimValue_Vector2, 2);
    }
    else if (param.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector3>::GetUuid())
    {
        pTrack = CreateVectorTrack(param, eUiAnimValue_Vector3, 3);
    }
    else if (param.GetTypeId() == AZ::SerializeTypeInfo<AZ::Vector4>::GetUuid())
    {
        pTrack = CreateVectorTrack(param, eUiAnimValue_Vector4, 4);
    }
    else if (param.GetTypeId() == AZ::SerializeTypeInfo<AZ::Color>::GetUuid())
    {
        // this is a compound type, create a compound track
        pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, eUiAnimValue_Vector3);

        pTrack->SetParamData(param);

        pTrack->SetSubTrackName(0, "R");
        pTrack->SetSubTrackName(1, "G");
        pTrack->SetSubTrackName(2, "B");

        int numSubTracks = pTrack->GetSubTrackCount();
        for (int i = 0; i < numSubTracks; ++i)
        {
            IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(i);
            pSubTrack->SetParameterType(eUiAnimParamType_Float);    // subtracks are not actual component properties
        }

        return pTrack;
    }
    else
    {
        if (param.GetTypeId() == AZ::SerializeTypeInfo<float>::GetUuid())
        {
            pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, eUiAnimValue_Unknown);
        }
        else if (param.GetTypeId() == AZ::SerializeTypeInfo<bool>::GetUuid())
        {
            pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, eUiAnimValue_Bool);
        }
        else if (param.GetTypeId() == AZ::SerializeTypeInfo<int>::GetUuid())
        {
            // no support for int yet
            pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, eUiAnimValue_Bool);
        }
        else if (param.GetTypeId() == AZ::SerializeTypeInfo<unsigned int>::GetUuid())
        {
            // no support for int yet
            pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, eUiAnimValue_Bool);
        }

        pTrack->SetParamData(param);
    }

    return pTrack;
}

void CUiAnimAzEntityNode::OnStart()
{
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::OnPause()
{
    ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::OnStop()
{
    ReleaseSounds();
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, float value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, bool value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->CreateKey(time);
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, int value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->CreateKey(time);
        // don't have int tracks yet
        //        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, unsigned int value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->CreateKey(time);
        // don't have unsigned int tracks yet
        //        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, const AZ::Vector2& value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, const AZ::Vector3& value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, const AZ::Vector4& value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::SetParamValueAz(float time, const UiAnimParamData& param, const AZ::Color& value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->SetValue(time, value);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
bool CUiAnimAzEntityNode::GetParamValueAz(float time, const UiAnimParamData& param, float& value)
{
    IUiAnimTrack* pTrack = GetTrackForAzField(param);
    if (pTrack)
    {
        pTrack->GetValue(time, value);
        return true;
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////
static bool HaveKeysChanged(int32 activeKeys[], int32 previousKeys[])
{
    return !((activeKeys[0] == previousKeys[0]) && (activeKeys[1] == previousKeys[1]));
}


//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
    CUiAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
    if (bLoading)
    {
        unsigned long idHi;
        unsigned long idLo;

        xmlNode->getAttr("EntityIdHi", idHi);
        xmlNode->getAttr("EntityIdLo", idLo);
        AZ::u64 id64 = ((AZ::u64)idHi) << 32 | idLo;
        m_entityId = AZ::EntityId(id64);
    }
    else
    {
        AZ::u64 id64 = static_cast<AZ::u64>(m_entityId);
        unsigned long idHi = id64 >> 32;
        unsigned long idLo = id64 & 0xFFFFFFFF;
        xmlNode->setAttr("EntityIdHi", idHi);
        xmlNode->setAttr("EntityIdLo", idLo);
    }
}

//////////////////////////////////////////////////////////////////////////
void CUiAnimAzEntityNode::InitPostLoad(IUiAnimSequence* pSequence, bool remapIds, LyShine::EntityIdMap* entityIdMap)
{
    // do base class init first
    CUiAnimNode::InitPostLoad(pSequence, remapIds, entityIdMap);

    if (remapIds)
    {
        // the UI element entityIDs were changed on load, so update the entityId of the
        // entity this node is animating using the given map
        AZ::EntityId newId = (*entityIdMap)[m_entityId];
        if (newId.IsValid())
        {
            m_entityId = newId;
        }
    }

    // We don't save the offset for each track in serialized data because, if they added or removed
    // fields in a component, the offset would be invalid. So we compute the offset on load using the
    // field name and type to find it in the class data
    ComputeOffsetsFromElementNames();
}

void CUiAnimAzEntityNode::PrecacheStatic(float time)
{
}

void CUiAnimAzEntityNode::PrecacheDynamic(float time)
{
    // Used to update durations of all character animations.
}

//////////////////////////////////////////////////////////////////////////
Vec3 CUiAnimAzEntityNode::Noise::Get(float time) const
{
    Vec3 noise;
    const float phase = time * m_freq;
    const Vec3 phase0 = Vec3(15.0f * m_freq, 55.1f * m_freq, 101.2f * m_freq);

    noise.x = gEnv->pSystem->GetNoiseGen()->Noise1D(phase + phase0.x) * m_amp;
    noise.y = gEnv->pSystem->GetNoiseGen()->Noise1D(phase + phase0.y) * m_amp;
    noise.z = gEnv->pSystem->GetNoiseGen()->Noise1D(phase + phase0.z) * m_amp;

    return noise;
}

IUiAnimTrack* CUiAnimAzEntityNode::CreateVectorTrack(const UiAnimParamData& param, EUiAnimValue valueType, int numElements)
{
    // this is a compound type, create a compound track
    IUiAnimTrack* pTrack = CreateTrackInternal(eUiAnimParamType_AzComponentField, eUiAnimCurveType_BezierFloat, valueType);

    pTrack->SetParamData(param);

    int numSubTracks = pTrack->GetSubTrackCount();

    pTrack->SetSubTrackName(0, "X");
    pTrack->SetSubTrackName(1, "Y");
    if (numElements > 2)
    {
        pTrack->SetSubTrackName(2, "Z");
        if (numElements > 3)
        {
            pTrack->SetSubTrackName(3, "W");
        }
    }

    for (int i = 0; i < numElements; ++i)
    {
        IUiAnimTrack* pSubTrack = pTrack->GetSubTrack(i);
        pSubTrack->SetParameterType(eUiAnimParamType_Float);    // subtracks are not actual component properties
    }

    return pTrack;
}
