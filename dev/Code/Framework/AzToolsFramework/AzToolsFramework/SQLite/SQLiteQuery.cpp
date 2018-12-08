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

#include "SQLiteQuery.h"

namespace AzToolsFramework
{
    namespace SQLite
    {
        namespace Internal
        {
            bool Bind(Statement* statement, int index, const AZ::Uuid& value)
            {
                return statement->BindValueUuid(index, value);
            }

            bool Bind(Statement* statement, int index, double value)
            {
                return statement->BindValueDouble(index, value);
            }

            bool Bind(Statement* statement, int index, AZ::s32 value)
            {
                return statement->BindValueInt(index, value);
            }

            bool Bind(Statement* statement, int index, AZ::u32 value)
            {
                return statement->BindValueInt(index, value);
            }

            bool Bind(Statement* statement, int index, const char* value)
            {
                return statement->BindValueText(index, value);
            }

            bool Bind(Statement* statement, int index, AZ::s64 value)
            {
                return statement->BindValueInt64(index, value);
            }

            bool Bind(Statement* statement, int index, AZ::u64 value)
            {
                return statement->BindValueInt64(index, value);
            }

            bool Bind(Statement* statement, int index, const SqlBlob& value)
            {
                return statement->BindValueBlob(index, value.m_data, value.m_dataSize);
            }
        } // namespace Internal
    } // namespace SQLite
} // namespace AZFramework