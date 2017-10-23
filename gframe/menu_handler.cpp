#include "config.h"
#include "menu_handler.h"
#include "netserver.h"
#include "duelclient.h"
#include "deck_manager.h"
#include "replay_mode.h"
#include "single_mode.h"
#include "image_manager.h"
#include "game.h"
#include "utils.h"

namespace ygo {

void UpdateDeck() {
	BufferIO::CopyWStr(mainGame->cbDeckSelect->getItem(mainGame->cbDeckSelect->getSelected()),
		mainGame->gameConf.lastdeck, 64);
	char deckbuf[1024];
	char* pdeck = deckbuf;
	BufferIO::WriteInt32(pdeck, deckManager.current_deck.main.size() + deckManager.current_deck.extra.size());
	BufferIO::WriteInt32(pdeck, deckManager.current_deck.side.size());
	for(size_t i = 0; i < deckManager.current_deck.main.size(); ++i)
		BufferIO::WriteInt32(pdeck, deckManager.current_deck.main[i]->first);
	for(size_t i = 0; i < deckManager.current_deck.extra.size(); ++i)
		BufferIO::WriteInt32(pdeck, deckManager.current_deck.extra[i]->first);
	for(size_t i = 0; i < deckManager.current_deck.side.size(); ++i)
		BufferIO::WriteInt32(pdeck, deckManager.current_deck.side[i]->first);
	DuelClient::SendBufferToServer(CTOS_UPDATE_DECK, deckbuf, pdeck - deckbuf);
}
bool MenuHandler::OnEvent(const irr::SEvent& event) {
	switch(event.EventType) {
	case irr::EET_GUI_EVENT: {
		irr::gui::IGUIElement* caller = event.GUIEvent.Caller;
		s32 id = caller->getID();
		if(mainGame->wRules->isVisible() && (id != BUTTON_RULE_OK && (id <CHECK_SEALED_DUEL || id>CHECK_DECK_MASTER_DUEL)))
			break;
		if(mainGame->wCustomRules->isVisible() && id != BUTTON_CUSTOM_RULE_OK)
			break;
		switch(event.GUIEvent.EventType) {
		case irr::gui::EGET_ELEMENT_HOVERED: {
			// Set cursor to an I-Beam if hovering over an edit box
			if (event.GUIEvent.Caller->getType() == EGUIET_EDIT_BOX)
			{
				utils.changeCursor(ECI_IBEAM);
			}
			break;
		}
		case irr::gui::EGET_ELEMENT_LEFT: {
			// Set cursor to normal if left an edit box
			if (event.GUIEvent.Caller->getType() == EGUIET_EDIT_BOX)
			{
				utils.changeCursor(ECI_NORMAL);
			}
			break;
		}
		case irr::gui::EGET_BUTTON_CLICKED: {
			switch(id) {
			case BUTTON_MODE_EXIT: {
				mainGame->device->closeDevice();
				break;
			}
			case BUTTON_LAN_MODE: {
				mainGame->btnCreateHost->setEnabled(true);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->HideElement(mainGame->wMainMenu);
				mainGame->ShowElement(mainGame->wLanWindow);
				break;
			}
			case BUTTON_JOIN_HOST: {
				char ip[20];
				const wchar_t* pstr = mainGame->ebJoinHost->getText();
				BufferIO::CopyWStr(pstr, ip, 16);
				unsigned int remote_addr = htonl(inet_addr(ip));
				if(remote_addr == -1) {
					char hostname[100];
					char port[6];
					BufferIO::CopyWStr(pstr, hostname, 100);
					BufferIO::CopyWStr(mainGame->ebJoinPort->getText(), port, 6);
					struct evutil_addrinfo hints;
					struct evutil_addrinfo *answer = NULL;
					memset(&hints, 0, sizeof(hints));
					hints.ai_family = AF_INET;
					hints.ai_socktype = SOCK_STREAM;
					hints.ai_protocol = IPPROTO_TCP;
					hints.ai_flags = EVUTIL_AI_ADDRCONFIG;
					int status=evutil_getaddrinfo(hostname, port, &hints, &answer);
					if(status != 0) {
						mainGame->gMutex.Lock();
						mainGame->env->addMessageBox(L"", dataManager.GetSysString(1412));
						mainGame->gMutex.Unlock();
						break;
					} else {
						sockaddr_in * sin = ((struct sockaddr_in *)answer->ai_addr);
						evutil_inet_ntop(AF_INET, &(sin->sin_addr), ip, 20);
						remote_addr = htonl(inet_addr(ip));
					}
				}
				unsigned int remote_port = _wtoi(mainGame->ebJoinPort->getText());
				BufferIO::CopyWStr(pstr, mainGame->gameConf.lasthost, 100);
				BufferIO::CopyWStr(mainGame->ebJoinPort->getText(), mainGame->gameConf.lastport, 20);
				if(DuelClient::StartClient(remote_addr, remote_port, false)) {
					mainGame->btnCreateHost->setEnabled(false);
					mainGame->btnJoinHost->setEnabled(false);
					mainGame->btnJoinCancel->setEnabled(false);
				}
				break;
			}
			case BUTTON_JOIN_CANCEL: {
				mainGame->HideElement(mainGame->wLanWindow);
				mainGame->ShowElement(mainGame->wMainMenu);
				if(exit_on_return)
					mainGame->device->closeDevice();
				break;
			}
			case BUTTON_LAN_REFRESH: {
				DuelClient::BeginRefreshHost();
				break;
			}
			case BUTTON_CREATE_HOST: {
				mainGame->btnHostConfirm->setEnabled(true);
				mainGame->btnHostCancel->setEnabled(true);
				mainGame->HideElement(mainGame->wLanWindow);
				mainGame->ShowElement(mainGame->wCreateHost);
				break;
			}
			case BUTTON_RULE_CARDS: {
				mainGame->PopupElement(mainGame->wRules);
				break;
			}
			case BUTTON_RULE_OK: {
				mainGame->HideElement(mainGame->wRules);
				break;
			}
			case BUTTON_CUSTOM_RULE: {
				mainGame->PopupElement(mainGame->wCustomRules);
				break;
			}
			case BUTTON_CUSTOM_RULE_OK: {
				uint32 flag = 0, filter = 0x100;
				for(int i = 0; i < 5; ++i, filter <<= 1)
					if(mainGame->chkCustomRules[i]->isChecked()) {
						flag |= filter;
					}
				mainGame->cbDuelRule->clear();
				mainGame->cbDuelRule->addItem(dataManager.GetSysString(1260));
				mainGame->cbDuelRule->addItem(dataManager.GetSysString(1261));
				mainGame->cbDuelRule->addItem(dataManager.GetSysString(1262));
				mainGame->cbDuelRule->addItem(dataManager.GetSysString(1263));
				switch (flag) {
				case 0x1F00: {
					mainGame->cbDuelRule->setSelected(0);
					break;
				}
				case 0x1E00: {
					mainGame->cbDuelRule->setSelected(1);
					break;
				}
				case 0x1000: {
					mainGame->cbDuelRule->setSelected(2);
					break;
				}
				case 0: {
					mainGame->cbDuelRule->setSelected(3);
					break;
				}
				default: {
					mainGame->cbDuelRule->addItem(dataManager.GetSysString(1264));
					mainGame->cbDuelRule->setSelected(4);
					break;
				}
				}
				mainGame->duel_param = flag;
				mainGame->HideElement(mainGame->wCustomRules);
				break;
			}
			case BUTTON_HOST_CONFIRM: {
				BufferIO::CopyWStr(mainGame->ebServerName->getText(), mainGame->gameConf.gamename, 20);
				if(!NetServer::StartServer(mainGame->gameConf.serverport))
					break;
				if(!DuelClient::StartClient(0x7f000001, mainGame->gameConf.serverport)) {
					NetServer::StopServer();
					break;
				}
				mainGame->btnHostConfirm->setEnabled(false);
				mainGame->btnHostCancel->setEnabled(false);
				break;
			}
			case BUTTON_HOST_CANCEL: {
				mainGame->btnCreateHost->setEnabled(true);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->HideElement(mainGame->wRules);
				mainGame->HideElement(mainGame->wCreateHost);
				mainGame->ShowElement(mainGame->wLanWindow);
				break;
			}
			case BUTTON_HP_DUELIST: {
				mainGame->cbDeckSelect->setEnabled(true);
				DuelClient::SendPacketToServer(CTOS_HS_TODUELIST);
				break;
			}
			case BUTTON_HP_OBSERVER: {
				DuelClient::SendPacketToServer(CTOS_HS_TOOBSERVER);
				break;
			}
			case BUTTON_HP_KICK: {
				int id = 0;
				while(id < 4) {
					if(mainGame->btnHostPrepKick[id] == caller)
						break;
					id++;
				}
				CTOS_Kick csk;
				csk.pos = id;
				DuelClient::SendPacketToServer(CTOS_HS_KICK, csk);
				break;
			}
			case BUTTON_HP_READY: {
				bool check = false;
				if(!mainGame->cbDeckSelect2->isVisible())
					check = (mainGame->cbDeckSelect->getSelected() == -1 || !deckManager.LoadDeck(mainGame->cbDeckSelect->getItem(mainGame->cbDeckSelect->getSelected())));
				else
					check = (mainGame->cbDeckSelect->getSelected() == -1 || mainGame->cbDeckSelect2->getSelected() == -1 || !deckManager.LoadDeckDouble(mainGame->cbDeckSelect->getItem(mainGame->cbDeckSelect->getSelected()), mainGame->cbDeckSelect2->getItem(mainGame->cbDeckSelect2->getSelected())));
				if(check)
					break;
				UpdateDeck();
				DuelClient::SendPacketToServer(CTOS_HS_READY);
				mainGame->cbDeckSelect->setEnabled(false);
				break;
			}
			case BUTTON_HP_NOTREADY: {
				DuelClient::SendPacketToServer(CTOS_HS_NOTREADY);
				mainGame->cbDeckSelect->setEnabled(true);
				break;
			}
			case BUTTON_HP_START: {
				DuelClient::SendPacketToServer(CTOS_HS_START);
				break;
			}
			case BUTTON_HP_CANCEL: {
				DuelClient::StopClient();
				mainGame->btnCreateHost->setEnabled(true);
				mainGame->btnJoinHost->setEnabled(true);
				mainGame->btnJoinCancel->setEnabled(true);
				mainGame->HideElement(mainGame->wHostPrepare);
				if(mainGame->wHostPrepare2->isVisible())
					mainGame->HideElement(mainGame->wHostPrepare2);
				mainGame->ShowElement(mainGame->wLanWindow);
				mainGame->wChat->setVisible(false);
				if(exit_on_return)
					mainGame->device->closeDevice();
				break;
			}
			case BUTTON_REPLAY_MODE: {
				mainGame->HideElement(mainGame->wMainMenu);
				mainGame->ShowElement(mainGame->wReplay);
				mainGame->ebRepStartTurn->setText(L"1");
				mainGame->RefreshReplay();
				break;
			}
			case BUTTON_SINGLE_MODE: {
				mainGame->HideElement(mainGame->wMainMenu);
				mainGame->ShowElement(mainGame->wSinglePlay);
				mainGame->RefreshSingleplay();
				break;
			}
			case BUTTON_LOAD_REPLAY: {
				if(open_file) {
					ReplayMode::cur_replay.OpenReplay(open_file_name);
					open_file = false;
				} else {
					if(mainGame->lstReplayList->getSelected() == -1)
						break;
					if(!ReplayMode::cur_replay.OpenReplay(mainGame->lstReplayList->getListItem(mainGame->lstReplayList->getSelected())))
						break;
				}
				mainGame->imgCard->setImage(imageManager.tCover[0]);
				mainGame->wCardImg->setVisible(true);
				mainGame->wInfos->setVisible(true);
				mainGame->wReplay->setVisible(true);
				mainGame->stName->setText(L"");
				mainGame->stInfo->setText(L"");
				mainGame->stDataInfo->setText(L"");
				mainGame->stSetName->setText(L"");
				mainGame->stText->setText(L"");
				mainGame->scrCardText->setVisible(false);
				mainGame->wReplayControl->setVisible(true);
				mainGame->btnReplayStart->setVisible(false);
				mainGame->btnReplayPause->setVisible(true);
				mainGame->btnReplayStep->setVisible(false);
				mainGame->btnReplayUndo->setVisible(false);
				mainGame->wPhase->setVisible(true);
				mainGame->dField.Clear();
				mainGame->HideElement(mainGame->wReplay);
				mainGame->device->setEventReceiver(&mainGame->dField);
				unsigned int start_turn = _wtoi(mainGame->ebRepStartTurn->getText());
				if(start_turn == 1)
					start_turn = 0;
				ReplayMode::StartReplay(start_turn);
				break;
			}
			case BUTTON_CANCEL_REPLAY: {
				mainGame->HideElement(mainGame->wReplay);
				mainGame->ShowElement(mainGame->wMainMenu);
				break;
			}
			case BUTTON_LOAD_SINGLEPLAY: {
				if(!open_file && mainGame->lstSinglePlayList->getSelected() == -1)
					break;
				mainGame->singleSignal.SetNoWait(false);
				SingleMode::StartPlay();
				break;
			}
			case BUTTON_CANCEL_SINGLEPLAY: {
				mainGame->HideElement(mainGame->wSinglePlay);
				mainGame->ShowElement(mainGame->wMainMenu);
				break;
			}
			case BUTTON_DECK_EDIT: {
				mainGame->RefreshDeck(mainGame->cbDBDecks);
				if(open_file && deckManager.LoadDeck(open_file_name)) {
#ifdef WIN32
					wchar_t *dash = wcsrchr(open_file_name, L'\\');
#else
					wchar_t *dash = wcsrchr(open_file_name, L'/');
#endif
					wchar_t *dot = wcsrchr(open_file_name, L'.');
					if(dash && dot) {
						wchar_t deck_name[256];
						wcsncpy(deck_name, dash + 1, dot - dash - 1);
						deck_name[dot - dash - 1] = L'\0';
						mainGame->ebDeckname->setText(deck_name);
						mainGame->cbDBDecks->setSelected(-1);
					} else {
						for(size_t i = 0; i < mainGame->cbDBDecks->getItemCount(); ++i) {
							if(!wcscmp(mainGame->cbDBDecks->getItem(i), open_file_name)) {
								mainGame->cbDBDecks->setSelected(i);
								break;
							}
						}
					}
					open_file = false;
				} else if(mainGame->cbDBDecks->getSelected() != -1) {
					deckManager.LoadDeck(mainGame->cbDBDecks->getItem(mainGame->cbDBDecks->getSelected()));
					mainGame->ebDeckname->setText(L"");
				}
				mainGame->HideElement(mainGame->wMainMenu);
				mainGame->deckBuilder.Initialize();
				break;
			}
			}
			break;
		}
		case irr::gui::EGET_LISTBOX_CHANGED: {
			switch(id) {
			case LISTBOX_LAN_HOST: {
				int sel = mainGame->lstHostList->getSelected();
				if(sel == -1)
					break;
				int addr = DuelClient::hosts[sel].ipaddr;
				int port = DuelClient::hosts[sel].port;
				wchar_t buf[20];
				myswprintf(buf, L"%d.%d.%d.%d", addr & 0xff, (addr >> 8) & 0xff, (addr >> 16) & 0xff, (addr >> 24) & 0xff);
				mainGame->ebJoinHost->setText(buf);
				myswprintf(buf, L"%d", port);
				mainGame->ebJoinPort->setText(buf);
				break;
			}
			case LISTBOX_REPLAY_LIST: {
				int sel = mainGame->lstReplayList->getSelected();
				if(sel == -1)
					break;
				if(!ReplayMode::cur_replay.OpenReplay(mainGame->lstReplayList->getListItem(sel)))
					break;
				wchar_t infobuf[256];
				std::wstring repinfo;
				time_t curtime = ReplayMode::cur_replay.pheader.seed;
				tm* st = localtime(&curtime);
				myswprintf(infobuf, L"%d/%d/%d %02d:%02d:%02d\n", st->tm_year + 1900, st->tm_mon + 1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec);
				repinfo.append(infobuf);
				wchar_t namebuf[4][20];
				ReplayMode::cur_replay.ReadName(namebuf[0]);
				ReplayMode::cur_replay.ReadName(namebuf[1]);
				if(ReplayMode::cur_replay.pheader.flag & REPLAY_TAG) {
					ReplayMode::cur_replay.ReadName(namebuf[2]);
					ReplayMode::cur_replay.ReadName(namebuf[3]);
				}
				if(ReplayMode::cur_replay.pheader.flag & REPLAY_TAG)
					myswprintf(infobuf, L"%ls\n%ls\n===VS===\n%ls\n%ls\n", namebuf[0], namebuf[1], namebuf[2], namebuf[3]);
				else
					myswprintf(infobuf, L"%ls\n===VS===\n%ls\n", namebuf[0], namebuf[1]);
				repinfo.append(infobuf);
				mainGame->ebRepStartTurn->setText(L"1");
				mainGame->SetStaticText(mainGame->stReplayInfo, 180, mainGame->guiFont, (wchar_t*)repinfo.c_str());
				break;
			}
			}
			break;
		}
		case irr::gui::EGET_CHECKBOX_CHANGED: {
			switch(id) {
			case CHECKBOX_HP_READY: {
				if(!caller->isEnabled())
					break;
				mainGame->env->setFocus(mainGame->wHostPrepare);
				if(static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					bool check = false;
					if (!mainGame->cbDeckSelect2->isVisible())
						check = (mainGame->cbDeckSelect->getSelected() == -1 || !deckManager.LoadDeck(mainGame->cbDeckSelect->getItem(mainGame->cbDeckSelect->getSelected())));
					else
						check = (mainGame->cbDeckSelect->getSelected() == -1 || mainGame->cbDeckSelect2->getSelected() == -1 || !deckManager.LoadDeckDouble(mainGame->cbDeckSelect->getItem(mainGame->cbDeckSelect->getSelected()), mainGame->cbDeckSelect2->getItem(mainGame->cbDeckSelect2->getSelected())));
					if(check) {
						static_cast<irr::gui::IGUICheckBox*>(caller)->setChecked(false);
						break;
					}
					UpdateDeck();
					DuelClient::SendPacketToServer(CTOS_HS_READY);
					mainGame->cbDeckSelect->setEnabled(false);
					mainGame->cbDeckSelect2->setEnabled(false);
				} else {
					DuelClient::SendPacketToServer(CTOS_HS_NOTREADY);
					mainGame->cbDeckSelect->setEnabled(true);
					mainGame->cbDeckSelect2->setEnabled(true);
				}
				break;
			}
			case CHECK_SEALED_DUEL: {
				if (static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					for (int i = 1; i < 14; ++i)
						if (i != 2 && i != 3 && i != 9)
						mainGame->chkRules[i]->setEnabled(false);
				}
				else {
					for (int i = 1; i < 14; ++i)
						if (i != 2 && i != 3 && i != 9)
						mainGame->chkRules[i]->setEnabled(true);
				}
				break;
			}
			case CHECK_BOOSTER_DUEL: {
				if (static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					for (int i = 0; i < 14; ++i)
						if(i != 1 && i != 2 && i != 3 && i != 9)
						mainGame->chkRules[i]->setEnabled(false);
				}
				else {
					for (int i = 0; i < 14; ++i)
						if (i != 1 && i != 2 && i != 3 && i != 9)
						mainGame->chkRules[i]->setEnabled(true);
				}
				break;
			}
			case CHECK_CONCENTRATION_DUEL:
			case CHECK_BOSS_DUEL:
			case CHECK_BATTLE_CITY:
			case CHECK_DUELIST_KINGDOM:
			case CHECK_COMMAND_DUEL:
			case CHECK_DECK_MASTER_DUEL: {
				if (static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					for (int i = 0; i < 2; ++i)
						mainGame->chkRules[i]->setEnabled(false);
				}
				else {
					for (int i = 0; i < 2; ++i)
						mainGame->chkRules[i]->setEnabled(true);
					for (int i = 0; i < 16; ++i) {
						if (mainGame->chkRules[i]->isChecked() && i != 3 && i != 9)
							for (int i = 0; i < 2; ++i)
								mainGame->chkRules[i]->setEnabled(false);
					}
				}
				break;
			}
			case CHECK_TURBO_DUEL_1: {
				if (static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					for (int i = 0; i < 3; ++i)
						mainGame->chkRules[i]->setEnabled(false);
					mainGame->chkRules[10]->setEnabled(false);
				}
				else {
					for (int i = 0; i < 3; ++i)
						mainGame->chkRules[i]->setEnabled(true);
					mainGame->chkRules[10]->setEnabled(true);
					for (int i = 0; i < 16; ++i) {
						if (mainGame->chkRules[i]->isChecked() && i != 3 && i != 9)
							for (int i = 0; i < 1; ++i)
								mainGame->chkRules[i]->setEnabled(false);
					}
				}
				break;
			}
			case CHECK_TURBO_DUEL_2: {
				if (static_cast<irr::gui::IGUICheckBox*>(caller)->isChecked()) {
					for (int i = 0; i < 3; ++i)
						mainGame->chkRules[i]->setEnabled(false);
					mainGame->chkRules[11]->setEnabled(false);
				}
				else {
					for (int i = 0; i < 3; ++i)
						mainGame->chkRules[i]->setEnabled(true);
					mainGame->chkRules[11]->setEnabled(true);
					for (int i = 0; i < 16; ++i) {
						if (mainGame->chkRules[i]->isChecked() && i != 3 && i != 9)
							for (int i = 0; i < 1; ++i)
								mainGame->chkRules[i]->setEnabled(false);
					}
				}
				break;
			}
			}
			break;
		}
		case irr::gui::EGET_EDITBOX_ENTER: {
			switch(id) {
			case EDITBOX_CHAT: {
				if(mainGame->dInfo.isReplay)
					break;
				const wchar_t* input = mainGame->ebChatInput->getText();
				if(input[0]) {
					unsigned short msgbuf[256];
					if(mainGame->dInfo.isStarted) {
						if(mainGame->dInfo.player_type < 7) {
							if(mainGame->dInfo.isTag && (mainGame->dInfo.player_type % 2))
								mainGame->AddChatMsg((wchar_t*)input, 2);
							else
								mainGame->AddChatMsg((wchar_t*)input, 0);
						} else
							mainGame->AddChatMsg((wchar_t*)input, 10);
					} else
						mainGame->AddChatMsg((wchar_t*)input, 7);
					int len = BufferIO::CopyWStr(input, msgbuf, 256);
					DuelClient::SendBufferToServer(CTOS_CHAT, msgbuf, (len + 1) * sizeof(short));
					mainGame->ebChatInput->setText(L"");
				}
				break;
			}
			}
			break;
		}
		case irr::gui::EGET_COMBO_BOX_CHANGED: {
			switch (id) {
			case COMBOBOX_DUEL_RULE: {
				switch (mainGame->cbDuelRule->getSelected()) {
				case 0:{
					mainGame->cbDuelRule->removeItem(4);
					mainGame->duel_param = 0x1F00;
					break;
				}
				case 1: {
					mainGame->cbDuelRule->removeItem(4);
					mainGame->duel_param = 0x1E00;
					break;
				}
				case 2: {
					mainGame->cbDuelRule->removeItem(4);
					mainGame->duel_param = 0x1000;
					break;
				}
				case 3: {
					mainGame->cbDuelRule->removeItem(4);
					mainGame->duel_param = 0;
					break;
				}
				}
				uint32 flag = 0, filter = 0x100;
				for (int i = 0; i < 5; ++i, filter <<= 1)
					if (mainGame->duel_param & filter)
						mainGame->chkCustomRules[i]->setChecked(true);
					else
						mainGame->chkCustomRules[i]->setChecked(false);
			}
			}
		}
		default: break;
		}
		break;
	}
	case irr::EET_KEY_INPUT_EVENT: {
		switch(event.KeyInput.Key) {
		case irr::KEY_KEY_R: {
			if(!event.KeyInput.PressedDown && !mainGame->HasFocus(EGUIET_EDIT_BOX))
				mainGame->textFont->setTransparency(true);
			break;
		}
		case irr::KEY_ESCAPE: {
			if(!mainGame->HasFocus(EGUIET_EDIT_BOX))
				mainGame->device->minimizeWindow();
			break;
		}
		case irr::KEY_F12: {
			if (!event.KeyInput.PressedDown)
				utils.takeScreenshot(mainGame->device);
			return true;
			break;
		}
		default: break;
		}
		break;
	}
	default: break;
	}
	return false;
}

}
