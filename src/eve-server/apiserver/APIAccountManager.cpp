/*
	------------------------------------------------------------------------------------
	LICENSE:
	------------------------------------------------------------------------------------
	This file is part of EVEmu: EVE Online Server Emulator
	Copyright 2006 - 2011 The EVEmu Team
	For the latest information visit http://evemu.org
	------------------------------------------------------------------------------------
	This program is free software; you can redistribute it and/or modify it under
	the terms of the GNU Lesser General Public License as published by the Free Software
	Foundation; either version 2 of the License, or (at your option) any later
	version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 59 Temple
	Place - Suite 330, Boston, MA 02111-1307, USA, or go to
	http://www.gnu.org/copyleft/lesser.txt.
	------------------------------------------------------------------------------------
	Author:		Aknor Jaden
*/


#include "EVEServerPCH.h"


std::string APIAccountManager::m_hexCharMap("0123456789ABCDEF");


APIAccountManager::APIAccountManager()
{
}

std::tr1::shared_ptr<std::string> APIAccountManager::ProcessCall(const APICommandCall * pAPICommandCall)
{
    sLog.Debug("APIAccountManager::ProcessCall()", "EVEmu API - Account Service Manager");

    if( pAPICommandCall->find( "servicehandler" ) == pAPICommandCall->end() )
    {
        sLog.Error( "APIAccountManager::ProcessCall()", "Cannot find 'servicehandler' specifier in pAPICommandCall packet" );
        return std::tr1::shared_ptr<std::string>(new std::string(""));
    }

    if( pAPICommandCall->find( "servicehandler" )->second == "APIKeyRequest.xml.aspx" )
        return _APIKeyRequest(pAPICommandCall);
    //else if( pAPICommandCall->find( "servicehandler" )->second == "TODO.xml.aspx" )
    //    return _TODO(pAPICommandCall);
    else
    {
        sLog.Error("APIAccountManager::ProcessCall()", "EVEmu API - Account Service Manager - ERROR: Cannot resolve '%s' as a valid service query for Admin Service Manager", pAPICommandCall->find("admin")->second.c_str() );
        return std::tr1::shared_ptr<std::string>(new std::string(""));
    }
}

std::tr1::shared_ptr<std::string> APIAccountManager::_APIKeyRequest(const APICommandCall * pAPICommandCall)
{
    // TEST of generating username/password hash for authentication for generating new API keys:
    std::string passHash;
    std::string username = "mdrayman";
    std::string pwd = "mdrayman";
    std::wstring w_username = Utils::Strings::StringToWString(username);
    std::wstring w_password = Utils::Strings::StringToWString(pwd);
    PasswordModule::GeneratePassHash(w_username,w_password,passHash);
    std::string hexHash = PasswordModule::GenerateHexString(passHash);
    std::string dateTime = Win32TimeToString(Win32TimeNow());
    // END TEST

    bool status = false;
    uint32 userID, apiRole;
    std::string accountID;
    std::string password;
    std::string keyType;
    std::string apiLimitedKey;
    std::string apiFullKey;
    sLog.Debug("APIAccountManager::_APIKeyRequest()", "EVEmu API - Account Service Manager - CALL: API Key Request");

    // 0: Decode arguments:
    if( pAPICommandCall->find( "accountID" ) != pAPICommandCall->end() )
        accountID = pAPICommandCall->find( "accountID" )->second;
    else
    {
        sLog.Error( "APIAccountManager::_APIKeyRequest()", "ERROR: No 'accountID' parameter found in call argument list - exiting with error" );
        return BuildErrorXMLResponse( "203", "Authentication failure." );
    }

    if( pAPICommandCall->find( "password" ) != pAPICommandCall->end() )
        password = pAPICommandCall->find( "password" )->second;
    else
    {
        sLog.Error( "APIAccountManager::_APIKeyRequest()", "ERROR: No 'password' parameter found in call argument list - exiting with error" );
        return BuildErrorXMLResponse( "203", "Authentication failure." );
    }

    if( pAPICommandCall->find( "keyType" ) != pAPICommandCall->end() )
        keyType = pAPICommandCall->find( "keyType" )->second;
    else
    {
        sLog.Error( "APIAccountManager::_APIKeyRequest()", "ERROR: No 'keyType' parameter found in call argument list - exiting with error" );
        return BuildErrorXMLResponse( "203", "Authentication failure." );
    }

    if( keyType != "full" )
        if( keyType != "limited" )
        {
            sLog.Error( "APIAccountManager::_APIKeyRequest()", "ERROR: 'keyType' parameter has invalid value '%s' - exiting with error", keyType.c_str() );
            return BuildErrorXMLResponse( "203", "Authentication failure." );
        }

    // 1: Determine if this account's userID exists:
    status = m_db.GetApiAccountInfoUsingAccountID( accountID, &userID, &apiFullKey, &apiLimitedKey, &apiRole );

    // 2: Generate new random 64-character hexadecimal API Keys:
    apiLimitedKey = _GenerateAPIKey();
    apiFullKey = _GenerateAPIKey();
    if( !(status) )
        // 2: If userID already exists, generate new API keys and write them back to the database under that userID:
        status = m_db.UpdateUserIdApiKeyDatabaseRow( userID, apiFullKey, apiLimitedKey );
    else
        // 3: If userID does not exist for this accountID, then insert a new row into the 'accountApi' table:
        status = m_db.InsertNewUserIdApiKeyInfoToDatabase( atol(accountID.c_str()), apiFullKey, apiLimitedKey, EVEAPI::Roles::Player );

    // 4: Build XML document to return to API client:
    status = m_db.GetApiAccountInfoUsingAccountID( accountID, &userID, &apiFullKey, &apiLimitedKey, &apiRole );
    _BuildXMLHeader();
    {
        _BuildSingleXMLTag( "userID", std::string(itoa(userID)) );
        if( keyType == "full" )
            _BuildSingleXMLTag( "newKey", apiFullKey );
        else
            _BuildSingleXMLTag( "newKey", apiLimitedKey );
    }
    _CloseXMLHeader( EVEAPI::CacheStyles::Long );

    return _GetXMLDocumentString();
}

std::string APIAccountManager::_GenerateAPIKey()
{
    std::string key = "";

    for( int i=0; i<64; i++ )
        key += m_hexCharMap.substr( MakeRandomInt( 0, 15 ), 1 );

    return key;
}