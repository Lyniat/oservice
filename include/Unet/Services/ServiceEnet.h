#pragma once

#include <Unet_common.h>
#include <Unet/Service.h>
#include <Unet/Context.h>

#include <enet/enet.h>
#include <udp_discovery/udp_discovery_peer.hpp>

// Windows sucks
#if defined(GetUserName)
#undef GetUserName
#endif

namespace Unet
{
	struct EnetPacket
	{
		ENetPacket* Packet;
		ENetPeer* Peer;
	};

	class ServiceEnet : public Service
	{
	private:
		ENetHost* m_host = nullptr;

		static std::string m_localUserName;
		static uint64_t m_macAddress;
	        static uint64_t m_applicationName;

		ENetPeer* m_peerHost = nullptr;
		std::vector<ENetPeer*> m_peers;

		std::vector<std::queue<EnetPacket>> m_channels;

		MultiCallback<LobbyJoinResult>::ServiceRequest* m_requestLobbyJoin = nullptr;
		MultiCallback<LobbyLeftResult>::ServiceRequest* m_requestLobbyLeft = nullptr;

		bool m_waitingForPeers = false;

	        udpdiscovery::PeerParameters m_discoveryParams;
	        udpdiscovery::Peer m_discoveryPeer;
	        std::list<udpdiscovery::DiscoveredPeer> m_discoveredPeers;
	        std::map<udpdiscovery::IpPort, std::string> m_lastSeenUserDatas;
	        std::chrono::system_clock::time_point m_lastDiscoveryUpdate;
	        bool m_searching = false;

	public:
		static Unet::ServiceID AddressToID(const ENetAddress &addr);

	public:
		ServiceEnet(Internal::Context* ctx, int numChannels);
		virtual ~ServiceEnet();

		virtual void SimulateOutage() override;

		virtual void RunCallbacks() override;

		virtual ServiceType GetType() override;

		virtual ServiceID GetUserID() override;
		virtual std::string GetServiceUserName() override;

		virtual void SetRichPresence(const char* key, const char* value) override;

		virtual void CreateLobby(LobbyPrivacy privacy, int maxPlayers, LobbyInfo lobbyInfo) override;
		virtual void SetLobbyPrivacy(const ServiceID &lobbyId, LobbyPrivacy privacy) override;
		virtual void SetLobbyJoinable(const ServiceID &lobbyId, bool joinable) override;

		virtual void GetLobbyList() override;
		virtual bool FetchLobbyInfo(const ServiceID &id) override;
		virtual void JoinLobby(const ServiceID &id) override;
		virtual void LeaveLobby() override;

		virtual int GetLobbyPlayerCount(const ServiceID &lobbyId) override;
		virtual void SetLobbyMaxPlayers(const ServiceID &lobbyId, int amount) override;
		virtual int GetLobbyMaxPlayers(const ServiceID &lobbyId) override;

		virtual std::string GetLobbyData(const ServiceID &lobbyId, const char* name) override;
		virtual int GetLobbyDataCount(const ServiceID &lobbyId) override;
		virtual LobbyData GetLobbyData(const ServiceID &lobbyId, int index) override;

		virtual ServiceID GetLobbyHost(const ServiceID &lobbyId) override;

		virtual void SetLobbyData(const ServiceID &lobbyId, const char* name, const char* value) override;
		virtual void RemoveLobbyData(const ServiceID &lobbyId, const char* name) override;

		virtual size_t ReliablePacketLimit() override;

		virtual void SendPacket(const ServiceID &peerId, const void* data, size_t size, PacketType type, uint8_t channel) override;
		virtual size_t ReadPacket(void* data, size_t maxSize, ServiceID* peerId, uint8_t channel) override;
		virtual bool IsPacketAvailable(size_t* outPacketSize, uint8_t channel) override;

		static void SetLocalUsername(std::string name);
		static void SetLocalMacAddress(uint64_t address);

	private:
		ENetPeer* GetPeer(const ServiceID &id);
		void Clear(size_t numChannels);
	        void StartSearch();
	        void StopSearch();
	        void Search();
	};
}

inline std::string Unet::ServiceEnet::m_localUserName;
inline uint64_t Unet::ServiceEnet::m_macAddress;
inline uint64_t Unet::ServiceEnet::m_applicationName;

inline void Unet::ServiceEnet::SetLocalUsername(std::string name) {
	m_localUserName = name;
}

inline void Unet::ServiceEnet::SetLocalMacAddress(uint64_t address) {
	m_macAddress = address;
}
