#pragma once

class RubyCallbacks : public Unet::ICallbacks {
public:
    void OnLogError(const std::string& str) override {
        LOG_ERROR(str);
    }

    void OnLogWarn(const std::string& str) override {
        LOG_WARN(str);
    }

    void OnLogInfo(const std::string& str) override {
        LOG_INFO(str);
    }

    void OnLogDebug(const std::string& str) override {
        #if defined(DEBUG)
        LOG_DEBUG(str);
        #endif
    }

    void OnLobbyCreated(const Unet::CreateLobbyResult& result) override {
        if (result.Code != Unet::Result::OK) {
            push_error("Couldn't create lobby!", -1);
            return;
        }

        auto& lobbyInfo = result.CreatedLobby->GetInfo();
        mrb_value lobby_hash = mrb_hash_new(update_state);
        cext_hash_set_kstr(update_state, lobby_hash, "lobby_name",
                           mrb_str_new_cstr(update_state, lobbyInfo.Name.c_str()));
        cext_hash_set_kstr(update_state, lobby_hash, "lobby_max_players",
                           mrb_int_value(update_state, lobbyInfo.MaxPlayers));
        auto entry_array = mrb_ary_new_capa(update_state, static_cast<mrb_int>(lobbyInfo.EntryPoints.size()));
        for (int j = 0; j < lobbyInfo.EntryPoints.size(); j++) {
            auto entry = lobbyInfo.EntryPoints[j];
            auto entry_hash = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, entry_hash, "service_type", Unet::GetServiceNameByType(entry.Service));
            // FIXME: ID might cause overflow
            pext_hash_set(update_state, entry_hash, "id", mrb_int_value(update_state, entry.ID));
            mrb_ary_set(update_state, entry_array, j, entry_hash);
        }
        pext_hash_set(update_state, lobby_hash, "entries", entry_array);
        push_to_updates("on_lobby_created", lobby_hash);
    }

    void OnLobbyList(const Unet::LobbyListResult& result) override {
        if (result.Code != Unet::Result::OK) {
            push_error("Couldn't get lobby list!", -1);
            return;
        }

        g_lastLobbyList = result;

        mrb_value mrb_array = mrb_ary_new_capa(update_state, static_cast<mrb_int>(result.Lobbies.size()));
        for (mrb_int i = 0; i < static_cast<mrb_int>(result.Lobbies.size()); i++) {
            auto& lobbyInfo = result.Lobbies[i];
            mrb_value lobby_hash = mrb_hash_new(update_state);
            cext_hash_set_kstr(update_state, lobby_hash, "lobby_name",
                               mrb_str_new_cstr(update_state, lobbyInfo.Name.c_str()));
            cext_hash_set_kstr(update_state, lobby_hash, "lobby_max_players",
                               mrb_int_value(update_state, lobbyInfo.MaxPlayers));
            auto entry_array = mrb_ary_new_capa(update_state, static_cast<mrb_int>(lobbyInfo.EntryPoints.size()));
            for (int j = 0; j < lobbyInfo.EntryPoints.size(); j++) {
                auto entry = lobbyInfo.EntryPoints[j];

                auto entry_hash = mrb_hash_new_capa(update_state, 2);
                pext_hash_set(update_state, entry_hash, "service_type", Unet::GetServiceNameByType(entry.Service));
                // FIXME: ID might cause overflow
                pext_hash_set(update_state, entry_hash, "id", mrb_int_value(update_state, entry.ID));
                mrb_ary_set(update_state, entry_array, j, entry_hash);
            }
            pext_hash_set(update_state, lobby_hash, "entries", entry_array);
            mrb_ary_set(update_state, mrb_array, i, lobby_hash);
        }

        push_to_updates("on_lobby_list", mrb_array);
    }

    void OnLobbyInfoFetched(const Unet::LobbyInfoFetchResult& result) override {
        if (result.Code != Unet::Result::OK) {
            push_error("Couldn't fetch lobby info!", -1);
            return;
        }
        auto info = mrb_hash_new_capa(update_state, 6);
        std::string lobbyGuid = result.Info.UnetGuid.str();
        pext_hash_set(update_state, info, "is_hosting", result.Info.IsHosting);
        auto privacy_val = result.Info.Privacy == Unet::LobbyPrivacy::Public ? "public" : "private";
        pext_hash_set(update_state, info, "privacy", privacy_val);
        pext_hash_set(update_state, info, "current_players", result.Info.NumPlayers);
        pext_hash_set(update_state, info, "max_players", result.Info.MaxPlayers);
        pext_hash_set(update_state, info, "guid", lobbyGuid);
        pext_hash_set(update_state, info, "name", result.Info.Name);

        auto entry_array = mrb_ary_new_capa(update_state, static_cast<mrb_int>(result.Info.EntryPoints.size()));
        for (int i = 0; i < result.Info.EntryPoints.size(); i++) {
            auto entry = result.Info.EntryPoints[i];
            auto entry_hash = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, entry_hash, "service_type", Unet::GetServiceNameByType(entry.Service));
            // FIXME: ID might cause overflow
            pext_hash_set(update_state, entry_hash, "id", mrb_int_value(update_state, entry.ID));
            mrb_ary_set(update_state, entry_array, i, entry_hash);
        }
        pext_hash_set(update_state, info, "entries", entry_array);
        push_to_updates("on_lobby_info_fetched", info);
    }

    void OnLobbyJoined(const Unet::LobbyJoinResult& result) override {
        if (result.Code != Unet::Result::OK) {
            push_error("Couldn't join lobby!", -1);
            return;
        }

        auto& info = result.JoinedLobby->GetInfo();
        auto name = mrb_hash_new_capa(update_state, 1);
        pext_hash_set(update_state, name, "name", info.Name);
        push_to_updates("on_lobby_joined", name);
    }

    void OnLobbyLeft(const Unet::LobbyLeftResult& result) override {
        std::string reasonStr = "Undefined";
        switch (result.Reason) {
            case Unet::LeaveReason::UserLeave: reasonStr = "User leave";
                break;
            case Unet::LeaveReason::Disconnected: reasonStr = "Disconnected";
                break;
            case Unet::LeaveReason::Kicked: reasonStr = "Kicked";
                break;
        }
        LOG_FROM_CALLBACK("Left lobby: " + reasonStr);
    }

    void OnLobbyNameChanged(const std::string& oldname, const std::string& newname) override {
        mrb_value info = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, info, "old", oldname);
        pext_hash_set(update_state, info, "new", newname);
        push_to_updates("on_lobby_name_changed", info);
    }

    void OnLobbyMaxPlayersChanged(int oldamount, int newamount) override {
        mrb_value info = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, info, "old", oldamount);
        pext_hash_set(update_state, info, "new", newamount);
        push_to_updates("on_lobby_max_players_changed", info);
    }

    void OnLobbyPlayerJoined(Unet::LobbyMember* member) override {
        mrb_value info = mrb_hash_new_capa(update_state, 1);
        pext_hash_set(update_state, info, "name", member->Name);
        push_to_updates("on_lobby_player_joined", info);
    }

    void OnLobbyPlayerLeft(Unet::LobbyMember* member) override {
        mrb_value info = mrb_hash_new_capa(update_state, 1);
        pext_hash_set(update_state, info, "name", member->Name);
        push_to_updates("on_lobby_player_left", info);
    }

    void OnLobbyDataChanged(const std::string& name) override {
        if (g_ctx == nullptr) {
            return;
        }
        auto currentLobby = g_ctx->CurrentLobby();
        auto value = currentLobby->GetData(name);
        auto content = mrb_hash_new_capa(update_state, 1);
        if (value.empty()) {
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

    void OnLobbyMemberDataChanged(Unet::LobbyMember* member, const std::string& name) override {
        auto value = member->GetData(name);
        auto content = mrb_hash_new_capa(update_state, 1);
        if (value.empty()) {
            auto changed_data = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, changed_data, "key", name.c_str());
            pext_hash_set(update_state, changed_data, "value", mrb_nil_value());
            push_to_updates("on_lobby_member_data_changed", content);
        } else {
            auto changed_data = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, changed_data, "key", name.c_str());
            pext_hash_set(update_state, changed_data, "value", value.c_str());
            push_to_updates("on_lobby_member_data_changed", content);
        }
    }

    void OnLobbyMemberNameChanged(Unet::LobbyMember* member, const std::string& oldname) override {
        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "old", oldname);
        pext_hash_set(update_state, changed_data, "new", member->Name.c_str());
        push_to_updates("on_lobby_member_name_changed", changed_data);
    }

    void OnLobbyFileAdded(Unet::LobbyMember* member, const Unet::LobbyFile* file) override {
        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", member->Name);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        push_to_updates("on_lobby_file_added", changed_data);
    }

    void OnLobbyFileRemoved(Unet::LobbyMember* member, const std::string& filename) override {
        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", member->Name);
        pext_hash_set(update_state, changed_data, "file", filename);
        push_to_updates("on_lobby_file_removed", changed_data);
    }

    void OnLobbyFileRequested(Unet::LobbyMember* receiver, const Unet::LobbyFile* file) override {
        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", receiver);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        push_to_updates("on_lobby_file_requested", changed_data);
    }

    void OnLobbyFileDataSendProgress(const Unet::OutgoingFileTransfer& transfer) override {
        if (g_ctx == nullptr) {
            return;
        }
        auto currentLobby = g_ctx->CurrentLobby();
        auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

        auto file = localMember->GetFile(transfer.FileHash);
        auto receiver = currentLobby->GetMember(transfer.MemberPeer);

        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", receiver->Name);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        pext_hash_set(update_state, changed_data, "percentage", file->GetPercentage(transfer) * 100.0);
        push_to_updates("on_lobby_file_data_send_progress", changed_data);
    }

    void OnLobbyFileDataSendFinished(const Unet::OutgoingFileTransfer& transfer) override {
        if (g_ctx == nullptr) {
            return;
        }
        auto currentLobby = g_ctx->CurrentLobby();
        auto localMember = currentLobby->GetMember(g_ctx->GetLocalPeer());

        auto file = localMember->GetFile(transfer.FileHash);
        auto receiver = currentLobby->GetMember(transfer.MemberPeer);

        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", receiver->Name);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        push_to_updates("on_lobby_file_data_send_finished", changed_data);
    }

    void OnLobbyFileDataReceiveProgress(Unet::LobbyMember* sender, const Unet::LobbyFile* file) override {

        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", sender->Name);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        pext_hash_set(update_state, changed_data, "percentage", file->GetPercentage() * 100.0);
        push_to_updates("on_lobby_file_data_receive_progress", changed_data);
    }

    void OnLobbyFileDataReceiveFinished(Unet::LobbyMember* sender, const Unet::LobbyFile* file, bool isValid) override {
        auto changed_data = mrb_hash_new_capa(update_state, 2);
        pext_hash_set(update_state, changed_data, "member", sender->Name);
        pext_hash_set(update_state, changed_data, "file", file->m_filename);
        pext_hash_set(update_state, changed_data, "valid", isValid);
        push_to_updates("on_lobby_file_data_receive_finished", changed_data);
    }

    void OnLobbyChat(Unet::LobbyMember* sender, const char* text) override {
        if (sender == nullptr) {
            auto changed_data = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, changed_data, "member", mrb_nil_value());
            pext_hash_set(update_state, changed_data, "text", text);
            push_to_updates("on_lobby_chat", changed_data);
        } else {
            auto changed_data = mrb_hash_new_capa(update_state, 2);
            pext_hash_set(update_state, changed_data, "member", sender->Name);
            pext_hash_set(update_state, changed_data, "text", text);
            push_to_updates("on_lobby_chat", changed_data);
        }
    }
};