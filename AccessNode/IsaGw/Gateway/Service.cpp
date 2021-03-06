/*
* Copyright (C) 2013 Nivis LLC.
* Email:   opensource@nivis.com
* Website: http://www.nivis.com
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, version 3 of the License.
* 
* Redistribution and use in source and binary forms must retain this
* copyright notice.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

////////////////////////////////////////////////////////////////////////////////
/// @file Service.h
/// @author Marcel Ionescu
/// @brief Generic service - implementation
////////////////////////////////////////////////////////////////////////////////
#include "../../Shared/Common.h"

#include "GwApp.h"
#include "Service.h"

////////////////////////////////////////////////////////////////////////////////
/// @class CService
/// @brief Generic Service - base class for all service classes
/// @remarks
/// Usage: Create a map associating Interface Object Id with CService* containers
///		for all services (CInterfaceObjMgr).
///	Use SetInterfaceOID to register each service into list, and associate with
///		unique Interface Object Id (generated by CInterfaceObjMgr)
/// This will allow multiple instances of the same service, each with it's own
///		Interface Object Id
///
/// (USER -> GW) flow: Parse map<CService*> and call CanHandleServiceType to find
///		the service handling the service type. Once the CService* is found, call
///		CService::ProcessUserRequest to process the user request (resulting usually
///		in one call down ISA stack, but may result in zero or multiple stack calls
///
/// (ISA -> GW) flow: Parse map<CService*> and find the CService with destination
///		OID from GENERIC_ASL_SRVC Response. Call CService::ProcessAPDU or
///		CService::ProcessISATimeout dispatching by GENERIC_ASL_SRVC::m_ucType
///	Derived classes SHOULD override ProcessAPDU/ProcessISATimeout to do anything
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @author Marcel Ionescu
/// @brief (ISA -> GW) Process an ADPU
/// @param p_unAppHandle - the request handle, used to find SessionID/TransactionID in tracker
/// @param p_pAPDUIdtf - needed for device TX time with usec resolution
/// @param p_pRsp The response from field, unpacked
/// @param p_pReq The original request
/// @retval false - field APDU not dispatched - default handler does not do processing, just log a message
/// @remarks This is the default handler, called if the derivated classess do not implement anything
/// We should never get here. If we do, it means:
///		1. Some derived class does not process it's APDU's
///		2. The field send unexpected APDU type
////////////////////////////////////////////////////////////////////////////////
bool CService::ProcessAPDU( uint16 p_unAppHandle, APDU_IDTF* /*p_pAPDUIdtf*/, GENERIC_ASL_SRVC* p_pRsp, GENERIC_ASL_SRVC* /*p_pReq*/ )
{
	LOG("ERROR CService::ProcessAPDU(H %u): unexpected ADPU type %X from OID %u",
		p_unAppHandle, p_pRsp->m_ucType, p_pRsp->m_stSRVC.m_stExecRsp.m_unSrcOID );

	return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @author Marcel Ionescu
/// @brief (ISA -> GW) Proces a ISA100 timeout
/// @param p_unAppHandle - the request handle, used to find SessionID/TransactionID in tracker
/// @param p_pOriginalReq the original request
/// @retval TBD
/// @remarks CALL whenever a timeout is received for a request generated from this class
/// (The method is called from CInterfaceObjMgr::DispatchISATimeout)
/// Derived classess should override this method.
////////////////////////////////////////////////////////////////////////////////
bool CService::ProcessISATimeout( uint16 p_unAppHandle, GENERIC_ASL_SRVC * p_pOriginalReq )
{
	LOG("ERROR CService::ProcessISATimeout(Hnd %u obj %d type %X) UNIMPLEMENTED",
		p_unAppHandle, p_pOriginalReq->m_stSRVC.m_stReadReq.m_unSrcOID, p_pOriginalReq->m_ucType );

	return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @author Marcel Ionescu
/// @brief Dump object status to LOG
/// @remarks Called on USR2
////////////////////////////////////////////////////////////////////////////////
void CService::Dump( void )
{
	LOG("%s: OID %u", Name(), m_ushInterfaceOID );
};

////////////////////////////////////////////////////////////////////////////////
/// @author Marcel Ionescu
/// @brief (GW -> User) send an Indication provided as parameter to the user
/// @param p_ucServiceType - the type of indication
/// @param p_pLease
/// @param p_pMsgData
/// @param p_dwMsgDataLen
/// @retval true always -just a convenience for the callers to send confirm and return in the same statement
/// @remarks Calls CGSAP::SendIndication. Update lease TX time
////////////////////////////////////////////////////////////////////////////////
bool CService::SendIndication( unsigned char p_ucServiceType, lease* p_pLease, uint8* p_pMsgData, uint32 p_dwMsgDataLen )
{
	CGSAP * pGSAP = g_stApp.m_oGwUAP.FindGSAP( p_pLease->nSessionId );
	if(pGSAP)
	{
		if( g_stApp.m_stCfg.AppLogAllowDBG() && (ALERT_NOTIFY != REQUEST(p_ucServiceType)) )/// Alina request to log some more info on publish
		{	//ALERT does not have netAddr in lease
			LOG("PS %s from %s:%u to L %2u data(%u) %s%s", getGSAPServiceNameNoDir(p_ucServiceType), GetHex( p_pLease->netAddr, sizeof(p_pLease->netAddr)), p_pLease->m_ushTSAPID,
				p_pLease->nLeaseId, p_dwMsgDataLen, GET_HEX_LIMITED( p_pMsgData, p_dwMsgDataLen, (unsigned)g_stApp.m_stCfg.m_nMaxPublishDataLog ) );
		}
		pGSAP->SendIndication( p_pLease->nSessionId, g_stApp.m_oGwUAP.NextTransactionId(), p_ucServiceType, p_pMsgData, p_dwMsgDataLen );
		//p_pLease->m_oLastTx.MarkStartTime();	///< Mark in lease LastTX as now()
		const struct timeval * pRxTv = g_stApp.m_oGwUAP.GetLastRxApduUTC();
		((timeval*)p_pLease->m_oLastTx)->tv_sec  = pRxTv->tv_sec;
		((timeval*)p_pLease->m_oLastTx)->tv_usec = pRxTv->tv_usec;
	}
	else
		LOG("indication(%u): Session id %u associated with lease id %u expired, cannot publish to it", p_ucServiceType, p_pLease->nSessionId, p_pLease->nLeaseId );
	return true;
}
