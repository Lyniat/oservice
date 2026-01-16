#include <Unet_common.h>
#include <Unet/Services/ServiceSteam.h>
#include <Unet/LobbyPacket.h>
#include <steam/steam_api_flat.h>
#include "../Ruby/print.h"

Unet::ServiceSteam::ServiceSteam(Internal::Context* ctx, int numChannels) :
	Service(ctx, numChannels),
	m_callLobbyDataUpdate(this, &ServiceSteam::OnLobbyDataUpdate),
	m_callLobbyKicked(this, &ServiceSteam::OnLobbyKicked),
	m_callLobbyChatUpdate(this, &ServiceSteam::OnLobbyChatUpdate),
	m_callP2PSessionRequest(this, &ServiceSteam::OnP2PSessionRequest)
{
}

Unet::ServiceSteam::~ServiceSteam()
{
}

void Unet::ServiceSteam::SimulateOutage()
{
	auto lobby = m_ctx->CurrentLobby();
	if (lobby == nullptr) {
		return;
	}

	auto &lobbyInfo = lobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(GetType());
	if (entryPoint == nullptr) {
		return;
	}

	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->LeaveLobby((uint64)entryPoint->ID);
	SteamAPI_ISteamMatchmaking_LeaveLobby(matchmaking, (uint64)entryPoint->ID);

	for (auto member : lobby->GetMembers()) {
		auto id = member->GetServiceID(ServiceType::Steam);
		if (id.IsValid()) {
			SteamNetworking()->CloseP2PSessionWithUser((uint64)id.ID);
		}
	}
}

Unet::ServiceType Unet::ServiceSteam::GetType()
{
	return ServiceType::Steam;
}

Unet::ServiceID Unet::ServiceSteam::GetUserID()
{
	return ServiceID(ServiceType::Steam, SteamAPI_ISteamUser_GetSteamID(SteamUser()));
}

std::string Unet::ServiceSteam::GetServiceUserName()
{
	return SteamFriends()->GetPersonaName();
}

void Unet::ServiceSteam::SetRichPresence(const char* key, const char* value)
{
	SteamFriends()->SetRichPresence(key, value);
}

void Unet::ServiceSteam::CreateLobby(LobbyPrivacy privacy, int maxPlayers, LobbyInfo lobbyInfo)
{
	ELobbyType type = k_ELobbyTypePublic;
	switch (privacy) {
	case LobbyPrivacy::Public: type = k_ELobbyTypePublic; break;
	case LobbyPrivacy::Private: type = k_ELobbyTypePrivate; break;
	case LobbyPrivacy::FriendsOnly: type = k_ELobbyTypeFriendsOnly; break;
	}

	auto matchmaking = SteamMatchmaking();
	//SteamAPICall_t call = SteamMatchmaking()->CreateLobby(type, maxPlayers);
	SteamAPICall_t call = SteamAPI_ISteamMatchmaking_CreateLobby(matchmaking, type, maxPlayers);
	m_requestLobbyCreated = m_ctx->m_callbackCreateLobby.AddServiceRequest(this);
	m_callLobbyCreated.Set(call, this, &ServiceSteam::OnLobbyCreated);
}

void Unet::ServiceSteam::SetLobbyPrivacy(const ServiceID &lobbyId, LobbyPrivacy privacy)
{
	ELobbyType type = k_ELobbyTypePublic;
	switch (privacy) {
	case LobbyPrivacy::Public: type = k_ELobbyTypePublic; break;
	case LobbyPrivacy::Private: type = k_ELobbyTypePrivate; break;
	case LobbyPrivacy::FriendsOnly: type = k_ELobbyTypeFriendsOnly; break;
	}

	assert(lobbyId.Service == ServiceType::Steam);
	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->SetLobbyType((uint64)lobbyId.ID, type);
	SteamAPI_ISteamMatchmaking_SetLobbyType(matchmaking, (uint64)lobbyId.ID, type);
}

void Unet::ServiceSteam::SetLobbyJoinable(const ServiceID &lobbyId, bool joinable)
{
	assert(lobbyId.Service == ServiceType::Steam);
	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->SetLobbyJoinable((uint64)lobbyId.ID, joinable);
	SteamAPI_ISteamMatchmaking_SetLobbyJoinable(matchmaking, (uint64)lobbyId.ID, joinable);
}

