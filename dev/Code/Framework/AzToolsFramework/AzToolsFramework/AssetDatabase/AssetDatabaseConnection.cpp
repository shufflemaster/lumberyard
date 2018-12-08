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

#include "StdAfx.h"
#include <AzToolsFramework/AssetDatabase/AssetDatabaseConnection.h>

#include <AzCore/IO/SystemFile.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/SQLite/SQLiteConnection.h>
#include <AzToolsFramework/API/AssetDatabaseBus.h>
#include <AzToolsFramework/SQLite/SQLiteQuery.h>

namespace AzToolsFramework
{
    namespace AssetDatabase
    {
        using namespace AzFramework;
        using namespace AzToolsFramework::SQLite;

        //since we can be derived from, prefix the namespace to avoid statement name collisions
        namespace
        {
            static const char* LOG_NAME = "AzToolsFramework::AssetDatabase";

            //////////////////////////////////////////////////////////////////////////
            //table queries
            static const char* QUERY_DATABASEINFO_TABLE = "AzToolsFramework::AssetDatabase::QueryDatabaseInfoTable";
            static const char* QUERY_DATABASEINFO_TABLE_STATEMENT =
                "SELECT * from dbinfo;";

            static const auto s_queryDatabaseinfoTable = MakeSqlQuery("dbinfo", QUERY_DATABASEINFO_TABLE, QUERY_DATABASEINFO_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_SCANFOLDERS_TABLE = "AzToolsFramework::AssetDatabase::QueryScanFoldersTable";
            static const char* QUERY_SCANFOLDERS_TABLE_STATEMENT =
                "SELECT * from ScanFolders;";

            static const auto s_queryScanfoldersTable = MakeSqlQuery("ScanFolders", QUERY_SCANFOLDERS_TABLE, QUERY_SCANFOLDERS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_SOURCES_TABLE = "AzToolsFramework::AssetDatabase::QuerySourcesTable";
            static const char* QUERY_SOURCES_TABLE_STATEMENT =
                "SELECT * from Sources;";

            static const auto s_querySourcesTable = MakeSqlQuery("Sources", QUERY_SOURCES_TABLE, QUERY_SOURCES_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_JOBS_TABLE = "AzToolsFramework::AssetDatabase::QueryJobsTable";
            static const char* QUERY_JOBS_TABLE_STATEMENT =
                "SELECT * from Jobs;";

            static const auto s_queryJobsTable = MakeSqlQuery("Jobs", QUERY_JOBS_TABLE, QUERY_JOBS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_JOBS_TABLE_PLATFORM = "AzToolsFramework::AssetDatabase::QueryJobsTablePlatform";
            static const char* QUERY_JOBS_TABLE_PLATFORM_STATEMENT =
                "SELECT * from Jobs WHERE "
                "Platform = :platform;";

            static const auto s_queryJobsTablePlatform = MakeSqlQuery("Jobs", QUERY_JOBS_TABLE_PLATFORM, QUERY_JOBS_TABLE_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCTS_TABLE = "AzToolsFramework::AssetDatabase::QueryProductsTable";
            static const char* QUERY_PRODUCTS_TABLE_STATEMENT =
                "SELECT * from Products INNER JOIN Jobs "
                "ON Products.JobPK = Jobs.JobID;";

            static const auto s_queryProductsTable = MakeSqlQuery("Products", QUERY_PRODUCTS_TABLE, QUERY_PRODUCTS_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_PRODUCTS_TABLE_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductsTablePlatform";
            static const char* QUERY_PRODUCTS_TABLE_PLATFORM_STATEMENT =
                "SELECT * from Products INNER JOIN Jobs "
                "ON Products.JobPK = Jobs.JobID WHERE "
                "Jobs.Platform = :platform;";

            static const auto s_queryProductsTablePlatform = MakeSqlQuery("Products", QUERY_PRODUCTS_TABLE_PLATFORM, QUERY_PRODUCTS_TABLE_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_LEGACYSUBIDSBYPRODUCTID = "AzToolsFramework::AssetDatabase::QueryLegacySubIDsByProductID";
            static const char* QUERY_LEGACYSUBIDSBYPRODUCTID_STATEMENT =
                "SELECT * from LegacySubIDs "
                " WHERE "
                "   LegacySubIDs.ProductPK = :productId;";

            static const auto s_queryLegacysubidsbyproductid = MakeSqlQuery("LegacySubIDs", QUERY_LEGACYSUBIDSBYPRODUCTID, QUERY_LEGACYSUBIDSBYPRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productId"));

            static const char* QUERY_PRODUCTDEPENDENCIES_TABLE = "AzToolsFramework::AssetDatabase::QueryProductDependencies";
            static const char* QUERY_PRODUCTDEPENDENCIES_TABLE_STATEMENT =
                "SELECT ProductDependencies.*, SourceGUID, SubID FROM ProductDependencies "
                "INNER JOIN Products ON ProductPK = ProductID "
                "INNER JOIN Jobs ON JobPK = JobID "
                "INNER JOIN Sources ON SourcePK = SourceID;";

            static const auto s_queryProductdependenciesTable = MakeSqlQuery("ProductDependencies", QUERY_PRODUCTDEPENDENCIES_TABLE, QUERY_PRODUCTDEPENDENCIES_TABLE_STATEMENT, LOG_NAME);

            static const char* QUERY_FILES_TABLE = "AzToolsFramework::AssetDatabase::QueryFilesTable";
            static const char* QUERY_FILES_TABLE_STATEMENT =
                "SELECT * from Files;";

            static const auto s_queryFilesTable = MakeSqlQuery("QueryFiles", QUERY_FILES_TABLE, QUERY_FILES_TABLE_STATEMENT, LOG_NAME);

            //////////////////////////////////////////////////////////////////////////
            //projection and combination queries

            // lookup by primary key
            static const char* QUERY_SCANFOLDER_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryScanfolderByScanfolderID";
            static const char* QUERY_SCANFOLDER_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "ScanFolderID = :scanfolderid;";

            static const auto s_queryScanfolderByScanfolderid = MakeSqlQuery("ScanFolders", QUERY_SCANFOLDER_BY_SCANFOLDERID, QUERY_SCANFOLDER_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SCANFOLDER_BY_DISPLAYNAME = "AzToolsFramework::AssetDatabase::QueryScanfolderByDisplayName";
            static const char* QUERY_SCANFOLDER_BY_DISPLAYNAME_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "DisplayName = :displayname;";

            static const auto s_queryScanfolderByDisplayname = MakeSqlQuery("ScanFolders", QUERY_SCANFOLDER_BY_DISPLAYNAME, QUERY_SCANFOLDER_BY_DISPLAYNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":displayname"));

            static const char* QUERY_SCANFOLDER_BY_PORTABLEKEY = "AzToolsFramework::AssetDatabase::QueryScanfolderByPortableKey";
            static const char* QUERY_SCANFOLDER_BY_PORTABLEKEY_STATEMENT =
                "SELECT * FROM ScanFolders WHERE "
                "PortableKey = :portablekey;";

            static const auto s_queryScanfolderByPortablekey = MakeSqlQuery("ScanFolders", QUERY_SCANFOLDER_BY_PORTABLEKEY, QUERY_SCANFOLDER_BY_PORTABLEKEY_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":portablekey"));

            // lookup by primary key
            static const char* QUERY_SOURCE_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceID";
            static const char* QUERY_SOURCE_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceID = :sourceid;";

            static const auto s_querySourceBySourceid = MakeSqlQuery("Sources", QUERY_SOURCE_BY_SOURCEID, QUERY_SOURCE_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_SOURCE_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QuerySourceByScanFolderID";
            static const char* QUERY_SOURCE_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "ScanFolderPK = :scanfolderid;";

            static const auto s_querySourceByScanfolderid = MakeSqlQuery("Sources", QUERY_SOURCE_BY_SCANFOLDERID, QUERY_SOURCE_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SOURCE_BY_SOURCEGUID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceGuid";
            static const char* QUERY_SOURCE_BY_SOURCEGUID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceGuid = :sourceguid;";

            static const auto s_querySourceBySourceguid = MakeSqlQuery("Sources", QUERY_SOURCE_BY_SOURCEGUID, QUERY_SOURCE_BY_SOURCEGUID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::Uuid>(":sourceguid"));

            static const char* QUERY_SOURCE_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QuerySourceBySourceName";
            static const char* QUERY_SOURCE_BY_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName = :sourcename;";

            static const auto s_querySourceBySourcename = MakeSqlQuery("Sources", QUERY_SOURCE_BY_SOURCENAME, QUERY_SOURCE_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            static const char* QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QuerySourceBySourceNameScanFolderID";
            static const char* QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName = :sourcename AND "
                "ScanFolderPK = :scanfolderid;";

            static const auto s_querySourceBySourcenameScanfolderid = MakeSqlQuery("Sources", QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID, QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_SOURCE_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QuerySourceLikeSourceName";
            static const char* QUERY_SOURCE_LIKE_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources WHERE "
                "SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_querySourceLikeSourcename = MakeSqlQuery("Sources", QUERY_SOURCE_LIKE_SOURCENAME, QUERY_SOURCE_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            // lookup by primary key
            static const char* QUERY_JOB_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryJobByJobID";
            static const char* QUERY_JOB_BY_JOBID_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobID = :jobid;";

            static const auto s_queryJobByJobid = MakeSqlQuery("Jobs", QUERY_JOB_BY_JOBID, QUERY_JOB_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_JOB_BY_JOBKEY = "AzToolsFramework::AssetDatabase::QueryJobByJobKey";
            static const char* QUERY_JOB_BY_JOBKEY_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobKey = :jobKey;";

            static const auto s_queryJobByJobkey = MakeSqlQuery("Jobs", QUERY_JOB_BY_JOBKEY, QUERY_JOB_BY_JOBKEY_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":jobKey"));

            static const char* QUERY_JOB_BY_JOBRUNKEY = "AzToolsFramework::AssetDatabase::QueryJobByJobRunKey";
            static const char* QUERY_JOB_BY_JOBRUNKEY_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "JobRunKey = :jobrunkey;";

            static const auto s_queryJobByJobrunkey = MakeSqlQuery("Jobs", QUERY_JOB_BY_JOBRUNKEY, QUERY_JOB_BY_JOBRUNKEY_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u64>(":jobrunkey"));

            static const char* QUERY_JOB_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryJobByProductID";
            static const char* QUERY_JOB_BY_PRODUCTID_STATEMENT =
                "SELECT Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryJobByProductid = MakeSqlQuery("Jobs", QUERY_JOB_BY_PRODUCTID, QUERY_JOB_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_JOB_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryJobBySourceID";
            static const char* QUERY_JOB_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "SourcePK = :sourceid;";

            static const auto s_queryJobBySourceid = MakeSqlQuery("Jobs", QUERY_JOB_BY_SOURCEID, QUERY_JOB_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_JOB_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryJobBySourceIDPlatform";
            static const char* QUERY_JOB_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs WHERE "
                "SourcePK = :sourceid AND "
                "Platform = :platform;";

            static const auto s_queryJobBySourceidPlatform = MakeSqlQuery("Jobs", QUERY_JOB_BY_SOURCEID_PLATFORM, QUERY_JOB_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            // lookup by primary key
            static const char* QUERY_PRODUCT_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryProductByProductID";
            static const char* QUERY_PRODUCT_BY_PRODUCTID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryProductByProductid = MakeSqlQuery("Products", QUERY_PRODUCT_BY_PRODUCTID, QUERY_PRODUCT_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_PRODUCT_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryProductByJobID";
            static const char* QUERY_PRODUCT_BY_JOBID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.JobPK = :jobid;";

            static const auto s_queryProductByJobid = MakeSqlQuery("Products", QUERY_PRODUCT_BY_JOBID, QUERY_PRODUCT_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_PRODUCT_BY_JOBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductByJobIDPlatform";
            static const char* QUERY_PRODUCT_BY_JOBID_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.JobPK = :jobid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryProductByJobidPlatform = MakeSqlQuery("Products", QUERY_PRODUCT_BY_JOBID_PLATFORM, QUERY_PRODUCT_BY_JOBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryProductBySourceID";
            static const char* QUERY_PRODUCT_BY_SOURCEID_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.SourcePK = :sourceid;";

            static const auto s_queryProductBySourceid = MakeSqlQuery("Jobs", QUERY_PRODUCT_BY_SOURCEID, QUERY_PRODUCT_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_PRODUCT_BY_SOURCEGUID_SUBID = "AzToolsFramework::AssetDatabase::QueryProductBySourceGuidSubid";
            static const char* QUERY_PRODUCT_BY_SOURCEGUID_SUBID_STATEMENT =
                "SELECT Sources.SourceGuid, Products.* FROM Sources INNER JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceGuid = :sourceguid AND Products.SubID = :productsubid;";

            static const auto s_queryProductBySourceGuidSubid = MakeSqlQuery("Products", QUERY_PRODUCT_BY_SOURCEGUID_SUBID, QUERY_PRODUCT_BY_SOURCEGUID_SUBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::Uuid>(":sourceguid"),
                    SqlParam<AZ::u32>(":productsubid"));

            static const char* QUERY_PRODUCT_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductBySourceIDPlatform";
            static const char* QUERY_PRODUCT_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.SourcePK = :sourceid AND "
                "Platform = :platform;";

            static const auto s_queryProductBySourceidPlatform = MakeSqlQuery("Products", QUERY_PRODUCT_BY_SOURCEID_PLATFORM, QUERY_PRODUCT_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryProductByProductName";
            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductName = :productname;";

            static const auto s_queryProductByProductname = MakeSqlQuery("Products", QUERY_PRODUCT_BY_PRODUCTNAME, QUERY_PRODUCT_BY_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductByProductNamePlatform";
            static const char* QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName = :productname;";

            static const auto s_queryProductByProductnamePlatform = MakeSqlQuery("Products", QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM, QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryProductLikeProductName";
            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeProductname = MakeSqlQuery("Products", QUERY_PRODUCT_LIKE_PRODUCTNAME, QUERY_PRODUCT_LIKE_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductLikeProductNamePlatform";
            static const char* QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Jobs INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeProductnamePlatform = MakeSqlQuery("Products", QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM, QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_PRODUCT_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryProductBySourceName";
            static const char* QUERY_PRODUCT_BY_SOURCENAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryProductBySourcename = MakeSqlQuery("Products", QUERY_PRODUCT_BY_SOURCENAME, QUERY_PRODUCT_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name
            static const char* QUERY_PRODUCT_BY_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductBySourceNamePlatform";
            static const char* QUERY_PRODUCT_BY_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryProductBySourcenamePlatform = MakeSqlQuery("Products", QUERY_PRODUCT_BY_SOURCENAME_PLATFORM, QUERY_PRODUCT_BY_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryProductLikeSourceName";
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeSourcename = MakeSqlQuery("Products", QUERY_PRODUCT_LIKE_SOURCENAME, QUERY_PRODUCT_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name and platform
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryProductLikeSourceNamePlatform";
            static const char* QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT Products.*, Jobs.* FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryProductLikeSourcenamePlatform = MakeSqlQuery("Products", QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM, QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                SqlParam<const char*>(":sourcename"),
                SqlParam<const char*>(":platform"));

            // JobPK and subid together uniquely identify a product.  Since JobPK is indexed, this is a fast query if you happen to know those details.
            static const char* QUERY_PRODUCT_BY_JOBID_SUBID = "AzToolsFramework::AssetDatabase::QueryProductByJobIDSubID";
            static const char* QUERY_PRODUCT_BY_JOBID_SUBID_STATEMENT =
                "SELECT * FROM Products "
                "WHERE JobPK = :jobpk "
                "AND SubID = :subid;";

            static const auto s_queryProductByJobIdSubId = MakeSqlQuery("Products", QUERY_PRODUCT_BY_JOBID_SUBID, QUERY_PRODUCT_BY_JOBID_SUBID_STATEMENT, LOG_NAME,
                SqlParam<AZ::s64>(":jobpk"),
                SqlParam<AZ::u32>(":subid"));

            //add sql statement for querying everything by platform
            static const char* QUERY_COMBINED = "AzToolsFramework::AssetDatabase::QueryCombined";
            static const char* QUERY_COMBINED_STATEMENT =
                "SELECT * FROM ScanFolders INNER JOIN Sources "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK;";

            static const auto s_queryCombined = MakeSqlQuery("ScanFolders", QUERY_COMBINED, QUERY_COMBINED_STATEMENT, LOG_NAME);

            //add sql statement for querying everything by platform
            static const char* QUERY_COMBINED_BY_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByPlatform";
            static const char* QUERY_COMBINED_BY_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByPlatform = MakeSqlQuery("Jobs", QUERY_COMBINED_BY_PLATFORM, QUERY_COMBINED_BY_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_SOURCEID = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceID";
            static const char* QUERY_COMBINED_BY_SOURCEID_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceID = :sourceid;";

            static const auto s_queryCombinedBySourceid = MakeSqlQuery("Sources", QUERY_COMBINED_BY_SOURCEID, QUERY_COMBINED_BY_SOURCEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"));

            static const char* QUERY_COMBINED_BY_SOURCEID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceIDPlatform";
            static const char* QUERY_COMBINED_BY_SOURCEID_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceID = :sourceid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedBySourceidPlatform = MakeSqlQuery("Sources", QUERY_COMBINED_BY_SOURCEID_PLATFORM, QUERY_COMBINED_BY_SOURCEID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_JOBID = "AzToolsFramework::AssetDatabase::QueryCombinedByJobID";
            static const char* QUERY_COMBINED_BY_JOBID_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.JobID = :jobid;";

            static const auto s_queryCombinedByJobid = MakeSqlQuery("Jobs", QUERY_COMBINED_BY_JOBID, QUERY_COMBINED_BY_JOBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"));

            static const char* QUERY_COMBINED_BY_JOBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByJobIDPlatform";
            static const char* QUERY_COMBINED_BY_JOBID_PLATFORM_STATEMENT =
                "SELECT * FROM Jobs LEFT JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.JobID = :jobid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByJobidPlatform = MakeSqlQuery("Jobs", QUERY_COMBINED_BY_JOBID_PLATFORM, QUERY_COMBINED_BY_JOBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":jobid"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_COMBINED_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryCombinedByProcductID";
            static const char* QUERY_COMBINED_BY_PRODUCTID_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductID = :productid;";

            static const auto s_queryCombinedByProductid = MakeSqlQuery("Products", QUERY_COMBINED_BY_PRODUCTID, QUERY_COMBINED_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_COMBINED_BY_PRODUCTID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByProductIDPlatform";
            static const char* QUERY_COMBINED_BY_PRODUCTID_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductID = :productid AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByProductidPlatform = MakeSqlQuery("Products", QUERY_COMBINED_BY_PRODUCTID_PLATFORM, QUERY_COMBINED_BY_PRODUCTID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"),
                    SqlParam<const char*>(":platform"));


            //add sql statement for querying everything by source guid and product subid
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceGuidProductSubID";
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.SubID = :productsubid AND "
                "(Sources.SourceGuid = :sourceguid OR "
                "Products.LegacyGuid = :sourceguid);";

            static const auto s_queryCombinedBySourceguidProductsubid = MakeSqlQuery("Products", QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID, QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u32>(":productsubid"),
                    SqlParam<AZ::Uuid>(":sourceguid"));

            //add sql statement for querying everything by source guid and product subid and platform
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceGuidProductSubIDPlatform";
            static const char* QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK  WHERE "
                "Products.SubID = :productsubid AND "
                "(Sources.SourceGuid = :sourceguid OR "
                "Products.LegacyGuid = :soruceguid) AND "
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedBySourceguidProductsubidPlatform = MakeSqlQuery("Products", QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM, QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<AZ::u32>(":productsubid"),
                    SqlParam<AZ::Uuid>(":sourceguid"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_BY_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceName";
            static const char* QUERY_COMBINED_BY_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryCombinedBySourcename = MakeSqlQuery("Sources", QUERY_COMBINED_BY_SOURCENAME, QUERY_COMBINED_BY_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_BY_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedBySourceNamePlatform";
            static const char* QUERY_COMBINED_BY_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName = :sourcename;";

            static const auto s_queryCombinedBySourcenamePlatform = MakeSqlQuery("Sources", QUERY_COMBINED_BY_SOURCENAME_PLATFORM, QUERY_COMBINED_BY_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by source name
            static const char* QUERY_COMBINED_LIKE_SOURCENAME = "AzToolsFramework::AssetDatabase::QueryCombinedLikeSourceName";
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeSourcename = MakeSqlQuery("Sources", QUERY_COMBINED_LIKE_SOURCENAME, QUERY_COMBINED_LIKE_SOURCENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"));

            //add sql statement for querying everything by source name and platform
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedLikeSourceNamePlatform";
            static const char* QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM_STATEMENT =
                "SELECT * FROM Sources LEFT JOIN Jobs "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN Products "
                "ON Jobs.JobID = Products.JobPK WHERE "
                "Jobs.Platform = :platform AND "
                "Sources.SourceName LIKE :sourcename ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeSourcenamePlatform = MakeSqlQuery("Sources", QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM, QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":sourcename"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by product name
            static const char* QUERY_COMBINED_BY_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryCombinedByProductName";
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_STATEMENT =
                "SELECT * "
                "FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ScanFolders ON ScanFolders.ScanFolderID = Sources.SourceID "
                "WHERE "
                "Products.ProductName = :productname;";

            static const auto s_queryCombinedByProductname = MakeSqlQuery("Products", QUERY_COMBINED_BY_PRODUCTNAME, QUERY_COMBINED_BY_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            //add sql statement for querying everything by product name and platform
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedByProductNamePlatorm";
            static const char* QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT * "
                "FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ScanFolders ON ScanFolders.ScanFolderID = Sources.SourceID "
                "WHERE "
                "Products.ProductName = :productname AND"
                "Jobs.Platform = :platform;";

            static const auto s_queryCombinedByProductnamePlatform = MakeSqlQuery("Products", QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM, QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            //add sql statement for querying everything by product name
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME = "AzToolsFramework::AssetDatabase::QueryCombinedLikeProductName";
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeProductname = MakeSqlQuery("Products", QUERY_COMBINED_LIKE_PRODUCTNAME, QUERY_COMBINED_LIKE_PRODUCTNAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"));

            //add sql statement for querying everything by product name and platform
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM = "AzToolsFramework::AssetDatabase::QueryCombinedLikeProductNamePlatorm";
            static const char* QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM_STATEMENT =
                "SELECT * FROM Products LEFT JOIN Jobs "
                "ON Jobs.JobID = Products.JobPK INNER JOIN Sources "
                "ON Sources.SourceID = Jobs.SourcePK INNER JOIN ScanFolders "
                "ON ScanFolders.ScanFolderID = Sources.ScanFolderPK WHERE "
                "Jobs.Platform = :platform AND "
                "Products.ProductName LIKE :productname ESCAPE '|';"; // use the pipe to escape since its NOT a valid file path or operator

            static const auto s_queryCombinedLikeProductnamePlatform = MakeSqlQuery("Products", QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM, QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":productname"),
                    SqlParam<const char*>(":platform"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID = "AzToolsFramework::AssetDatabase::QuerySourceDependencyBySourceDependencyID";
            static const char* QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID_STATEMENT =
                "SELECT * FROM SourceDependency WHERE "
                "SourceDependencyID = :sourceDependencyid;";

            static const auto s_querySourcedependencyBySourcedependencyid = MakeSqlQuery("SourceDependency", QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID, QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":sourceDependencyid"));

            static const char* QUERY_SOURCEDEPENDENCY = "AzToolsFramework::AssetDatabase::QuerySourceDependency";
            static const char* QUERY_SOURCEDEPENDENCY_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "BuilderGuid = :builderGuid AND "
                "Source = :source AND "
                "DependsOnSource = :dependsOnSource;";

            static const auto s_querySourcedependency = MakeSqlQuery("SourceDependency", QUERY_SOURCEDEPENDENCY, QUERY_SOURCEDEPENDENCY_STATEMENT, LOG_NAME,
                    SqlParam<AZ::Uuid>(":builderGuid"),
                    SqlParam<const char*>(":source"),
                    SqlParam<const char*>(":dependsOnSource"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE = "AzToolsFramework::AssetDatabase::QuerySourceDependencyByDependsOnSource";
            static const char* QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "DependsOnSource = :dependsOnSource AND "
                "Source LIKE :dependentFilter;";
            static const auto s_querySourcedependencyByDependsonsource = MakeSqlQuery("SourceDependency", QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE, QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":dependsOnSource"));

            static const char* QUERY_DEPENDSONSOURCE_BY_SOURCE = "AzToolsFramework::AssetDatabase::QueryDependsOnSourceBySource";
            static const char* QUERY_DEPENDSONSOURCE_BY_SOURCE_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "Source = :source AND "
                "DependsOnSource LIKE :dependencyFilter;";

            static const auto s_queryDependsonsourceBySource = MakeSqlQuery("SourceDependency", QUERY_DEPENDSONSOURCE_BY_SOURCE, QUERY_DEPENDSONSOURCE_BY_SOURCE_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":source"));

            static const char* QUERY_SOURCEDEPENDENCY_BY_BUILDERGUID_SOURCE = "AzToolsFramework::AssetDatabase::QuerySourceDependencyByBuilderGUIDAndSource";
            static const char* QUERY_SOURCEDEPENDENCY_BY_BUILDERGUID_SOURCE_STATEMENT =
                "SELECT * from SourceDependency WHERE "
                "Source = :source AND "
                "BuilderGuid = :builderGuid;";

            static const auto s_querySourcedependencyByBuilderguidSource = MakeSqlQuery("SourceDependency", QUERY_SOURCEDEPENDENCY_BY_BUILDERGUID_SOURCE, QUERY_SOURCEDEPENDENCY_BY_BUILDERGUID_SOURCE_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":source"),
                    SqlParam<AZ::Uuid>(":builderGuid"));

            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID = "AzToolsFramework::AssetDatabase::QueryProductDependencyByProductDependencyID";
            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID_STATEMENT =
                "SELECT * FROM ProductDependencies WHERE "
                "ProductDependencyID = :productdependencyid;";

            static const auto s_queryProductdependencyByProductdependencyid = MakeSqlQuery("ProductDependencies", QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID, QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productdependencyid"));

            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID = "AzToolsFramework::AssetDatabase::QueryProductDependencyByProductID";
            static const char* QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID_STATEMENT =
                "SELECT * FROM ProductDependencies WHERE "
                "ProductPK = :productid;";

            static const auto s_queryProductdependencyByProductid = MakeSqlQuery("ProductDependencies", QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID, QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_DIRECT_PRODUCTDEPENDENCIES = "AzToolsFramework::AssetDatabase::QueryDirectProductDependencies";
            static const char* QUERY_DIRECT_PRODUCTDEPENDENCIES_STATEMENT =
                "SELECT * FROM Products "
                "LEFT OUTER JOIN Jobs ON Jobs.JobID = Products.JobPK "
                "LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "LEFT OUTER JOIN ProductDependencies "
                "  ON Sources.SourceGuid = ProductDependencies.DependencySourceGuid "
                "  AND Products.SubID = ProductDependencies.DependencySubID "
                "WHERE ProductDependencies.ProductPK = :productid;";

            static const auto s_queryDirectProductdependencies = MakeSqlQuery("Products", QUERY_DIRECT_PRODUCTDEPENDENCIES, QUERY_DIRECT_PRODUCTDEPENDENCIES_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            static const char* QUERY_ALL_PRODUCTDEPENDENCIES = "AzToolsFramework::AssetDatabase::QueryAllProductDependencies";
            static const char* QUERY_ALL_PRODUCTDEPENDENCIES_STATEMENT =
                "WITH RECURSIVE "
                "  allProductDeps(ProductID, JobPK, ProductName, SubID, AssetType, LegacyGuid) AS (  "
                "    SELECT * FROM Products "
                "    WHERE ProductID = :productid "
                "    UNION "
                "    SELECT P.ProductID, P.JobPK, P.ProductName, P.SubID, P.AssetType, P.LegacyGuid FROM Products P, allProductDeps"
                "    LEFT OUTER JOIN Jobs ON Jobs.JobID = P.JobPK "
                "    LEFT OUTER JOIN Sources ON Sources.SourceID = Jobs.SourcePK "
                "    LEFT OUTER JOIN ProductDependencies"
                "    ON Sources.SourceGuid = ProductDependencies.DependencySourceGuid "
                "    AND P.SubID = ProductDependencies.DependencySubID "
                "    WHERE ProductDependencies.ProductPK = allProductDeps.ProductID "
                "    LIMIT -1 OFFSET 1 " // This will ignore the first Product selected which is not a dependency but the root of the tree
                "  ) "
                "SELECT * FROM allProductDeps;";

            static const auto s_queryAllProductdependencies = MakeSqlQuery("Products", QUERY_ALL_PRODUCTDEPENDENCIES, QUERY_ALL_PRODUCTDEPENDENCIES_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":productid"));

            // lookup by primary key
            static const char* QUERY_FILE_BY_FILEID = "AzToolsFramework::AssetDatabase::QueryFileByFileID";
            static const char* QUERY_FILE_BY_FILEID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "FileID = :fileid;";

            static const auto s_queryFileByFileid = MakeSqlQuery("Files", QUERY_FILE_BY_FILEID, QUERY_FILE_BY_FILEID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":fileid"));

            static const char* QUERY_FILES_BY_FILENAME_AND_SCANFOLDER = "AzToolsFramework::AssetDatabase::QueryFilesByFileNameAndScanFolderID";
            static const char* QUERY_FILES_BY_FILENAME_AND_SCANFOLDER_STATEMENT =
                "SELECT * FROM Files WHERE "
                    "ScanFolderPK = :scanfolderpk AND "
                    "FileName = :filename;";

            static const auto s_queryFilesByFileName = MakeSqlQuery("Files", QUERY_FILES_BY_FILENAME_AND_SCANFOLDER, QUERY_FILES_BY_FILENAME_AND_SCANFOLDER_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderpk"),
                    SqlParam<const char*>(":filename") 
                );

            static const char* QUERY_FILES_LIKE_FILENAME = "AzToolsFramework::AssetDatabase::QueryFilesLikeFileName";
            static const char* QUERY_FILES_LIKE_FILENAME_STATEMENT =
                "SELECT * FROM Files WHERE "
                "FileName LIKE :filename ESCAPE '|';";

            static const auto s_queryFilesLikeFileName = MakeSqlQuery("Files", QUERY_FILES_LIKE_FILENAME, QUERY_FILES_LIKE_FILENAME_STATEMENT, LOG_NAME,
                    SqlParam<const char*>(":filename"));

            static const char* QUERY_FILES_BY_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryFilesByScanFolderID";
            static const char* QUERY_FILES_BY_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "ScanFolderPK = :scanfolderid;";

            static const auto s_queryFilesByScanfolderid = MakeSqlQuery("Files", QUERY_FILES_BY_SCANFOLDERID, QUERY_FILES_BY_SCANFOLDERID_STATEMENT, LOG_NAME,
                SqlParam<AZ::s64>(":scanfolderid"));

            static const char* QUERY_FILE_BY_FILENAME_SCANFOLDERID = "AzToolsFramework::AssetDatabase::QueryFileByFileNameScanFolderID";
            static const char* QUERY_FILE_BY_FILENAME_SCANFOLDERID_STATEMENT =
                "SELECT * FROM Files WHERE "
                "ScanFolderPK = :scanfolderid AND "
                "FileName = :filename;";

            static const auto s_queryFileByFileNameScanfolderid = MakeSqlQuery("Files", QUERY_FILE_BY_FILENAME_SCANFOLDERID, QUERY_FILE_BY_FILENAME_SCANFOLDERID_STATEMENT, LOG_NAME,
                    SqlParam<AZ::s64>(":scanfolderid"),
                    SqlParam<const char*>(":filename"));

            void PopulateJobInfo(AzToolsFramework::AssetSystem::JobInfo& jobinfo, JobDatabaseEntry& jobDatabaseEntry)
            {
                jobinfo.m_platform = AZStd::move(jobDatabaseEntry.m_platform);
                jobinfo.m_builderGuid = jobDatabaseEntry.m_builderGuid;
                jobinfo.m_jobKey = AZStd::move(jobDatabaseEntry.m_jobKey);
                jobinfo.m_status = jobDatabaseEntry.m_status;
                jobinfo.m_jobRunKey = jobDatabaseEntry.m_jobRunKey;
                jobinfo.m_firstFailLogTime = jobDatabaseEntry.m_firstFailLogTime;
                jobinfo.m_firstFailLogFile = AZStd::move(jobDatabaseEntry.m_firstFailLogFile);
                jobinfo.m_lastFailLogTime = jobDatabaseEntry.m_lastFailLogTime;
                jobinfo.m_lastFailLogFile = AZStd::move(jobDatabaseEntry.m_lastFailLogFile);
                jobinfo.m_lastLogTime = jobDatabaseEntry.m_lastLogTime;
                jobinfo.m_lastLogFile = AZStd::move(jobDatabaseEntry.m_lastLogFile);
                jobinfo.m_jobID = jobDatabaseEntry.m_jobID;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        //DatabaseInfoEntry
        DatabaseInfoEntry::DatabaseInfoEntry(AZ::s64 rowID, DatabaseVersion version)
            : m_rowID(rowID)
            , m_version(version)
        {
        }

        //////////////////////////////////////////////////////////////////////////
        //ScanFolderDatabaseEntry
        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(
            AZ::s64 scanFolderID,
            const char* scanFolder,
            const char* displayName,
            const char* portableKey,
            const char* outputPrefix,
            int isRoot)
            : m_scanFolderID(scanFolderID)
            , m_outputPrefix(outputPrefix)
            , m_isRoot(isRoot)
        {
            if (scanFolder)
            {
                m_scanFolder = scanFolder;
            }
            if (displayName)
            {
                m_displayName = displayName;
            }
            if (portableKey)
            {
                m_portableKey = portableKey;
            }
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(
            const char* scanFolder,
            const char* displayName,
            const char* portableKey,
            const char* outputPrefix,
            int isRoot)
            : m_scanFolderID(-1)
            , m_outputPrefix(outputPrefix)
            , m_isRoot(isRoot)
        {
            if (scanFolder)
            {
                m_scanFolder = scanFolder;
            }

            if (displayName)
            {
                m_displayName = displayName;
            }

            if (portableKey)
            {
                m_portableKey = portableKey;
            }
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(const ScanFolderDatabaseEntry& other)
            : m_scanFolderID(other.m_scanFolderID)
            , m_scanFolder(other.m_scanFolder)
            , m_displayName(other.m_displayName)
            , m_portableKey(other.m_portableKey)
            , m_outputPrefix(other.m_outputPrefix)
            , m_isRoot(other.m_isRoot)
        {
        }

        ScanFolderDatabaseEntry::ScanFolderDatabaseEntry(ScanFolderDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        ScanFolderDatabaseEntry& ScanFolderDatabaseEntry::operator=(ScanFolderDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_scanFolder = AZStd::move(other.m_scanFolder);
                m_scanFolderID = other.m_scanFolderID;
                m_displayName = AZStd::move(other.m_displayName);
                m_portableKey = AZStd::move(other.m_portableKey);
                m_outputPrefix = AZStd::move(other.m_outputPrefix);
                m_isRoot = other.m_isRoot;
            }
            return *this;
        }

        ScanFolderDatabaseEntry& ScanFolderDatabaseEntry::operator=(const ScanFolderDatabaseEntry& other)
        {
            m_scanFolder = other.m_scanFolder;
            m_scanFolderID = other.m_scanFolderID;
            m_displayName = other.m_displayName;
            m_portableKey = other.m_portableKey;
            m_outputPrefix = other.m_outputPrefix;
            m_isRoot = other.m_isRoot;
            return *this;
        }

        bool ScanFolderDatabaseEntry::operator==(const ScanFolderDatabaseEntry& other) const
        {
            // its the same database entry when the portable key is the same.
            return m_portableKey == other.m_portableKey;
        }

        AZStd::string ScanFolderDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ScanFolderDatabaseEntry id:%i path: %s, displayname: %s, portable key: %s",
                m_scanFolderID,
                m_scanFolder.c_str(),
                m_displayName.c_str(),
                m_portableKey.c_str());
        }

        //////////////////////////////////////////////////////////////////////////
        //SourceDatabaseEntry
        SourceDatabaseEntry::SourceDatabaseEntry(AZ::s64 sourceID, AZ::s64 scanFolderPK, const char* sourceName, AZ::Uuid sourceGuid)
            : m_sourceID(sourceID)
            , m_scanFolderPK(scanFolderPK)
            , m_sourceGuid(sourceGuid)
        {
            if (sourceName)
            {
                m_sourceName = sourceName;
            }
        }

        SourceDatabaseEntry::SourceDatabaseEntry(AZ::s64 scanFolderPK, const char* sourceName, AZ::Uuid sourceGuid)
            : m_sourceID(-1)
            , m_scanFolderPK(scanFolderPK)
            , m_sourceGuid(sourceGuid)
        {
            if (sourceName)
            {
                m_sourceName = sourceName;
            }
        }

        SourceDatabaseEntry::SourceDatabaseEntry(const SourceDatabaseEntry& other)
            : m_sourceID(other.m_sourceID)
            , m_scanFolderPK(other.m_scanFolderPK)
            , m_sourceName(other.m_sourceName)
            , m_sourceGuid(other.m_sourceGuid)
        {
        }

        SourceDatabaseEntry::SourceDatabaseEntry(SourceDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        SourceDatabaseEntry& SourceDatabaseEntry::operator=(SourceDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_sourceID = other.m_sourceID;
                m_scanFolderPK = other.m_scanFolderPK;
                m_sourceName = AZStd::move(other.m_sourceName);
                m_sourceGuid = other.m_sourceGuid;
            }
            return *this;
        }

        SourceDatabaseEntry& SourceDatabaseEntry::operator=(const SourceDatabaseEntry& other)
        {
            m_sourceID = other.m_sourceID;
            m_scanFolderPK = other.m_scanFolderPK;
            m_sourceName = other.m_sourceName;
            m_sourceGuid = other.m_sourceGuid;
            return *this;
        }

        AZStd::string SourceDatabaseEntry::ToString() const
        {
            return AZStd::string::format("SourceDatabaseEntry id:%i scanfolderpk: %i sourcename: %s sourceguid: %s", m_sourceID, m_scanFolderPK, m_sourceName.c_str(), m_sourceGuid.ToString<AZStd::string>().c_str());
        }

        //////////////////////////////////////////////////////////////////////////
        //SourceFileDependencyEntry

        SourceFileDependencyEntry::SourceFileDependencyEntry(AZ::Uuid builderGuid, const AZStd::string&  source, const AZStd::string& dependsOnSource)
            : m_builderGuid(builderGuid)
            , m_source(source)
            , m_dependsOnSource(dependsOnSource)
        {
        }

        SourceFileDependencyEntry::SourceFileDependencyEntry(const SourceFileDependencyEntry& other)
            : m_sourceDependencyID(other.m_sourceDependencyID)
            , m_builderGuid(other.m_builderGuid)
            , m_source(other.m_source)
            , m_dependsOnSource(other.m_dependsOnSource)
        {
        }

        SourceFileDependencyEntry::SourceFileDependencyEntry(SourceFileDependencyEntry&& other)
        {
            *this = AZStd::move(other);
        }

        SourceFileDependencyEntry& SourceFileDependencyEntry::operator=(SourceFileDependencyEntry&& other)
        {
            if (this != &other)
            {
                m_sourceDependencyID = other.m_sourceDependencyID;
                m_builderGuid = other.m_builderGuid;
                m_source = AZStd::move(other.m_source);
                m_dependsOnSource = AZStd::move(other.m_dependsOnSource);
            }
            return *this;
        }

        SourceFileDependencyEntry& SourceFileDependencyEntry::operator=(const SourceFileDependencyEntry& other)
        {
            m_sourceDependencyID = other.m_sourceDependencyID;
            m_builderGuid = other.m_builderGuid;
            m_source = AZStd::move(other.m_source);
            m_dependsOnSource = AZStd::move(other.m_dependsOnSource);
            return *this;
        }

        AZStd::string SourceFileDependencyEntry::ToString() const
        {
            return AZStd::string::format("SourceFileDependencyEntry id:%i builderGuid: %s source: %s dependsOnSource: %s", m_sourceDependencyID, m_builderGuid.ToString<AZStd::string>().c_str(), m_source.c_str(), m_dependsOnSource.c_str());
        }

        //////////////////////////////////////////////////////////////////////////
        //JobDatabaseEntry
        JobDatabaseEntry::JobDatabaseEntry(AZ::s64 jobID, AZ::s64 sourcePK, const char* jobKey, AZ::u32 fingerprint, const char* platform, AZ::Uuid builderGuid, AssetSystem::JobStatus status, AZ::u64 jobRunKey, AZ::s64 firstFailLogTime, const char* firstFailLogFile, AZ::s64 lastFailLogTime, const char* lastFailLogFile, AZ::s64 lastLogTime, const char* lastLogFile)
            : m_jobID(jobID)
            , m_sourcePK(sourcePK)
            , m_fingerprint(fingerprint)
            , m_builderGuid(builderGuid)
            , m_status(status)
            , m_jobRunKey(jobRunKey)
            , m_firstFailLogTime(firstFailLogTime)
            , m_lastFailLogTime(lastFailLogTime)
            , m_lastLogTime(lastLogTime)
        {
            if (jobKey)
            {
                m_jobKey = jobKey;
            }
            if (platform)
            {
                m_platform = platform;
            }
            if (firstFailLogFile)
            {
                m_firstFailLogFile = firstFailLogFile;
            }
            if (lastFailLogFile)
            {
                m_lastFailLogFile = lastFailLogFile;
            }
            if (lastLogFile)
            {
                m_lastLogFile = lastLogFile;
            }
        }

        JobDatabaseEntry::JobDatabaseEntry(AZ::s64 sourcePK, const char* jobKey, AZ::u32 fingerprint, const char* platform, AZ::Uuid builderGuid, AssetSystem::JobStatus status, AZ::u64 jobRunKey, AZ::s64 firstFailLogTime, const char* firstFailLogFile, AZ::s64 lastFailLogTime, const char* lastFailLogFile, AZ::s64 lastLogTime, const char* lastLogFile)
            : m_jobID(-1)
            , m_sourcePK(sourcePK)
            , m_fingerprint(fingerprint)
            , m_builderGuid(builderGuid)
            , m_status(status)
            , m_jobRunKey(jobRunKey)
            , m_firstFailLogTime(firstFailLogTime)
            , m_lastFailLogTime(lastFailLogTime)
            , m_lastLogTime(lastLogTime)
        {
            if (jobKey)
            {
                m_jobKey = jobKey;
            }
            if (platform)
            {
                m_platform = platform;
            }
            if (firstFailLogFile)
            {
                m_firstFailLogFile = firstFailLogFile;
            }
            if (lastFailLogFile)
            {
                m_lastFailLogFile = lastFailLogFile;
            }
            if (lastLogFile)
            {
                m_lastLogFile = lastLogFile;
            }
        }

        JobDatabaseEntry::JobDatabaseEntry(const JobDatabaseEntry& other)
            : m_jobID(other.m_jobID)
            , m_sourcePK(other.m_sourcePK)
            , m_jobKey(other.m_jobKey)
            , m_fingerprint(other.m_fingerprint)
            , m_platform(other.m_platform)
            , m_builderGuid(other.m_builderGuid)
            , m_status(other.m_status)
            , m_jobRunKey(other.m_jobRunKey)
            , m_firstFailLogTime(other.m_firstFailLogTime)
            , m_firstFailLogFile(other.m_firstFailLogFile)
            , m_lastFailLogTime(other.m_lastFailLogTime)
            , m_lastFailLogFile(other.m_lastFailLogFile)
            , m_lastLogTime(other.m_lastLogTime)
            , m_lastLogFile(other.m_lastLogFile)
        {
        }

        JobDatabaseEntry::JobDatabaseEntry(JobDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        JobDatabaseEntry& JobDatabaseEntry::operator=(JobDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_jobID = other.m_jobID;
                m_sourcePK = other.m_sourcePK;
                m_jobKey = AZStd::move(other.m_jobKey);
                m_fingerprint = other.m_fingerprint;
                m_platform = AZStd::move(other.m_platform);
                m_builderGuid = other.m_builderGuid;
                m_status = other.m_status;
                m_jobRunKey = other.m_jobRunKey;
                m_firstFailLogTime = other.m_firstFailLogTime;
                m_firstFailLogFile = AZStd::move(other.m_firstFailLogFile);
                m_lastFailLogTime = other.m_lastFailLogTime;
                m_lastFailLogFile = AZStd::move(other.m_lastFailLogFile);
                m_lastLogTime = other.m_lastLogTime;
                m_lastLogFile = AZStd::move(other.m_lastLogFile);
            }
            return *this;
        }

        JobDatabaseEntry& JobDatabaseEntry::operator=(const JobDatabaseEntry& other)
        {
            m_jobID = other.m_jobID;
            m_sourcePK = other.m_sourcePK;
            m_jobKey = other.m_jobKey;
            m_fingerprint = other.m_fingerprint;
            m_platform = other.m_platform;
            m_builderGuid = other.m_builderGuid;
            m_status = other.m_status;
            m_jobRunKey = other.m_jobRunKey;
            m_firstFailLogTime = other.m_firstFailLogTime;
            m_firstFailLogFile = other.m_firstFailLogFile;
            m_lastFailLogTime = other.m_lastFailLogTime;
            m_lastFailLogFile = other.m_lastFailLogFile;
            m_lastLogTime = other.m_lastLogTime;
            m_lastLogFile = other.m_lastLogFile;

            return *this;
        }

        bool JobDatabaseEntry::operator==(const JobDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_sourcePK == other.m_sourcePK &&
                   m_fingerprint == other.m_fingerprint &&
                   AzFramework::StringFunc::Equal(m_jobKey.c_str(), other.m_jobKey.c_str()) &&
                   AzFramework::StringFunc::Equal(m_platform.c_str(), other.m_platform.c_str()) &&
                   m_builderGuid == other.m_builderGuid &&
                   m_status == other.m_status &&
                   m_jobRunKey == other.m_jobRunKey &&
                   m_firstFailLogTime == other.m_firstFailLogTime &&
                   AzFramework::StringFunc::Equal(m_firstFailLogFile.c_str(), other.m_firstFailLogFile.c_str()) &&
                   m_lastFailLogTime == other.m_lastFailLogTime &&
                   AzFramework::StringFunc::Equal(m_lastFailLogFile.c_str(), other.m_lastFailLogFile.c_str()) &&
                   m_lastLogTime == other.m_lastLogTime &&
                   AzFramework::StringFunc::Equal(m_lastLogFile.c_str(), other.m_lastLogFile.c_str());
        }

        AZStd::string JobDatabaseEntry::ToString() const
        {
            return AZStd::string::format("JobDatabaseEntry id:%i sourcepk: %i jobkey: %s fingerprint: %i platform: %s builderguid: %s status: %s", m_jobID, m_sourcePK, m_jobKey.c_str(), m_fingerprint, m_platform.c_str(), m_builderGuid.ToString<AZStd::string>().c_str(), AssetSystem::JobStatusString(m_status));
        }

        //////////////////////////////////////////////////////////////////////////
        //ProductDatabaseEntry
        ProductDatabaseEntry::ProductDatabaseEntry(AZ::s64 productID, AZ::s64 jobPK, AZ::u32 subID, const char* productName,
            AZ::Data::AssetType assetType, AZ::Uuid legacyGuid)
            : m_productID(productID)
            , m_jobPK(jobPK)
            , m_subID(subID)
            , m_assetType(assetType)
            , m_legacyGuid(legacyGuid)
        {
            if (productName)
            {
                m_productName = productName;
            }
        }

        ProductDatabaseEntry::ProductDatabaseEntry(AZ::s64 jobPK, AZ::u32 subID, const char* productName,
            AZ::Data::AssetType assetType, AZ::Uuid legacyGuid)
            : m_productID(-1)
            , m_jobPK(jobPK)
            , m_subID(subID)
            , m_assetType(assetType)
            , m_legacyGuid(legacyGuid)
        {
            if (productName)
            {
                m_productName = productName;
            }
        }

        ProductDatabaseEntry::ProductDatabaseEntry(const ProductDatabaseEntry& other)
            : m_productID(other.m_productID)
            , m_jobPK(other.m_jobPK)
            , m_subID(other.m_subID)
            , m_productName(other.m_productName)
            , m_assetType(other.m_assetType)
            , m_legacyGuid(other.m_legacyGuid)
        {
        }

        ProductDatabaseEntry::ProductDatabaseEntry(ProductDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        ProductDatabaseEntry& ProductDatabaseEntry::operator=(ProductDatabaseEntry&& other)
        {
            if (this != &other)
            {
                m_productID = other.m_productID;
                m_jobPK = other.m_jobPK;
                m_subID = other.m_subID;
                m_productName = AZStd::move(other.m_productName);
                m_assetType = other.m_assetType;
                m_legacyGuid = other.m_legacyGuid;
            }
            return *this;
        }

        ProductDatabaseEntry& ProductDatabaseEntry::operator=(const ProductDatabaseEntry& other)
        {
            m_productID = other.m_productID;
            m_jobPK = other.m_jobPK;
            m_subID = other.m_subID;
            m_productName = other.m_productName;
            m_assetType = other.m_assetType;
            m_legacyGuid = other.m_legacyGuid;
            return *this;
        }

        bool ProductDatabaseEntry::operator==(const ProductDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_jobPK == other.m_jobPK &&
                   m_subID == other.m_subID &&
                   m_assetType == other.m_assetType &&
                   AzFramework::StringFunc::Equal(m_productName.c_str(), other.m_productName.c_str());//don't compare legacy guid
        }

        AZStd::string ProductDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ProductDatabaseEntry id:%i jobpk: %i subid: %i productname: %s assettype: %s", m_productID, m_jobPK, m_subID, m_productName.c_str(), m_assetType.ToString<AZStd::string>().c_str());
        }

        /////////////////////////////
        // LegacySubIDsEntry
        // loaded from db, and thus includes the PK
        LegacySubIDsEntry::LegacySubIDsEntry(AZ::s64 subIDsEntryID, AZ::s64 productPK, AZ::u32 subId)
            : m_subIDsEntryID(subIDsEntryID)
            , m_productPK(productPK)
            , m_subID(subId)
        {
        }

        LegacySubIDsEntry::LegacySubIDsEntry(AZ::s64 productPK, AZ::u32 subId)
            : m_productPK(productPK)
            , m_subID(subId)
        {
        }

        //////////////////////////////////////////////////////////////////////////
        //ProductDepdendencyDatabaseEntry
        ProductDependencyDatabaseEntry::ProductDependencyDatabaseEntry(AZ::s64 productDependencyID, AZ::s64 productPK, AZ::Uuid dependencySourceGuid, AZ::u32 dependencySubID, AZStd::bitset<64> dependencyFlags)
            : m_productDependencyID(productDependencyID)
            , m_productPK(productPK)
            , m_dependencySourceGuid(dependencySourceGuid)
            , m_dependencySubID(dependencySubID)
            , m_dependencyFlags(dependencyFlags)
        {
        }

        ProductDependencyDatabaseEntry::ProductDependencyDatabaseEntry(AZ::s64 productPK, AZ::Uuid dependencySourceGuid, AZ::u32 dependencySubID, AZStd::bitset<64> dependencyFlags)
            : m_productDependencyID(-1)
            , m_productPK(productPK)
            , m_dependencySourceGuid(dependencySourceGuid)
            , m_dependencySubID(dependencySubID)
            , m_dependencyFlags(dependencyFlags)
        {
        }

        ProductDependencyDatabaseEntry::ProductDependencyDatabaseEntry(ProductDependencyDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        ProductDependencyDatabaseEntry& ProductDependencyDatabaseEntry::operator=(ProductDependencyDatabaseEntry&& other)
        {
            m_productDependencyID = AZStd::move(other.m_productDependencyID);
            m_productPK = AZStd::move(other.m_productPK);
            m_dependencySourceGuid = AZStd::move(other.m_dependencySourceGuid);
            m_dependencySubID = AZStd::move(other.m_dependencySubID);
            m_dependencyFlags = AZStd::move(other.m_dependencyFlags);
            return *this;
        }

        bool ProductDependencyDatabaseEntry::operator==(const ProductDependencyDatabaseEntry& other) const
        {
            //equivalence is when everything but the id is the same
            return m_productPK == other.m_productPK &&
                   m_dependencySourceGuid == other.m_dependencySourceGuid &&
                   m_dependencySubID == other.m_dependencySubID &&
                   m_dependencyFlags == other.m_dependencyFlags;
        }

        AZStd::string ProductDependencyDatabaseEntry::ToString() const
        {
            return AZStd::string::format("ProductDependencyDatabaseEntry id: %i productpk: %i dependencysourceguid: %s dependencysubid: %i dependencyflags: %i", m_productDependencyID, m_productPK, m_dependencySourceGuid.ToString<AZStd::string>().c_str(), m_dependencySubID, m_dependencyFlags);
        }

        //////////////////////////////////////////////////////////////////////////
        //FileDatabaseEntry
        FileDatabaseEntry::FileDatabaseEntry(const FileDatabaseEntry& other)
            : m_fileID(other.m_fileID)
            , m_scanFolderPK(other.m_scanFolderPK)
            , m_fileName(other.m_fileName)
            , m_isFolder(other.m_isFolder)
        {}

        FileDatabaseEntry::FileDatabaseEntry(FileDatabaseEntry&& other)
        {
            *this = AZStd::move(other);
        }

        FileDatabaseEntry& FileDatabaseEntry::operator=(FileDatabaseEntry&& other)
        {
            m_fileID = AZStd::move(other.m_fileID);
            m_scanFolderPK = AZStd::move(other.m_scanFolderPK);
            m_fileName = AZStd::move(other.m_fileName);
            m_isFolder = AZStd::move(other.m_isFolder);
            return *this;
        }

        FileDatabaseEntry& FileDatabaseEntry::operator=(const FileDatabaseEntry& other)
        {
            m_fileID = other.m_fileID;
            m_scanFolderPK = other.m_scanFolderPK;
            m_fileName = other.m_fileName;
            m_isFolder = other.m_isFolder;
            return *this;
        }

        bool FileDatabaseEntry::operator==(const FileDatabaseEntry& other) const
        {
            return m_scanFolderPK == other.m_scanFolderPK  &&
                   m_fileName == other.m_fileName &&
                   m_isFolder == other.m_isFolder;
        }

        AZStd::string FileDatabaseEntry::ToString() const
        {
            return AZStd::string::format("FileDatabaseEntry id: %i scanfolderpk: %i filename: %s isfolder: %i",
                m_fileID, m_scanFolderPK, m_fileName.c_str(), m_isFolder);
        }

        //////////////////////////////////////////////////////////////////////////
        //AssetDatabaseConnection
        AssetDatabaseConnection::AssetDatabaseConnection()
            : m_databaseConnection(nullptr)
        {
        }

        AssetDatabaseConnection::~AssetDatabaseConnection()
        {
            CloseDatabase();
        }

        void AssetDatabaseConnection::CloseDatabase()
        {
            if (m_databaseConnection)
            {
                m_databaseConnection->Close();
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
            }
            m_validatedTables.clear();
        }

        AZStd::string AssetDatabaseConnection::GetAssetDatabaseFilePath()
        {
            AZStd::string databaseLocation;
            EBUS_EVENT(AssetDatabaseRequests::Bus, GetAssetDatabaseLocation, databaseLocation);
            if (databaseLocation.empty())
            {
                databaseLocation = "assetdb.sqlite";
            }
            return databaseLocation;
        }

        bool AssetDatabaseConnection::OpenDatabase()
        {
            AZ_Assert(!m_databaseConnection, "Already open!");
            AZStd::string assetDatabaseLocation = GetAssetDatabaseFilePath();
            AZStd::string parentFolder = assetDatabaseLocation;
            AzFramework::StringFunc::Path::StripFullName(parentFolder);
            if (!parentFolder.empty())
            {
                AZ::IO::SystemFile::CreateDir(parentFolder.c_str());
            }

            if (!IsReadOnly() && AZ::IO::SystemFile::Exists(assetDatabaseLocation.c_str()) && !AZ::IO::SystemFile::IsWritable(assetDatabaseLocation.c_str()))
            {
                AZ_Error("Connection", false, "Asset database file %s is marked read-only.  The cache should not be checked into source control.", assetDatabaseLocation.c_str());
                return false;
            }

            m_databaseConnection = aznew SQLite::Connection();
            AZ_Assert(m_databaseConnection, "No database created");

            if (!m_databaseConnection->Open(assetDatabaseLocation, IsReadOnly()))
            {
                delete m_databaseConnection;
                m_databaseConnection = nullptr;
                AZ_Warning("Connection", false, "Unable to open the asset database at %s\n", assetDatabaseLocation.c_str());
                return false;
            }

            m_validatedTables.clear();
            CreateStatements();

            if (!PostOpenDatabase())
            {
                CloseDatabase();
                return false;
            }

            return true;
        }

        bool AssetDatabaseConnection::PostOpenDatabase()
        {
            if (QueryDatabaseVersion() != CurrentDatabaseVersion())
            {
                AZ_Error(LOG_NAME, false, "Unable to open database - invalid version - database has %i and we want %i\n", QueryDatabaseVersion(), CurrentDatabaseVersion());
                return false;
            }
            return true;
        }

        void AssetDatabaseConnection::CreateStatements()
        {
            AZ_Assert(m_databaseConnection, "No connection!");
            AZ_Assert(m_databaseConnection->IsOpen(), "Connection is not open");

            //////////////////////////////////////////////////////////////////////////
            //table queries
            AddStatement(m_databaseConnection, s_queryDatabaseinfoTable);
            AddStatement(m_databaseConnection, s_queryScanfoldersTable);
            AddStatement(m_databaseConnection, s_querySourcesTable);
            AddStatement(m_databaseConnection, s_queryJobsTable);
            AddStatement(m_databaseConnection, s_queryJobsTablePlatform);
            AddStatement(m_databaseConnection, s_queryProductsTable);
            AddStatement(m_databaseConnection, s_queryProductsTablePlatform);
            AddStatement(m_databaseConnection, s_queryLegacysubidsbyproductid);
            AddStatement(m_databaseConnection, s_queryProductdependenciesTable);
            AddStatement(m_databaseConnection, s_queryFilesTable);

            //////////////////////////////////////////////////////////////////////////
            //projection and combination queries
            AddStatement(m_databaseConnection, s_queryScanfolderByScanfolderid);
            AddStatement(m_databaseConnection, s_queryScanfolderByDisplayname);
            AddStatement(m_databaseConnection, s_queryScanfolderByPortablekey);

            AddStatement(m_databaseConnection, s_querySourceBySourceid);
            AddStatement(m_databaseConnection, s_querySourceByScanfolderid);
            AddStatement(m_databaseConnection, s_querySourceBySourceguid);

            AddStatement(m_databaseConnection, s_querySourceBySourcename);
            AddStatement(m_databaseConnection, s_querySourceBySourcenameScanfolderid);
            AddStatement(m_databaseConnection, s_querySourceLikeSourcename);

            AddStatement(m_databaseConnection, s_queryJobByJobid);
            AddStatement(m_databaseConnection, s_queryJobByJobkey);
            AddStatement(m_databaseConnection, s_queryJobByJobrunkey);
            AddStatement(m_databaseConnection, s_queryJobByProductid);
            AddStatement(m_databaseConnection, s_queryJobBySourceid);
            AddStatement(m_databaseConnection, s_queryJobBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryProductByProductid);
            AddStatement(m_databaseConnection, s_queryProductByJobid);
            AddStatement(m_databaseConnection, s_queryProductByJobidPlatform);
            AddStatement(m_databaseConnection, s_queryProductBySourceid);
            AddStatement(m_databaseConnection, s_queryProductBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryProductByProductname);
            AddStatement(m_databaseConnection, s_queryProductByProductnamePlatform);
            AddStatement(m_databaseConnection, s_queryProductLikeProductname);
            AddStatement(m_databaseConnection, s_queryProductLikeProductnamePlatform);

            AddStatement(m_databaseConnection, s_queryProductBySourcename);
            AddStatement(m_databaseConnection, s_queryProductBySourcenamePlatform);
            AddStatement(m_databaseConnection, s_queryProductLikeSourcename);
            AddStatement(m_databaseConnection, s_queryProductLikeSourcenamePlatform);
            AddStatement(m_databaseConnection, s_queryProductByJobIdSubId);
            AddStatement(m_databaseConnection, s_queryProductBySourceGuidSubid);

            AddStatement(m_databaseConnection, s_queryCombined);
            AddStatement(m_databaseConnection, s_queryCombinedByPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourceid);
            AddStatement(m_databaseConnection, s_queryCombinedBySourceidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByJobid);
            AddStatement(m_databaseConnection, s_queryCombinedByJobidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByProductid);
            AddStatement(m_databaseConnection, s_queryCombinedByProductidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourceguidProductsubid);
            AddStatement(m_databaseConnection, s_queryCombinedBySourceguidProductsubidPlatform);

            AddStatement(m_databaseConnection, s_queryCombinedBySourcename);
            AddStatement(m_databaseConnection, s_queryCombinedBySourcenamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedLikeSourcename);
            AddStatement(m_databaseConnection, s_queryCombinedLikeSourcenamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedByProductname);
            AddStatement(m_databaseConnection, s_queryCombinedByProductnamePlatform);

            AddStatement(m_databaseConnection, s_queryCombinedLikeProductname);
            AddStatement(m_databaseConnection, s_queryCombinedLikeProductnamePlatform);

            AddStatement(m_databaseConnection, s_querySourcedependencyBySourcedependencyid);
            AddStatement(m_databaseConnection, s_querySourcedependency);
            AddStatement(m_databaseConnection, s_querySourcedependencyByDependsonsource);
            AddStatement(m_databaseConnection, s_queryDependsonsourceBySource);
            AddStatement(m_databaseConnection, s_querySourcedependencyByBuilderguidSource);

            AddStatement(m_databaseConnection, s_queryProductdependencyByProductdependencyid);
            AddStatement(m_databaseConnection, s_queryProductdependencyByProductid);
            AddStatement(m_databaseConnection, s_queryDirectProductdependencies);
            AddStatement(m_databaseConnection, s_queryAllProductdependencies);

            AddStatement(m_databaseConnection, s_queryFileByFileid);
            AddStatement(m_databaseConnection, s_queryFilesByFileName);
            AddStatement(m_databaseConnection, s_queryFilesLikeFileName);
            AddStatement(m_databaseConnection, s_queryFilesByScanfolderid);
            AddStatement(m_databaseConnection, s_queryFileByFileNameScanfolderid);
        }

        //////////////////////////////////////////////////////////////////////////
        //Like
        AZStd::string AssetDatabaseConnection::GetLikeActualSearchTerm(const char* likeString, LikeType likeType)
        {
            AZStd::string actualSearchTerm = likeString;
            if (likeType == StartsWith)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Append(actualSearchTerm, "%");
            }
            else if (likeType == EndsWith)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Prepend(actualSearchTerm, "%");
            }
            else if (likeType == Matches)
            {
                StringFunc::Replace(actualSearchTerm, "%", "|%");
                StringFunc::Replace(actualSearchTerm, "_", "|_");
                StringFunc::Prepend(actualSearchTerm, "%");
                StringFunc::Append(actualSearchTerm, "%");
            }
            //raw default

            return actualSearchTerm;
        }

        //////////////////////////////////////////////////////////////////////////
        //Table queries

        bool AssetDatabaseConnection::QueryDatabaseInfoTable(databaseInfoHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_DATABASEINFO_TABLE, "dbinfo"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, QUERY_DATABASEINFO_TABLE);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, false, "Unable to find SQL statement: %s", QUERY_DATABASEINFO_TABLE);
                return false;
            }

            Statement::SqlStatus result = statement->Step();

            int rowIDColumnIdx = statement->FindColumn("rowID");
            if (rowIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "rowID", QUERY_DATABASEINFO_TABLE);
                return false;
            }

            int versionColumnIdx = statement->FindColumn("version");
            if (versionColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "version", QUERY_DATABASEINFO_TABLE);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                DatabaseInfoEntry databaseinfo(statement->GetColumnInt(rowIDColumnIdx),
                    static_cast<DatabaseVersion>(statement->GetColumnInt(versionColumnIdx)));
                if (handler(databaseinfo))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }

                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", QUERY_DATABASEINFO_TABLE);
                return false;
            }

            return validResult;
        }

        DatabaseVersion AssetDatabaseConnection::QueryDatabaseVersion()
        {
            DatabaseVersion dbVersion;
            bool res = QueryDatabaseInfoTable(
                    [&](DatabaseInfoEntry& entry)
                    {
                        dbVersion = entry.m_version;
                        return true; //see all of them
                    });

            if (res)
            {
                return dbVersion;
            }

            return DatabaseVersion::DatabaseDoesNotExist;
        }

        bool AssetDatabaseConnection::QueryScanFoldersTable(scanFolderHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SCANFOLDERS_TABLE, "ScanFolders"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryScanfoldersTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetScanFolderResult(QUERY_SCANFOLDERS_TABLE, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourcesTable(sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCES_TABLE, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourcesTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCES_TABLE, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobsTable(jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_JOBS_TABLE;
            if (platform && strlen(platform))
            {
                name = QUERY_JOBS_TABLE_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryJobsTablePlatform.Bind(*m_databaseConnection, autoFinal, platform))
                {
                    return false;
                }
            }
            else if (!s_queryJobsTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductsTable(productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_PRODUCTS_TABLE;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCTS_TABLE_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductsTablePlatform.Bind(*m_databaseConnection, autoFinal, platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductsTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductDependenciesTable(combinedProductDependencyHandler handler)
        {
            const char* name = QUERY_PRODUCTDEPENDENCIES_TABLE;
            if (!ValidateDatabaseTable(name, "ProductDependencies"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryProductdependenciesTable.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedDependencyResult(name, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFilesTable(fileHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_FILES_TABLE, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, QUERY_FILES_TABLE);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, false, "Unable to find SQL statement: %s", QUERY_FILES_TABLE);
                return false;
            }

            return GetFileResult(QUERY_FILES_TABLE, statement, handler);
        }

        bool AssetDatabaseConnection::QueryScanFolderByScanFolderID(AZ::s64 scanfolderid, scanFolderHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SCANFOLDER_BY_SCANFOLDERID, "ScanFolders"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryScanfolderByScanfolderid.Bind(*m_databaseConnection, autoFinal, scanfolderid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetScanFolderResult(QUERY_SCANFOLDER_BY_SCANFOLDERID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryScanFolderBySourceID(AZ::s64 sourceID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedBySourceID(sourceID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByJobID(AZ::s64 jobID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedByJobID(jobID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByProductID(AZ::s64 productID, scanFolderHandler handler)
        {
            bool found = false;
            bool succeeded = QueryCombinedBySourceID(productID,
                    [&](CombinedDatabaseEntry& combined)
                    {
                        found = true;
                        ScanFolderDatabaseEntry scanFolder = AZStd::move(combined);
                        return handler(scanFolder);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryScanFolderByDisplayName(const char* displayName, scanFolderHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SCANFOLDER_BY_DISPLAYNAME, "ScanFolders"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryScanfolderByDisplayname.Bind(*m_databaseConnection, autoFinal, displayName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetScanFolderResult(QUERY_SCANFOLDER_BY_DISPLAYNAME, statement, handler);
        }

        bool AssetDatabaseConnection::QueryScanFolderByPortableKey(const char* portableKey, scanFolderHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SCANFOLDER_BY_PORTABLEKEY, "ScanFolders"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryScanfolderByPortablekey.Bind(*m_databaseConnection, autoFinal, portableKey))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetScanFolderResult(QUERY_SCANFOLDER_BY_PORTABLEKEY, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceID(AZ::s64 sourceid, sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCE_BY_SOURCEID, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceBySourceid.Bind(*m_databaseConnection, autoFinal, sourceid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_BY_SOURCEID, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceByScanFolderID(AZ::s64 scanFolderID, sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCE_BY_SCANFOLDERID, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceByScanfolderid.Bind(*m_databaseConnection, autoFinal, scanFolderID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_BY_SCANFOLDERID, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceByJobID(AZ::s64 jobid, sourceHandler handler)
        {
            return QueryCombinedByJobID(jobid,
                [&](CombinedDatabaseEntry& combined)
                {
                    SourceDatabaseEntry source;
                    source = AZStd::move(combined);
                    handler(source);
                    return false;//one
                }, nullptr,
                nullptr);
        }

        bool AssetDatabaseConnection::QuerySourceByProductID(AZ::s64 productid, sourceHandler handler)
        {
            return QueryCombinedByProductID(productid,
                [&](CombinedDatabaseEntry& combined)
                {
                    SourceDatabaseEntry source;
                    source = AZStd::move(combined);
                    handler(source);
                    return false;//one
                }, nullptr);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceGuid(AZ::Uuid sourceGuid, sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCE_BY_SOURCEGUID, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceBySourceguid.Bind(*m_databaseConnection, autoFinal, sourceGuid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_BY_SOURCEGUID, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceName(const char* exactSourceName, sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCE_BY_SOURCENAME, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceBySourcename.Bind(*m_databaseConnection, autoFinal, exactSourceName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_BY_SOURCENAME, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceBySourceNameScanFolderID(const char* exactSourceName, AZ::s64 scanFolderID, sourceHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceBySourcenameScanfolderid.Bind(*m_databaseConnection, autoFinal, exactSourceName, scanFolderID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_BY_SOURCENAME_SCANFOLDERID, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceLikeSourceName(const char* likeSourceName, LikeType likeType, sourceHandler handler)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            if (!ValidateDatabaseTable(QUERY_SOURCE_LIKE_SOURCENAME, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourceLikeSourcename.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceResult(QUERY_SOURCE_LIKE_SOURCENAME, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobByJobID(AZ::s64 jobid, jobHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_JOB_BY_JOBID, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryJobByJobid.Bind(*m_databaseConnection, autoFinal, jobid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(QUERY_JOB_BY_JOBID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobByJobKey(AZStd::string jobKey, jobHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_JOB_BY_JOBKEY, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryJobByJobkey.Bind(*m_databaseConnection, autoFinal, jobKey.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(QUERY_JOB_BY_JOBKEY, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobByJobRunKey(AZ::u64 jobrunkey, jobHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_JOB_BY_JOBRUNKEY, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryJobByJobrunkey.Bind(*m_databaseConnection, autoFinal, jobrunkey))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(QUERY_JOB_BY_JOBRUNKEY, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobByProductID(AZ::s64 productid, jobHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_JOB_BY_PRODUCTID, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryJobByProductid.Bind(*m_databaseConnection, autoFinal, productid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(QUERY_JOB_BY_PRODUCTID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryJobBySourceID(AZ::s64 sourceID, jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_JOB_BY_SOURCEID;
            if (platform && strlen(platform))
            {
                name = QUERY_JOB_BY_SOURCEID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Jobs"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryJobBySourceidPlatform.Bind(*m_databaseConnection, autoFinal, sourceID, platform))
                {
                    return false;
                }
            }
            else if (!s_queryJobBySourceid.Bind(*m_databaseConnection, autoFinal, sourceID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetJobResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductByProductID(AZ::s64 productid, productHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_PRODUCT_BY_PRODUCTID, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryProductByProductid.Bind(*m_databaseConnection, autoFinal, productid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(QUERY_PRODUCT_BY_PRODUCTID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryProductByJobID(AZ::s64 jobid, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_PRODUCT_BY_JOBID;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_BY_JOBID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductByJobidPlatform.Bind(*m_databaseConnection, autoFinal, jobid, platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductByJobid.Bind(*m_databaseConnection, autoFinal, jobid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceID(AZ::s64 sourceid, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_PRODUCT_BY_SOURCEID;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_BY_SOURCEID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductBySourceidPlatform.Bind(*m_databaseConnection, autoFinal, sourceid, platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductBySourceid.Bind(*m_databaseConnection, autoFinal, sourceid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceGuid(AZ::Uuid sourceGuid, productHandler handler)
        {
            const char* name = QUERY_PRODUCT_BY_SOURCEID;

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, name);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, false, "Unable to find SQL statement: %s", name);
                return false;
            }

            int sourceGuidIdx = statement->GetNamedParamIdx(":sourceguid");
            if (!sourceGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find :sourceguid parameter in %s", name);
                return false;
            }
            statement->BindValueUuid(sourceGuidIdx, sourceGuid);

            return GetProductResult(name, statement, handler);
        }

        bool AssetDatabaseConnection::QueryProductBySourceGuidSubID(AZ::Uuid sourceGuid, AZ::u32 productSubID, productHandler handler)
        {
            const char* name = QUERY_PRODUCT_BY_SOURCEGUID_SUBID;

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal(*m_databaseConnection, name);
            Statement* statement = autoFinal.Get();
            if (!statement)
            {
                AZ_Error(LOG_NAME, false, "Unable to find SQL statement: %s", name);
                return false;
            }

            int sourceGuidIdx = statement->GetNamedParamIdx(":sourceguid");
            if (!sourceGuidIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find :sourceguid parameter in %s", name);
                return false;
            }
            statement->BindValueUuid(sourceGuidIdx, sourceGuid);

            int productsubidIdx = statement->GetNamedParamIdx(":productsubid");
            if (!productsubidIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find :productsubid parameter in %s", name);
                return false;
            }
            statement->BindValueInt(productsubidIdx, productSubID);

            return GetProductResult(name, statement, handler);
        }

        bool AssetDatabaseConnection::QueryProductByProductName(const char* exactProductname, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_PRODUCT_BY_PRODUCTNAME;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_BY_PRODUCTNAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductByProductnamePlatform.Bind(*m_databaseConnection, autoFinal, exactProductname, platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductByProductname.Bind(*m_databaseConnection, autoFinal, exactProductname))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductLikeProductName(const char* likeProductname, LikeType likeType, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeProductname, likeType);

            const char* name = QUERY_PRODUCT_LIKE_PRODUCTNAME;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_LIKE_PRODUCTNAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductLikeProductnamePlatform.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str(), platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductLikeProductname.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductBySourceName(const char* exactSourceName, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_PRODUCT_BY_SOURCENAME;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_BY_SOURCENAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductBySourcenamePlatform.Bind(*m_databaseConnection, autoFinal, exactSourceName, platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductBySourcename.Bind(*m_databaseConnection, autoFinal, exactSourceName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductLikeSourceName(const char* likeSourceName, LikeType likeType, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            const char* name = QUERY_PRODUCT_LIKE_SOURCENAME;
            if (platform && strlen(platform))
            {
                name = QUERY_PRODUCT_LIKE_SOURCENAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryProductLikeSourcenamePlatform.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str(), platform))
                {
                    return false;
                }
            }
            else if (!s_queryProductLikeSourcename.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryProductByJobIDSubID(AZ::s64 jobID, AZ::u32 subId, productHandler handler)
        {
            const char* name = QUERY_PRODUCT_BY_JOBID_SUBID;

            if (!ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryProductByJobIdSubId.Bind(*m_databaseConnection, autoFinal, jobID, subId))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();
            return GetProductResult(name, statement, handler);
        }

        bool AssetDatabaseConnection::QueryLegacySubIdsByProductID(AZ::s64 productId, legacySubIDsHandler handler)
        {
            const char* name = QUERY_LEGACYSUBIDSBYPRODUCTID;
            if (!ValidateDatabaseTable(name, "LegacySubIDs"))
            {
                return false;
            }
            StatementAutoFinalizer autoFinal;

            if (!s_queryLegacysubidsbyproductid.Bind(*m_databaseConnection, autoFinal, productId))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetLegacySubIDsResult(name, statement, handler);
        }

        bool AssetDatabaseConnection::QueryCombined(combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status, bool includeLegacySubIDs)
        {
            const char* name = QUERY_COMBINED;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedByPlatform.Bind(*m_databaseConnection, autoFinal, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombined.Bind(*m_databaseConnection, autoFinal))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status, includeLegacySubIDs);
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceID(AZ::s64 sourceID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_SOURCEID;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_SOURCEID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedBySourceidPlatform.Bind(*m_databaseConnection, autoFinal, sourceID, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedBySourceid.Bind(*m_databaseConnection, autoFinal, sourceID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByJobID(AZ::s64 jobID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_JOBID;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_JOBID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedByJobidPlatform.Bind(*m_databaseConnection, autoFinal, jobID, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedByJobid.Bind(*m_databaseConnection, autoFinal, jobID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByProductID(AZ::s64 productID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_PRODUCTID;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_PRODUCTID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedByProductidPlatform.Bind(*m_databaseConnection, autoFinal, productID, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedByProductid.Bind(*m_databaseConnection, autoFinal, productID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceGuidProductSubId(AZ::Uuid sourceGuid, AZ::u32 productSubID, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_SOURCEGUID_PRODUCTSUBID_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedBySourceguidProductsubidPlatform.Bind(*m_databaseConnection, autoFinal, productSubID, sourceGuid, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedBySourceguidProductsubid.Bind(*m_databaseConnection, autoFinal, productSubID, sourceGuid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedBySourceName(const char* exactSourceName, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_SOURCENAME;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_SOURCENAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedBySourcenamePlatform.Bind(*m_databaseConnection, autoFinal, exactSourceName, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedBySourcename.Bind(*m_databaseConnection, autoFinal, exactSourceName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedLikeSourceName(const char* likeSourceName, LikeType likeType, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeSourceName, likeType);

            const char* name = QUERY_COMBINED_LIKE_SOURCENAME;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_LIKE_SOURCENAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedLikeSourcenamePlatform.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str(), platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedLikeSourcename.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedByProductName(const char* exactProductName, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            const char* name = QUERY_COMBINED_BY_PRODUCTNAME;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_BY_PRODUCTNAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedByProductnamePlatform.Bind(*m_databaseConnection, autoFinal, exactProductName, platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedByProductname.Bind(*m_databaseConnection, autoFinal, exactProductName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryCombinedLikeProductName(const char* likeProductName, LikeType likeType, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeProductName, likeType);

            const char* name = QUERY_COMBINED_LIKE_PRODUCTNAME;
            if (platform && strlen(platform))
            {
                name = QUERY_COMBINED_LIKE_PRODUCTNAME_PLATFORM;
            }

            if (!ValidateDatabaseTable(name, "ScanFolders") ||
                !ValidateDatabaseTable(name, "Sources") ||
                !ValidateDatabaseTable(name, "Jobs") ||
                !ValidateDatabaseTable(name, "Products"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (platform && strlen(platform))
            {
                if (!s_queryCombinedLikeProductnamePlatform.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str(), platform))
                {
                    return false;
                }
            }
            else if (!s_queryCombinedLikeProductname.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetCombinedResult(name, statement, handler, builderGuid, jobKey, status);
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobID(AZ::s64 jobID, jobInfoHandler handler)
        {
            SourceDatabaseEntry source;

            bool found = false;
            bool succeeded = QuerySourceByJobID(jobID,
                    [&](SourceDatabaseEntry& entry)
                    {
                        found = true;
                        source = AZStd::move(entry);
                        return false;//one
                    });

            if (!found || !succeeded)
            {
                return false;
            }

            found = false;
            succeeded = QueryJobByJobID(jobID,
                    [&](JobDatabaseEntry& entry)
                    {
                        found = true;
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        jobinfo.m_sourceFile = source.m_sourceName;
                        PopulateJobInfo(jobinfo, entry);
                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobRunKey(AZ::u64 jobRunKey, jobInfoHandler handler)
        {
            bool found = false;
            bool succeeded = QueryJobByJobRunKey(jobRunKey,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        succeeded = QuerySourceBySourceID(entry.m_sourcePK,
                                [&](SourceDatabaseEntry& sourceEntry)
                                {
                                    found = true;
                                    jobinfo.m_sourceFile = AZStd::move(sourceEntry.m_sourceName);
                                    return true;
                                });

                        if (!found)
                        {
                            return false;
                        }

                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoByJobKey(AZStd::string jobKey, jobInfoHandler handler)
        {
            bool found = false;
            bool succeeded = QueryJobByJobKey(jobKey,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        succeeded = QuerySourceBySourceID(entry.m_sourcePK,
                                [&](SourceDatabaseEntry& sourceEntry)
                                {
                                    jobinfo.m_sourceFile = AZStd::move(sourceEntry.m_sourceName);
                                    QueryScanFolderBySourceID(sourceEntry.m_sourceID,
                                        [&](ScanFolderDatabaseEntry& scanFolderEntry)
                                        {
                                            found = true;
                                            jobinfo.m_watchFolder = scanFolderEntry.m_scanFolder;
                                            return false;
                                        });
                                    return true;
                                });

                        if (!found)
                        {
                            return false;
                        }

                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    });
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QueryJobInfoBySourceName(const char* sourceName, jobInfoHandler handler, AZ::Uuid builderGuid, const char* jobKey, const char* platform, AssetSystem::JobStatus status)
        {
            SourceDatabaseEntry source;

            bool found = false;
            bool succeeded = QuerySourceBySourceName(sourceName,
                    [&](SourceDatabaseEntry& entry)
                    {
                        found = true;
                        source = AZStd::move(entry);
                        return false;//one
                    });

            if (!found || !succeeded)
            {
                return false;
            }

            found = false;
            succeeded = QueryJobBySourceID(source.m_sourceID,
                    [&](JobDatabaseEntry& entry)
                    {
                        AzToolsFramework::AssetSystem::JobInfo jobinfo;
                        jobinfo.m_sourceFile = source.m_sourceName; //dont move, we may have many that need this name
                        QueryScanFolderBySourceID(source.m_sourceID,
                            [&](ScanFolderDatabaseEntry& scanFolderEntry)
                            {
                                found = true;
                                jobinfo.m_watchFolder = scanFolderEntry.m_scanFolder;
                                return false;
                            });
                        PopulateJobInfo(jobinfo, entry);

                        return handler(jobinfo);
                    }, builderGuid,
                    jobKey,
                    platform,
                    status);
            return found && succeeded;
        }

        bool AssetDatabaseConnection::QuerySourceDependency(const AZ::Uuid& builderId, const char* source, const char* dependsOnSource, sourceFileDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCEDEPENDENCY, "SourceDependency"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourcedependency.Bind(*m_databaseConnection, autoFinal, builderId, source, dependsOnSource))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceDependencyResult(QUERY_SOURCEDEPENDENCY, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceDependencyBySourceDependencyId(AZ::s64 sourceDependencyID, sourceFileDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID, "SourceDependency"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourcedependencyBySourcedependencyid.Bind(*m_databaseConnection, autoFinal, sourceDependencyID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceDependencyResult(QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceDependencyByDependsOnSource(const char* dependsOnSource, const char* dependentFilter, sourceFileDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE, "SourceDependency"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourcedependencyByDependsonsource.Bind(*m_databaseConnection, autoFinal, dependsOnSource))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();


            int dependentFilterIdx = statement->GetNamedParamIdx(":dependentFilter");
            if (!dependentFilterIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find :dependentFilter parameter in %s", QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE);
                return false;
            }

            statement->BindValueText(dependentFilterIdx, dependentFilter == nullptr ? "%" : dependentFilter);

            return GetSourceDependencyResult(QUERY_SOURCEDEPENDENCY_BY_DEPENDSONSOURCE, statement, handler);
        }

        bool AssetDatabaseConnection::QueryDependsOnSourceBySourceDependency(const char* sourceDependency, const char* dependencyFilter, sourceFileDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_DEPENDSONSOURCE_BY_SOURCE, "SourceDependency"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryDependsonsourceBySource.Bind(*m_databaseConnection, autoFinal, sourceDependency))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            int dependencyFilterIdx = statement->GetNamedParamIdx(":dependencyFilter");
            if (!dependencyFilterIdx)
            {
                AZ_Error(LOG_NAME, false, "Could not find :dependencyFilter parameter in %s", QUERY_DEPENDSONSOURCE_BY_SOURCE);
                return false;
            }

            statement->BindValueText(dependencyFilterIdx, dependencyFilter == nullptr ? "%" : dependencyFilter);

            return GetSourceDependencyResult(QUERY_DEPENDSONSOURCE_BY_SOURCE, statement, handler);
        }

        bool AssetDatabaseConnection::QuerySourceDependencyByBuilderGUIDAndSource(const AZ::Uuid& builderGuid, const char* source, sourceFileDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_SOURCEDEPENDENCY_BY_BUILDERGUID_SOURCE, "SourceDependency"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_querySourcedependencyByBuilderguidSource.Bind(*m_databaseConnection, autoFinal, source, builderGuid))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetSourceDependencyResult(QUERY_SOURCEDEPENDENCY_BY_SOURCEDEPENDENCYID, statement, handler);
        }

        // Product Dependencies
        bool AssetDatabaseConnection::QueryProductDependencyByProductDependencyId(AZ::s64 productDependencyID, productDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID, "ProductDependencies"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryProductdependencyByProductdependencyid.Bind(*m_databaseConnection, autoFinal, productDependencyID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductDependencyResult(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTDEPENDENCYID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryProductDependencyByProductId(AZ::s64 productID, productDependencyHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID, "ProductDependencies"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryProductdependencyByProductid.Bind(*m_databaseConnection, autoFinal, productID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductDependencyResult(QUERY_PRODUCTDEPENDENCY_BY_PRODUCTID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryDirectProductDependencies(AZ::s64 productID, productHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_DIRECT_PRODUCTDEPENDENCIES, "ProductDependencies") ||
                !ValidateDatabaseTable(QUERY_DIRECT_PRODUCTDEPENDENCIES, "Products") ||
                !ValidateDatabaseTable(QUERY_DIRECT_PRODUCTDEPENDENCIES, "Jobs") ||
                !ValidateDatabaseTable(QUERY_DIRECT_PRODUCTDEPENDENCIES, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryDirectProductdependencies.Bind(*m_databaseConnection, autoFinal, productID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(QUERY_DIRECT_PRODUCTDEPENDENCIES, statement, handler);
        }

        bool AssetDatabaseConnection::QueryAllProductDependencies(AZ::s64 productID, productHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_ALL_PRODUCTDEPENDENCIES, "ProductDependencies") ||
                !ValidateDatabaseTable(QUERY_ALL_PRODUCTDEPENDENCIES, "Products") ||
                !ValidateDatabaseTable(QUERY_ALL_PRODUCTDEPENDENCIES, "Jobs") ||
                !ValidateDatabaseTable(QUERY_ALL_PRODUCTDEPENDENCIES, "Sources"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryAllProductdependencies.Bind(*m_databaseConnection, autoFinal, productID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetProductResult(QUERY_ALL_PRODUCTDEPENDENCIES, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFileByFileID(AZ::s64 fileID, fileHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_FILE_BY_FILEID, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryFileByFileid.Bind(*m_databaseConnection, autoFinal, fileID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetFileResult(QUERY_FILE_BY_FILEID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFilesByFileNameAndScanFolderID(const char* fileName, AZ::s64 scanFolderID, fileHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_FILES_BY_FILENAME_AND_SCANFOLDER, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryFilesByFileName.Bind(*m_databaseConnection, autoFinal, scanFolderID, fileName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetFileResult(QUERY_FILES_BY_FILENAME_AND_SCANFOLDER, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFilesLikeFileName(const char* likeFileName, LikeType likeType, fileHandler handler)
        {
            AZStd::string actualSearchTerm = GetLikeActualSearchTerm(likeFileName, likeType);

            if (!ValidateDatabaseTable(QUERY_FILES_LIKE_FILENAME, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryFilesLikeFileName.Bind(*m_databaseConnection, autoFinal, actualSearchTerm.c_str()))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetFileResult(QUERY_FILES_LIKE_FILENAME, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFilesByScanFolderID(AZ::s64 scanFolderID, fileHandler handler) 
        {
            if (!ValidateDatabaseTable(QUERY_FILES_BY_SCANFOLDERID, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryFilesByScanfolderid.Bind(*m_databaseConnection, autoFinal, scanFolderID))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetFileResult(QUERY_FILES_BY_SCANFOLDERID, statement, handler);
        }

        bool AssetDatabaseConnection::QueryFileByFileNameScanFolderID(const char* fileName, AZ::s64 scanFolderID, fileHandler handler)
        {
            if (!ValidateDatabaseTable(QUERY_FILE_BY_FILENAME_SCANFOLDERID, "Files"))
            {
                return false;
            }

            StatementAutoFinalizer autoFinal;

            if (!s_queryFileByFileNameScanfolderid.Bind(*m_databaseConnection, autoFinal, scanFolderID, fileName))
            {
                return false;
            }

            Statement* statement = autoFinal.Get();

            return GetFileResult(QUERY_FILE_BY_FILENAME_SCANFOLDERID, statement, handler);
        }

        bool AssetDatabaseConnection::ValidateDatabaseTable(const char* callName, const char* tableName)
        {
            (void)callName; // for release mode, when AZ_Error is compiled down to nothing.
            (void)tableName;

            if (m_validatedTables.find(tableName) != m_validatedTables.end())
            {
                return true; // already validated.
            }

            AZ_Error(LOG_NAME, m_databaseConnection, "Fatal: attempt to work on a database connection that doesn't exist: %s", callName);

            if (!m_databaseConnection)
            {
                return false;
            }

            AZ_Error(LOG_NAME, m_databaseConnection->IsOpen(), "Fatal: attempt to work on a database connection that isn't open: %s", callName);

            if ((!m_databaseConnection) || (!m_databaseConnection->IsOpen()))
            {
                return false;
            }

            if (!m_databaseConnection->DoesTableExist(tableName))
            {
                return false;
            }

            m_validatedTables.insert(tableName);

            return true;
        }

        bool AssetDatabaseConnection::GetScanFolderResult(const char* callName, Statement* statement, scanFolderHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int scanFolderIDColumnIdx = statement->FindColumn("ScanFolderID");
            if (scanFolderIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolderID", callName);
                return false;
            }

            int scanFolderColumnIdx = statement->FindColumn("ScanFolder");
            if (scanFolderColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolder", callName);
                return false;
            }

            int displayNameColumnIdx = statement->FindColumn("DisplayName");
            if (displayNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "DisplayName", callName);
                return false;
            }

            int portableKeyColumnIdx = statement->FindColumn("PortableKey");
            if (portableKeyColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "PortableKey", callName);
                return false;
            }

            int outputPrefixColumnIdx = statement->FindColumn("OutputPrefix");
            if (outputPrefixColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "OutputPrefix", callName);
                return false;
            }

            int isRootColumnIdx = statement->FindColumn("IsRoot");
            if (isRootColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "IsRoot", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                ScanFolderDatabaseEntry scanfolder;
                scanfolder.m_scanFolderID = statement->GetColumnInt64(scanFolderIDColumnIdx);
                scanfolder.m_scanFolder = AZStd::move(statement->GetColumnText(scanFolderColumnIdx));
                scanfolder.m_displayName = AZStd::move(statement->GetColumnText(displayNameColumnIdx));
                scanfolder.m_portableKey = AZStd::move(statement->GetColumnText(portableKeyColumnIdx));
                scanfolder.m_outputPrefix = AZStd::move(statement->GetColumnText(outputPrefixColumnIdx));
                scanfolder.m_isRoot = AZStd::move(statement->GetColumnInt(isRootColumnIdx));

                if (handler(scanfolder))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetSourceResult(const char* callName, Statement* statement, sourceHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int sourceIDColumnIdx = statement->FindColumn("SourceID");
            if (sourceIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceID", callName);
                return false;
            }

            int scanFolderColumnIdx = statement->FindColumn("ScanFolderPK");
            if (scanFolderColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolderPK", callName);
                return false;
            }

            int sourceNameColumnIdx = statement->FindColumn("SourceName");
            if (sourceNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceName", callName);
                return false;
            }

            int guidValueColumnIdx = statement->FindColumn("SourceGuid");
            if (guidValueColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceGuid", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                SourceDatabaseEntry source;
                source.m_sourceID = statement->GetColumnInt64(sourceIDColumnIdx);
                source.m_scanFolderPK = statement->GetColumnInt64(scanFolderColumnIdx);
                source.m_sourceName = AZStd::move(statement->GetColumnText(sourceNameColumnIdx));
                source.m_sourceGuid = statement->GetColumnUuid(guidValueColumnIdx);

                if (handler(source))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetSourceDependencyResult(const char* callName, SQLite::Statement* statement, sourceFileDependencyHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int sourceDependencyIdx = statement->FindColumn("SourceDependencyID");
            if (sourceDependencyIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceDependencyID", callName);
                return false;
            }

            int builderGuidIdx = statement->FindColumn("BuilderGuid");
            if (builderGuidIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "BuilderGuid", callName);
                return false;
            }

            int sourceIdx = statement->FindColumn("Source");
            if (sourceIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Source", callName);
                return false;
            }

            int dependsOnSourceIdx = statement->FindColumn("DependsOnSource");
            if (dependsOnSourceIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "DependsOnSource", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                SourceFileDependencyEntry entry;
                entry.m_sourceDependencyID = statement->GetColumnInt64(sourceDependencyIdx);
                entry.m_builderGuid = statement->GetColumnUuid(builderGuidIdx);
                entry.m_source = AZStd::move(statement->GetColumnText(sourceIdx));
                entry.m_dependsOnSource = AZStd::move(statement->GetColumnText(dependsOnSourceIdx));

                if (handler(entry))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetJobResult(const char* callName, Statement* statement, jobHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int jobIDColumnIdx = statement->FindColumn("JobID");
            if (jobIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobID", callName);
                return false;
            }

            int sourcePKColumnIdx = statement->FindColumn("SourcePK");
            if (sourcePKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourcePK", callName);
                return false;
            }

            int jobKeyColumnIdx = statement->FindColumn("JobKey");
            if (jobKeyColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobKey", callName);
                return false;
            }

            int fingerprintColumnIdx = statement->FindColumn("Fingerprint");
            if (fingerprintColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Fingerprint", callName);
                return false;
            }

            int platformColumnIdx = statement->FindColumn("Platform");
            if (platformColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Platform", callName);
                return false;
            }

            int buildeGuidColumnIdx = statement->FindColumn("BuilderGuid");
            if (buildeGuidColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "BuilderGuid", callName);
                return false;
            }

            int statusColumnIdx = statement->FindColumn("Status");
            if (statusColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Status", callName);
                return false;
            }

            int jobRunKeyColumnIdx = statement->FindColumn("JobRunKey");
            if (jobRunKeyColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobRunKey", callName);
                return false;
            }

            int firstFailLogTimeColumnIdx = statement->FindColumn("FirstFailLogTime");
            if (firstFailLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FirstFailLogTime", callName);
                return false;
            }

            int firstFailLogFileColumnIdx = statement->FindColumn("FirstFailLogFile");
            if (firstFailLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FirstFailLogFile", callName);
                return false;
            }

            int lastFailLogTimeColumnIdx = statement->FindColumn("LastFailLogTime");
            if (lastFailLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastFailLogTime", callName);
                return false;
            }

            int lastFailLogFileColumnIdx = statement->FindColumn("LastFailLogFile");
            if (lastFailLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastFailLogFile", callName);
                return false;
            }

            int lastLogTimeColumnIdx = statement->FindColumn("LastLogTime");
            if (lastLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastLogTime", callName);
                return false;
            }

            int lastLogFileColumnIdx = statement->FindColumn("LastLogFile");
            if (lastLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastLogFile", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                JobDatabaseEntry job;
                job.m_jobKey = AZStd::move(statement->GetColumnText(jobKeyColumnIdx));
                job.m_builderGuid = statement->GetColumnUuid(buildeGuidColumnIdx);
                job.m_status = static_cast<AssetSystem::JobStatus>(statement->GetColumnInt(statusColumnIdx));

                bool callHandler = (jobKey ? job.m_jobKey == jobKey : true &&
                                    !builderGuid.IsNull() ? job.m_builderGuid == builderGuid : true &&
                                    status != AssetSystem::JobStatus::Any ? job.m_status == status : true);
                if (callHandler)
                {
                    job.m_jobID = statement->GetColumnInt64(jobIDColumnIdx);
                    job.m_sourcePK = statement->GetColumnInt64(sourcePKColumnIdx);
                    job.m_fingerprint = statement->GetColumnInt(fingerprintColumnIdx);
                    job.m_platform = AZStd::move(statement->GetColumnText(platformColumnIdx));
                    job.m_status = static_cast<AssetSystem::JobStatus>(statement->GetColumnInt(statusColumnIdx));
                    job.m_jobRunKey = statement->GetColumnInt64(jobRunKeyColumnIdx);
                    job.m_firstFailLogTime = statement->GetColumnInt64(firstFailLogTimeColumnIdx);
                    job.m_firstFailLogFile = AZStd::move(statement->GetColumnText(firstFailLogFileColumnIdx));
                    job.m_lastFailLogTime = statement->GetColumnInt64(lastFailLogTimeColumnIdx);
                    job.m_lastFailLogFile = AZStd::move(statement->GetColumnText(lastFailLogFileColumnIdx));
                    job.m_lastLogTime = statement->GetColumnInt64(lastLogTimeColumnIdx);
                    job.m_lastLogFile = AZStd::move(statement->GetColumnText(lastLogFileColumnIdx));

                    if (handler(job))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                }
                else
                {
                    result = statement->Step();
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetProductResult(const char* callName, Statement* statement, productHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.
            Statement::SqlStatus result = statement->Step();

            int productIDColumnIdx = statement->FindColumn("ProductID");
            if (productIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ProductID", callName);
                return false;
            }

            int jobPKColumnIdx = statement->FindColumn("JobPK");
            if (jobPKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobPK", callName);
                return false;
            }

            int subIDColumnIdx = statement->FindColumn("SubID");
            if (subIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SubID", callName);
                return false;
            }

            int productNameColumnIdx = statement->FindColumn("ProductName");
            if (productNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ProductName", callName);
                return false;
            }

            int assetTypeColumnIdx = statement->FindColumn("AssetType");
            if (assetTypeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "AssetType", callName);
                return false;
            }

            int legacyGuidColumnIdx = statement->FindColumn("LegacyGuid");
            if (legacyGuidColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LegacyGuid", callName);
                return false;
            }

            int jobKeyColumnIdx = -1;
            if (jobKey)
            {
                jobKeyColumnIdx = statement->FindColumn("JobKey");
                if (jobKeyColumnIdx == -1)
                {
                    AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobKey", callName);
                    return false;
                }
            }

            int builderGuidColumnIdx = -1;
            if (!builderGuid.IsNull())
            {
                builderGuidColumnIdx = statement->FindColumn("BuilderGuid");
                if (builderGuidColumnIdx == -1)
                {
                    AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "BuilderGuid", callName);
                    return false;
                }
            }

            int statusColumnIdx = -1;
            if (status != AssetSystem::JobStatus::Any)
            {
                statusColumnIdx = statement->FindColumn("Status");
                if (statusColumnIdx == -1)
                {
                    AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Status", callName);
                    return false;
                }
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                bool callHandler = (jobKey ? statement->GetColumnText(jobKeyColumnIdx) == jobKey : true &&
                                    !builderGuid.IsNull() ? statement->GetColumnUuid(builderGuidColumnIdx) == builderGuid : true &&
                                    status != AssetSystem::JobStatus::Any ? static_cast<AssetSystem::JobStatus>(statement->GetColumnInt(statusColumnIdx)) == status : true);
                if (callHandler)
                {
                    ProductDatabaseEntry product;
                    product.m_productID = statement->GetColumnInt64(productIDColumnIdx);
                    product.m_jobPK = statement->GetColumnInt64(jobPKColumnIdx);
                    product.m_subID = statement->GetColumnInt(subIDColumnIdx);
                    product.m_productName = AZStd::move(statement->GetColumnText(productNameColumnIdx));
                    product.m_assetType = statement->GetColumnUuid(assetTypeColumnIdx);
                    product.m_legacyGuid = statement->GetColumnUuid(legacyGuidColumnIdx);

                    if (handler(product))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                }
                else
                {
                    result = statement->Step();
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetProductDependencyResult(const char* callName, Statement* statement, productDependencyHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.
            Statement::SqlStatus result = statement->Step();

            int productDependencyIDColumnIdx = statement->FindColumn("ProductDependencyID");
            if (productDependencyIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "ProductDependencyID");
                return false;
            }

            int productPKColumnIdx = statement->FindColumn("ProductPK");
            if (productPKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "ProductPK");
                return false;
            }

            int dependencySourceGuidIdx = statement->FindColumn("DependencySourceGuid");
            if (dependencySourceGuidIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "DependencySourceGuid");
                return false;
            }

            int dependencySubIDIdx = statement->FindColumn("DependencySubID");
            if (dependencySubIDIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "DependencySubID");
                return false;
            }

            int dependencyFlagsIdx = statement->FindColumn("DependencyFlags");
            if (dependencyFlagsIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "DependencyFlags");
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                ProductDependencyDatabaseEntry productDependency;
                productDependency.m_productDependencyID = statement->GetColumnInt64(productDependencyIDColumnIdx);
                productDependency.m_productPK = statement->GetColumnInt64(productPKColumnIdx);
                productDependency.m_dependencySourceGuid = statement->GetColumnUuid(dependencySourceGuidIdx);
                productDependency.m_dependencySubID = statement->GetColumnInt(dependencySubIDIdx);
                productDependency.m_dependencyFlags = statement->GetColumnInt64(dependencyFlagsIdx);

                if (handler(productDependency))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetLegacySubIDsResult(const char* callName, SQLite::Statement* statement, legacySubIDsHandler handler)
        {
            AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int sourceLegacySubIDIdx = statement->FindColumn("LegacySubID");
            if (sourceLegacySubIDIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "LegacySubID");
                return false;
            }

            int productPKIdx = statement->FindColumn("ProductPK");
            if (productPKIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "ProductPK");
                return false;
            }

            int subIDIdx = statement->FindColumn("SubID");
            if (subIDIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", callName, "SubID");
                return false;
            }
            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                LegacySubIDsEntry entry;
                entry.m_subIDsEntryID = statement->GetColumnInt64(sourceLegacySubIDIdx);
                entry.m_productPK = statement->GetColumnInt64(productPKIdx);
                entry.m_subID = statement->GetColumnInt(subIDIdx);

                if (handler(entry))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }


        bool AssetDatabaseConnection::GetCombinedResult(const char* callName, Statement* statement, combinedHandler handler, AZ::Uuid builderGuid, const char* jobKey, AssetSystem::JobStatus status, bool includeLegacySubIDs)
        {
            AZ_UNUSED(callName); // AZ_Error may be compiled out entirely in release builds.
            Statement::SqlStatus result = statement->Step();

            //////////////////////////////////////////////////////////////////////////
            //scanfolder
            int scanFolderIDColumnIdx = statement->FindColumn("ScanFolderID");
            if (scanFolderIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolderID", callName);
                return false;
            }

            int scanFolderColumnIdx = statement->FindColumn("ScanFolder");
            if (scanFolderColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolder", callName);
                return false;
            }

            int displayNameIdx = statement->FindColumn("DisplayName");
            if (displayNameIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "DisplayName", callName);
                return false;
            }

            int outputPrefixIdx = statement->FindColumn("OutputPrefix");
            if (outputPrefixIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "OutputPrefix", callName);
                return false;
            }

            //////////////////////////////////////////////////////////////////////////
            //source
            int sourceIDColumnIdx = statement->FindColumn("SourceID");
            if (sourceIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceID", callName);
                return false;
            }

            int scanFolderPKColumnIdx = statement->FindColumn("ScanFolderPK");
            if (scanFolderPKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolderPK", callName);
                return false;
            }

            int sourceNameColumnIdx = statement->FindColumn("SourceName");
            if (sourceNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceName", callName);
                return false;
            }

            int guidValueColumnIdx = statement->FindColumn("SourceGuid");
            if (guidValueColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourceGuid", callName);
                return false;
            }

            //////////////////////////////////////////////////////////////////////////
            //job
            int jobIDColumnIdx = statement->FindColumn("JobID");
            if (jobIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobID", callName);
                return false;
            }

            int sourcePKColumnIdx = statement->FindColumn("SourcePK");
            if (sourcePKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SourcePK", callName);
                return false;
            }

            int jobKeyColumnIdx = statement->FindColumn("JobKey");
            if (jobKeyColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobKey", callName);
                return false;
            }

            int fingerprintColumnIdx = statement->FindColumn("Fingerprint");
            if (fingerprintColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Fingerprint", callName);
                return false;
            }

            int platformColumnIdx = statement->FindColumn("Platform");
            if (platformColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Platform", callName);
                return false;
            }

            int builderGuidColumnIdx = statement->FindColumn("BuilderGuid");
            if (builderGuidColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "BuilderGuid", callName);
                return false;
            }

            int statusColumnIdx = statement->FindColumn("Status");
            if (statusColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "Status", callName);
                return false;
            }

            int jobRunKeyColumnIdx = statement->FindColumn("JobRunKey");
            if (jobRunKeyColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobRunKey", callName);
                return false;
            }

            int firstFailLogTimeColumnIdx = statement->FindColumn("FirstFailLogTime");
            if (firstFailLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FirstFailLogTime", callName);
                return false;
            }

            int firstFailLogFileColumnIdx = statement->FindColumn("FirstFailLogFile");
            if (firstFailLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FirstFailLogFile", callName);
                return false;
            }

            int lastFailLogTimeColumnIdx = statement->FindColumn("LastFailLogTime");
            if (lastFailLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastFailLogTime", callName);
                return false;
            }

            int lastFailLogFileColumnIdx = statement->FindColumn("LastFailLogFile");
            if (lastFailLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastFailLogFile", callName);
                return false;
            }

            int lastLogTimeColumnIdx = statement->FindColumn("LastLogTime");
            if (lastLogTimeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastLogTime", callName);
                return false;
            }

            int lastLogFileColumnIdx = statement->FindColumn("LastLogFile");
            if (lastLogFileColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LastLogFile", callName);
                return false;
            }

            //////////////////////////////////////////////////////////////////////////
            //product

            int productIDColumnIdx = statement->FindColumn("ProductID");
            if (productIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ProductID", callName);
                return false;
            }

            int jobPKColumnIdx = statement->FindColumn("JobPK");
            if (jobPKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "JobPK", callName);
                return false;
            }

            int subIDColumnIdx = statement->FindColumn("SubID");
            if (subIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "SubID", callName);
                return false;
            }

            int productNameColumnIdx = statement->FindColumn("ProductName");
            if (productNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ProductName", callName);
                return false;
            }

            int assetTypeColumnIdx = statement->FindColumn("AssetType");
            if (assetTypeColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "AssetType", callName);
                return false;
            }

            int legacyGuidColumnIdx = statement->FindColumn("LegacyGuid");
            if (legacyGuidColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "LegacyGuid", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                CombinedDatabaseEntry combined;
                combined.m_builderGuid = statement->GetColumnUuid(builderGuidColumnIdx);
                combined.m_jobKey = AZStd::move(statement->GetColumnText(jobKeyColumnIdx));
                combined.m_status = static_cast<AssetSystem::JobStatus>(statement->GetColumnInt(statusColumnIdx));

                bool callHandler = (jobKey ? combined.m_jobKey == jobKey : true &&
                                    !builderGuid.IsNull() ? combined.m_builderGuid == builderGuid : true &&
                                    status != AssetSystem::JobStatus::Any ? combined.m_status == status : true);
                if (callHandler)
                {
                    //scan folder
                    combined.m_scanFolderID = statement->GetColumnInt64(scanFolderIDColumnIdx);
                    combined.m_scanFolder = AZStd::move(statement->GetColumnText(scanFolderColumnIdx));
                    combined.m_displayName = AZStd::move(statement->GetColumnText(displayNameIdx));
                    combined.m_outputPrefix = AZStd::move(statement->GetColumnText(outputPrefixIdx));

                    //source
                    combined.m_sourceID = statement->GetColumnInt64(sourceIDColumnIdx);
                    combined.m_scanFolderPK = statement->GetColumnInt64(scanFolderPKColumnIdx);
                    combined.m_sourceName = AZStd::move(statement->GetColumnText(sourceNameColumnIdx));
                    combined.m_sourceGuid = statement->GetColumnUuid(guidValueColumnIdx);

                    //job
                    combined.m_jobID = statement->GetColumnInt64(jobIDColumnIdx);
                    combined.m_sourcePK = statement->GetColumnInt64(sourcePKColumnIdx);
                    //combined.m_jobKey = AZStd::move(statement->GetColumnText(jobKeyColumnIdx)); //is done above for filtering
                    combined.m_fingerprint = statement->GetColumnInt(fingerprintColumnIdx);
                    combined.m_platform = AZStd::move(statement->GetColumnText(platformColumnIdx));
                    //combined.m_builderGuid = statement->GetColumnUuid(builderGuidColumnIdx); //is done above for filtering
                    //combined.m_status = static_cast<AssetSystem::JobStatus>(statement->GetColumnInt(statusColumnIdx)); //is done above for filtering
                    combined.m_jobRunKey = statement->GetColumnInt64(jobRunKeyColumnIdx);
                    combined.m_firstFailLogTime = statement->GetColumnInt64(firstFailLogFileColumnIdx);
                    combined.m_firstFailLogFile = AZStd::move(statement->GetColumnText(firstFailLogFileColumnIdx));
                    combined.m_lastFailLogTime = statement->GetColumnInt64(lastFailLogTimeColumnIdx);
                    combined.m_lastFailLogFile = AZStd::move(statement->GetColumnText(lastFailLogFileColumnIdx));
                    combined.m_lastLogTime = statement->GetColumnInt64(lastLogTimeColumnIdx);
                    combined.m_lastLogFile = AZStd::move(statement->GetColumnText(lastLogFileColumnIdx));

                    //product
                    combined.m_productID = statement->GetColumnInt64(productIDColumnIdx);
                    combined.m_jobPK = statement->GetColumnInt64(jobPKColumnIdx);
                    combined.m_subID = statement->GetColumnInt(subIDColumnIdx);
                    combined.m_productName = AZStd::move(statement->GetColumnText(productNameColumnIdx));
                    combined.m_assetType = statement->GetColumnUuid(assetTypeColumnIdx);
                    combined.m_legacyGuid = statement->GetColumnUuid(legacyGuidColumnIdx);

                    if (includeLegacySubIDs)
                    {
                        QueryLegacySubIdsByProductID(combined.m_productID, [&combined](LegacySubIDsEntry& entry)
                            {
                                combined.m_legacySubIDs.emplace_back(AZStd::move(entry));
                                return true;
                            }
                            );
                    }

                    if (handler(combined))
                    {
                        result = statement->Step();
                    }
                    else
                    {
                        result = Statement::SqlDone;
                    }
                }
                else
                {
                    result = statement->Step();
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }
            return validResult;
        }

        bool AzToolsFramework::AssetDatabase::AssetDatabaseConnection::GetCombinedDependencyResult(const char* callName, SQLite::Statement* statement, combinedProductDependencyHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            AZStd::vector<int> columns;
            if (!GetStatementColumns(statement, callName, { "ProductDependencyID", "ProductPK", "DependencySourceGuid", "DependencySubID", "DependencyFlags", "SourceGuid", "SubID" }, columns))
            {
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                ProductDependencyDatabaseEntry entry;

                entry.m_productDependencyID = statement->GetColumnInt64(columns[0]);
                entry.m_productPK = statement->GetColumnInt64(columns[1]);
                entry.m_dependencySourceGuid = statement->GetColumnUuid(columns[2]);
                entry.m_dependencySubID = statement->GetColumnInt(columns[3]);
                entry.m_dependencyFlags = statement->GetColumnInt64(columns[4]);

                AZ::Data::AssetId assetId(statement->GetColumnUuid(columns[5]), statement->GetColumnInt(columns[6]));

                if (handler(assetId, entry))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }

        bool AssetDatabaseConnection::GetStatementColumns(Statement* statement, const char* callName, AZStd::initializer_list<const char*> columnNames, AZStd::vector<int>& columnIndices)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            columnIndices.reserve(columnNames.size());

            for (const char* column : columnNames)
            {
                int index = statement->FindColumn(column);

                if (index == -1)
                {
                    AZ_Error(LOG_NAME, false, "Missing %s column in %s result", column, callName);
                    return false;
                }

                columnIndices.push_back(index);
            }

            return true;
        }

        bool AssetDatabaseConnection::GetFileResult(const char* callName, SQLite::Statement* statement, fileHandler handler)
        {
            (void)callName; // AZ_Error may be compiled out entirely in release builds.

            Statement::SqlStatus result = statement->Step();

            int fileIDColumnIdx = statement->FindColumn("FileID");
            if (fileIDColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FileID", callName);
                return false;
            }

            int scanFolderPKColumnIdx = statement->FindColumn("ScanFolderPK");
            if (scanFolderPKColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "ScanFolderPK", callName);
                return false;
            }

            int fileNameColumnIdx = statement->FindColumn("FileName");
            if (fileNameColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "FileName", callName);
                return false;
            }

            int isFolderColumnIdx = statement->FindColumn("IsFolder");
            if (isFolderColumnIdx == -1)
            {
                AZ_Error(LOG_NAME, false, "Results from %s failed to have a %s column", "IsFolder", callName);
                return false;
            }

            bool validResult = result == Statement::SqlDone;
            while (result == Statement::SqlOK)
            {
                FileDatabaseEntry file;
                file.m_fileID = statement->GetColumnInt64(fileIDColumnIdx);
                file.m_scanFolderPK = statement->GetColumnInt64(scanFolderPKColumnIdx);
                file.m_fileName = AZStd::move(statement->GetColumnText(fileNameColumnIdx));
                file.m_isFolder = statement->GetColumnInt64(isFolderColumnIdx);

                if (handler(file))
                {
                    result = statement->Step();
                }
                else
                {
                    result = Statement::SqlDone;
                }
                validResult = true;
            }

            if (result == Statement::SqlError)
            {
                AZ_Warning(LOG_NAME, false, "Error occurred while stepping %s", callName);
                return false;
            }

            return validResult;
        }
    } // namespace AssetDatabase
} // namespace AZToolsFramework
