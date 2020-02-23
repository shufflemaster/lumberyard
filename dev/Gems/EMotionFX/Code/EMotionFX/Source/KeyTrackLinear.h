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

#pragma once

// include required headers
#include <MCore/Source/StandardHeaders.h>
#include <MCore/Source/SmallArray.h>
#include <MCore/Source/Compare.h>

#include "EMotionFXConfig.h"
#include "KeyFrame.h"
#include "CompressedKeyFrames.h"
#include "KeyFrameFinder.h"


namespace EMotionFX
{
    /**
     * The keyframe track.
     * This is a class holding a set of keyframes.
     */
    template <class ReturnType, class StorageType>
    class KeyTrackLinear
    {
        MCORE_MEMORYOBJECTCATEGORY(KeyTrackLinear, EMFX_DEFAULT_ALIGNMENT, EMFX_MEMCATEGORY_MOTIONS_KEYTRACKS);

    public:
        /**
         * Default constructor.
         */
        KeyTrackLinear();

        /**
         * Constructor.
         * @param nrKeys The number of keyframes which the keytrack contains (preallocates this amount of keyframes).
         */
        KeyTrackLinear(uint32 nrKeys);

        /**
         * Destructor.
         */
        ~KeyTrackLinear();

        /**
         * Initialize all keyframes in this keytrack.
         * Call this after all keys are added and setup, otherwise the interpolation won't work and nothing will happen.
         */
        void Init();

        /**
         * Perform interpolation between two keyframes.
         * @param startKey The first keyframe index. The interpolation will take place between this keyframe and the one after this one.
         * @param currentTime The global time, in seconds. This time value has to be between the time value of the startKey and the one after that.
         * @result The interpolated value.
         */
        MCORE_INLINE ReturnType Interpolate(uint32 startKey, float currentTime) const;

        /**
         * Add a key to the track (at the back).
         * @param time The time of the keyframe, in seconds.
         * @param value The value of the keyframe.
         */
        MCORE_INLINE void AddKey(float time, const ReturnType& value);

        /**
         * Adds a key to the track, and automatically detects where to place it.
         * NOTE: you will have to call the Init() method again when you finished adding keys.
         * @param time The time value of the keyframe, in seconds.
         * @param value The value of the keyframe at that time.
         */
        MCORE_INLINE void AddKeySorted(float time, const ReturnType& value);

        /**
         * Remove a given keyframe, by number.
         * Do not forget that you have to re-initialize the keytrack after you have removed one or more
         * keyframes. The reason for this is that the possible tangents inside the interpolators have to be
         * recalculated when the key structure has changed.
         * @param keyNr The keyframe number, must be in range of [0..GetNumKeys()-1].
         */
        MCORE_INLINE void RemoveKey(uint32 keyNr);

        /**
         * Clear all keys.
         */
        void ClearKeys();

        /**
         * Check if a given keytrack is animated or not.
         * @param initialPose The base or initial pose to compare to. If the keyframe values are different from
         *                    this value, the keytrack is considered to be animated.
         * @param maxError The maximum error/difference between the specified initial pose and the keyframes.
         * @result True in case the keytrack is animated, otherwise false is returned.
         */
        MCORE_INLINE bool CheckIfIsAnimated(const ReturnType& initialPose, float maxError) const;

        /**
         * Return the interpolated value at the specified time.
         * @param currentTime The time in seconds.
         * @param cachedKey The keyframe number that should first be checked before finding the keyframes to
         *                  interpolate between using the keyframe finder. If this value is nullptr (default), this cached key is ignored.
         *                  The cached value will also be overwritten with the new cached key in case of a cache miss.
         * @param outWasCacheHit This output value will contain 0 when this method had an internal cache miss and a value of 1 in case it was a cache hit.
         * @result Returns the value at the specified time.
         */
        ReturnType GetValueAtTime(float currentTime) const;
        ReturnType GetValueAtTime(float currentTime, uint32& cachedKey, uint8& outWasCacheHit) const;

        /**
         * Get a given keyframe.
         * @param nr The index, so the keyframe number.
         * @result A pointer to the keyframe.
         */
        MCORE_INLINE KeyFrame<ReturnType, StorageType>* GetKey(uint32 nr);

        /**
         * Returns the first keyframe.
         * @result A pointer to the first keyframe.
         */
        MCORE_INLINE KeyFrame<ReturnType, StorageType>* GetFirstKey();