void Unet::ServiceSteam::GetLobbyList()
{
	auto matchmaking = SteamMatchmaking();
	//SteamAPICall_t call = SteamMatchmaking()->RequestLobbyList();
	SteamAPICall_t call = SteamAPI_ISteamMatchmaking_RequestLobbyList(matchmaking);
	m_requestLobbyList = m_ctx->m_callbackLobbyList.AddServiceRequest(this);
	m_callLobbyList.Set(call, this, &ServiceSteam::OnLobbyList);
}

bool Unet::ServiceSteam::FetchLobbyInfo(const ServiceID &id)
{
	assert(id.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//if (SteamMatchmaking()->RequestLobbyData((uint64)id.ID)) {
	if (SteamAPI_ISteamMatchmaking_RequestLobbyData(matchmaking, (uint64)id.ID)) {
		m_dataFetch.emplace_back(id.ID);
		return true;
	}

	return false;
}

void Unet::ServiceSteam::JoinLobby(const ServiceID &id)
{
	assert(id.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//SteamAPICall_t call = SteamMatchmaking()->JoinLobby((uint64)id.ID);
	SteamAPICall_t call = SteamAPI_ISteamMatchmaking_JoinLobby(matchmaking, (uint64)id.ID);
	m_requestLobbyJoin = m_ctx->m_callbackLobbyJoin.AddServiceRequest(this);
	m_callLobbyJoin.Set(call, this, &ServiceSteam::OnLobbyJoin);
}

void Unet::ServiceSteam::LeaveLobby()
{
	auto lobby = m_ctx->CurrentLobby();
	if (lobby == nullptr) {
		return;
	}

	auto &lobbyInfo = lobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(GetType());
	if (entryPoint == nullptr) {
		return;
	}

	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->LeaveLobby((uint64)entryPoint->ID);
	SteamAPI_ISteamMatchmaking_LeaveLobby(matchmaking,(uint64)entryPoint->ID);

	for (auto member : lobby->GetMembers()) {
		auto id = member->GetServiceID(ServiceType::Steam);
		if (id.IsValid()) {
			//SteamNetworking()->CloseP2PSessionWithUser((uint64)id.ID);
			SteamAPI_ISteamNetworking_CloseP2PSessionWithUser(SteamNetworking(), (uint64)id.ID);
		}
	}

	auto serviceRequest = m_ctx->m_callbackLobbyLeft.AddServiceRequest(this);
	serviceRequest->Code = Result::OK;
}

int Unet::ServiceSteam::GetLobbyPlayerCount(const ServiceID &lobbyId)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//return SteamMatchmaking()->GetNumLobbyMembers((uint64)lobbyId.ID);
	return SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(matchmaking, (uint64)lobbyId.ID);
}

void Unet::ServiceSteam::SetLobbyMaxPlayers(const ServiceID &lobbyId, int amount)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->SetLobbyMemberLimit((uint64)lobbyId.ID, amount);
	SteamAPI_ISteamMatchmaking_SetLobbyMemberLimit(matchmaking, (uint64)lobbyId.ID, amount);
}

int Unet::ServiceSteam::GetLobbyMaxPlayers(const ServiceID &lobbyId)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//return SteamMatchmaking()->GetLobbyMemberLimit((uint64)lobbyId.ID);
	return SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(matchmaking, (uint64)lobbyId.ID);
}

std::string Unet::ServiceSteam::GetLobbyData(const ServiceID &lobbyId, const char* name)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//return SteamMatchmaking()->GetLobbyData((uint64)lobbyId.ID, name);
	return SteamAPI_ISteamMatchmaking_GetLobbyData(matchmaking, (uint64)lobbyId.ID, name);
}

int Unet::ServiceSteam::GetLobbyDataCount(const ServiceID &lobbyId)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//return SteamMatchmaking()->GetLobbyDataCount((uint64)lobbyId.ID);
	return SteamAPI_ISteamMatchmaking_GetLobbyDataCount(matchmaking,(uint64)lobbyId.ID);
}

