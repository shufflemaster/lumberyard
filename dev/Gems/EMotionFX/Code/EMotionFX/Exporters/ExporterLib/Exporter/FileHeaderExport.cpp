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

#include "Exporter.h"

#include <EMotionFX/Source/Importer/ActorFileFormat.h>
#include <EMotionFX/Source/Importer/MotionFileFormat.h>
//#include <EMotionFX/Source/Importer/XPMFileFormat.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionFX/Source/WaveletSkeletalMotion.h>
#include <EMotionFX/Source/Actor.h>


namespace ExporterLib
{
    void SaveActorHeader(MCore::Stream* file, MCore::Endian::EEndianType targetEndianType)
    {
        // the header information
        EMotionFX::FileFormat::Actor_Header header;
        memset(&header, 0, sizeof(EMotionFX::FileFormat::Actor_Header));
        header.mFourcc[0] = 'A';
        header.mFourcc[1] = 'C';
        header.mFourcc[2] = 'T';
        header.mFourcc[3] = 'R';
        header.mHiVersion   = static_cast<uint8>(GetFileHighVersion());
        header.mLoVersion   = static_cast<uint8>(GetFileLowVersion());
        header.mEndianType  = static_cast<uint8>(targetEndianType);
        //header.mMulOrder  = EMotionFX::FileFormat::MULORDER_ROT_SCALE_TRANS;

        // write the header to the stream
        file->Write(&header, sizeof(EMotionFX::FileFormat::Actor_Header));
    }


    void SaveActorFileInfo(MCore::Stream* file,
        uint32 numLODLevels,
        uint32 motionExtractionNodeIndex,
        uint32 retargetRootNodeIndex,
        const char* sourceApp,
        const char* orgFileName,
        const char* actorName,
        MCore::Distance::EUnitType unitType,
        MCore::Endian::EEndianType targetEndianType)
    {
        // chunk header
        EMotionFX::FileFormat::FileChunk chunkHeader;
        chunkHeader.mChunkID        = EMotionFX::FileFormat::ACTOR_CHUNK_INFO;

        chunkHeader.mSizeInBytes    = sizeof(EMotionFX::FileFormat::Actor_Info2);
        chunkHeader.mSizeInBytes    += GetStringChunkSize(sourceApp);
        chunkHeader.mSizeInBytes    += GetStringChunkSize(orgFileName);
        chunkHeader.mSizeInBytes    += GetStringChunkSize(GetCompilationDate());
        chunkHeader.mSizeInBytes    += GetStringChunkSize(actorName);

        chunkHeader.mVersion        = 2;

        EMotionFX::FileFormat::Actor_Info2 infoChunk;
        memset(&infoChunk, 0, sizeof(EMotionFX::FileFormat::Actor_Info2));
        infoChunk.mNumLODs                      = numLODLevels;
        infoChunk.mMotionExtractionNodeIndex    = motionExtractionNodeIndex;
        infoChunk.mRetargetRootNodeIndex        = retargetRootNodeIndex;
        infoChunk.mExporterHighVersion          = static_cast<uint8>(EMotionFX::GetEMotionFX().GetHighVersion());
        infoChunk.mExporterLowVersion           = static_cast<uint8>(EMotionFX::GetEMotionFX().GetLowVersion());
        infoChunk.mUnitType                     = static_cast<uint8>(unitType);

        // print repositioning node information
        MCore::LogDetailedInfo("- File Info");
        MCore::LogDetailedInfo("   + Actor Name: '%s'", actorName);
        MCore::LogDetailedInfo("   + Source Application: '%s'", sourceApp);
        MCore::LogDetailedInfo("   + Original File: '%s'", orgFileName);
        MCore::LogDetailedInfo("   + Exporter Version: v%d.%d", infoChunk.mExporterHighVersion, infoChunk.mExporterLowVersion);
        MCore::LogDetailedInfo("   + Exporter Compilation Date: '%s'", GetCompilationDate());
        MCore::LogDetailedInfo("   + Num LODs = %d", infoChunk.mNumLODs);
        MCore::LogDetailedInfo("   + Motion extraction node index = %d", infoChunk.mMotionExtractionNodeIndex);
        MCore::LogDetailedInfo("   + Retarget root node index = %d", infoChunk.mRetargetRootNodeIndex);

        // endian conversion
        ConvertFileChunk(&chunkHeader, targetEndianType);
        ConvertUnsignedInt(&infoChunk.mMotionExtractionNodeIndex, targetEndianType);
        ConvertUnsignedInt(&infoChunk.mRetargetRootNodeIndex, targetEndianType);
        ConvertUnsignedInt(&infoChunk.mNumLODs, targetEndianType);

        file->Write(&chunkHeader, sizeof(EMotionFX::FileFormat::FileChunk));
        file->Write(&infoChunk, sizeof(EMotionFX::FileFormat::Actor_Info2));

        SaveString(sourceApp, file, targetEndianType);
        SaveString(orgFileName, file, targetEndianType);
        // Save an empty string as the compilation date (Don't need that information anymore).
        SaveString("", file, targetEndianType);
        SaveString(actorName, file, targetEndianType);
    }