        /**
         * Returns the last keyframe.
         * @result A pointer to the last keyframe.
         */
        MCORE_INLINE KeyFrame<ReturnType, StorageType>* GetLastKey();

        /**
         * Get a given keyframe.
         * @param nr The index, so the keyframe number.
         * @result A pointer to the keyframe.
         */
        MCORE_INLINE const KeyFrame<ReturnType, StorageType>* GetKey(uint32 nr) const;

        /**
         * Returns the first keyframe.
         * @result A pointer to the first keyframe.
         */
        MCORE_INLINE const KeyFrame<ReturnType, StorageType>* GetFirstKey() const;

        /**
         * Returns the last keyframe.
         * @result A pointer to the last keyframe.
         */
        MCORE_INLINE const KeyFrame<ReturnType, StorageType>* GetLastKey() const;


        /**
         * Returns the time value of the first keyframe.
         * @result The time value of the first keyframe, in seconds.
         */
        MCORE_INLINE float GetFirstTime() const;

        /**
         * Return the time value of the last keyframe.
         * @result The time value of the last keyframe, in seconds.
         */
        MCORE_INLINE float GetLastTime() const;

        /**
         * Returns the number of keyframes in this track.
         * @result The number of currently stored keyframes.
         */
        MCORE_INLINE uint32 GetNumKeys() const;

        /**
         * Find a key at a given time.
         * @param curTime The time of the key.
         * @return A pointer to the keyframe.
         */
        MCORE_INLINE KeyFrame<ReturnType, StorageType>* FindKey(float curTime) const;

        /**
         * Find a key number at a given time.
         * You will need to interpolate between this and the next key.
         * @param curTime The time to retreive the key for.
         * @result Returns the key number or MCORE_INVALIDINDEX32 when not found.
         */
        MCORE_INLINE uint32 FindKeyNumber(float curTime) const;

        /**
         * Make the keytrack loopable, by adding a new keyframe at the end of the keytrack.
         * This added keyframe will have the same value as the first keyframe.
         * @param fadeTime The relative offset after the last keyframe. If this value is 0.5, it means it will add
         *                 a keyframe half a second after the last keyframe currently in the keytrack.
         */
        void MakeLoopable(float fadeTime = 0.3f);

        /**
         * Optimize the keytrack by removing redundent frames.
         * The way this is done is by comparing differences between the resulting curves when removing specific keyframes.
         * If the error (difference) between those curve before and after keyframe removal is within a given maximum error value, the keyframe can be
         * safely removed since there will not be much "visual" difference.
         * The metric of this maximum error depends on the ReturnType of the keytrack (so the return type values of the keyframes).
         * For vectors this will be the squared length between those two vectors.
         * Partial template specialization needs to be used to add support for optimizing different types, such as quaternions.
         * You can do this by creating a specialization of the MCore::Compare<ReturnType>::CheckIfIsClose(...) method. You can find it in the
         * MCore project in the file Compare.h. We provide implementations for several different types, such as vectors, quaternions and floats.
         * @param maxError The maximum allowed error value. The higher you set this value, the more keyframes will be removed.
         * @result The method returns the number of removed keyframes.
         */
        uint32 Optimize(float maxError);

        /**
         * Pre-allocate a given number of keys.
         * Please keep in mind that any existing keys will remain unchanged.
         * However, newly created keys will be uninitialized.
         * @param numKeys The number of keys to allocate.
         * @result Returns false if the number of keys couldnt be set (i.e. numKeys is too big)
         */
        bool SetNumKeys(uint32 numKeys);

        /**
         * Set the value of a key.
         * Please note that you have to make sure your keys remain in sorted order! (sorted on time value).
         * @param keyNr The keyframe number.
         * @param time The time value, in seconds.
         * @param value The value of the key.
         */
        MCORE_INLINE void SetKey(uint32 keyNr, float time, const ReturnType& value);

        /**
         * Set the storage type value of a key.
         * Please note that you have to make sure your keys remain in sorted order! (sorted on time value).
         * @param keyNr The keyframe number.
         * @param time The time value, in seconds.
         * @param value The storage type value of the key.
         */
        MCORE_INLINE void SetStorageTypeKey(uint32 keyNr, float time, const StorageType& value);

    protected:
        MCore::SmallArray< KeyFrame<ReturnType, StorageType> >  mKeys;          /**< The collection of keys which form the track. */
    };


    // include keytrack inline code
#include "KeyTrackLinear.inl"
} // namespace EMotionFX