Unet::LobbyData Unet::ServiceSteam::GetLobbyData(const ServiceID &lobbyId, int index)
{
	assert(lobbyId.Service == ServiceType::Steam);

	char szKey[512];
	char szValue[512];

	LobbyData ret;
	auto matchmaking = SteamMatchmaking();
	//if (SteamMatchmaking()->GetLobbyDataByIndex((uint64)lobbyId.ID, index, szKey, 512, szValue, 512)) {
	if (SteamAPI_ISteamMatchmaking_GetLobbyDataByIndex(matchmaking, (uint64)lobbyId.ID, index, szKey, 512, szValue, 512)) {
		ret.Name = szKey;
		ret.Value = szValue;
	}

	return ret;
}

Unet::ServiceID Unet::ServiceSteam::GetLobbyHost(const ServiceID &lobbyId)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	auto owner = SteamAPI_ISteamMatchmaking_GetLobbyOwner(matchmaking, (uint64)lobbyId.ID);
	return ServiceID(ServiceType::Steam, owner);
}

void Unet::ServiceSteam::SetLobbyData(const ServiceID &lobbyId, const char* name, const char* value)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->SetLobbyData((uint64)lobbyId.ID, name, value);
	SteamAPI_ISteamMatchmaking_SetLobbyData(matchmaking, (uint64)lobbyId.ID, name, value);
}

void Unet::ServiceSteam::RemoveLobbyData(const ServiceID &lobbyId, const char* name)
{
	assert(lobbyId.Service == ServiceType::Steam);

	auto matchmaking = SteamMatchmaking();
	//SteamMatchmaking()->DeleteLobbyData((uint64)lobbyId.ID, name);
	SteamAPI_ISteamMatchmaking_DeleteLobbyData(matchmaking, (uint64)lobbyId.ID, name);

}

size_t Unet::ServiceSteam::ReliablePacketLimit()
{
	//TODO: Optimization: If we know we're not going to send more than this, we can safely return 0 here.
	//      Perhaps this could be a service-specific option flag?
	return 1200; //max MTU size //1024 * 1024;
}

void Unet::ServiceSteam::SendPacket(const ServiceID &peerId, const void* data, size_t size, PacketType type, uint8_t channel)
{
	assert(peerId.Service == ServiceType::Steam);

	EP2PSend sendType = k_EP2PSendUnreliable;
	switch (type) {
	case PacketType::Unreliable: sendType = k_EP2PSendUnreliable; break;
	case PacketType::Reliable: sendType = k_EP2PSendReliable; break;
	}
	SteamNetworking()->SendP2PPacket((uint64)peerId.ID, data, (uint32)size, sendType, (int)channel);
}

size_t Unet::ServiceSteam::ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel)
{
	uint32 readSize;
	CSteamID peer;
	SteamNetworking()->ReadP2PPacket(data, (uint32)maxSize, &readSize, &peer, (int)channel);

	if (peerId != nullptr) {
		*peerId = ServiceID(ServiceType::Steam, peer.ConvertToUint64());
	}
	return (size_t)readSize;
}

bool Unet::ServiceSteam::IsPacketAvailable(size_t* outPacketSize, uint8_t channel)
{
	uint32 packetSize;
	bool ret = SteamNetworking()->IsP2PPacketAvailable(&packetSize, (int)channel);

	if (outPacketSize != nullptr) {
		*outPacketSize = (size_t)packetSize;
	}
	return ret;
}

void Unet::ServiceSteam::OnLobbyCreated(LobbyCreated_t* result, bool bIOFailure)
{
	if (bIOFailure) {
		m_requestLobbyCreated->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug("[Steam] IO Failure while creating lobby");
		return;
	}

	if (result->m_eResult != k_EResultOK) {
		m_requestLobbyCreated->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Failed creating lobby, error %d", (int)result->m_eResult));
		return;
	}

	m_hostID = SteamAPI_ISteamUser_GetSteamID(SteamUser());

	m_requestLobbyCreated->Data->CreatedLobby->AddEntryPoint(ServiceID(ServiceType::Steam, result->m_ulSteamIDLobby));
	m_requestLobbyCreated->Code = Result::OK;

	m_ctx->GetCallbacks()->OnLogDebug("[Steam] Lobby created");
}

