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
#pragma once

#include <LyShine/Animation/IUiAnimation.h>
#include "AnimTrack.h"
#include "AnimKey.h"

class CUiAnimStringTable
    : public IUiAnimStringTable
{
public:
    AZ_CLASS_ALLOCATOR(CUiAnimStringTable, AZ::SystemAllocator, 0);
    AZ_RTTI(CUiAnimStringTable, "{4640F535-0417-4BE6-A856-80A2C7D9E885}", IUiAnimStringTable);

    CUiAnimStringTable();
    ~CUiAnimStringTable();

    const char* Add(const char* p) override;

    //////////////////////////////////////////////////////////////////////////
    // for intrusive_ptr support
    void add_ref() override;
    void release() override;
    //////////////////////////////////////////////////////////////////////////

    static void Reflect(AZ::SerializeContext* serializeContext) {}

private:
    struct Page
    {
        Page* pPrev;
        char mem[512 - sizeof(Page*)];
    };

    typedef std::unordered_map<const char*, const char*, stl::hash_string<const char*>, stl::equality_string<const char*> > TableMap;

private:
    CUiAnimStringTable(const CUiAnimStringTable&);
    CUiAnimStringTable& operator = (const CUiAnimStringTable&);

private:
    int m_refCount;
    Page* m_pLastPage;
    char* m_pEnd;
    TableMap m_table;
};

class CUiTrackEventTrack
    : public TUiAnimTrack<IEventKey>
{
public:
    AZ_CLASS_ALLOCATOR(CUiTrackEventTrack, AZ::SystemAllocator, 0);
    AZ_RTTI(CUiTrackEventTrack, "{18AB327E-02EA-43D9-BA3B-FB93B6C15837}", IUiAnimTrack);

    explicit CUiTrackEventTrack(IUiAnimStringTable* pStrings);
    CUiTrackEventTrack();     // default constr needed for AZ Serialization

    //////////////////////////////////////////////////////////////////////////
    // Overrides of IAnimTrack.
    //////////////////////////////////////////////////////////////////////////
    void GetKeyInfo(int key, const char*& description, float& duration);
    void SerializeKey(IEventKey& key, XmlNodeRef& keyNode, bool bLoading);
    void SetKey(int index, IKey* key);
    void InitPostLoad(IUiAnimSequence* sequence) override;

    static void Reflect(AZ::SerializeContext* serializeContext);

private:
    AZStd::intrusive_ptr<IUiAnimStringTable> m_pStrings;
};
