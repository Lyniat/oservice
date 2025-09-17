class TestCallbacks : public Unet::ICallbacks
{
public:
	 void OnLogError(const std::string &str) override
	{
		LOG_ERROR("%s", str.c_str());
	}

	 void OnLogWarn(const std::string &str) override
	{
		LOG_WARN("%s", str.c_str());
	}

	 void OnLogInfo(const std::string &str) override
	{
		LOG_INFO("%s", str.c_str());
	}

	 void OnLogDebug(const std::string &str) override
	{
#if defined(DEBUG)
		LOG_DEBUG("%s", str.c_str());
#endif
	}

	 void OnLobbyCreated(const Unet::CreateLobbyResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			push_error("Couldn't create lobby!", -1);
			return;
		}

		auto &info = result.CreatedLobby->GetInfo();
		LOG_FROM_CALLBACK("Lobby created: \"%s\"", info.Name.c_str());
	}

	 void OnLobbyList(const Unet::LobbyListResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			push_error("Couldn't get lobby list!", -1);
			return;
		}

		g_lastLobbyList = result;

	 	mrb_value mrb_array = mrb_ary_new_capa(update_state, result.Lobbies.size());
		LOG_FROM_CALLBACK("%d lobbies: (%d filtered)", (int)result.Lobbies.size(), result.NumFiltered);
		for (size_t i = 0; i < result.Lobbies.size(); i++) {
			auto &lobbyInfo = result.Lobbies[i];
			mrb_value lobby_hash = mrb_hash_new(update_state);
			cext_hash_set_kstr(update_state, lobby_hash, "lobby_name",
				mrb_str_new_cstr(update_state, lobbyInfo.Name.c_str()));
			cext_hash_set_kstr(update_state, lobby_hash, "lobby_max_players",
					mrb_int_value(update_state, lobbyInfo.MaxPlayers));
			//LOG_FROM_CALLBACK("  [%d] \"%s\" (max %d)", (int)i, lobbyInfo.Name.c_str(), lobbyInfo.MaxPlayers);
			auto entry_array = mrb_ary_new_capa(update_state, lobbyInfo.EntryPoints.size());
			for (int j = 0; j < lobbyInfo.EntryPoints.size(); j++) {
				auto entry = lobbyInfo.EntryPoints[j];
				//LOG_FROM_CALLBACK("    %s (0x%016" PRIx64 ")", Unet::GetServiceNameByType(entry.Service), entry.ID);
				auto entry_hash = mrb_hash_new_capa(update_state, 2);
				pext_hash_set(update_state, entry_hash, "service_type", Unet::GetServiceNameByType(entry.Service));
				pext_hash_set(update_state, entry_hash, "id", mrb_int_value(update_state, entry.ID));
				mrb_ary_set(update_state, entry_array, j, entry_hash);
			}
			pext_hash_set(update_state, lobby_hash, "entries", entry_array);
			mrb_ary_set(update_state, mrb_array, i, lobby_hash);
		}

	 	push_to_updates("on_lobby_list", mrb_array);
	}

	 void OnLobbyInfoFetched(const Unet::LobbyInfoFetchResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			push_error("Couldn't fetch lobby info!", -1);
			return;
		}
		auto info = mrb_hash_new_capa(update_state, 6);
		//LOG_FROM_CALLBACK("Fetched lobby info:");
		//LOG_FROM_CALLBACK("    Is hosting: %s", result.Info.IsHosting ? "yes" : "no");
		//LOG_FROM_CALLBACK("       Privacy: %s", result.Info.Privacy == Unet::LobbyPrivacy::Public ? "public" : "private");
		//LOG_FROM_CALLBACK("       Players: %d / %d", result.Info.NumPlayers, result.Info.MaxPlayers);
		std::string lobbyGuid = result.Info.UnetGuid.str();
		//LOG_FROM_CALLBACK("          Guid: %s", lobbyGuid.c_str());
		//LOG_FROM_CALLBACK("          Name: \"%s\"", result.Info.Name.c_str());
		//LOG_FROM_CALLBACK("  Entry points: %d", (int)result.Info.EntryPoints.size());
	 	pext_hash_set(update_state, info, "is_hosting", result.Info.IsHosting);
	 	auto privacy_val = result.Info.Privacy == Unet::LobbyPrivacy::Public ? "public" : "private";
	 	pext_hash_set(update_state, info, "privacy", privacy_val);
	 	pext_hash_set(update_state, info, "current_players", result.Info.NumPlayers);
	 	pext_hash_set(update_state, info, "max_players", result.Info.MaxPlayers);
	 	pext_hash_set(update_state, info, "guid", lobbyGuid);
	 	pext_hash_set(update_state, info, "name", result.Info.Name);

	 	auto entry_array = mrb_ary_new_capa(update_state, result.Info.EntryPoints.size());
			for (int i = 0; i < result.Info.EntryPoints.size(); i++) {
				auto entry = result.Info.EntryPoints[i];
				//LOG_FROM_CALLBACK("    %s (0x%016" PRIx64 ")", Unet::GetServiceNameByType(entry.Service), entry.ID);
				auto entry_hash = mrb_hash_new_capa(update_state, 2);
				pext_hash_set(update_state, entry_hash, "service_type", Unet::GetServiceNameByType(entry.Service));
				pext_hash_set(update_state, entry_hash, "id", mrb_int_value(update_state, entry.ID));
				mrb_ary_set(update_state, entry_array, i, entry_hash);
			}
	 	pext_hash_set(update_state, info, "entries", entry_array);
	 	push_to_updates("on_lobby_info_etched", info);
	}

	 void OnLobbyJoined(const Unet::LobbyJoinResult &result) override
	{
		if (result.Code != Unet::Result::OK) {
			push_error("Couldn't join lobby!", -1);
			return;
		}

		auto &info = result.JoinedLobby->GetInfo();
		//LOG_FROM_CALLBACK("Joined lobby: \"%s\"", info.Name.c_str());
	 	auto name = mrb_hash_new_capa(update_state, 1);
	 	pext_hash_set(update_state, name, "name", info.Name);
	 	push_to_updates("on_lobby_joined", name);
	}

	 void OnLobbyLeft(const Unet::LobbyLeftResult &result) override
	{
		const char* reasonStr = "Undefined";
		switch (result.Reason) {
		case Unet::LeaveReason::UserLeave: reasonStr = "User leave"; break;
		case Unet::LeaveReason::Disconnected: reasonStr = "Disconnected"; break;
		case Unet::LeaveReason::Kicked: reasonStr = "Kicked"; break;
		}
		LOG_FROM_CALLBACK("Left lobby: %s", reasonStr);
	}

	 void OnLobbyNameChanged(const std::string &oldname, const std::string &newname) override
	{
		//LOG_FROM_CALLBACK("Lobby name changed: \"%s\" => \"%s\"", oldname.c_str(), newname.c_str());
	 	mrb_value info = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, info, "old", oldname);
	 	pext_hash_set(update_state, info, "new", newname);
	 	push_to_updates("on_lobby_name_changed", info);
	}

	 void OnLobbyMaxPlayersChanged(int oldamount, int newamount) override
	{
		//LOG_FROM_CALLBACK("Max players changed: %d => %d", oldamount, newamount);
	 	mrb_value info = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, info, "old", oldamount);
	 	pext_hash_set(update_state, info, "new", newamount);
	 	push_to_updates("on_lobby_max_players_changed", info);
	}

	 void OnLobbyPlayerJoined(Unet::LobbyMember* member) override
	{
		//LOG_FROM_CALLBACK("Player joined: %s", member->Name.c_str());
	 	mrb_value info = mrb_hash_new_capa(update_state, 1);
	 	pext_hash_set(update_state, info, "name", member->Name);
	 	push_to_updates("on_lobby_player_joined", info);
	}

	 void OnLobbyPlayerLeft(Unet::LobbyMember* member) override
	{
		//LOG_FROM_CALLBACK("Player left: %s", member->Name.c_str());
	 	mrb_value info = mrb_hash_new_capa(update_state, 1);
	 	pext_hash_set(update_state, info, "name", member->Name);
	 	push_to_updates("on_lobby_player_left", info);
	}

	 void OnLobbyDataChanged(const std::string &name) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto value = currentLobby->GetData(name);
	 	auto content = mrb_hash_new_capa(update_state, 1);
		if (value == "") {
			//LOG_FROM_CALLBACK("Lobby data removed: \"%s\"", name.c_str());
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "key", name.c_str());
			pext_hash_set(update_state, changed_data, "value", mrb_nil_value());
			push_to_updates("on_lobby_data_changed", content);
		} else {
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "key", name.c_str());
			pext_hash_set(update_state, changed_data, "value", value.c_str());
			push_to_updates("on_lobby_data_changed", content);
		}
	}

	 void OnLobbyMemberDataChanged(Unet::LobbyMember* member, const std::string &name) override
	{
		auto value = member->GetData(name);
	 	auto content = mrb_hash_new_capa(update_state, 1);
		if (value == "") {
			//LOG_FROM_CALLBACK("Lobby member data removed: \"%s\"", name.c_str());
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "key", name.c_str());
			pext_hash_set(update_state, changed_data, "value", mrb_nil_value());
			push_to_updates("on_lobby_member_data_changed", content);
		} else {
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "key", name.c_str());
			pext_hash_set(update_state, changed_data, "value", value.c_str());
			push_to_updates("on_lobby_member_data_changed", content);
			//LOG_FROM_CALLBACK("Lobby member data changed: \"%s\" => \"%s\"", name.c_str(), value.c_str());
		}
	}

	 void OnLobbyMemberNameChanged(Unet::LobbyMember* member, const std::string &oldname) override
	{
		//LOG_FROM_CALLBACK("Lobby member changed their name: \"%s\" => \"%s\"", oldname.c_str(), member->Name.c_str());
	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "old", oldname);
	 	pext_hash_set(update_state, changed_data, "new", member->Name.c_str());
	 	push_to_updates("on_lobby_member_name_changed", changed_data);
	}

	 void OnLobbyFileAdded(Unet::LobbyMember* member, const Unet::LobbyFile* file) override
	{
		//LOG_FROM_CALLBACK("%s added file \"%s\"", member->Name.c_str(), file->m_filename.c_str());
	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", member->Name);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	push_to_updates("on_lobby_file_added", changed_data);
	}

	 void OnLobbyFileRemoved(Unet::LobbyMember* member, const std::string &filename) override
	{
		//LOG_FROM_CALLBACK("%s removed file \"%s\"", member->Name.c_str(), filename.c_str());
	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", member->Name);
	 	pext_hash_set(update_state, changed_data, "file", filename);
	 	push_to_updates("on_lobby_file_removed", changed_data);
	}

	 void OnLobbyFileRequested(Unet::LobbyMember* receiver, const Unet::LobbyFile* file) override
	{
		//LOG_FROM_CALLBACK("%s requested our file \"%s\" for download", receiver->Name.c_str(), file->m_filename.c_str());
	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", receiver);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	push_to_updates("on_lobby_file_requested", changed_data);
	}

	 void OnLobbyFileDataSendProgress(const Unet::OutgoingFileTransfer& transfer) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

		auto file = localMember->GetFile(transfer.FileHash);
		auto receiver = currentLobby->GetMember(transfer.MemberPeer);

		//LOG_FROM_CALLBACK("Sending file \"%s\" to %s: %.1f%%", file->m_filename.c_str(), receiver->Name.c_str(), file->GetPercentage(transfer) * 100.0);

	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", receiver->Name);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	pext_hash_set(update_state, changed_data, "percentage", file->GetPercentage(transfer) * 100.0);
	 	push_to_updates("on_lobby_file_data_send_progress", changed_data);
	}

	 void OnLobbyFileDataSendFinished(const Unet::OutgoingFileTransfer& transfer) override
	{
		auto currentLobby = g_ctx->CurrentLobby();
		auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

		auto file = localMember->GetFile(transfer.FileHash);
		auto receiver = currentLobby->GetMember(transfer.MemberPeer);

		//LOG_FROM_CALLBACK("Completed sending file \"%s\" to %s!", file->m_filename.c_str(), receiver->Name.c_str());

	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", receiver->Name);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	push_to_updates("on_lobby_file_data_send_finished", changed_data);
	}

	 void OnLobbyFileDataReceiveProgress(Unet::LobbyMember* sender, const Unet::LobbyFile* file) override
	{
		//LOG_FROM_CALLBACK("Receiving file \"%s\" from %s: %.1f%%", file->m_filename.c_str(), sender->Name.c_str(), file->GetPercentage() * 100.0);

	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", sender->Name);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	pext_hash_set(update_state, changed_data, "percentage", file->GetPercentage() * 100.0);
	 	push_to_updates("on_lobby_file_data_receive_progress", changed_data);
	}

	 void OnLobbyFileDataReceiveFinished(Unet::LobbyMember* sender, const Unet::LobbyFile* file, bool isValid) override
	{
		//LOG_FROM_CALLBACK("Completed receiving file \"%s\" from %s! Valid: %s", file->m_filename.c_str(), sender->Name.c_str(), isValid ? "yes" : "no");
	 	auto changed_data = mrb_hash_new_capa(update_state, 2);
	 	pext_hash_set(update_state, changed_data, "member", sender->Name);
	 	pext_hash_set(update_state, changed_data, "file", file->m_filename);
	 	pext_hash_set(update_state, changed_data, "valid", isValid);
	 	push_to_updates("on_lobby_file_data_receive_finished", changed_data);
	}

	 void OnLobbyChat(Unet::LobbyMember* sender, const char* text) override
	{
		if (sender == nullptr) {
			//LOG_FROM_CALLBACK("> %s", text);
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "member", mrb_nil_value());
			pext_hash_set(update_state, changed_data, "text", text);
			push_to_updates("on_lobby_chat", changed_data);
		} else {
			//LOG_FROM_CALLBACK("> %s: %s", sender->Name.c_str(), text);
			auto changed_data = mrb_hash_new_capa(update_state, 2);
			pext_hash_set(update_state, changed_data, "member", sender->Name);
			pext_hash_set(update_state, changed_data, "text", text);
			push_to_updates("on_lobby_chat", changed_data);
		}
	}
};