void Unet::ServiceSteam::OnLobbyList(LobbyMatchList_t* result, bool bIOFailure)
{
	if (bIOFailure) {
		m_requestLobbyList->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug("[Steam] IO Failure while listing lobbies");
		return;
	}

	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Lobby list received (%d)", (int)result->m_nLobbiesMatching));

	m_listDataFetch.clear();

	for (int i = 0; i < (int)result->m_nLobbiesMatching; i++) {
		//CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
		auto matchmaking = SteamMatchmaking();
		CSteamID lobbyId = SteamAPI_ISteamMatchmaking_GetLobbyByIndex(matchmaking, i);
		if (!lobbyId.IsValid()) {
			continue;
		}

		//if (SteamMatchmaking()->RequestLobbyData(lobbyId)) {
		if (SteamAPI_ISteamMatchmaking_RequestLobbyData(matchmaking, lobbyId.ConvertToUint64())){ // FIXME: ConvertToUint64()?
			m_listDataFetch.emplace_back(lobbyId.ConvertToUint64());
		}
	}

	auto friends = SteamAPI_SteamFriends();
	int nFriends = SteamAPI_ISteamFriends_GetFriendCount(friends, k_EFriendFlagImmediate );
	if ( nFriends == -1)
	{
		nFriends = 0;
	}

	for ( int i = 0; i < nFriends; ++i )
	{
		CSteamID friendSteamID = SteamAPI_SteamFriends()->GetFriendByIndex( i, k_EFriendFlagImmediate );

		FriendGameInfo_t info;
		bool valid = SteamAPI_SteamFriends()->GetFriendGamePlayed(friendSteamID, &info);
		if (valid)
		{
			auto game_id = info.m_gameID;
			if (game_id.IsValid())
			{
			        auto lobby_id = info.m_steamIDLobby;
			        if (lobby_id.IsValid()) {
				        if (std::find(m_listDataFetch.begin(), m_listDataFetch.end(), lobby_id.ConvertToUint64()) == m_listDataFetch.end()) {
					        auto matchmaking = SteamMatchmaking();
					        if (SteamAPI_ISteamMatchmaking_RequestLobbyData(matchmaking, lobby_id.ConvertToUint64())){ // FIXME: ConvertToUint64()?
						        m_listDataFetch.emplace_back(lobby_id.ConvertToUint64());
					        }
				        }
			        }
			}
		}
	}
}

void Unet::ServiceSteam::OnLobbyJoin(LobbyEnter_t* result, bool bIOFailure)
{
	if (bIOFailure) {
		m_requestLobbyJoin->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug("[Steam] IO Failure while joining lobby");
		return;
	}

	if (result->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
		m_requestLobbyJoin->Code = Result::Error;
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Failed joining lobby, error %d", (int)result->m_EChatRoomEnterResponse));
		return;
	}

	//m_hostID = SteamMatchmaking()->GetLobbyOwner(result->m_ulSteamIDLobby);
	auto matchmaking = SteamMatchmaking();
	m_hostID = SteamAPI_ISteamMatchmaking_GetLobbyOwner(matchmaking, result->m_ulSteamIDLobby);

	ServiceID newEntryPoint;
	newEntryPoint.Service = GetType();
	newEntryPoint.ID = result->m_ulSteamIDLobby;
	m_requestLobbyJoin->Data->JoinedLobby->AddEntryPoint(newEntryPoint);

	m_requestLobbyJoin->Code = Result::OK;

	m_ctx->GetCallbacks()->OnLogDebug("[Steam] Lobby joined");

	//auto lobbyOwner = SteamMatchmaking()->GetLobbyOwner(result->m_ulSteamIDLobby);
	auto lobbyOwner = SteamAPI_ISteamMatchmaking_GetLobbyOwner(matchmaking, result->m_ulSteamIDLobby);

	json js;
	js["t"] = (uint8_t)LobbyPacketType::Handshake;
	js["guid"] = m_requestLobbyJoin->Data->JoinGuid.str();
	m_ctx->InternalSendTo(ServiceID(ServiceType::Steam, lobbyOwner), js);

	m_ctx->GetCallbacks()->OnLogDebug("[Steam] Handshake sent");
}

void Unet::ServiceSteam::LobbyListDataUpdated()
{
	if (m_listDataFetch.size() == 0) {
		m_requestLobbyList->Code = Result::OK;
	}
}

