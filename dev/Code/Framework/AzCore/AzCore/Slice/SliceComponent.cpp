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
#include <AzCore/Slice/SliceComponent.h>
#include <AzCore/Slice/SliceMetadataInfoComponent.h>
#include <AzCore/Slice/SliceBus.h>

#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Debug/Profiler.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Component/TransformBus.h>

///////////////////////////////////////////////////////
// Temp while the asset system is done
#include <AzCore/Serialization/Utils.h>
///////////////////////////////////////////////////////
namespace AZ
{
    namespace Converters
    {
        // SliceReference Version 1 -> 2
        // SliceReference::m_instances converted from AZStd::list<SliceInstance> to AZStd::unordered_set<SliceInstance>.
        bool SliceReferenceVersionConverter(SerializeContext& context, SerializeContext::DataElementNode& classElement)
        {
            if (classElement.GetVersion() < 2)
            {
                const int instancesIndex = classElement.FindElement(AZ_CRC("Instances", 0x7a270069));
                if (instancesIndex > -1)
                {
                    auto& instancesElement = classElement.GetSubElement(instancesIndex);

                    // Extract sub-elements, which we can just re-add under the set.
                    AZStd::vector<SerializeContext::DataElementNode> subElements;
                    subElements.reserve(instancesElement.GetNumSubElements());
                    for (int i = 0, n = instancesElement.GetNumSubElements(); i < n; ++i)
                    {
                        subElements.push_back(instancesElement.GetSubElement(i));
                    }

                    // Convert to unordered_set.
                    using SetType = AZStd::unordered_set<SliceComponent::SliceInstance>;
                    if (instancesElement.Convert<SetType>(context))
                    {
                        for (const SerializeContext::DataElementNode& subElement : subElements)
                        {
                            instancesElement.AddElement(subElement);
                        }
                    }

                    return true;
                }

                return false;
            }

            return true;
        }

    } // namespace Converters

    // storage for the static member for cyclic instantiation checking.
    // note that this vector is only used during slice instantiation and is set to capacity 0 when it empties.
    SliceComponent::AssetIdVector SliceComponent::m_instantiateCycleChecker; // dependency checker.
    // this is cleared and set to 0 capacity after each use.

    //////////////////////////////////////////////////////////////////////////
    // SliceInstanceAddress
    //////////////////////////////////////////////////////////////////////////
    SliceComponent::SliceInstanceAddress::SliceInstanceAddress()
        : first(m_reference)
        , second(m_instance)
    {}

    SliceComponent::SliceInstanceAddress::SliceInstanceAddress(SliceReference* reference, SliceInstance* instance)
        : m_reference(reference)
        , m_instance(instance)
        , first(m_reference)
        , second(m_instance)
    {}

    SliceComponent::SliceInstanceAddress::SliceInstanceAddress(const SliceComponent::SliceInstanceAddress& RHS)
        : m_reference(RHS.m_reference)
        , m_instance(RHS.m_instance)
        , first(m_reference)
        , second(m_instance)
    {}

    SliceComponent::SliceInstanceAddress::SliceInstanceAddress(const SliceComponent::SliceInstanceAddress&& RHS)
        : m_reference(AZStd::move(RHS.m_reference))
        , m_instance(AZStd::move(RHS.m_instance))
        , first(m_reference)
        , second(m_instance)
    {}

    bool SliceComponent::SliceInstanceAddress::operator==(const SliceComponent::SliceInstanceAddress& rhs) const
    {
        return m_reference == rhs.m_reference && m_instance == rhs.m_instance;
    }

    bool SliceComponent::SliceInstanceAddress::operator!=(const SliceComponent::SliceInstanceAddress& rhs) const { return m_reference != rhs.m_reference || m_instance != rhs.m_instance; }

    SliceComponent::SliceInstanceAddress& SliceComponent::SliceInstanceAddress::operator=(const SliceComponent::SliceInstanceAddress& RHS)
    {
        m_reference = RHS.m_reference;
        m_instance = RHS.m_instance;
        first = m_reference;
        second = m_instance;
        return *this;
    }

    bool SliceComponent::SliceInstanceAddress::IsValid() const
    {
        return m_reference && m_instance;
    }

    SliceComponent::SliceInstanceAddress::operator bool() const
    {
        return IsValid();
    }

    const SliceComponent::SliceReference* SliceComponent::SliceInstanceAddress::GetReference() const
    {
        return m_reference;
    }

    SliceComponent::SliceReference* SliceComponent::SliceInstanceAddress::GetReference()
    {
        return m_reference;
    }

    void SliceComponent::SliceInstanceAddress::SetReference(SliceComponent::SliceReference* reference)
    {
        m_reference = reference;
    }

    const SliceComponent::SliceInstance* SliceComponent::SliceInstanceAddress::GetInstance() const
    {
        return m_instance;
    }

    SliceComponent::SliceInstance* SliceComponent::SliceInstanceAddress::GetInstance()
    {
        return m_instance;
    }

    void SliceComponent::SliceInstanceAddress::SetInstance(SliceComponent::SliceInstance* instance)
    {
        m_instance = instance;
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::Reflect(ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<SerializeContext*>(context))
        {
            serializeContext->Class<DataFlagsPerEntity>()
                ->Version(1)
                ->Field("EntityToDataFlags", &DataFlagsPerEntity::m_entityToDataFlags)
                ;
        }
    }

