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

#include <AzCore/std/any.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/string/string.h>

#include <ExpressionEvaluation/ExpressionEngine/ExpressionTypes.h>
#include <ExpressionEngine/InternalTypes.h>

namespace ExpressionEvaluation
{
    // Interface for expanding the available grammar for parsing.
    //
    // Must handle both parsing and execution.
    class ExpressionElementParser
    {
    public:
        AZ_CLASS_ALLOCATOR(ExpressionElementParser, AZ::SystemAllocator, 0);

        struct ParseResult
        {
            // Returns the number of characters that the parser wants to consume from the input text.
            size_t m_charactersConsumed = 0;

            ElementInformation m_element;
        };

    protected:
        ExpressionElementParser() = default;
    
    public:

        virtual ~ExpressionElementParser() = default;
        
        virtual ExpressionParserId GetParserId() const = 0;

        // Attempt to parse the specified element in the text at the specified offset.
        virtual ParseResult ParseElement(const AZStd::string& inputText, size_t offset) const = 0;

        // Evaluate the given token using the current evaluation stack.
        virtual void EvaluateToken(const ElementInformation& parseResult, ExpressionResultStack& evaluationStack) const = 0;
    };
}