void Unet::ServiceSteam::OnLobbyDataUpdate(LobbyDataUpdate_t* result)
{
	if (result->m_ulSteamIDMember == result->m_ulSteamIDLobby) {
		// Lobby data

		auto itListDataFetch = std::find(m_listDataFetch.begin(), m_listDataFetch.end(), result->m_ulSteamIDLobby);
		if (itListDataFetch != m_listDataFetch.end()) {
			// Server list data request
			m_listDataFetch.erase(itListDataFetch);

			auto matchmaking = SteamMatchmaking();
			xg::Guid unetGuid(SteamAPI_ISteamMatchmaking_GetLobbyData(matchmaking, result->m_ulSteamIDLobby, "unet-guid"));
			//xg::Guid unetGuid(SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "unet-guid"));
			if (!unetGuid.isValid()) {
				m_ctx->GetCallbacks()->OnLogDebug("[Steam] unet-guid is not valid!");
				LobbyListDataUpdated();
				return;
			}

			ServiceID newEntryPoint;
			newEntryPoint.Service = ServiceType::Steam;
			newEntryPoint.ID = result->m_ulSteamIDLobby;
			m_requestLobbyList->Data->AddEntryPoint(unetGuid, newEntryPoint);

			LobbyListDataUpdated();
		}

		auto itDataFetch = std::find(m_dataFetch.begin(), m_dataFetch.end(), result->m_ulSteamIDLobby);
		if (itDataFetch != m_dataFetch.end()) {
			// Fetch LobbyInfo data request
			m_dataFetch.erase(itDataFetch);

			LobbyInfoFetchResult res;
			res.ID = ServiceID(ServiceType::Steam, result->m_ulSteamIDLobby);

			auto matchmaking = SteamMatchmaking();
			xg::Guid unetGuid(SteamAPI_ISteamMatchmaking_GetLobbyData(matchmaking, result->m_ulSteamIDLobby, "unet-guid"));
			//xg::Guid unetGuid(SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "unet-guid"));
			if (!unetGuid.isValid()) {
				m_ctx->GetCallbacks()->OnLogDebug("[Steam] unet-guid is not valid!");

				res.Code = Result::Error;
				m_ctx->GetCallbacks()->OnLobbyInfoFetched(res);
				return;
			}

			matchmaking = SteamMatchmaking();
			/*
			res.Info.IsHosting = SteamMatchmaking()->GetLobbyOwner(result->m_ulSteamIDLobby) == SteamAPI_ISteamUser_GetSteamID(SteamUser());
			res.Info.Privacy = (LobbyPrivacy)atoi(SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "unet-privacy"));
			res.Info.NumPlayers = SteamMatchmaking()->GetNumLobbyMembers(result->m_ulSteamIDLobby);
			res.Info.MaxPlayers = SteamMatchmaking()->GetLobbyMemberLimit(result->m_ulSteamIDLobby);
			res.Info.UnetGuid = unetGuid;
			res.Info.Name = SteamMatchmaking()->GetLobbyData(result->m_ulSteamIDLobby, "unet-name");
			res.Info.EntryPoints.emplace_back(ServiceID(ServiceType::Steam, result->m_ulSteamIDLobby));
			*/

			res.Info.IsHosting = SteamAPI_ISteamMatchmaking_GetLobbyOwner(matchmaking, result->m_ulSteamIDLobby) == SteamAPI_ISteamUser_GetSteamID(SteamUser());
			res.Info.Privacy = (LobbyPrivacy)atoi(SteamAPI_ISteamMatchmaking_GetLobbyData(matchmaking, result->m_ulSteamIDLobby, "unet-privacy"));
			res.Info.NumPlayers = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(matchmaking, result->m_ulSteamIDLobby);
			res.Info.MaxPlayers = SteamAPI_ISteamMatchmaking_GetLobbyMemberLimit(matchmaking, result->m_ulSteamIDLobby);
			res.Info.UnetGuid = unetGuid;
			res.Info.Name = SteamAPI_ISteamMatchmaking_GetLobbyData(matchmaking, result->m_ulSteamIDLobby, "unet-name");
			res.Info.EntryPoints.emplace_back(ServiceID(ServiceType::Steam, result->m_ulSteamIDLobby));

			res.Code = Result::OK;
			m_ctx->GetCallbacks()->OnLobbyInfoFetched(res);
		}

		// Regular lobby data update

	} else {
		// Member data

	}
}