    //=========================================================================
    SliceComponent::DataFlagsPerEntity::DataFlagsPerEntity(const IsValidEntityFunction& isValidEntityFunction)
        : m_isValidEntityFunction(isValidEntityFunction)
    {
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::SetIsValidEntityFunction(const IsValidEntityFunction& isValidEntityFunction)
    {
        m_isValidEntityFunction = isValidEntityFunction;
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::CopyDataFlagsFrom(const DataFlagsPerEntity& rhs)
    {
        m_entityToDataFlags = rhs.m_entityToDataFlags;
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::CopyDataFlagsFrom(DataFlagsPerEntity&& rhs)
    {
        m_entityToDataFlags = AZStd::move(rhs.m_entityToDataFlags);
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::ImportDataFlagsFrom(
        const DataFlagsPerEntity& from,
        const EntityIdToEntityIdMap* remapFromIdToId/*=nullptr*/,
        const DataFlagsTransformFunction& dataFlagsTransformFn/*=nullptr*/)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        for (const auto& entityIdFlagsMapPair : from.m_entityToDataFlags)
        {
            const EntityId& fromEntityId = entityIdFlagsMapPair.first;
            const DataPatch::FlagsMap& fromFlagsMap = entityIdFlagsMapPair.second;

            // remap EntityId if necessary
            EntityId toEntityId = fromEntityId;
            if (remapFromIdToId)
            {
                auto foundRemap = remapFromIdToId->find(fromEntityId);
                if (foundRemap == remapFromIdToId->end())
                {
                    // leave out entities that don't occur in the map
                    continue;
                }
                toEntityId = foundRemap->second;
            }

            // merge masked values with existing flags
            for (const auto& addressFlagPair : fromFlagsMap)
            {
                DataPatch::Flags fromFlags = addressFlagPair.second;

                // apply transform function if necessary
                if (dataFlagsTransformFn)
                {
                    fromFlags = dataFlagsTransformFn(fromFlags);
                }

                if (fromFlags != 0) // don't create empty entries
                {
                    m_entityToDataFlags[toEntityId][addressFlagPair.first] |= fromFlags;
                }
            }
        }
    }

    //=========================================================================
    DataPatch::FlagsMap SliceComponent::DataFlagsPerEntity::GetDataFlagsForPatching(const EntityIdToEntityIdMap* remapFromIdToId /*=nullptr*/) const
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // Collect together data flags from all entities
        DataPatch::FlagsMap dataFlagsForAllEntities;

        for (auto& entityIdDataFlagsPair : m_entityToDataFlags)
        {
            const AZ::EntityId& fromEntityId = entityIdDataFlagsPair.first;
            const DataPatch::FlagsMap& entityDataFlags = entityIdDataFlagsPair.second;

            AZ::EntityId toEntityId = fromEntityId;
            if (remapFromIdToId)
            {
                auto foundRemap = remapFromIdToId->find(fromEntityId);
                if (foundRemap == remapFromIdToId->end())
                {
                    // leave out entities that don't occur in the map
                    continue;
                }
                toEntityId = foundRemap->second;
            }

            // Make the addressing relative to InstantiatedContainer (entityDataFlags are relative to the individual entity)
            DataPatch::AddressType addressPrefix;
            addressPrefix.push_back(AZ_CRC("Entities", 0x50ec64e5));
            addressPrefix.push_back(static_cast<u64>(toEntityId));

            for (auto& addressFlagsPair : entityDataFlags)
            {
                const DataPatch::AddressType& originalAddress = addressFlagsPair.first;

                DataPatch::AddressType prefixedAddress;
                prefixedAddress.reserve(addressPrefix.size() + originalAddress.size());
                prefixedAddress.insert(prefixedAddress.end(), addressPrefix.begin(), addressPrefix.end());
                prefixedAddress.insert(prefixedAddress.end(), originalAddress.begin(), originalAddress.end());

                dataFlagsForAllEntities.emplace(AZStd::move(prefixedAddress), addressFlagsPair.second);
            }
        }

        return dataFlagsForAllEntities;
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::RemapEntityIds(const EntityIdToEntityIdMap& remapFromIdToId)
    {
        // move all data flags to this map, using remapped EntityIds
        AZStd::unordered_map<EntityId, DataPatch::FlagsMap> remappedEntityToDataFlags;

        for (auto& entityToDataFlagsPair : m_entityToDataFlags)
        {
            auto foundRemappedId = remapFromIdToId.find(entityToDataFlagsPair.first);
            if (foundRemappedId != remapFromIdToId.end())
            {
                remappedEntityToDataFlags[foundRemappedId->second] = AZStd::move(entityToDataFlagsPair.second);
            }
        }

        // replace current data with remapped data
        m_entityToDataFlags = AZStd::move(remappedEntityToDataFlags);
    }

    //=========================================================================
    const DataPatch::FlagsMap& SliceComponent::DataFlagsPerEntity::GetEntityDataFlags(EntityId entityId) const
    {
        auto foundDataFlags = m_entityToDataFlags.find(entityId);
        if (foundDataFlags != m_entityToDataFlags.end())
        {
            return foundDataFlags->second;
        }

        static const DataPatch::FlagsMap emptyFlagsMap;
        return emptyFlagsMap;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::SetEntityDataFlags(EntityId entityId, const DataPatch::FlagsMap& dataFlags)
    {
        if (IsValidEntity(entityId))
        {
            if (!dataFlags.empty())
            {
                m_entityToDataFlags[entityId] = dataFlags;
            }
            else
            {
                // If entity has no data flags, erase the entity's map entry.
                m_entityToDataFlags.erase(entityId);
            }
            return true;
        }
        return false;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::ClearEntityDataFlags(EntityId entityId)
    {
        if (IsValidEntity(entityId))
        {
            m_entityToDataFlags.erase(entityId);
            return true;
        }
        return false;
    }

    //=========================================================================
    DataPatch::Flags SliceComponent::DataFlagsPerEntity::GetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress) const
    {
        auto foundEntityDataFlags = m_entityToDataFlags.find(entityId);
        if (foundEntityDataFlags != m_entityToDataFlags.end())
        {
            const DataPatch::FlagsMap& entityDataFlags = foundEntityDataFlags->second;
            auto foundDataFlags = entityDataFlags.find(dataAddress);
            if (foundDataFlags != entityDataFlags.end())
            {
                return foundDataFlags->second;
            }
        }

        return 0;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::SetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress, DataPatch::Flags flags)
    {
        if (IsValidEntity(entityId))
        {
            if (flags != 0)
            {
                m_entityToDataFlags[entityId][dataAddress] = flags;
            }
            else
            {
                // If clearing the flags, erase the data address's map entry.
                // If the entity no longer has any data flags, erase the entity's map entry.
                auto foundEntityDataFlags = m_entityToDataFlags.find(entityId);
                if (foundEntityDataFlags != m_entityToDataFlags.end())
                {
                    DataPatch::FlagsMap& entityDataFlags = foundEntityDataFlags->second;
                    entityDataFlags.erase(dataAddress);
                    if (entityDataFlags.empty())
                    {
                        m_entityToDataFlags.erase(foundEntityDataFlags);
                    }
                }
            }

            return true;
        }
        return false;
    }

    //=========================================================================
    bool SliceComponent::DataFlagsPerEntity::IsValidEntity(EntityId entityId) const
    {
        if (m_isValidEntityFunction)
        {
            if (m_isValidEntityFunction(entityId))
            {
                return true;
            }
            return false;
        }
        return true; // if no validity function is set, always return true
    }

    //=========================================================================
    void SliceComponent::DataFlagsPerEntity::Cleanup(const EntityList& validEntities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        EntityIdSet validEntityIds;
        for (const Entity* entity : validEntities)
        {
            validEntityIds.insert(entity->GetId());
        }

        for (auto entityDataFlagIterator = m_entityToDataFlags.begin(); entityDataFlagIterator != m_entityToDataFlags.end(); )
        {
            EntityId entityId = entityDataFlagIterator->first;
            if (validEntityIds.find(entityId) != validEntityIds.end())
            {
                // TODO LY-52686: Prune flags if their address doesn't line up with anything in this entity.

                ++entityDataFlagIterator;
            }
            else
            {
                entityDataFlagIterator = m_entityToDataFlags.erase(entityDataFlagIterator);
            }
        }
    }

    SliceComponent::InstantiatedContainer::InstantiatedContainer(bool deleteEntitiesOnDestruction)
        : m_deleteEntitiesOnDestruction(deleteEntitiesOnDestruction)
    {
    }

    //=========================================================================
    // SliceComponent::InstantiatedContainer::~InstanceContainer
    //=========================================================================
    SliceComponent::InstantiatedContainer::~InstantiatedContainer()
    {
        if (m_deleteEntitiesOnDestruction)
        {
            DeleteEntities();
        }
    }

    //=========================================================================
    // SliceComponent::InstantiatedContainer::DeleteEntities
    //=========================================================================
    void SliceComponent::InstantiatedContainer::DeleteEntities()
    {
        for (Entity* entity : m_entities)
        {
            delete entity;
        }
        m_entities.clear();

        for (Entity* metaEntity : m_metadataEntities)
        {
            if (metaEntity->GetState() == AZ::Entity::ES_ACTIVE)
            {
                SliceInstanceNotificationBus::Broadcast(&SliceInstanceNotificationBus::Events::OnMetadataEntityDestroyed, metaEntity->GetId());
            }

            delete metaEntity;
        }
        m_metadataEntities.clear();
    }

    //=========================================================================
    // SliceComponent::InstantiatedContainer::ClearAndReleaseOwnership
    //=========================================================================
    void SliceComponent::InstantiatedContainer::ClearAndReleaseOwnership()
    {
        m_entities.clear();
        m_metadataEntities.clear();
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(const SliceInstanceId& id)
        : m_instantiated(nullptr)
        , m_instanceId(id)
        , m_metadataEntity(nullptr)
    {
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(SliceInstance&& rhs)
    {
        m_instantiated = rhs.m_instantiated;
        rhs.m_instantiated = nullptr;
        m_baseToNewEntityIdMap = AZStd::move(rhs.m_baseToNewEntityIdMap);
        m_entityIdToBaseCache = AZStd::move(rhs.m_entityIdToBaseCache);
        m_dataPatch = AZStd::move(rhs.m_dataPatch);
        m_dataFlags.CopyDataFlagsFrom(AZStd::move(rhs.m_dataFlags));
        m_instanceId = rhs.m_instanceId;
        rhs.m_instanceId = SliceInstanceId::CreateNull();
        m_metadataEntity = AZStd::move(rhs.m_metadataEntity);
    }

    //=========================================================================
    // SliceComponent::SliceInstance::SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::SliceInstance(const SliceInstance& rhs)
    {
        m_instantiated = rhs.m_instantiated;
        m_baseToNewEntityIdMap = rhs.m_baseToNewEntityIdMap;
        m_entityIdToBaseCache = rhs.m_entityIdToBaseCache;
        m_dataPatch = rhs.m_dataPatch;
        m_dataFlags.CopyDataFlagsFrom(rhs.m_dataFlags);
        m_instanceId = rhs.m_instanceId;
        m_metadataEntity = rhs.m_metadataEntity;
    }

    //=========================================================================
    // SliceComponent::SliceInstance::BuildReverseLookUp
    //=========================================================================
    void SliceComponent::SliceInstance::BuildReverseLookUp() const
    {
        m_entityIdToBaseCache.clear();
        for (const auto& entityIdPair : m_baseToNewEntityIdMap)
        {
            m_entityIdToBaseCache.insert(AZStd::make_pair(entityIdPair.second, entityIdPair.first));
        }
    }

    //=========================================================================
    // SliceComponent::SliceInstance::~SliceInstance
    //=========================================================================
    SliceComponent::SliceInstance::~SliceInstance()
    {
        delete m_instantiated;
    }

    //=========================================================================
    // SliceComponent::SliceInstance::IsValidEntity
    //=========================================================================
    bool SliceComponent::SliceInstance::IsValidEntity(EntityId entityId) const
    {
        const EntityIdToEntityIdMap& entityIdToBaseMap = GetEntityIdToBaseMap();
        return entityIdToBaseMap.find(entityId) != entityIdToBaseMap.end();
    }

    //=========================================================================
    // Returns the metadata entity for this instance.
    //=========================================================================
    Entity* SliceComponent::SliceInstance::GetMetadataEntity() const
    {
        return m_metadataEntity;
    }

    //=========================================================================
    // SliceComponent::SliceReference::SliceReference
    //=========================================================================
    SliceComponent::SliceReference::SliceReference()
        : m_isInstantiated(false)
        , m_component(nullptr)
    {
    }
    
    //=========================================================================
    // SliceComponent::SliceReference::CreateEmptyInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CreateEmptyInstance(const SliceInstanceId& instanceId)
    {
        SliceInstance* instance = &(*m_instances.emplace(instanceId).first);
        return instance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::CreateInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CreateInstance(const AZ::IdUtils::Remapper<AZ::EntityId>::IdMapper& customMapper, 
        SliceInstanceId sliceInstanceId)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // create an empty instance (just copy of the exiting data)
        SliceInstance* instance = CreateEmptyInstance(sliceInstanceId);

        if (m_isInstantiated)
        {
            AZ_Assert(m_asset.IsReady(), "If we an in instantiated state all dependent asset should be ready!");
            SliceComponent* dependentSlice = m_asset.Get()->GetComponent();

            // Now we can build a temporary structure of all the entities we're going to clone
            InstantiatedContainer sourceObjects(false);
            dependentSlice->GetEntities(sourceObjects.m_entities);
            dependentSlice->GetAllMetadataEntities(sourceObjects.m_metadataEntities);

            instance->m_instantiated = dependentSlice->GetSerializeContext()->CloneObject(&sourceObjects);

            AZ_Assert(!sourceObjects.m_metadataEntities.empty(), "Metadata Entities must exist at slice instantiation time");

            AZ::IdUtils::Remapper<EntityId>::RemapIds(
                instance->m_instantiated,
                [&](const EntityId& originalId, bool isEntityId, const AZStd::function<EntityId()>& idGenerator) -> EntityId
                {
                    EntityId newId = customMapper ? customMapper(originalId, isEntityId, idGenerator) : idGenerator();
                    auto insertIt = instance->m_baseToNewEntityIdMap.insert(AZStd::make_pair(originalId, newId));
                    return insertIt.first->second;

                }, dependentSlice->GetSerializeContext(), true);

            AZ::IdUtils::Remapper<EntityId>::RemapIds(
                instance->m_instantiated,
                [instance](const EntityId& originalId, bool /*isEntityId*/, const AZStd::function<EntityId()>& /*idGenerator*/) -> EntityId
                {
                    auto findIt = instance->m_baseToNewEntityIdMap.find(originalId);
                    if (findIt == instance->m_baseToNewEntityIdMap.end())
                    {
                        return originalId; // Referenced EntityId is not part of the slice, so keep the same id reference.
                    }
                    else
                    {
                        return findIt->second; // return the remapped id
                    }
                }, dependentSlice->GetSerializeContext(), false);

            AZ_Assert(m_component, "We need a valid component to use this operation!");

            if (!m_component->m_entityInfoMap.empty())
            {
                AddInstanceToEntityInfoMap(*instance);
            }

            FixUpMetadataEntityForSliceInstance(instance);
        }

        return instance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::CloneInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::CloneInstance(SliceComponent::SliceInstance* instance, 
                                                                                 SliceComponent::EntityIdToEntityIdMap& sourceToCloneEntityIdMap)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // check if source instance belongs to this slice reference
        auto findIt = AZStd::find_if(m_instances.begin(), m_instances.end(), [instance](const SliceInstance& element) -> bool { return &element == instance; });
        if (findIt == m_instances.end())
        {
            AZ_Error("Slice", false, "SliceInstance %p doesn't belong to this SliceReference %p!", instance, this);
            return nullptr;
        }

        SliceInstance* newInstance = CreateEmptyInstance();

        if (m_isInstantiated)
        {
            SerializeContext* serializeContext = m_asset.Get()->GetComponent()->GetSerializeContext();

            // clone the entities
            newInstance->m_instantiated = IdUtils::Remapper<EntityId>::CloneObjectAndGenerateNewIdsAndFixRefs(instance->m_instantiated, sourceToCloneEntityIdMap, serializeContext);

            const EntityIdToEntityIdMap& instanceToBaseSliceEntityIdMap = instance->GetEntityIdToBaseMap();
            for (AZStd::pair<AZ::EntityId, AZ::EntityId>& sourceIdToCloneId : sourceToCloneEntityIdMap)
            {
                EntityId sourceId = sourceIdToCloneId.first;
                EntityId cloneId = sourceIdToCloneId.second;

                auto instanceIdToBaseId = instanceToBaseSliceEntityIdMap.find(sourceId);
                if (instanceIdToBaseId == instanceToBaseSliceEntityIdMap.end())
                {
                    AZ_Assert(false, "An entity cloned (id: %s) couldn't be found in the source slice instance!", sourceId.ToString().c_str());
                }
                EntityId baseId = instanceIdToBaseId->second;

                newInstance->m_baseToNewEntityIdMap.insert(AZStd::make_pair(baseId, cloneId));
                newInstance->m_entityIdToBaseCache.insert(AZStd::make_pair(cloneId, baseId));

                newInstance->m_dataFlags.SetEntityDataFlags(cloneId, instance->m_dataFlags.GetEntityDataFlags(sourceId));
            }

            if (!m_component->m_entityInfoMap.empty())
            {
                AddInstanceToEntityInfoMap(*newInstance);
            }
        }
        else
        {
            // clone data patch
            AZ_Assert(false, "todo regenerate the entity map id and copy data flags");
            newInstance->m_dataPatch = instance->m_dataPatch;
        }

        return newInstance;
    }

    //=========================================================================
    // SliceComponent::SliceReference::FindInstance
    //=========================================================================
    SliceComponent::SliceInstance* SliceComponent::SliceReference::FindInstance(const SliceInstanceId& instanceId)
    {
        auto iter = m_instances.find_as(instanceId, AZStd::hash<SliceInstanceId>(), 
            [](const SliceInstanceId& id, const SliceInstance& instance)
            {
                return (id == instance.GetId());
            }
        );

        if (iter != m_instances.end())
        {
            return &(*iter);
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstance
    //=========================================================================
    bool SliceComponent::SliceReference::RemoveInstance(SliceComponent::SliceInstance* instance)
    {
        AZ_Assert(instance, "Invalid instance provided to SliceReference::RemoveInstance");
        auto iter = m_instances.find(*instance);
        if (iter != m_instances.end())
        {
            RemoveInstanceFromEntityInfoMap(*instance);
            m_instances.erase(iter);
            return true;
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstance
    //=========================================================================
    bool SliceComponent::SliceReference::RemoveEntity(EntityId entityId, bool isDeleteEntity, SliceInstance* instance)
    {
        if (!instance)
        {
            instance = m_component->FindSlice(entityId).GetInstance();

            if (!instance)
            {
                return false; // can't find an instance the owns this entity
            }
        }

        auto entityIt = AZStd::find_if(instance->m_instantiated->m_entities.begin(), instance->m_instantiated->m_entities.end(), [entityId](Entity* entity) -> bool { return entity->GetId() == entityId; });
        if (entityIt != instance->m_instantiated->m_entities.end())
        {
            if (isDeleteEntity)
            {
                delete *entityIt;
            }
            instance->m_instantiated->m_entities.erase(entityIt);
            if (instance->m_entityIdToBaseCache.empty())
            {
                instance->BuildReverseLookUp();
            }

            instance->m_dataFlags.ClearEntityDataFlags(entityId);

            auto entityToBaseIt = instance->m_entityIdToBaseCache.find(entityId);
            bool entityToBaseExists = entityToBaseIt != instance->m_entityIdToBaseCache.end();
            AZ_Assert(entityToBaseExists, "Reverse lookup cache is inconsistent, please check it's logic!");

            if (entityToBaseExists)
            {
                instance->m_baseToNewEntityIdMap.erase(entityToBaseIt->second);
                instance->m_entityIdToBaseCache.erase(entityToBaseIt);
                return true;
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::GetInstances
    //=========================================================================
    const SliceComponent::SliceReference::SliceInstances& SliceComponent::SliceReference::GetInstances() const
    {
        return m_instances;
    }

    //=========================================================================
    // SliceComponent::SliceReference::IsInstantiated
    //=========================================================================
    bool SliceComponent::SliceReference::IsInstantiated() const
    {
        return m_isInstantiated;
    }

    //=========================================================================
    // SliceComponent::SliceReference::Instantiate
    //=========================================================================
    bool SliceComponent::SliceReference::Instantiate(const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (m_isInstantiated)
        {
            return true;
        }

        if (!m_asset.IsReady())
        {
            #if defined(AZ_ENABLE_TRACING)
            Data::Asset<SliceAsset> owningAsset = Data::AssetManager::Instance().FindAsset(m_component->m_myAsset->GetId());
            AZ_Error("Slice", false, 
                "Instantiation of %d slice instance(s) of asset %s failed - asset not ready or not found during instantiation of owning slice %s!" 
                "Saving owning slice will lose data for these instances.",
                m_instances.size(),
                m_asset.ToString<AZStd::string>().c_str(),
                owningAsset.ToString<AZStd::string>().c_str());
            #endif // AZ_ENABLE_TRACING
            return false;
        }

        SliceComponent* dependentSlice = m_asset.Get()->GetComponent();
        InstantiateResult instantiationResult = dependentSlice->Instantiate();
        if (instantiationResult != InstantiateResult::Success)
        {
            #if defined(AZ_ENABLE_TRACING)
            Data::Asset<SliceAsset> owningAsset = Data::AssetManager::Instance().FindAsset(m_component->m_myAsset->GetId());
            AZ_Error("Slice", false, "Instantiation of %d slice instance(s) of asset %s failed - asset ready and available, but unable to instantiate "
                "for owning slice %s! Saving owning slice will lose data for these instances.",
                m_instances.size(),
                m_asset.ToString<AZStd::string>().c_str(),
                owningAsset.ToString<AZStd::string>().c_str());
            #endif // AZ_ENABLE_TRACING
            // Cannot partially instantiate when a cyclical dependency exists.
            if (instantiationResult == InstantiateResult::CyclicalDependency)
            {
                return false;
            }
        }

        m_isInstantiated = true;

        for (SliceInstance& instance : m_instances)
        {
            InstantiateInstance(instance, filterDesc);
        }
        return true;
    }

    //=========================================================================
    // SliceComponent::SliceReference::UnInstantiate
    //=========================================================================
    void SliceComponent::SliceReference::UnInstantiate()
    {
        if (m_isInstantiated)
        {
            m_isInstantiated = false;

            for (SliceInstance& instance : m_instances)
            {
                delete instance.m_instantiated;
                instance.m_instantiated = nullptr;
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::InstantiateInstance
    //=========================================================================
    void SliceComponent::SliceReference::InstantiateInstance(SliceInstance& instance, const AZ::ObjectStream::FilterDescriptor& filterDesc)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // Could have set this during SliceInstance() constructor, but we wait until instantiation since it involves allocation.
        instance.m_dataFlags.SetIsValidEntityFunction([&instance](EntityId entityId) { return instance.IsValidEntity(entityId); });

        InstantiatedContainer sourceObjects(false);
        SliceComponent* dependentSlice = m_asset.Get()->GetComponent();

        // Build a temporary collection of all the entities need to clone to create our instance
        const DataPatch& dataPatch = instance.m_dataPatch;
        EntityIdToEntityIdMap& entityIdMap = instance.m_baseToNewEntityIdMap;
        dependentSlice->GetEntities(sourceObjects.m_entities);

        dependentSlice->GetAllMetadataEntities(sourceObjects.m_metadataEntities);

        // We need to be able to quickly determine if EntityIDs belong to metadata entities
        EntityIdSet sourceMetadataEntityIds;
        dependentSlice->GetMetadataEntityIds(sourceMetadataEntityIds);

        // An empty map indicates its a fresh instance (i.e. has never be instantiated and then serialized).
        if (entityIdMap.empty())
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponent::SliceReference::InstantiateInstance:FreshInstanceClone");

            // Generate new Ids and populate the map.
            AZ_Assert(!dataPatch.IsValid(), "Data patch is valid for slice instance, but entity Id map is not!");
            instance.m_instantiated = IdUtils::Remapper<EntityId>::CloneObjectAndGenerateNewIdsAndFixRefs(&sourceObjects, entityIdMap, dependentSlice->GetSerializeContext());
        }
        else
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponent::SliceReference::InstantiateInstance:CloneAndApplyDataPatches");

            // Clone entities while applying any data patches.
            AZ_Assert(dataPatch.IsValid(), "Data patch is not valid for existing slice instance!");

            // Get data flags from source slice and slice instance
            DataPatch::FlagsMap sourceDataFlags = dependentSlice->GetDataFlagsForInstances().GetDataFlagsForPatching();
            DataPatch::FlagsMap targetDataFlags = instance.GetDataFlags().GetDataFlagsForPatching(&instance.GetEntityIdToBaseMap());

            instance.m_instantiated = dataPatch.Apply(&sourceObjects, dependentSlice->GetSerializeContext(), filterDesc, sourceDataFlags, targetDataFlags);

            // Remap Ids & references.
            IdUtils::Remapper<EntityId>::ReplaceIdsAndIdRefs(instance.m_instantiated, [&entityIdMap](const EntityId& sourceId, bool isEntityId, const AZStd::function<EntityId()>& idGenerator) -> EntityId
            {
                auto findIt = entityIdMap.find(sourceId);
                if (findIt != entityIdMap.end() && findIt->second.IsValid())
                {
                    return findIt->second;
                }
                else
                {
                    if (isEntityId)
                    {
                        const EntityId id = idGenerator();
                        entityIdMap[sourceId] = id;
                        return id;
                    }

                    return sourceId;
                }
            }, dependentSlice->GetSerializeContext());

            // Prune any entities in from our map that are no longer present in the dependent slice.
            if (entityIdMap.size() != ( sourceObjects.m_entities.size() + sourceObjects.m_metadataEntities.size()))
            {
                const SliceComponent::EntityInfoMap& dependentInfoMap = dependentSlice->GetEntityInfoMap();
                for (auto entityMapIter = entityIdMap.begin(); entityMapIter != entityIdMap.end();)
                {
                    // Skip metadata entities
                    if (sourceMetadataEntityIds.find(entityMapIter->first) != sourceMetadataEntityIds.end())
                    {
                        ++entityMapIter;
                        continue;
                    }

                    auto findInDependentIt = dependentInfoMap.find(entityMapIter->first);
                    if (findInDependentIt == dependentInfoMap.end())
                    {
                        entityMapIter = entityIdMap.erase(entityMapIter);
                        continue;
                    }

                    ++entityMapIter;
                }
            }
        }

        // Find the instanced version of the source metadata entity from the asset associated with this reference
        // and store it in the instance for quick lookups later
        auto metadataIdMapIter = instance.m_baseToNewEntityIdMap.find(dependentSlice->GetMetadataEntity()->GetId());
        AZ_Assert(metadataIdMapIter != instance.m_baseToNewEntityIdMap.end(), "Dependent Metadata Entity ID was not remapped properly.");
        AZ::EntityId instancedMetadataEntityId = metadataIdMapIter->second;

        for (AZ::Entity* entity : instance.m_instantiated->m_metadataEntities)
        {
            if (instancedMetadataEntityId == entity->GetId())
            {
                instance.m_metadataEntity = entity;
                break;
            }
        }

        AZ_Assert(instance.m_metadataEntity, "Instance requires an attached metadata entity!");

        // Ensure reverse lookup is cleared (recomputed on access).
        instance.m_entityIdToBaseCache.clear();

        // Broadcast OnSliceEntitiesLoaded for freshly instantiated entities.
        if (!instance.m_instantiated->m_entities.empty())
        {
            AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponent::SliceReference::InstantiateInstance:OnSliceEntitiesLoaded");
            SliceAssetSerializationNotificationBus::Broadcast(&SliceAssetSerializationNotificationBus::Events::OnSliceEntitiesLoaded, instance.m_instantiated->m_entities);
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::AddInstanceToEntityInfoMap
    //=========================================================================
    void SliceComponent::SliceReference::AddInstanceToEntityInfoMap(SliceInstance& instance)
    {
        AZ_Assert(m_component, "You need to have a valid component set to update the global entityInfoMap!");
        if (instance.m_instantiated)
        {
            auto& entityInfoMap = m_component->m_entityInfoMap;
            for (Entity* entity : instance.m_instantiated->m_entities)
            {
                entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress(this, &instance))));
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::RemoveInstanceFromEntityInfoMap
    //=========================================================================
    void SliceComponent::SliceReference::RemoveInstanceFromEntityInfoMap(SliceInstance& instance)
    {
        AZ_Assert(m_component, "You need to have a valid component set to update the global entityInfoMap!");
        if (!m_component->m_entityInfoMap.empty() && instance.m_instantiated)
        {
            for (Entity* entity : instance.m_instantiated->m_entities)
            {
                m_component->m_entityInfoMap.erase(entity->GetId());
            }
        }
    }

    //=========================================================================
    // SliceComponent::SliceReference::GetInstanceEntityAncestry
    //=========================================================================
    bool SliceComponent::SliceReference::GetInstanceEntityAncestry(const EntityId& instanceEntityId, EntityAncestorList& ancestors, u32 maxLevels) const
    {
        maxLevels = AZStd::GetMax(maxLevels, static_cast<u32>(1));

        // End recursion when we've reached the max level of ancestors requested
        if (ancestors.size() == maxLevels)
        {
            return true;
        }

        // Locate the instance containing the input entity, which should be a live instanced entity.
        for (const SliceInstance& instance : m_instances)
        {
            // Given the instance's entity Id, resolve the Id of the source entity in the asset.
            auto foundIt = instance.GetEntityIdToBaseMap().find(instanceEntityId);
            if (foundIt != instance.GetEntityIdToBaseMap().end())
            {
                const EntityId assetEntityId = foundIt->second;

                // Ancestor is assetEntityId in this instance's asset.
                const EntityInfoMap& assetEntityInfoMap = m_asset.Get()->GetComponent()->GetEntityInfoMap();
                auto entityInfoIt = assetEntityInfoMap.find(assetEntityId);
                if (entityInfoIt != assetEntityInfoMap.end())
                {
                    ancestors.emplace_back(entityInfoIt->second.m_entity, SliceInstanceAddress(const_cast<SliceReference*>(this), const_cast<SliceInstance*>(&instance)));
                    if (entityInfoIt->second.m_sliceAddress.IsValid())
                    {
                        return entityInfoIt->second.m_sliceAddress.GetReference()->GetInstanceEntityAncestry(assetEntityId, ancestors, maxLevels);
                    }
                }
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::SliceReference::ComputeDataPatch
    //=========================================================================
    void SliceComponent::SliceReference::ComputeDataPatch()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        // Get source entities from the base asset (instantiate if needed)
        InstantiatedContainer source(false);
        m_asset.Get()->GetComponent()->GetEntities(source.m_entities);
        m_asset.Get()->GetComponent()->GetAllMetadataEntities(source.m_metadataEntities);

        SerializeContext* serializeContext = m_asset.Get()->GetComponent()->GetSerializeContext();

        // Compute the delta/changes for each instance
        for (SliceInstance& instance : m_instances)
        {
            // remap entity ids to the "original"
            const EntityIdToEntityIdMap& reverseLookUp = instance.GetEntityIdToBaseMap();
            IdUtils::Remapper<EntityId>::ReplaceIdsAndIdRefs(instance.m_instantiated, [&reverseLookUp](const EntityId& sourceId, bool /*isEntityId*/, const AZStd::function<EntityId()>& /*idGenerator*/) -> EntityId
            {
                auto findIt = reverseLookUp.find(sourceId);
                if (findIt != reverseLookUp.end())
                {
                    return findIt->second;
                }
                else
                {
                    return sourceId;
                }
            }, serializeContext);

            // Get data flags from source slice and slice instance
            DataPatch::FlagsMap sourceDataFlags = m_asset.Get()->GetComponent()->GetDataFlagsForInstances().GetDataFlagsForPatching();
            DataPatch::FlagsMap targetDataFlags = instance.GetDataFlags().GetDataFlagsForPatching(&instance.GetEntityIdToBaseMap());

            // compute the delta (what we changed from the base slice)
            instance.m_dataPatch.Create(&source, instance.m_instantiated, sourceDataFlags, targetDataFlags, serializeContext);

            // remap entity ids back to the "instance onces"
            IdUtils::Remapper<EntityId>::ReplaceIdsAndIdRefs(instance.m_instantiated, [&instance](const EntityId& sourceId, bool /*isEntityId*/, const AZStd::function<EntityId()>& /*idGenerator*/) -> EntityId
            {
                auto findIt = instance.m_baseToNewEntityIdMap.find(sourceId);
                if (findIt != instance.m_baseToNewEntityIdMap.end())
                {
                    return findIt->second;
                }
                else
                {
                    return sourceId;
                }
            }, serializeContext);

            // clean up orphaned data flags (ex: for entities that no longer exist).
            instance.m_dataFlags.Cleanup(instance.m_instantiated->m_entities);
        }
    }

    void SliceComponent::SliceReference::FixUpMetadataEntityForSliceInstance(SliceInstance* sliceInstance)
    {
        // Let the engine know about the newly created metadata entities.
        SliceComponent* assetSlice = m_asset.Get()->GetComponent();
        AZ::EntityId instancedMetadataEntityId = sliceInstance->m_baseToNewEntityIdMap.find(assetSlice->GetMetadataEntity()->GetId())->second;
        AZ_Assert(instancedMetadataEntityId.IsValid(), "Must have a valid entity ID.");
        for (AZ::Entity* instantiatedMetadataEntity : sliceInstance->m_instantiated->m_metadataEntities)
        {
            // While we're touching each one, find the instantiated entity associated with this instance and store it
            if (instancedMetadataEntityId == instantiatedMetadataEntity->GetId())
            {
                AZ_Assert(sliceInstance->m_metadataEntity == nullptr, "Multiple metadata entities associated with this instance found.");
                sliceInstance->m_metadataEntity = instantiatedMetadataEntity;
            }

            SliceInstanceNotificationBus::Broadcast(&SliceInstanceNotificationBus::Events::OnMetadataEntityCreated, SliceInstanceAddress(this, sliceInstance), *instantiatedMetadataEntity);
        }

        // Make sure we found the metadata entity associated with this instance
        AZ_Assert(sliceInstance->m_metadataEntity, "Instance requires an attached metadata entity!");
    }

    //=========================================================================
    // SliceComponent
    //=========================================================================
    SliceComponent::SliceComponent()
        : m_myAsset(nullptr)
        , m_serializeContext(nullptr)
        , m_hasGeneratedCachedDataFlags(false)
        , m_slicesAreInstantiated(false)
        , m_allowPartialInstantiation(true)
        , m_isDynamic(false)
        , m_filterFlags(0)
    {
    }

    //=========================================================================
    // ~SliceComponent
    //=========================================================================
    SliceComponent::~SliceComponent()
    {
        for (Entity* entity : m_entities)
        {
            delete entity;
        }

        if (m_metadataEntity.GetState() == AZ::Entity::ES_ACTIVE)
        {
            SliceInstanceNotificationBus::Broadcast(&SliceInstanceNotificationBus::Events::OnMetadataEntityDestroyed, m_metadataEntity.GetId());
        }
    }

    //=========================================================================
    // SliceComponent::GetNewEntities
    //=========================================================================
    const SliceComponent::EntityList& SliceComponent::GetNewEntities() const
    {
        return m_entities;
    }

    //=========================================================================
    // SliceComponent::GetEntities
    //=========================================================================
    bool SliceComponent::GetEntities(EntityList& entities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        bool result = true;

        // make sure we have all entities instantiated
        if (Instantiate() != InstantiateResult::Success)
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                if (instance.m_instantiated)
                {
                    entities.insert(entities.end(), instance.m_instantiated->m_entities.begin(), instance.m_instantiated->m_entities.end());
                }
            }
        }

        // add new entities (belong to the current slice)
        entities.insert(entities.end(), m_entities.begin(), m_entities.end());

        return result;
    }

    //=========================================================================
    // SliceComponent::GetEntityIds
    //=========================================================================
    bool SliceComponent::GetEntityIds(EntityIdSet& entities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        bool result = true;

        // make sure we have all entities instantiated
        if (Instantiate() != InstantiateResult::Success)
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                if (instance.m_instantiated)
                {
                    for (const AZ::Entity* entity : instance.m_instantiated->m_entities)
                    {
                        entities.insert(entity->GetId());
                    }
                }
            }
        }

        // add new entities (belong to the current slice)
        for (const AZ::Entity* entity : m_entities)
        {
            entities.insert(entity->GetId());
        }

        return result;
    }

    //=========================================================================
    size_t SliceComponent::GetInstantiatedEntityCount() const
    {
        if (!m_slicesAreInstantiated)
        {
            return 0;
        }

        return const_cast<SliceComponent*>(this)->GetEntityInfoMap().size();
    }
    //=========================================================================
    // SliceComponent::GetMetadataEntityIds
    //=========================================================================
    bool SliceComponent::GetMetadataEntityIds(EntityIdSet& metadataEntities)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        bool result = true;

        // make sure we have all entities instantiated
        if (Instantiate() != InstantiateResult::Success)
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                if (instance.m_instantiated)
                {
                    for (const AZ::Entity* entity : instance.m_instantiated->m_metadataEntities)
                    {
                        metadataEntities.insert(entity->GetId());
                    }
                }
            }
        }

        metadataEntities.insert(m_metadataEntity.GetId());

        return result;
    }

    //=========================================================================
    // SliceComponent::GetSlices
    //=========================================================================
    const SliceComponent::SliceList& SliceComponent::GetSlices() const
    {
        return m_slices;
    }

    const SliceComponent::SliceList& SliceComponent::GetInvalidSlices() const
    {
        return m_invalidSlices;
    }

    //=========================================================================
    // SliceComponent::GetSlice
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::GetSlice(const Data::Asset<SliceAsset>& sliceAsset)
    {
        return GetSlice(sliceAsset.GetId());
    }

    //=========================================================================
    // SliceComponent::GetSlice
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::GetSlice(const Data::AssetId& assetId)
    {
        for (SliceReference& slice : m_slices)
        {
            if (slice.m_asset.GetId() == assetId)
            {
                return &slice;
            }
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::Instantiate
    //=========================================================================
    SliceComponent::InstantiateResult SliceComponent::Instantiate()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);
        AZStd::unique_lock<AZStd::recursive_mutex> lock(m_instantiateMutex);

        if (m_slicesAreInstantiated)
        {
            return InstantiateResult::Success;
        }

        // Could have set this during constructor, but we wait until Instantiate() since it involves allocation.
        m_dataFlagsForNewEntities.SetIsValidEntityFunction([this](EntityId entityId) { return IsNewEntity(entityId); });

        InstantiateResult result(InstantiateResult::Success);

        if (m_myAsset)
        {
            PushInstantiateCycle(m_myAsset->GetId());
        }
        
        for (SliceReference& slice : m_slices)
        {
            slice.m_component = this;
            AZ::Data::AssetId sliceAssetId = slice.m_asset.GetId();
            
            if (CheckContainsInstantiateCycle(sliceAssetId))
            {
                result = InstantiateResult::CyclicalDependency;
            }
            else
            {
                if (!slice.Instantiate(AZ::ObjectStream::FilterDescriptor(m_assetLoadFilterCB, m_filterFlags)))
                {
                    result = InstantiateResult::MissingDependency;
                }
            }
        }

        if (m_myAsset)
        {
            PopInstantiateCycle(m_myAsset->GetId());
        }

        m_slicesAreInstantiated = result == InstantiateResult::Success;

        if (result != InstantiateResult::Success)
        {
            if (m_allowPartialInstantiation)
            {
                // Strip any references that failed to instantiate.
                for (auto iter = m_slices.begin(); iter != m_slices.end(); )
                {
                    if (!iter->IsInstantiated())
                    {
                        #if defined(AZ_ENABLE_TRACING)
                        Data::Asset<SliceAsset> thisAsset = Data::AssetManager::Instance().FindAsset(GetMyAsset()->GetId());
                        AZ_Warning("Slice", false, "Removing %d instances of slice asset %s from parent asset %s due to failed instantiation. " 
                            "Saving parent asset will result in loss of slice data.", 
                            iter->GetInstances().size(),
                            iter->GetSliceAsset().ToString<AZStd::string>().c_str(),
                            thisAsset.ToString<AZStd::string>().c_str());
                        #endif // AZ_ENABLE_TRACING
                        Data::AssetBus::MultiHandler::BusDisconnect(iter->GetSliceAsset().GetId());
                        // Track missing dependencies. Cyclical dependencies should not be tracked, to make sure
                        // the asset isn't kept loaded due to a cyclical asset reference.
                        if (result == InstantiateResult::MissingDependency)
                        {
                            m_invalidSlices.push_back(*iter);
                        }
                        iter = m_slices.erase(iter);
                    }
                    else
                    {
                        ++iter;
                        m_slicesAreInstantiated = true; // At least one reference was instantiated.
                    }
                }
            }
            else
            {
                // Completely roll back instantiation.
                for (SliceReference& slice : m_slices)
                {
                    if (slice.IsInstantiated())
                    {
                        slice.UnInstantiate();
                    }
                }
            }
        }

        if (m_slicesAreInstantiated)
        {
            if (m_myAsset)
            {
                AZStd::string sliceAssetPath;
                AZ::Data::AssetCatalogRequestBus::BroadcastResult(sliceAssetPath, &AZ::Data::AssetCatalogRequests::GetAssetPathById, m_myAsset->GetId());
                m_metadataEntity.SetName(sliceAssetPath);
            }
            else
            {
                m_metadataEntity.SetName("No Asset Association");
            }

            InitMetadata();
        }

        return result;
    }

    //=========================================================================
    // SliceComponent::IsInstantiated
    //=========================================================================
    bool SliceComponent::IsInstantiated() const
    {
        return m_slicesAreInstantiated;
    }

    //=========================================================================
    // SliceComponent::GenerateNewEntityIds
    //=========================================================================
    void SliceComponent::GenerateNewEntityIds(EntityIdToEntityIdMap* previousToNewIdMap)
    {
        InstantiatedContainer entityContainer(false);
        if (!GetEntities(entityContainer.m_entities))
        {
            return;
        }

        EntityIdToEntityIdMap entityIdMap;
        if (!previousToNewIdMap)
        {
            previousToNewIdMap = &entityIdMap;
        }
        AZ::EntityUtils::GenerateNewIdsAndFixRefs(&entityContainer, *previousToNewIdMap, m_serializeContext);

        m_dataFlagsForNewEntities.RemapEntityIds(*previousToNewIdMap);

        for (SliceReference& sliceReference : m_slices)
        {
            for (SliceInstance& instance : sliceReference.m_instances)
            {
                for (auto& entityIdPair : instance.m_baseToNewEntityIdMap)
                {
                    auto iter = previousToNewIdMap->find(entityIdPair.second);
                    if (iter != previousToNewIdMap->end())
                    {
                        entityIdPair.second = iter->second;
                    }
                }

                instance.m_entityIdToBaseCache.clear();
                instance.m_dataFlags.RemapEntityIds(*previousToNewIdMap);
            }
        }

        RebuildEntityInfoMapIfNecessary();
    }

    //=========================================================================
    // SliceComponent::AddSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::AddSlice(const Data::Asset<SliceAsset>& sliceAsset, const AZ::IdUtils::Remapper<AZ::EntityId>::IdMapper& customMapper, 
        SliceInstanceId sliceInstanceId)
    {
        SliceReference* slice = AddOrGetSliceReference(sliceAsset);
        return SliceInstanceAddress(slice, slice->CreateInstance(customMapper, sliceInstanceId));
    }

    //=========================================================================
    // SliceComponent::AddSliceInstance
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::AddSlice(SliceReference& sliceReference)
    {
        // Assert for input parameters
        // Assert that we don't already have a reference for this assetId
        AZ_Assert(!Data::AssetBus::MultiHandler::BusIsConnectedId(sliceReference.GetSliceAsset().GetId()), "We already have a slice reference to this asset");
        AZ_Assert(false == sliceReference.m_isInstantiated, "Slice reference is already instantiated.");

        Data::AssetBus::MultiHandler::BusConnect(sliceReference.GetSliceAsset().GetId());
        m_slices.emplace_back(AZStd::move(sliceReference));
        SliceReference* slice = &m_slices.back();
        slice->m_component = this;

        // check if we instantiated but the reference is not, instantiate
        // if the reference is and we are not, delete it
        if (m_slicesAreInstantiated)
        {
            slice->Instantiate(AZ::ObjectStream::FilterDescriptor(m_assetLoadFilterCB, m_filterFlags));
        }

        // Refresh entity map for newly-created instances.
        RebuildEntityInfoMapIfNecessary();

        return slice;
    }
    
    //=========================================================================
    // SliceComponent::GetEntityRestoreInfo
    //=========================================================================
    bool SliceComponent::GetEntityRestoreInfo(const EntityId entityId, EntityRestoreInfo& restoreInfo)
    {
        restoreInfo = EntityRestoreInfo();

        const EntityInfoMap& entityInfo = GetEntityInfoMap();
        auto entityInfoIter = entityInfo.find(entityId);
        if (entityInfoIter != entityInfo.end())
        {
            const SliceReference* reference = entityInfoIter->second.m_sliceAddress.GetReference();
            if (reference)
            {
                const SliceInstance* instance = entityInfoIter->second.m_sliceAddress.GetInstance();
                AZ_Assert(instance, "Entity %llu was found to belong to reference %s, but instance is invalid.", 
                          entityId, reference->GetSliceAsset().ToString<AZStd::string>().c_str());

                EntityAncestorList ancestors;
                reference->GetInstanceEntityAncestry(entityId, ancestors, 1);
                if (!ancestors.empty())
                {
                    restoreInfo = EntityRestoreInfo(reference->GetSliceAsset(), instance->GetId(), ancestors.front().m_entity->GetId(), instance->m_dataFlags.GetEntityDataFlags(entityId));
                    return true;
                }
                else
                {
                    AZ_Error("Slice", false, "Entity with id %llu was found, but has no valid ancestry.", entityId);
                }
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::RestoreEntity
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::RestoreEntity(Entity* entity, const EntityRestoreInfo& restoreInfo)
    {
        Data::Asset<SliceAsset> asset = Data::AssetManager::Instance().FindAsset(restoreInfo.m_assetId);

        if (!asset.IsReady())
        {
            AZ_Error("Slice", false, "Slice asset %s is not ready. Caller needs to ensure the asset is loaded.", restoreInfo.m_assetId.ToString<AZStd::string>().c_str());
            return SliceInstanceAddress();
        }

        if (!IsInstantiated())
        {
            AZ_Error("Slice", false, "Cannot add entities to existing instances if the slice hasn't yet been instantiated.");
            return SliceInstanceAddress();
        }
        
        SliceComponent* sourceSlice = asset.GetAs<SliceAsset>()->GetComponent();
        sourceSlice->Instantiate();
        auto& sourceEntityMap = sourceSlice->GetEntityInfoMap();
        if (sourceEntityMap.find(restoreInfo.m_ancestorId) == sourceEntityMap.end())
        {
            AZ_Error("Slice", false, "Ancestor Id of %llu is invalid. It must match an entity in source asset %s.", 
                     restoreInfo.m_ancestorId, asset.ToString<AZStd::string>().c_str());
            return SliceInstanceAddress();
        }

        const SliceComponent::SliceInstanceAddress address = FindSlice(entity);
        if (address.IsValid())
        {
            return address;
        }

        SliceReference* reference = AddOrGetSliceReference(asset);
        SliceInstance* instance = reference->FindInstance(restoreInfo.m_instanceId);

        if (!instance)
        {
            // We're creating an instance just to hold the entity we're re-adding. We don't want to instantiate the underlying asset.
            instance = reference->CreateEmptyInstance(restoreInfo.m_instanceId);

            // Make sure the appropriate metadata entities are created with the instance
            InstantiatedContainer sourceObjects(false);
            sourceSlice->GetAllMetadataEntities(sourceObjects.m_metadataEntities);
            instance->m_instantiated = IdUtils::Remapper<EntityId>::CloneObjectAndGenerateNewIdsAndFixRefs(&sourceObjects, instance->m_baseToNewEntityIdMap, GetSerializeContext());

            // Find the instanced version of the source metadata entity from the asset associated with this reference
            // and store it in the instance for quick lookups later
            auto metadataIdMapIter = instance->m_baseToNewEntityIdMap.find(sourceSlice->GetMetadataEntity()->GetId());
            AZ_Assert(metadataIdMapIter != instance->m_baseToNewEntityIdMap.end(), "Dependent Metadata Entity ID was not remapped properly.");
            AZ::EntityId instancedMetadataEntityId = metadataIdMapIter->second;

            for (AZ::Entity* metadataEntity : instance->m_instantiated->m_metadataEntities)
            {
                if (instancedMetadataEntityId == metadataEntity->GetId())
                {
                    instance->m_metadataEntity = metadataEntity;
                    break;
                }
            }

            AZ_Assert(instance->m_metadataEntity, "The instance must have a metadata entity");
        }

        // Add the entity to the instance, and wipe the reverse lookup cache so it's updated on access.
        instance->m_instantiated->m_entities.push_back(entity);
        instance->m_baseToNewEntityIdMap[restoreInfo.m_ancestorId] = entity->GetId();
        instance->m_entityIdToBaseCache.clear();
        instance->m_dataFlags.SetEntityDataFlags(entity->GetId(), restoreInfo.m_dataFlags);

        RebuildEntityInfoMapIfNecessary();

        return SliceInstanceAddress(reference, instance);
    }

    //=========================================================================
    // SliceComponent::GetReferencedSliceAssets
    //=========================================================================
    void SliceComponent::GetReferencedSliceAssets(AssetIdSet& idSet, bool recurse)
    {
        for (auto& sliceReference : m_slices)
        {
            const Data::Asset<SliceAsset>& referencedSliceAsset = sliceReference.GetSliceAsset();
            const Data::AssetId referencedSliceAssetId = referencedSliceAsset.GetId();
            if (idSet.find(referencedSliceAssetId) == idSet.end())
            {
                idSet.insert(referencedSliceAssetId);
                if (recurse)
                {
                    referencedSliceAsset.Get()->GetComponent()->GetReferencedSliceAssets(idSet, recurse);
                }
            }
        }
    }

    //=========================================================================
    // SliceComponent::AddSliceInstance
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::AddSliceInstance(SliceReference* sliceReference, SliceInstance* sliceInstance)
    {
        if (sliceReference && sliceInstance)
        {
            // sanity check that instance belongs to slice reference
            auto findIt = AZStd::find_if(sliceReference->m_instances.begin(), sliceReference->m_instances.end(), [sliceInstance](const SliceInstance& element) -> bool { return &element == sliceInstance; });
            if (findIt == sliceReference->m_instances.end())
            {
                AZ_Error("Slice", false, "SliceInstance %p doesn't belong to SliceReference %p!", sliceInstance, sliceReference);
                return SliceInstanceAddress();
            }

            if (!m_slicesAreInstantiated && sliceReference->m_isInstantiated)
            {
                // if we are not instantiated, but the source sliceInstance is we need to instantiate
                // to capture any changes that might come with it.
                if (Instantiate() != InstantiateResult::Success)
                {
                    return SliceInstanceAddress();
                }
            }

            SliceReference* newReference = GetSlice(sliceReference->m_asset);
            if (!newReference)
            {
                Data::AssetBus::MultiHandler::BusConnect(sliceReference->m_asset.GetId());
                m_slices.push_back();
                newReference = &m_slices.back();
                newReference->m_component = this;
                newReference->m_asset = sliceReference->m_asset;
                newReference->m_isInstantiated = m_slicesAreInstantiated;
            }

            // Move the instance to the new reference and remove it from its old owner.
            const SliceInstanceId instanceId = sliceInstance->GetId();
            sliceReference->RemoveInstanceFromEntityInfoMap(*sliceInstance);
            SliceInstance& newInstance = *newReference->m_instances.emplace(AZStd::move(*sliceInstance)).first;
            // Set the id of old slice-instance back to what it was so that it can be removed, because SliceInstanceId is used as hash key.
            sliceInstance->SetId(instanceId);

            if (!m_entityInfoMap.empty())
            {
                newReference->AddInstanceToEntityInfoMap(newInstance);
            }

            sliceReference->RemoveInstance(sliceInstance);

            if (newReference->m_isInstantiated && !sliceReference->m_isInstantiated)
            {
                // the source instance is not instantiated, make sure we instantiate it.
                newReference->InstantiateInstance(newInstance, AZ::ObjectStream::FilterDescriptor(m_assetLoadFilterCB, m_filterFlags));
            }

            return SliceInstanceAddress(newReference, &newInstance);
        }
        return SliceInstanceAddress();
    }

    SliceComponent::SliceInstanceAddress SliceComponent::CloneAndAddSubSliceInstance(
        const SliceComponent::SliceInstance* sourceSliceInstance,
        const AZStd::vector<AZ::SliceComponent::SliceInstanceAddress>& sourceSubSliceInstanceAncestry,
        const AZ::SliceComponent::SliceInstanceAddress& sourceSubSliceInstanceAddress,
        AZ::SliceComponent::EntityIdToEntityIdMap* out_sourceToCloneEntityIdMap /*= nullptr*/, bool preserveIds /*= false*/)
    {
        // Check if sourceSlceInstance belongs to this SliceComponent.
        if (sourceSliceInstance->m_instantiated->m_entities.empty() 
            || !FindEntity(sourceSliceInstance->m_instantiated->m_entities[0]->GetId()))
        {
            return SliceComponent::SliceInstanceAddress();
        }

        const SliceComponent::EntityIdToEntityIdMap& baseToLiveEntityIdMap = sourceSliceInstance->GetEntityIdMap();
        const SliceComponent::EntityIdToEntityIdMap& subSliceBaseToInstanceEntityIdMap = sourceSubSliceInstanceAddress.GetInstance()->GetEntityIdMap();

        EntityIdToEntityIdMap subSliceBaseToLiveEntityIdMap;
        for (auto& pair : subSliceBaseToInstanceEntityIdMap)
        {
            EntityId subSliceIntanceEntityId = pair.second;

            for (auto revIt = sourceSubSliceInstanceAncestry.rbegin(); revIt != sourceSubSliceInstanceAncestry.rend(); revIt++)
            {
                const SliceComponent::EntityIdToEntityIdMap& ancestorBaseToInstanceEntityIdMap = revIt->GetInstance()->GetEntityIdMap();
                auto foundItr = ancestorBaseToInstanceEntityIdMap.find(subSliceIntanceEntityId);
                if (foundItr != ancestorBaseToInstanceEntityIdMap.end())
                {
                    subSliceIntanceEntityId = foundItr->second;
                }
            }

            // If a live entity is deleted, it will also be deleted from baseToLiveEntityIdMap, so no need to check existence of entities here.
            auto foundItr = baseToLiveEntityIdMap.find(subSliceIntanceEntityId);
            if (foundItr != baseToLiveEntityIdMap.end())
            {
                subSliceBaseToLiveEntityIdMap.emplace(pair.first, foundItr->second);
            }
        }

        if (subSliceBaseToLiveEntityIdMap.empty())
        {
            return SliceComponent::SliceInstanceAddress();
        }

        AZStd::unordered_map<AZ::EntityId, AZ::Entity*> liveMetadataEntityMap;
        for (AZ::Entity* metaEntity : sourceSliceInstance->GetInstantiated()->m_metadataEntities)
        {
            liveMetadataEntityMap.emplace(metaEntity->GetId(), metaEntity);
        }

        // Separate MetaData entities from normal ones.
        InstantiatedContainer sourceEntitiesContainer(false); // to be cloned
        for (auto& pair : subSliceBaseToLiveEntityIdMap)
        {
            Entity* entity = FindEntity(pair.second);
            if (entity)
            {
                sourceEntitiesContainer.m_entities.push_back(entity);
            }
            else
            {
                auto foundItr = liveMetadataEntityMap.find(pair.second);
                if (foundItr != liveMetadataEntityMap.end())
                {
                    sourceEntitiesContainer.m_metadataEntities.push_back(foundItr->second);
                }
            }
        }

        SliceReference* newSliceReference = AddOrGetSliceReference(sourceSubSliceInstanceAddress.GetReference()->GetSliceAsset());
        SliceInstance* newInstance = newSliceReference->CreateEmptyInstance();

        EntityIdToEntityIdMap sourceToCloneEntityIdMap;

        // If we are preserving entity ID's we want to clone directly and not generate new IDs
        if (preserveIds)
        {
            AZ::SerializeContext* serializeContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            if (!serializeContext)
            {
                AZ_Error("Serialization", false, "No serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or input serialize context should not be null!");
                newInstance->m_instantiated = nullptr;
            }
            else
            {
                newInstance->m_instantiated = serializeContext->CloneObject(&sourceEntitiesContainer);
            }
        }
        else
        {
            newInstance->m_instantiated = IdUtils::Remapper<AZ::EntityId>::CloneObjectAndGenerateNewIdsAndFixRefs(&sourceEntitiesContainer, sourceToCloneEntityIdMap);
        }

        // Generate EntityId map between entities in base sub-slice and the cloned entities, 
        // also generate DataFlags for cloned entities (in newInstance) based on its corresponding source entities.
        const DataFlagsPerEntity& liveInstanceDataFlags = sourceSliceInstance->GetDataFlags();
        DataFlagsPerEntity cloneInstanceDataFlags;
        EntityIdToEntityIdMap subSliceBaseToCloneEntityIdMap;
        for (auto& pair : subSliceBaseToLiveEntityIdMap)
        {
            if (preserveIds)
            {
                // If preserving ID's the map from subslice to live entity is valid over
                // The map of base to generated IDs
                subSliceBaseToCloneEntityIdMap.emplace(pair);
                newInstance->m_dataFlags.SetEntityDataFlags(pair.second, liveInstanceDataFlags.GetEntityDataFlags(pair.second));
            }
            else
            {
                AZ::EntityId liveEntityId = pair.second;
                auto foundItr = sourceToCloneEntityIdMap.find(liveEntityId);
                if (foundItr != sourceToCloneEntityIdMap.end())
                {
                    AZ::EntityId cloneId = foundItr->second;
                    subSliceBaseToCloneEntityIdMap.emplace(pair.first, cloneId);

                    newInstance->m_dataFlags.SetEntityDataFlags(cloneId, liveInstanceDataFlags.GetEntityDataFlags(liveEntityId));
                }
            }
        }
        newInstance->m_baseToNewEntityIdMap = AZStd::move(subSliceBaseToCloneEntityIdMap);

        newSliceReference->FixUpMetadataEntityForSliceInstance(newInstance);

        newSliceReference->AddInstanceToEntityInfoMap(*newInstance);

        if (out_sourceToCloneEntityIdMap)
        {
            *out_sourceToCloneEntityIdMap = AZStd::move(sourceToCloneEntityIdMap);
        }

        return SliceInstanceAddress(newSliceReference, newInstance);
    }

    //=========================================================================
    // SliceComponent::RemoveSlice
    //=========================================================================
    bool SliceComponent::RemoveSlice(const Data::Asset<SliceAsset>& sliceAsset)
    {
        for (auto sliceIt = m_slices.begin(); sliceIt != m_slices.end(); ++sliceIt)
        {
            if (sliceIt->m_asset == sliceAsset)
            {
                RemoveSliceReference(sliceIt);
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSlice
    //=========================================================================
    bool SliceComponent::RemoveSlice(const SliceReference* slice)
    {
        if (slice)
        {
            return RemoveSlice(slice->m_asset);
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSliceInstance
    //=========================================================================
    bool SliceComponent::RemoveSliceInstance(SliceComponent::SliceInstanceAddress sliceAddress)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);
        if (!sliceAddress.IsValid())
        {
            AZ_Error("Slices", false, "Slice address is invalid.");
            return false;
        }

        if (sliceAddress.GetReference()->GetSliceComponent() != this)
        {
            AZ_Error("Slices", false, "SliceComponent does not own this slice");
            return false;
        }

        if (sliceAddress.IsValid())
        {
            SliceReference* sliceReference = sliceAddress.GetReference();

            if (sliceReference->RemoveInstance(sliceAddress.GetInstance()))
            {
                if (sliceReference->m_instances.empty())
                {
                    RemoveSlice(sliceReference);
                }

                return true;
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSliceInstance
    //=========================================================================
    bool SliceComponent::RemoveSliceInstance(SliceInstance* instance)
    {
        for (auto sliceReferenceIt = m_slices.begin(); sliceReferenceIt != m_slices.end(); ++sliceReferenceIt)
        {
            // note move this function to the slice reference for consistency
            if (sliceReferenceIt->RemoveInstance(instance))
            {
                if (sliceReferenceIt->m_instances.empty())
                {
                    RemoveSliceReference(sliceReferenceIt);
                }
                return true;
            }
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveSliceReference
    //=========================================================================
    void SliceComponent::RemoveSliceReference(SliceComponent::SliceList::iterator sliceReferenceIt)
    {
        Data::AssetBus::MultiHandler::BusDisconnect(sliceReferenceIt->GetSliceAsset().GetId());
        m_slices.erase(sliceReferenceIt);
    }

    //=========================================================================
    // SliceComponent::AddEntity
    //=========================================================================
    void SliceComponent::AddEntity(Entity* entity)
    {
        AZ_Assert(entity, "You passed an invalid entity!");
        m_entities.push_back(entity);

        if (!m_entityInfoMap.empty())
        {
            m_entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress())));
        }
    }

    //=========================================================================
    // SliceComponent::RemoveEntity
    //=========================================================================
    bool SliceComponent::RemoveEntity(Entity* entity, bool isDeleteEntity, bool isRemoveEmptyInstance)
    {
        if (entity)
        {
            return RemoveEntity(entity->GetId(), isDeleteEntity, isRemoveEmptyInstance);
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::RemoveEntity
    //=========================================================================
    bool SliceComponent::RemoveEntity(EntityId entityId, bool isDeleteEntity, bool isRemoveEmptyInstance)
    {
        GetEntityInfoMap(); // ensure map is built
        auto entityInfoMapIt = m_entityInfoMap.find(entityId);
        if (entityInfoMapIt != m_entityInfoMap.end())
        {
            if (entityInfoMapIt->second.m_sliceAddress.GetInstance() == nullptr)
            {
                // should be in the entity lists
                auto entityIt = AZStd::find_if(m_entities.begin(), m_entities.end(), [entityId](Entity* entity) -> bool { return entity->GetId() == entityId; });
                if (entityIt != m_entities.end())
                {
                    if (isDeleteEntity)
                    {
                        delete *entityIt;
                    }
                    m_dataFlagsForNewEntities.ClearEntityDataFlags(entityId);
                    m_entityInfoMap.erase(entityInfoMapIt);
                    m_entities.erase(entityIt);
                    return true;
                }
            }
            else
            {
                SliceReference* sliceReference = entityInfoMapIt->second.m_sliceAddress.GetReference();
                SliceInstance* sliceInstance = entityInfoMapIt->second.m_sliceAddress.GetInstance();

                if (sliceReference->RemoveEntity(entityId, isDeleteEntity, sliceInstance))
                {
                    if (isRemoveEmptyInstance)
                    {
                        if (sliceInstance->m_instantiated->m_entities.empty())
                        {
                            RemoveSliceInstance(sliceInstance);
                        }
                    }

                    m_entityInfoMap.erase(entityInfoMapIt);
                    return true;
                }
            }
        }

        return false;
    }

    //=========================================================================
    // SliceComponent::FindEntity
    //=========================================================================
    AZ::Entity* SliceComponent::FindEntity(EntityId entityId)
    {
        const EntityInfoMap& entityInfoMap = GetEntityInfoMap();
        auto entityInfoMapIt = entityInfoMap.find(entityId);
        if (entityInfoMapIt != entityInfoMap.end())
        {
            return entityInfoMapIt->second.m_entity;
        }

        return nullptr;
    }

    //=========================================================================
    // SliceComponent::FindSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::FindSlice(Entity* entity)
    {
        // if we have valid entity pointer and we are instantiated (if we have not instantiated the entities, there is no way
        // to have a pointer to it... that pointer belongs somewhere else).
        if (entity && m_slicesAreInstantiated)
        {
            return FindSlice(entity->GetId());
        }

        return SliceInstanceAddress();
    }

    //=========================================================================
    // SliceComponent::FindSlice
    //=========================================================================
    SliceComponent::SliceInstanceAddress SliceComponent::FindSlice(EntityId entityId)
    {
        if (entityId.IsValid())
        {
            const EntityInfoMap& entityInfo = GetEntityInfoMap();
            auto entityInfoIter = entityInfo.find(entityId);
            if (entityInfoIter != entityInfo.end())
            {
                return entityInfoIter->second.m_sliceAddress;
            }
        }

        return SliceInstanceAddress();
    }

    bool SliceComponent::FlattenSlice(SliceComponent::SliceReference* toFlatten, const EntityId& toFlattenRoot)
    {
        // Sanity check that we're operating on a reference we own
        if (!toFlatten || toFlatten->GetSliceComponent() != this)
        {
            AZ_Warning("Slice", false, "Slice Component attempted to flatten a Slice Reference it doesn't own");
            return false;
        }

        // Sanity check that the root entity is owned by the reference
        SliceInstanceAddress rootSlice = FindSlice(toFlattenRoot);
        if (!rootSlice || rootSlice.GetReference() != toFlatten)
        {
            AZ_Warning("Slice", false, "Attempted to flatten Slice Reference using a root entity it doesn't own");
            return false;
        }

        // Get slice path for error printouts
        AZStd::string slicePath;
        AZ::Data::AssetCatalogRequestBus::BroadcastResult(
            slicePath,
            &AZ::Data::AssetCatalogRequestBus::Events::GetAssetPathById,
            toFlatten->GetSliceAsset().GetId());

        // Our shared ancestry is the ancestry of the root entity in the slice we are flattening
        EntityAncestorList sharedAncestry;
        toFlatten->GetInstanceEntityAncestry(toFlattenRoot, sharedAncestry);

        // Walk through each instance's instantiated entities
        // Since they are instantiated they will already be data patched
        // Each entity will fall under 4 catagories:
        // 1. The root entity
        // 2. An entity owned directly by the slice instance
        // 3. A subslice instance root entity
        // 4. An entity owned by a subslice instance root entity
        for (SliceInstance& instance : toFlatten->m_instances)
        {
            // Make a copy of our instantiated entities so we can safely remove them
            // from the source container while iterating
            EntityList instantiatedEntityList = instance.m_instantiated->m_entities;
            for (AZ::Entity* entity : instantiatedEntityList)
            {
                if (!entity)
                {
                    AZ_Error("Slice", false , "Null entity found in a slice instance of %s in FlattenSlice", slicePath.c_str());
                    return false;
                }
                // Gather the ancestry of each entity to compare against the shared ancestry
                EntityAncestorList entityAncestors;
                toFlatten->GetInstanceEntityAncestry(entity->GetId(), entityAncestors);

                // Get rid of the first element as all entities are guaranteed to share it
                AZStd::vector<SliceInstanceAddress> ancestorList;
                entityAncestors.erase(entityAncestors.begin());

                // Walk the entities ancestry to confirm which of the 4 cases the entity is
                bool addAsEntity = true;
                for (const Ancestor& ancestor : entityAncestors)
                {
                    // Keep track of each ancestor in preperation for cloning a subslice
                    ancestorList.push_back(ancestor.m_sliceAddress);

                    // Check if the entity's ancestor is shared with the root
                    AZ::Data::Asset<SliceAsset> ancestorAsset = ancestor.m_sliceAddress.GetReference()->GetSliceAsset();
                    auto isShared = AZStd::find_if(sharedAncestry.begin(), sharedAncestry.end(),
                        [ancestorAsset](const Ancestor& sharedAncestor) 
                        {
                            return sharedAncestor.m_sliceAddress.GetReference()->GetSliceAsset() == ancestorAsset; 
                        });

                    // If it is keep checking
                    if (isShared != sharedAncestry.end())
                    {
                        continue;
                    }

                    // If it isn't we've found a subslice entity
                    if (ancestor.m_entity)
                    {
                        AZ::Entity::State originalState = ancestor.m_entity->GetState();

                        // Quickly activate the ancestral entity so we can get its transform data
                        if (originalState == AZ::Entity::State::ES_CONSTRUCTED)
                        {
                            ancestor.m_entity->Init();
                        }

                        if (ancestor.m_entity->GetState() == AZ::Entity::State::ES_INIT)
                        {
                            ancestor.m_entity->Activate();
                        }

                        if (ancestor.m_entity->GetState() != AZ::Entity::State::ES_ACTIVE)
                        {
                            AZ_Error("Slice", false, "Could not activate entity with id %s during FlattenSlice", ancestor.m_entity->GetId().ToString().c_str());
                            return false;
                        }

                        // If the ancestor's entity doesn't have a valid parent we've found
                        // Case 3 a subslice root entity
                        AZ::EntityId parentId;
                        AZ::TransformBus::EventResult(parentId, ancestor.m_entity->GetId(), &AZ::TransformBus::Events::GetParentId);
                        if (!parentId.IsValid())
                        {
                            // Acquire the subslice instance and promote it from an instance in our toFlatten reference
                            // into a reference owned directly by the Slice component
                            SliceInstanceAddress owningSlice = FindSlice(entity);
                            CloneAndAddSubSliceInstance(owningSlice.GetInstance(), ancestorList, ancestor.m_sliceAddress, nullptr, true);
                        }

                        // else if we have a valid parent we are case 4 an entity owned by a subslice root
                        // We will be included in the subslice root's clone and can be skipped

                        // Deactivate the entity if we started that way
                        if (originalState != AZ::Entity::State::ES_ACTIVE && originalState != AZ::Entity::State::ES_ACTIVATING)
                        {
                            ancestor.m_entity->Deactivate();
                        }
                    }

                    // We've either added a subslice root or skipped an entity owned by one
                    // We should not add it as a plain entity
                    addAsEntity = false;
                    break;
                }

                // If all our ancestry is shared with the root then we're directly owned by it
                // We can add it directly to the slice component
                if (addAsEntity)
                {
                    // Remove the entity non-destructively from our reference
                    // and add it directly to the slice component
                    toFlatten->RemoveEntity(entity->GetId(), false, &instance);
                    AddEntity(entity);

                    // Register entity with this components metadata
                    SliceMetadataInfoComponent* metadataComponent = m_metadataEntity.FindComponent<SliceMetadataInfoComponent>();
                    if (!metadataComponent)
                    {
                        AZ_Error("Slice", false, "Attempted to add entity to Slice Component with no Metadata component");
                        return false;
                    }
                    metadataComponent->AddAssociatedEntity(entity->GetId());
                }
            }
        }

        // All entities have been promoted directly into the Slice component
        // Removing it will remove our dependency on it and its ancestors
        if (!RemoveSlice(toFlatten))
        {
            AZ_Error("Slice", false, "Failed to remove flattened reference of slice %s from Slice Component", slicePath.c_str());
        }

        CleanMetadataAssociations();
        return true;
    }
    //=========================================================================
    // SliceComponent::GetEntityInfoMap
    //=========================================================================
    const SliceComponent::EntityInfoMap& SliceComponent::GetEntityInfoMap() const
    {
        // Don't build the entity info map until it's queried.
        // The full map can't be built until the slice has been instantiated.
        AZ_Assert(IsInstantiated(), "GetEntityInfoMap() should only be called on an instantiated slice");
        if (m_entityInfoMap.empty())
        {
            const_cast<SliceComponent*>(this)->BuildEntityInfoMap();
        }

        return m_entityInfoMap;
    }

    //=========================================================================
    // SliceComponent::ListenForAssetChanges
    //=========================================================================
    void SliceComponent::ListenForAssetChanges()
    {
        if (!m_serializeContext)
        {
            // use the default app serialize context
            EBUS_EVENT_RESULT(m_serializeContext, ComponentApplicationBus, GetSerializeContext);
            if (!m_serializeContext)
            {
                AZ_Error("Slices", false, "SliceComponent: No serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or SliceComponent serialize context should not be null!");
            }
        }

        // Listen for asset events and set reference to myself
        for (SliceReference& slice : m_slices)
        {
            slice.m_component = this;
            Data::AssetBus::MultiHandler::BusConnect(slice.m_asset.GetId());
        }
    }

    //=========================================================================
    // SliceComponent::ListenForDependentAssetChanges
    //=========================================================================
    void SliceComponent::ListenForDependentAssetChanges()
    {
        if (!m_serializeContext)
        {
            // use the default app serialize context
            ComponentApplicationBus::BroadcastResult(m_serializeContext, &ComponentApplicationBus::Events::GetSerializeContext);
            if (!m_serializeContext)
            {
                AZ_Error("Slices", false, "SliceComponent: No serialize context provided! Failed to get component application default serialize context! ComponentApp is not started or SliceComponent serialize context should not be null!");
            }
        }

        ListenForAssetChanges();

        // Listen for asset events and set reference to myself
        for (SliceReference& slice : m_slices)
        {
            // recursively listen down the slice tree.
            slice.GetSliceAsset().Get()->GetComponent()->ListenForDependentAssetChanges();
        }
    }

    //=========================================================================
    // SliceComponent::Activate
    //=========================================================================
    void SliceComponent::Activate()
    {
        ListenForAssetChanges();
    }

    //=========================================================================
    // SliceComponent::Deactivate
    //=========================================================================
    void SliceComponent::Deactivate()
    {
        Data::AssetBus::MultiHandler::BusDisconnect();
    }

    //=========================================================================
    // SliceComponent::Deactivate
    //=========================================================================
    void SliceComponent::OnAssetReloaded(Data::Asset<Data::AssetData> /*asset*/)
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (!m_myAsset)
        {
            AZ_Assert(false, "Cannot reload a slice component that is not owned by an asset.");
            return;
        }

        AZ::Data::AssetId myAssetId = m_myAsset->GetId();

        // One of our dependent assets has changed.
        // We need to identify any dependent assets that have changed, since there may be multiple
        // due to the nature of cascaded slice reloading.
        // Because we're about to re-construct our own asset and dispatch the change notification, 
        // we need to handle all pending dependent changes.
        bool dependencyHasChanged = false;
        for (SliceReference& slice : m_slices)
        {
            Data::Asset<SliceAsset> dependentAsset = Data::AssetManager::Instance().FindAsset(slice.m_asset.GetId());

            if (slice.m_asset.Get() != dependentAsset.Get())
            {
                dependencyHasChanged = true;
                break;
            }
        }

        if (!dependencyHasChanged)
        {
            return;
        }

        // Create new SliceAsset data based on our data. We don't want to update our own
        // data in place, but instead propagate through asset reloading. Otherwise data
        // patches are not maintained properly up the slice dependency chain.
        SliceComponent* updatedAssetComponent = Clone(*m_serializeContext);
        Entity* updatedAssetEntity = aznew Entity();
        updatedAssetEntity->AddComponent(updatedAssetComponent);

        Data::Asset<SliceAsset> updatedAsset(m_myAsset->Clone());
        updatedAsset.Get()->SetData(updatedAssetEntity, updatedAssetComponent);
        updatedAssetComponent->SetMyAsset(updatedAsset.Get());

        // Update data patches against the old version of the asset.
        updatedAssetComponent->PrepareSave();

        PushInstantiateCycle(myAssetId);

        // Update asset reference for any modified dependencies, and re-instantiate nested instances.
        for (auto listIterator = updatedAssetComponent->m_slices.begin(); listIterator != updatedAssetComponent->m_slices.end();)
        {
            SliceReference& slice = *listIterator;
            Data::Asset<SliceAsset> dependentAsset = Data::AssetManager::Instance().FindAsset(slice.m_asset.GetId());

            bool recursionDetected = CheckContainsInstantiateCycle(dependentAsset.GetId());
            bool wasModified = (dependentAsset.Get() != slice.m_asset.Get());
            
            if (wasModified || recursionDetected)
            {
                // uninstantiate if its been instantiated:
                if (slice.m_isInstantiated && !slice.m_instances.empty())
                {
                    for (SliceInstance& instance : slice.m_instances)
                    {
                        delete instance.m_instantiated;
                        instance.m_instantiated = nullptr;
                    }
                    slice.m_isInstantiated = false;
                }
            }
            
            if (!recursionDetected)
            {
                if (wasModified) // no need to do anything here if it wasn't actually changed.
                {
                    // Asset data differs. Acquire new version and re-instantiate.
                    slice.m_asset = dependentAsset;
                    slice.Instantiate(AZ::ObjectStream::FilterDescriptor(m_assetLoadFilterCB, m_filterFlags));
                }
                ++listIterator;
            }
            else
            {
                // whenever recursion was detected we remove this element.
                listIterator = updatedAssetComponent->m_slices.erase(listIterator);
            }
        }

        PopInstantiateCycle(myAssetId);

        // Rebuild entity info map based on new instantiations.
        RebuildEntityInfoMapIfNecessary();
       
        // note that this call will destroy the "this" pointer, because the main editor context will respond to this by deleting and recreating all slices
        // (this call will cascade up the tree of inheritence all the way to the root.)
        // for this reason, we disconnect from the busses to prevent recursion.
        Data::AssetBus::MultiHandler::BusDisconnect();
        Data::AssetManager::Instance().ReloadAssetFromData(updatedAsset);
    }

    //=========================================================================
    // SliceComponent::GetProvidedServices
    //=========================================================================
    void SliceComponent::GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("Prefab", 0xa60af5fc));
    }

    //=========================================================================
    // SliceComponent::GetDependentServices
    //=========================================================================
    void SliceComponent::GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("AssetDatabaseService", 0x3abf5601));
    }

    /**
    * \note when we reflecting we can check if the class is inheriting from IEventHandler
    * and use the this->events.
    */
    class SliceComponentSerializationEvents
        : public SerializeContext::IEventHandler
    {
        /// Called right before we start reading from the instance pointed by classPtr.
        void OnReadBegin(void* classPtr)
        {
            SliceComponent* component = reinterpret_cast<SliceComponent*>(classPtr);
            component->PrepareSave();
        }

        /// Called right after we finish writing data to the instance pointed at by classPtr.
        void OnWriteEnd(void* classPtr) override
        {
            AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

            SliceComponent* sliceComponent = reinterpret_cast<SliceComponent*>(classPtr);
            EBUS_EVENT(SliceAssetSerializationNotificationBus, OnWriteDataToSliceAssetEnd, *sliceComponent);

            // Broadcast OnSliceEntitiesLoaded for entities that are "new" to this slice.
            // We can't broadcast this event for instanced entities yet, since they don't exist until instantiation.
            if (!sliceComponent->GetNewEntities().empty())
            {
                AZ_PROFILE_SCOPE(AZ::Debug::ProfileCategory::AzCore, "SliceComponentSerializationEvents::OnWriteEnd:OnSliceEntitiesLoaded");
                EBUS_EVENT(SliceAssetSerializationNotificationBus, OnSliceEntitiesLoaded, sliceComponent->GetNewEntities());
            }
        }
    };

    //=========================================================================
    // Reflect
    //=========================================================================
    void SliceComponent::PrepareSave()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (m_slicesAreInstantiated)
        {
            // if we had the entities instantiated, recompute the deltas.
            for (SliceReference& slice : m_slices)
            {
                slice.ComputeDataPatch();
            }
        }

        // clean up orphaned data flags (ex: for entities that no longer exist).
        m_dataFlagsForNewEntities.Cleanup(m_entities);
    }

    //=========================================================================
    // InitMetadata
    //=========================================================================
    void SliceComponent::InitMetadata()
    {
        // If we have a new metadata entity, we need to add a SliceMetadataInfoComponent to it and 
        // fill it in with general metadata hierarchy and association data as well as assign a
        // useful name to the entity.
        auto* metadataInfoComponent = m_metadataEntity.FindComponent<SliceMetadataInfoComponent>();
        if (!metadataInfoComponent)
        {
            // If this component is missing, we can assume this is a brand new empty metadata entity that has
            // either come from a newly created slice or loaded from a legacy slice without metadata. We can use
            // this opportunity to give it an appropriate name and a required component. We're adding the component
            // here during the various steps of slice instantiation because it becomes costly and difficult to do
            // so later.
            metadataInfoComponent = m_metadataEntity.CreateComponent<SliceMetadataInfoComponent>();

            // Every instance of another slice referenced by this component has set of metadata entities.
            // Exactly one of those metadata entities in each of those sets is instanced from the slice asset
            // that was used to create the instance. We want only those metadata entities.
            EntityList instanceEntities;
            GetInstanceMetadataEntities(instanceEntities);

            for (auto& instanceMetadataEntity : instanceEntities)
            {
                // Add hierarchy information to our new info component
                metadataInfoComponent->AddChildMetadataEntity(instanceMetadataEntity->GetId());

                auto* instanceInfoComponent = instanceMetadataEntity->FindComponent<SliceMetadataInfoComponent>();
                AZ_Assert(instanceInfoComponent, "Instanced entity must have a metadata component.");
                if (instanceInfoComponent)
                {
                    metadataInfoComponent->SetParentMetadataEntity(m_metadataEntity.GetId());
                }
            }

            // All new entities in the current slice need to be associated with the slice's metadata entity
            for (auto& entity : m_entities)
            {
                metadataInfoComponent->AddAssociatedEntity(entity->GetId());
            }
        }

        // Finally, clean up all the metadata associations belonging to our instances
        // We need to do this every time we initialize a slice because we can't know if one of our referenced slices has changed
        // since our last initialization.
        CleanMetadataAssociations();
    }

    void SliceComponent::CleanMetadataAssociations()
    {
        // Now we need to get all of the metadata entities that belong to all of our slice instances.
        EntityList instancedMetadataEntities;
        GetAllInstanceMetadataEntities(instancedMetadataEntities);

        // Easy out - No instances means no associations to fix
        if (!instancedMetadataEntities.empty())
        {
            // We need to check every associated entity in every instance to see if it wasn't removed by our data patch.
            // First, Turn the list of all entities belonging to this component into a set of IDs to reduce find times.
            EntityIdSet entityIds;
            GetEntityIds(entityIds);

            // Now we need to get the metadata info component for each of these entities and get the list of their associated entities.
            for (auto& instancedMetadataEntity : instancedMetadataEntities)
            {
                auto* instancedInfoComponent = instancedMetadataEntity->FindComponent<SliceMetadataInfoComponent>();
                AZ_Assert(instancedInfoComponent, "Instanced metadata entities MUST have metadata info components.");
                if (instancedInfoComponent)
                {
                    // Copy the set of associated IDs to iterate through to avoid invalidating our iterator as we check each and
                    // remove it from the info component if it wasn't cloned into this slice.
                    EntityIdSet associatedEntityIds;
                    instancedInfoComponent->GetAssociatedEntities(associatedEntityIds);
                    for (const auto& entityId : associatedEntityIds)
                    {
                        if (entityIds.find(entityId) == entityIds.end())
                        {
                            instancedInfoComponent->RemoveAssociatedEntity(entityId);
                        }
                    }
                }
            }
        }
    }

    //=========================================================================
    // SliceComponent::BuildEntityInfoMap
    //=========================================================================
    void SliceComponent::BuildEntityInfoMap()
    {
        m_entityInfoMap.clear();

        for (Entity* entity : m_entities)
        {
            m_entityInfoMap.insert(AZStd::make_pair(entity->GetId(), EntityInfo(entity, SliceInstanceAddress())));
        }

        for (SliceReference& slice : m_slices)
        {
            for (SliceInstance& instance : slice.m_instances)
            {
                slice.AddInstanceToEntityInfoMap(instance);
            }
        }
    }

    //=========================================================================
    // SliceComponent::RebuildEntityInfoMapIfNecessary
    //=========================================================================
    void SliceComponent::RebuildEntityInfoMapIfNecessary()
    {
        if (!m_entityInfoMap.empty())
        {
            BuildEntityInfoMap();
        }
    }

    //=========================================================================
    // ApplyEntityMapId
    //=========================================================================
    void SliceComponent::ApplyEntityMapId(EntityIdToEntityIdMap& destination, const EntityIdToEntityIdMap& remap)
    {
        // apply the instance entityIdMap on the top of the base
        for (const auto& entityIdPair : remap)
        {
            auto iteratorBoolPair = destination.insert(entityIdPair);
            if (!iteratorBoolPair.second)
            {
                // if not inserted just overwrite the value
                iteratorBoolPair.first->second = entityIdPair.second;
            }
        }
    }

    //=========================================================================
    // PushInstantiateCycle
    //=========================================================================
    void SliceComponent::PushInstantiateCycle(const AZ::Data::AssetId& assetId)
    {
        AZStd::unique_lock<AZStd::recursive_mutex> lock(m_instantiateMutex);
        m_instantiateCycleChecker.push_back(assetId);
    }

    //=========================================================================
    // ContainsInstantiateCycle
    //=========================================================================
    bool SliceComponent::CheckContainsInstantiateCycle(const AZ::Data::AssetId& assetId)
    {
        /* this function is called during recursive slice instantiation to check for cycles.
        /  It is assumed that before recursing, PushInstantiateCycle is called, 
        /  and when recursion returns, PopInstantiateCycle is called.
        /  we want to make sure we are never in the following situation:
        /     SliceA
        /        + SliceB
        /           + SliceA (again - cyclic!)
        /  note that its safe for the same asset to pop up a few times in different branches of the instantiation
        /  hierarchy, just not within the same direct line as its parents
        */

        AZStd::unique_lock<AZStd::recursive_mutex> lock(m_instantiateMutex);
        if (AZStd::find(m_instantiateCycleChecker.begin(), m_instantiateCycleChecker.end(), assetId) != m_instantiateCycleChecker.end())
        {
   
#if defined(AZ_ENABLE_TRACING)
            AZ::Data::Asset<SliceAsset> fullAsset = AZ::Data::AssetManager::Instance().FindAsset(assetId);
            AZStd::string messageString = "Cyclic Slice references detected!  Please see the hierarchy below:";
            for (const AZ::Data::AssetId& assetListElement : m_instantiateCycleChecker)
            {
                Data::Asset<SliceAsset> callerAsset = Data::AssetManager::Instance().FindAsset(assetListElement);
                if (callerAsset.GetId() == assetId)
                {
                    messageString.append("\n --> "); // highlight the ones that are the problem!
                }
                else
                {
                    messageString.append("\n     ");
                }
                messageString.append(callerAsset.ToString<AZStd::string>().c_str());
            }
            // put the one that we are currently checking for at the end of the list for easy readability:
            messageString.append("\n --> ");
            messageString.append(fullAsset.ToString<AZStd::string>().c_str());
            AZ_Error("Slice", false, "%s", messageString.c_str());
#endif // AZ_ENABLE_TRACING

            return true;
        }

        return false;
    }

    //=========================================================================
    // PopInstantiateCycle
    //=========================================================================
    void SliceComponent::PopInstantiateCycle(const AZ::Data::AssetId& assetId)
    {
#if defined(AZ_ENABLE_TRACING)
        AZ_Assert(m_instantiateCycleChecker.back() == assetId, "Programmer error - Cycle-Checking vector should be symmetrically pushing and popping elements.");
#else
        AZ_UNUSED(assetId);
#endif
        m_instantiateCycleChecker.pop_back();
        if (m_instantiateCycleChecker.empty())
        {
            // reclaim memory so that there is no leak of this static object.
            m_instantiateCycleChecker.set_capacity(0);
        }
    }

    //=========================================================================
    // SliceComponent::AddOrGetSliceReference
    //=========================================================================
    SliceComponent::SliceReference* SliceComponent::AddOrGetSliceReference(const Data::Asset<SliceAsset>& sliceAsset)
    {
        SliceReference* reference = GetSlice(sliceAsset.GetId());
        if (!reference)
        {
            Data::AssetBus::MultiHandler::BusConnect(sliceAsset.GetId());
            m_slices.push_back();
            reference = &m_slices.back();
            reference->m_component = this;
            reference->m_asset = sliceAsset;
            reference->m_isInstantiated = m_slicesAreInstantiated;
        }

        return reference;
    }

    //=========================================================================
    const SliceComponent::DataFlagsPerEntity& SliceComponent::GetDataFlagsForInstances() const
    {
        // DataFlags can affect how DataPaches are applied when a slice is instantiated.
        // We need to collect the DataFlags of each entity's entire ancestry.
        // We cache these flags in the slice to avoid recursing the ancestry each time that slice is instanced.
        // The cache is generated the first time it's requested.
        if (!m_hasGeneratedCachedDataFlags)
        {
            const_cast<SliceComponent*>(this)->BuildDataFlagsForInstances();
        }
        return m_cachedDataFlagsForInstances;
    }

    //=========================================================================
    void SliceComponent::BuildDataFlagsForInstances()
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);
        AZ_Assert(IsInstantiated(), "Slice must be instantiated before the ancestry of its data flags can be calculated.");

        // Use lock since slice instantiation can occur from multiple threads
        AZStd::unique_lock<AZStd::recursive_mutex> lock(m_instantiateMutex);
        if (m_hasGeneratedCachedDataFlags)
        {
            return;
        }

        // Collect data flags from entities originating in this slice.
        m_cachedDataFlagsForInstances.CopyDataFlagsFrom(m_dataFlagsForNewEntities);

        // Collect data flags from each slice that's instantiated within this slice...
        for (const SliceReference& sliceReference : m_slices)
        {
            for (const SliceInstance& sliceInstance : sliceReference.m_instances)
            {
                // Collect data flags from the entire ancestry by incorporating the cached data flags
                // from the SliceComponent that this instance was instantiated from.
                m_cachedDataFlagsForInstances.ImportDataFlagsFrom(
                    sliceReference.GetSliceAsset().Get()->GetComponent()->GetDataFlagsForInstances(),
                    &sliceInstance.GetEntityIdMap(),
                    DataPatch::GetEffectOfSourceFlagsOnThisAddress);

                // Collect data flags unique to this instance
                m_cachedDataFlagsForInstances.ImportDataFlagsFrom(
                    sliceInstance.GetDataFlags(),
                    &sliceInstance.GetEntityIdMap());
            }
        }

        m_hasGeneratedCachedDataFlags = true;
    }

    //=========================================================================
    const SliceComponent::DataFlagsPerEntity* SliceComponent::GetCorrectBundleOfDataFlags(EntityId entityId) const
    {
        // It would be possible to search non-instantiated slices by crawling over lists, but we haven't needed the capability yet.
        AZ_Assert(IsInstantiated(), "Data flag access is only permitted after slice is instantiated.")

        if (IsInstantiated())
        {
            // Slice is instantiated, we can use EntityInfoMap
            const EntityInfoMap& entityInfoMap = GetEntityInfoMap();
            auto foundEntityInfo = entityInfoMap.find(entityId);
            if (foundEntityInfo != entityInfoMap.end())
            {
                const EntityInfo& entityInfo = foundEntityInfo->second;
                if (const SliceInstance* entitySliceInstance = entityInfo.m_sliceAddress.GetInstance())
                {
                    return &entitySliceInstance->GetDataFlags();
                }
                else
                {
                    return &m_dataFlagsForNewEntities;
                }
            }
        }

        return nullptr;
    }

    //=========================================================================
    SliceComponent::DataFlagsPerEntity* SliceComponent::GetCorrectBundleOfDataFlags(EntityId entityId)
    {
        // call const version of same function
        return const_cast<DataFlagsPerEntity*>(const_cast<const SliceComponent*>(this)->GetCorrectBundleOfDataFlags(entityId));
    }

    //=========================================================================
    bool SliceComponent::IsNewEntity(EntityId entityId) const
    {
        // return true if this entity is not based on an entity from another slice
        if (IsInstantiated())
        {
            // Slice is instantiated, use the entity info map
            const EntityInfoMap& entityInfoMap = GetEntityInfoMap();
            auto foundEntityInfo = entityInfoMap.find(entityId);
            if (foundEntityInfo != entityInfoMap.end())
            {
                return !foundEntityInfo->second.m_sliceAddress.IsValid();
            }
        }
        else
        {
            // Slice is not instantiated, crawl over list of new entities
            for (Entity* newEntity : GetNewEntities())
            {
                if (newEntity->GetId() == entityId)
                {
                    return true;
                }
            }
        }

        return false;
    }

    //=========================================================================
    const DataPatch::FlagsMap& SliceComponent::GetEntityDataFlags(EntityId entityId) const
    {
        if (const DataFlagsPerEntity* dataFlagsPerEntity = GetCorrectBundleOfDataFlags(entityId))
        {
            return dataFlagsPerEntity->GetEntityDataFlags(entityId);
        }

        static const DataPatch::FlagsMap emptyFlagsMap;
        return emptyFlagsMap;
    }

    //=========================================================================
    bool SliceComponent::SetEntityDataFlags(EntityId entityId, const DataPatch::FlagsMap& dataFlags)
    {
        if (DataFlagsPerEntity* dataFlagsPerEntity = GetCorrectBundleOfDataFlags(entityId))
        {
            return dataFlagsPerEntity->SetEntityDataFlags(entityId, dataFlags);
        }
        return false;
    }

    //=========================================================================
    DataPatch::Flags SliceComponent::GetEffectOfEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress) const
    {
        // if this function is a performance bottleneck, it could be optimized with caching
        // be wary not to create the cache in-game if the information is only needed by tools
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        if (!IsInstantiated())
        {
            AZ_Assert(false, "Data flag access is only permitted after slice is instantiated.");
            return 0;
        }

        // look up entity
        const EntityInfoMap& entityInfoMap = GetEntityInfoMap();
        auto foundEntityInfo = entityInfoMap.find(entityId);
        if (foundEntityInfo == entityInfoMap.end())
        {
            AZ_Assert(false, "Entity is unknown to this slice");
            return 0;
        }

        const EntityInfo& entityInfo = foundEntityInfo->second;

        // Get data flags for this instance of the entity.
        // If this entity is based on another slice, get the data flags from that slice as well.
        const DataFlagsPerEntity* dataFlagsForEntity = nullptr;
        const DataFlagsPerEntity* dataFlagsForBaseEntity = nullptr;
        EntityId baseEntityId;

        if (const SliceInstance* entitySliceInstance = entityInfo.m_sliceAddress.GetInstance())
        {
            dataFlagsForEntity = &entitySliceInstance->GetDataFlags();

            const EntityIdToEntityIdMap& entityIdToBaseMap = entitySliceInstance->GetEntityIdToBaseMap();
            auto foundBaseEntity = entityIdToBaseMap.find(entityId);
            if (foundBaseEntity != entityIdToBaseMap.end())
            {
                baseEntityId = foundBaseEntity->second;
                dataFlagsForBaseEntity = &entityInfo.m_sliceAddress.GetReference()->GetSliceAsset().Get()->GetComponent()->GetDataFlagsForInstances();
            }
        }
        else
        {
            dataFlagsForEntity = &m_dataFlagsForNewEntities;
        }

        // To calculate the full effect of data flags on an address,
        // we need to compile flags from all parent addresses, and all ancestor addresses.
        DataPatch::Flags flags = 0;

        // iterate through address, beginning with the "blank" initial address
        DataPatch::AddressType address;
        address.reserve(dataAddress.size());
        DataPatch::AddressType::const_iterator addressIterator = dataAddress.begin();
        while(true)
        {
            // functionality is identical to DataPatch::CalculateDataFlagAtThisAddress(...)
            // but this gets data from DataFlagsPerEntity instead of DataPatch::FlagsMap
            if (flags)
            {
                flags |= DataPatch::GetEffectOfParentFlagsOnThisAddress(flags);
            }

            if (dataFlagsForBaseEntity)
            {
                if (DataPatch::Flags baseEntityFlags = dataFlagsForBaseEntity->GetEntityDataFlagsAtAddress(baseEntityId, address))
                {
                    flags |= DataPatch::GetEffectOfSourceFlagsOnThisAddress(baseEntityFlags);
                }
            }

            if (DataPatch::Flags entityFlags = dataFlagsForEntity->GetEntityDataFlagsAtAddress(entityId, address))
            {
                flags |= DataPatch::GetEffectOfTargetFlagsOnThisAddress(entityFlags);
            }

            if (addressIterator != dataAddress.end())
            {
                address.push_back(*addressIterator++);
            }
            else
            {
                break;
            }
        }

        return flags;
    }

    //=========================================================================
    DataPatch::Flags SliceComponent::GetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress) const
    {
        if (const DataFlagsPerEntity* dataFlagsPerEntity = GetCorrectBundleOfDataFlags(entityId))
        {
            return dataFlagsPerEntity->GetEntityDataFlagsAtAddress(entityId, dataAddress);
        }
        return 0;
    }

    //=========================================================================
    bool SliceComponent::SetEntityDataFlagsAtAddress(EntityId entityId, const DataPatch::AddressType& dataAddress, DataPatch::Flags flags)
    {
        if (DataFlagsPerEntity* dataFlagsPerEntity = GetCorrectBundleOfDataFlags(entityId))
        {
            return dataFlagsPerEntity->SetEntityDataFlagsAtAddress(entityId, dataAddress, flags);
        }
        return false;
    }

    //=========================================================================
    // SliceComponent::GetInstanceMetadataEntities
    //=========================================================================
    bool SliceComponent::GetInstanceMetadataEntities(EntityList& outMetadataEntities)
    {
        bool result = true;

        // make sure we have all entities instantiated
        if (Instantiate() != InstantiateResult::Success)
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                outMetadataEntities.push_back(instance.GetMetadataEntity());
            }
        }
        return result;
    }

    //=========================================================================
    // SliceComponent::GetAllInstanceMetadataEntities
    //=========================================================================
    bool SliceComponent::GetAllInstanceMetadataEntities(EntityList& outMetadataEntities)
    {
        bool result = true;

        // make sure we have all entities instantiated
        if (Instantiate() != InstantiateResult::Success)
        {
            result = false;
        }

        // add all entities from base slices
        for (const SliceReference& slice : m_slices)
        {
            for (const SliceInstance& instance : slice.m_instances)
            {
                if (instance.m_instantiated)
                {
                    outMetadataEntities.insert(outMetadataEntities.end(), instance.m_instantiated->m_metadataEntities.begin(), instance.m_instantiated->m_metadataEntities.end());
                }
            }
        }

        return result;
    }

    //=========================================================================
    // SliceComponent::GetAllMetadataEntities
    //=========================================================================
    bool SliceComponent::GetAllMetadataEntities(EntityList& outMetadataEntities)
    {
        bool result = GetAllInstanceMetadataEntities(outMetadataEntities);

        // add the entity from this component
        outMetadataEntities.push_back(GetMetadataEntity());

        return result;
    }

    AZ::Entity* SliceComponent::GetMetadataEntity()
    {
        return &m_metadataEntity;
    }

    //=========================================================================
    // Clone
    //=========================================================================
    SliceComponent* SliceComponent::Clone(AZ::SerializeContext& serializeContext, SliceInstanceToSliceInstanceMap* sourceToCloneSliceInstanceMap) const
    {
        AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::AzCore);

        SliceComponent* clonedComponent = serializeContext.CloneObject(this);

        if (!clonedComponent)
        {
            AZ_Error("SliceAsset", false, "Failed to clone asset.");
            return nullptr;
        }

        clonedComponent->SetSerializeContext(&serializeContext);

        AZ_Assert(clonedComponent, "Cloned asset doesn't contain a slice component.");
        AZ_Assert(clonedComponent->GetSlices().size() == GetSlices().size(),
            "Cloned asset does not match source asset.");

        const SliceComponent::SliceList& myReferences = m_slices;
        SliceComponent::SliceList& clonedReferences = clonedComponent->m_slices;

        auto myReferencesIt = myReferences.begin();
        auto clonedReferencesIt = clonedReferences.begin();

        // For all slice references, clone instantiated instances.
        for (; myReferencesIt != myReferences.end(); ++myReferencesIt, ++clonedReferencesIt)
        {
            const SliceComponent::SliceReference::SliceInstances& myRefInstances = myReferencesIt->m_instances;
            SliceComponent::SliceReference::SliceInstances& clonedRefInstances = clonedReferencesIt->m_instances;

            AZ_Assert(myRefInstances.size() == clonedRefInstances.size(),
                "Cloned asset reference does not contain the same number of instances as the source asset reference.");

            auto myRefInstancesIt = myRefInstances.begin();
            auto clonedRefInstancesIt = clonedRefInstances.begin();

            for (; myRefInstancesIt != myRefInstances.end(); ++myRefInstancesIt, ++clonedRefInstancesIt)
            {
                const SliceComponent::SliceInstance& myRefInstance = (*myRefInstancesIt);
                SliceComponent::SliceInstance& clonedRefInstance = (*clonedRefInstancesIt);

                if (sourceToCloneSliceInstanceMap)
                {
                    SliceComponent::SliceInstanceAddress sourceAddress(const_cast<SliceComponent::SliceReference*>(&(*myReferencesIt)), const_cast<SliceComponent::SliceInstance*>(&myRefInstance));
                    SliceComponent::SliceInstanceAddress clonedAddress(&(*clonedReferencesIt), &clonedRefInstance);
                    (*sourceToCloneSliceInstanceMap)[sourceAddress] = clonedAddress;
                }

                // Slice instances should not support copying natively, but to clone we copy member-wise
                // and clone instantiated entities.
                clonedRefInstance.m_baseToNewEntityIdMap = myRefInstance.m_baseToNewEntityIdMap;
                clonedRefInstance.m_entityIdToBaseCache = myRefInstance.m_entityIdToBaseCache;
                clonedRefInstance.m_dataPatch = myRefInstance.m_dataPatch;
                clonedRefInstance.m_dataFlags.CopyDataFlagsFrom(myRefInstance.m_dataFlags);
                clonedRefInstance.m_instantiated = myRefInstance.m_instantiated ? serializeContext.CloneObject(myRefInstance.m_instantiated) : nullptr;
            }

            clonedReferencesIt->m_isInstantiated = myReferencesIt->m_isInstantiated;
            clonedReferencesIt->m_asset = myReferencesIt->m_asset;
            clonedReferencesIt->m_component = clonedComponent;
        }

        // Finally, mark the cloned asset as instantiated.
        clonedComponent->m_slicesAreInstantiated = IsInstantiated();

        return clonedComponent;
    }

    //=========================================================================
    // Reflect
    //=========================================================================
    void SliceComponent::Reflect(ReflectContext* reflection)
    {
        DataFlagsPerEntity::Reflect(reflection);

        SerializeContext* serializeContext = azrtti_cast<SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<SliceComponent, Component>()->
                Version(3)->
                EventHandler<SliceComponentSerializationEvents>()->
                Field("Entities", &SliceComponent::m_entities)->
                Field("Prefabs", &SliceComponent::m_slices)->
                Field("IsDynamic", &SliceComponent::m_isDynamic)->
                Field("MetadataEntity", &SliceComponent::m_metadataEntity)->
                Field("DataFlagsForNewEntities", &SliceComponent::m_dataFlagsForNewEntities); // added at v3

            serializeContext->Class<InstantiatedContainer>()->
                Version(2)->
                Field("Entities", &InstantiatedContainer::m_entities)->
                Field("MetadataEntities", &InstantiatedContainer::m_metadataEntities);

            serializeContext->Class<SliceInstance>()->
                Version(3)->
                Field("Id", &SliceInstance::m_instanceId)->
                Field("EntityIdMap", &SliceInstance::m_baseToNewEntityIdMap)->
                Field("DataPatch", &SliceInstance::m_dataPatch)->
                Field("DataFlags", &SliceInstance::m_dataFlags); // added at v3

            serializeContext->Class<SliceReference>()->
                Version(2, &Converters::SliceReferenceVersionConverter)->
                Field("Instances", &SliceReference::m_instances)->
                Field("Asset", &SliceReference::m_asset);

            serializeContext->Class<EntityRestoreInfo>()->
                Version(1)->
                Field("AssetId", &EntityRestoreInfo::m_assetId)->
                Field("InstanceId", &EntityRestoreInfo::m_instanceId)->
                Field("AncestorId", &EntityRestoreInfo::m_ancestorId)->
                Field("DataFlags", &EntityRestoreInfo::m_dataFlags); // added at v1
        }
    }
} // namespace AZ
