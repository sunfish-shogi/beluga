#pragma once

#include "game_manager.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace beluga {

class MainWindow : public GameManagerHandler {
public:

  MainWindow() : gameManager_(this) {}

  HWND Create(HINSTANCE hInstance, WNDPROC wndProc);

  LRESULT OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void Repaint();

  // GameManagerHandler
  void OnNewGame();
  void OnPlayerTurn();
  void OnComTurn();
  void OnMove();
  void OnSearchScore();
  void OnPass();
  void OnResult();
  void OnEnd();
  void OnLog(LPCTSTR msg);

private:

  enum State {
    StateGameTypeSelection,
    StateComLevelSelection,
    StateDiskSelection,
    StateDuringGame,
  };

  enum Button {
    ButtonFree = 100,
    ButtonTournament,
    ButtonLevel1,
    ButtonLevel2,
    ButtonLevel3,
    ButtonLevel4,
    ButtonLevel5,
    ButtonBlack,
    ButtonWhite,
    ButtonRandom,
    ButtonBack,
  };

  struct ButtonInfo {
    Button button;
    HWND hwnd;
  };

  static const std::unordered_map<State, std::vector<Button>> StateButtonMap;
  static const std::unordered_map<Button, const char*> ButtonTextMap;
  static const std::unordered_map<Button, int> ButtonIDMap;

  void OnCreate(HWND hwnd, LPARAM lp);
  void OnDestroy();
  void OnLButtonDown(HWND hwnd, WORD x, WORD y);
  void OnLButtonUp(HWND hwnd, WORD x, WORD y);
  void OnTimer();
  void Paint(HDC wdc);
  void OnChangeState();
  void OnPushButton(Button button);
  HDC LoadImageAsDC(LPCTSTR filename);
  void DeleteImageDCs();

  HINSTANCE hInstance_;
  HWND mainWindow_;
  HDC bmpBoard_;
  HDC bmpDisk_;
  HDC bmpFace_;
  HFONT hEditFont_;
  HFONT hMessageFont_;
  HFONT hButtonFont_;
  HWND msgText_;
  HWND kifuText_;
  HWND evalText_;
  HWND debugText_;
  GameSetting gameSetting_;
  GameManager gameManager_;
  State state_;
  std::vector<ButtonInfo> buttons_;
  int cx_;
  int cy_;
  std::vector<HDC> dcList_;
  std::vector<HGDIOBJ> objList_;

};

} // namespace beluga