void Unet::ServiceSteam::OnLobbyKicked(LobbyKicked_t* result)
{
	auto currentLobby = m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto &lobbyInfo = currentLobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(ServiceType::Steam);
	if (entryPoint == nullptr) {
		return;
	}

	if (entryPoint->ID != result->m_ulSteamIDLobby) {
		return;
	}

	currentLobby->ServiceDisconnected(ServiceType::Steam);
}

void Unet::ServiceSteam::OnLobbyChatUpdate(LobbyChatUpdate_t* result)
{
	auto currentLobby = m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto &lobbyInfo = currentLobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(ServiceType::Steam);
	if (entryPoint == nullptr) {
		return;
	}

	if (entryPoint->ID != result->m_ulSteamIDLobby) {
		return;
	}

	if (result->m_rgfChatMemberStateChange & k_EChatMemberStateChangeEntered) {
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Player entered: 0x%016llX", result->m_ulSteamIDUserChanged));
	} else if (BChatMemberStateChangeRemoved(result->m_rgfChatMemberStateChange)) {
		m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Player left: 0x%016llX (code %X)", result->m_ulSteamIDUserChanged, result->m_rgfChatMemberStateChange));

		SteamNetworking()->CloseP2PSessionWithUser(result->m_ulSteamIDUserChanged);

		if (m_hostID == result->m_ulSteamIDUserChanged) {
			m_ctx->GetCallbacks()->OnLogDebug("[Steam] Host disconnected from lobby, disconnecting and leaving!");

			auto matchmaking = SteamMatchmaking();

			//int numMembers = SteamMatchmaking()->GetNumLobbyMembers(result->m_ulSteamIDLobby);
			int numMembers = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(matchmaking, result->m_ulSteamIDLobby);
			for (int i = 0; i < numMembers; i++) {
				//auto memberId = SteamMatchmaking()->GetLobbyMemberByIndex(result->m_ulSteamIDLobby, i);
				auto memberId = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(matchmaking, result->m_ulSteamIDLobby, i);
				SteamNetworking()->CloseP2PSessionWithUser(memberId);
			}
			SteamMatchmaking()->LeaveLobby(result->m_ulSteamIDLobby);

			currentLobby->ServiceDisconnected(ServiceType::Steam);
		} else {
			currentLobby->RemoveMemberService(ServiceID(ServiceType::Steam, result->m_ulSteamIDUserChanged));
		}
	}
}

void Unet::ServiceSteam::OnP2PSessionRequest(P2PSessionRequest_t* result)
{
	auto currentLobby = m_ctx->CurrentLobby();
	if (currentLobby == nullptr) {
		return;
	}

	auto &lobbyInfo = currentLobby->GetInfo();
	auto entryPoint = lobbyInfo.GetEntryPoint(ServiceType::Steam);
	if (entryPoint == nullptr) {
		return;
	}


	auto matchmaking = SteamMatchmaking();
	//int numMembers = SteamMatchmaking()->GetNumLobbyMembers((uint64)entryPoint->ID);
	int numMembers = SteamAPI_ISteamMatchmaking_GetNumLobbyMembers(matchmaking, (uint64)entryPoint->ID);

	for (int i = 0; i < numMembers; i++) {
		//auto memberId = SteamMatchmaking()->GetLobbyMemberByIndex((uint64)entryPoint->ID, i);
		auto memberId = SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex(matchmaking, (uint64)entryPoint->ID, i);
		//if (memberId == result->m_steamIDRemote.GetAccountID()) { // FIXME: GetAccountID() might be wrong
		if (memberId == result->m_steamIDRemote.ConvertToUint64()) {
			m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Accepting P2P Session Request from 0x%016llX", result->m_steamIDRemote.ConvertToUint64()));
			SteamNetworking()->AcceptP2PSessionWithUser(result->m_steamIDRemote);
			return;
		}
	}

	m_ctx->GetCallbacks()->OnLogDebug(strPrintF("[Steam] Rejecting P2P Session Request from 0x%016llX because they're not in the current Steam lobby!", result->m_steamIDRemote.ConvertToUint64()));
}