    void SaveSkeletalMotionHeader(MCore::Stream* file, EMotionFX::Motion* motion, MCore::Endian::EEndianType targetEndianType)
    {
        // the header information
        EMotionFX::FileFormat::Motion_Header header;
        memset(&header, 0, sizeof(EMotionFX::FileFormat::Motion_Header));

        header.mFourcc[0]   = 'M';
        header.mFourcc[1]   = 'O';
        header.mFourcc[2]   = 'T';

        switch (motion->GetType())
        {
        case EMotionFX::SkeletalMotion::TYPE_ID:
        {
            header.mFourcc[3] = ' ';
            break;
        }
        case EMotionFX::WaveletSkeletalMotion::TYPE_ID:
        {
            header.mFourcc[3] = 'W';
            break;
        }
        default:
        {
            header.mFourcc[3] = '?';
        }
        }

        header.mHiVersion   = static_cast<uint8>(GetFileHighVersion());
        header.mLoVersion   = static_cast<uint8>(GetFileLowVersion());
        header.mEndianType  = static_cast<uint8>(targetEndianType);

        // write the header to the stream
        file->Write(&header, sizeof(EMotionFX::FileFormat::Motion_Header));
    }


    void SaveSkeletalMotionFileInfo(MCore::Stream* file, EMotionFX::SkeletalMotion* motion, MCore::Endian::EEndianType targetEndianType)
    {
        // chunk header
        EMotionFX::FileFormat::FileChunk chunkHeader;
        chunkHeader.mChunkID      = EMotionFX::FileFormat::MOTION_CHUNK_INFO;
        chunkHeader.mSizeInBytes  = sizeof(EMotionFX::FileFormat::Motion_Info3);
        chunkHeader.mVersion      = 3;

        EMotionFX::FileFormat::Motion_Info3 infoChunk;
        memset(&infoChunk, 0, sizeof(EMotionFX::FileFormat::Motion_Info3));

        infoChunk.mMotionExtractionFlags    = motion->GetMotionExtractionFlags();
        infoChunk.mMotionExtractionNodeIndex= MCORE_INVALIDINDEX32; // not used anymore
        infoChunk.mUnitType                 = static_cast<uint8>(motion->GetUnitType());
        infoChunk.mIsAdditive               = motion->GetIsAdditive() ? 1 : 0;

        MCore::LogDetailedInfo("- File Info");
        MCore::LogDetailedInfo("   + Exporter Compilation Date    = '%s'", GetCompilationDate());
        MCore::LogDetailedInfo("   + Motion Extraction Flags      = 0x%x [capZ=%d]", infoChunk.mMotionExtractionFlags, (infoChunk.mMotionExtractionFlags & EMotionFX::MOTIONEXTRACT_CAPTURE_Z) ? 1 : 0);

        // endian conversion
        ConvertFileChunk(&chunkHeader, targetEndianType);
        ConvertUnsignedInt(&infoChunk.mMotionExtractionFlags, targetEndianType);
        ConvertUnsignedInt(&infoChunk.mMotionExtractionNodeIndex, targetEndianType);

        file->Write(&chunkHeader, sizeof(EMotionFX::FileFormat::FileChunk));
        file->Write(&infoChunk, sizeof(EMotionFX::FileFormat::Motion_Info2));
    }
} // namespace ExporterLib
