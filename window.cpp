#include "stdafx.h"
#include "resource.h"
#include "window.h"
#include <sstream>
#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <utility>

#define DEBUG_TEXT 0

namespace beluga {

constexpr int MainFrameWidth = 383;
#if DEBUG_TEXT
constexpr int MainFrameHeight = 521;
#else
constexpr int MainFrameHeight = 318;
#endif
constexpr int BackgroundWidth = 383;
constexpr int BackgroundHeight = 283;

constexpr int SquareX = 18;
constexpr int SquareY = 18;
constexpr int SquareWidth = 32;
constexpr int SquareHeight = 32;
constexpr int DiskWidth = 29;
constexpr int DiskHeight = 29;

constexpr int MenuButtonWidth     = 72;
constexpr int MenuButtonHeight    = 24;
constexpr int MenuButtonX         = 297;
constexpr int MenuButtonY         = 76;
constexpr int MenuBackButtonX     = 297;
constexpr int MenuBackButtonY     = 245;

constexpr int FaceX = 301;
constexpr int FaceY = 17;
constexpr int FaceWidth = 64;
constexpr int FaceHeight = 44;
constexpr int FaceSrcXNormal = 0;
constexpr int FaceSrcXThink  = 64;
constexpr int FaceSrcYNormal = 0;
constexpr int FaceSrcYHappy  = 44;
constexpr int FaceSrcYSad    = 88;

constexpr int MessageFontSize = 16;
constexpr int ButtonFontSize  = 14;

constexpr int MessageTextX = 0;
constexpr int MessageTextY = 283;
constexpr int MessageTextWidth = 383;
constexpr int MessageTextHeight = 16;

constexpr int KifuTextX = 0;
constexpr int KifuTextY = 302;
constexpr int KifuTextWidth = 250;
constexpr int KifuTextHeight = 16;

constexpr int EvalTextX = 253;
constexpr int EvalTextY = 302;
constexpr int EvalTextWidth = 130;
constexpr int EvalTextHeight = 16;

constexpr int DebugTextX = 0;
constexpr int DebugTextY = 321;
constexpr int DebugTextWidth = 383;
constexpr int DebugTextHeight = 200;

constexpr UINT MainTimer = 1;

constexpr int WM_BELUGA_REPAINT    = WM_APP + 1;
constexpr int WM_BELUGA_STATE_BACK = WM_APP + 2;

const std::unordered_map<MainWindow::State, std::vector<MainWindow::Button>> MainWindow::StateButtonMap = {
  { StateGameTypeSelection, { ButtonFree, ButtonTournament } },
  { StateComLevelSelection, { ButtonLevel1, ButtonLevel2, ButtonLevel3, ButtonLevel4, ButtonLevel5, ButtonBack } },
  { StateDiskSelection    , { ButtonBlack, ButtonWhite, ButtonRandom, ButtonBack } },
  { StateDuringGame       , { ButtonBack } },
};

const std::unordered_map<MainWindow::Button, LPCWSTR> MainWindow::ButtonTextMap = {
  { ButtonFree,      L"Free" },
  { ButtonTournament,L"Tournament" },
  { ButtonLevel1,    L"Level 1" },
  { ButtonLevel2,    L"Level 2" },
  { ButtonLevel3,    L"Level 3" },
  { ButtonLevel4,    L"Level 4" },
  { ButtonLevel5,    L"Level 5" },
  { ButtonBlack,     L"Black" },
  { ButtonWhite,     L"White" },
  { ButtonRandom,    L"Random" },
  { ButtonBack,      L"Back" },
};

std::mutex mainThreadQueueMutex;
std::list<std::function<void()>> mainThreadQueue;

HWND MainWindow::Create(HINSTANCE hInstance, WNDPROC wndProc) {
  WNDCLASS winc;
  winc.style = CS_HREDRAW | CS_VREDRAW;
  winc.lpfnWndProc = wndProc;
  winc.cbClsExtra = winc.cbWndExtra = 0;
  winc.hInstance = hInstance;
  winc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BELUGA));
  winc.hCursor = LoadCursor(NULL, IDC_ARROW);
  winc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
  winc.lpszMenuName = NULL;
  winc.lpszClassName = L"MainWindow";

  if (!RegisterClass(&winc)) {
    return NULL;
  }

  return CreateWindow(
    L"MainWindow", L"Beluga",
    WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
    CW_USEDEFAULT, CW_USEDEFAULT,
    0, 0,
    NULL, NULL,
    hInstance, NULL
  );
}

LRESULT MainWindow::OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE:
    OnCreate(hwnd, lp);
    return 0;

  case WM_DESTROY:
    OnDestroy();
    return 0;

  case WM_PAINT:
    HDC hdc;
    PAINTSTRUCT ps;
    hdc = BeginPaint(hwnd, &ps);
    Paint(hdc);
    EndPaint(hwnd, &ps);
    DeleteDC(hdc);
    return 0;

  case WM_COMMAND:
    OnPushButton((Button)LOWORD(wp));
    return 0;

  case WM_LBUTTONDOWN:
    OnLButtonDown(hwnd, LOWORD(lp), HIWORD(lp));
    return 0;

  case WM_LBUTTONUP:
    OnLButtonUp(hwnd, LOWORD(lp), HIWORD(lp));
    return 0;

  case WM_TIMER:
    OnTimer();
    return 0;

  case WM_BELUGA_REPAINT:
    Repaint();
    return 0;

  case WM_BELUGA_STATE_BACK:
    state_ = StateGameTypeSelection;
    OnChangeState();
    return 0;
  }

  return DefWindowProc(hwnd, msg, wp, lp);
}

void MainWindow::OnCreate(HWND hwnd, LPARAM lp) {
  mainWindow_ = hwnd;
  hInstance_ = ((LPCREATESTRUCT)(lp))->hInstance;
  cx_ = -1;
  cy_ = -1;

  RECT winRect;
  RECT cliRect;
  int width  = MainFrameWidth;
  int height = MainFrameHeight;
  if (GetWindowRect(mainWindow_, &winRect) && GetClientRect(mainWindow_, &cliRect)) {
    width = winRect.right - winRect.left + MainFrameWidth - cliRect.right;
    height = winRect.bottom - winRect.top + MainFrameHeight - cliRect.bottom;
  }
  SetWindowPos(mainWindow_, HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW | SWP_NOMOVE);

  bmpBoard_ = LoadImageAsDC(L"board.bmp");
  bmpDisk_ = LoadImageAsDC(L"disk.bmp");
  bmpFace_ = LoadImageAsDC(L"face.bmp");

  hButtonFont_ = CreateFont(
    ButtonFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    ANSI_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    VARIABLE_PITCH | FF_ROMAN, L"Cambria");
  objList_.push_back(hButtonFont_);

  hMessageFont_ = CreateFont(
    MessageFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    ANSI_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    VARIABLE_PITCH | FF_ROMAN, L"Cambria");
  objList_.push_back(hMessageFont_);

  msgText_ = CreateWindow(
    L"STATIC", L"",
    WS_CHILD | WS_VISIBLE | SS_LEFT,
    MessageTextX, MessageTextY, MessageTextWidth, MessageTextHeight,
    hwnd, NULL, hInstance_, NULL
  );
  SendMessage(msgText_, WM_SETFONT, (WPARAM)hMessageFont_, TRUE);

  hEditFont_ = CreateFont(
    MessageFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    ANSI_CHARSET, OUT_DEFAULT_PRECIS,
    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
    VARIABLE_PITCH | FF_ROMAN, L"Courier New");
  objList_.push_back(hEditFont_);

  kifuText_ = CreateWindow(
    L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_LEFT |
    ES_AUTOHSCROLL,
    KifuTextX, KifuTextY, KifuTextWidth, KifuTextHeight,
    hwnd, NULL, hInstance_, NULL
  );
  SendMessage(kifuText_, WM_SETFONT, (WPARAM)hEditFont_, TRUE);

  evalText_ = CreateWindow(
    L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_LEFT |
    ES_AUTOHSCROLL,
    EvalTextX, EvalTextY, EvalTextWidth, EvalTextHeight,
    hwnd, NULL, hInstance_, NULL
  );
  SendMessage(evalText_, WM_SETFONT, (WPARAM)hEditFont_, TRUE);

#if DEBUG_TEXT
  debugText_ = CreateWindow(
    L"EDIT", L"",
    WS_CHILD | WS_VISIBLE | ES_LEFT | ES_MULTILINE |
    WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL,
    DebugTextX, DebugTextY, DebugTextWidth, DebugTextHeight,
    hwnd, NULL, hInstance_, NULL
  );
  SendMessage(debugText_, WM_SETFONT, (WPARAM)hEditFont_, TRUE);
#endif

  SetTimer(mainWindow_, MainTimer, 0, NULL);

  state_ = StateGameTypeSelection;
  OnChangeState();
}

void MainWindow::OnDestroy() {
  gameManager_.Stop();
  KillTimer(mainWindow_, MainTimer);
  PostQuitMessage(0);
  DeleteImageDCs();
}

void MainWindow::OnLButtonDown(HWND hwnd, WORD x, WORD y) {
  if (SquareX <= x && x < SquareX + SquareWidth * 8 &&
      SquareY <= y && y < SquareY + SquareHeight * 8) {
    cx_ = (x - SquareX) / SquareWidth;
    cy_ = (y - SquareY) / SquareHeight;
  }
}

void MainWindow::OnLButtonUp(HWND hwnd, WORD x, WORD y) {
  if (SquareX <= x && x < SquareX + SquareWidth * 8 &&
      SquareY <= y && y < SquareY + SquareHeight * 8) {
    int cx = (x - SquareX) / SquareWidth;
    int cy = (y - SquareY) / SquareHeight;
    if (cx_ == cx && cy_ == cy) {
      gameManager_.InputPlayerMove(Square(cx, cy));
    }
  }
  cx_ = -1;
  cy_ = -1;
}

void MainWindow::OnTimer() {
  std::function<void()> func;
  {
    std::lock_guard<std::mutex> lock(mainThreadQueueMutex);
    if (mainThreadQueue.empty()) {
      return;
    }
    func = mainThreadQueue.front();
    mainThreadQueue.pop_front();
  }
  func();
}

void MainWindow::Repaint() {
  HDC hdc = GetDC(mainWindow_);
  Paint(hdc);
  ReleaseDC(mainWindow_, hdc);

  for (auto bi : buttons_) {
    InvalidateRect(bi.hwnd, NULL, TRUE);
  }
}

void MainWindow::Paint(HDC wdc) {
  HDC dc;
  HBITMAP bmp;

  bmp = CreateCompatibleBitmap(wdc, BackgroundWidth, BackgroundHeight);
  dc = CreateCompatibleDC(wdc);

  SelectObject(dc, bmp);

  BitBlt(dc, 0, 0, BackgroundWidth, BackgroundHeight, bmpBoard_, 0, 0, SRCCOPY);

  switch (state_) {
  case StateGameTypeSelection:
    break;

  case StateComLevelSelection:
    break;

  case StateDiskSelection:
    break;

  case StateDuringGame:
    Board board = gameManager_.GetBoard();
    for (Square square = Square::Begin(); square != Square::End(); square = square.Next()) {
      DiskColor stone = board.Get(square);
      int x = square.GetX();
      int y = square.GetY();
      switch (stone) {
      case ColorBlack:
        BitBlt(dc, SquareX + x * SquareWidth, SquareY + y * SquareHeight, DiskWidth, DiskHeight, bmpDisk_, 0, 0, SRCCOPY);
        break;
      case ColorWhite:
        BitBlt(dc, SquareX + x * SquareWidth, SquareY + y * SquareHeight, DiskWidth, DiskHeight, bmpDisk_, DiskWidth, 0, SRCCOPY);
        break;
      }
    }

    DiskColor playerColor = gameManager_.GetPlayerColor();
    bool end = board.IsEnd();
    auto totalScore = board.GetTotalScore();
    SearchResult searchResult = gameManager_.GetSearchResult();
    int faceSrcX = end || gameManager_.IsPlayerTurn() ? FaceSrcXNormal
                                                      : FaceSrcXThink;
    int faceSrcY = (end && totalScore.winner == playerColor ) || searchResult.score <= -10 * ScoreScale ? FaceSrcYSad
                 : (end && totalScore.winner != Winner::Draw) || searchResult.score >=  10 * ScoreScale ? FaceSrcYHappy
                                                                                           : FaceSrcYNormal;
    BitBlt(dc, FaceX, FaceY, FaceWidth, FaceHeight, bmpFace_, faceSrcX, faceSrcY, SRCCOPY);

    break;
  }

  BitBlt(wdc, 0, 0, BackgroundWidth, BackgroundHeight, dc, 0, 0, SRCCOPY);

  DeleteDC(dc);
  DeleteObject(bmp);
}

void MainWindow::OnChangeState() {
  switch (state_) {
  case StateDuringGame:
    gameManager_.Start(gameSetting_);
    break;
  default:
    gameManager_.Stop();
    break;
  }

  Repaint();

  for (auto bi : buttons_) {
    DestroyWindow(bi.hwnd);
  }

  buttons_.clear();

  int nextY = MenuButtonY;
  for (auto button : StateButtonMap.at(state_)) {
    int x, y;
    if (button == ButtonBack) {
      x = MenuBackButtonX;
      y = MenuBackButtonY;
    } else {
      x = MenuButtonX;
      y = nextY;
      nextY += MenuButtonHeight;
    }

    HWND hwnd = CreateWindow(
      L"BUTTON", ButtonTextMap.at(button),
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      x, y, MenuButtonWidth, MenuButtonHeight,
      mainWindow_, (HMENU)button,
      hInstance_, NULL
    );
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hButtonFont_, TRUE);

    buttons_.push_back({ button, hwnd });
  }
}

void MainWindow::OnPushButton(Button button) {
  switch (button) {
  case ButtonFree:
    gameSetting_.gameType = GameTypeFree;
    state_ = StateComLevelSelection;
    OnChangeState();
    break;

  case ButtonTournament:
    gameSetting_.gameType = GameTypeTournament;
    gameSetting_.playerDisk = PlayerDiskRandom;
    state_ = StateDuringGame;
    OnChangeState();
    break;

  case ButtonLevel1:
    gameSetting_.comLevel = ComLevel1;
    state_ = StateDiskSelection;
    OnChangeState();
    break;

  case ButtonLevel2:
    gameSetting_.comLevel = ComLevel2;
    state_ = StateDiskSelection;
    OnChangeState();
    break;

  case ButtonLevel3:
    gameSetting_.comLevel = ComLevel3;
    state_ = StateDiskSelection;
    OnChangeState();
    break;

  case ButtonLevel4:
    gameSetting_.comLevel = ComLevel4;
    state_ = StateDiskSelection;
    OnChangeState();
    break;

  case ButtonLevel5:
    gameSetting_.comLevel = ComLevel5;
    state_ = StateDiskSelection;
    OnChangeState();
    break;

  case ButtonBlack:
    gameSetting_.playerDisk = PlayerDiskBlack;
    state_ = StateDuringGame;
    OnChangeState();
    break;

  case ButtonWhite:
    gameSetting_.playerDisk = PlayerDiskWhite;
    state_ = StateDuringGame;
    OnChangeState();
    break;

  case ButtonRandom:
    gameSetting_.playerDisk = PlayerDiskRandom;
    state_ = StateDuringGame;
    OnChangeState();
    break;

  case ButtonBack:
    state_ = StateGameTypeSelection;
    OnChangeState();
    break;

  }
}

HDC MainWindow::LoadImageAsDC(LPCTSTR filename) {
  HDC dc = CreateCompatibleDC(NULL);
  auto bitmap = (HBITMAP)LoadImage(NULL, filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
  SelectObject(dc, bitmap);
  dcList_.push_back(dc);
  objList_.push_back(bitmap);
  return dc;
}

void MainWindow::DeleteImageDCs() {
  for (const auto& dc : dcList_) {
    DeleteDC(dc);
  }
  for (const auto& bmp : objList_) {
    DeleteObject(bmp);
  }
}

template <class T>
void PushMainThreadJob(T&& func) {
  std::lock_guard<std::mutex> lock(mainThreadQueueMutex);
  mainThreadQueue.push_back(std::forward<T>(func));
}

void MainWindow::OnNewGame() {
  PushMainThreadJob([this]() {
    SetWindowText(kifuText_, L"");
    SetWindowText(evalText_, L"");
  });
}

void MainWindow::OnPlayerTurn() {
  ComLevel level = gameManager_.GetComLevel();
  PushMainThreadJob([this, level]() {
    SendMessage(mainWindow_, WM_BELUGA_REPAINT, 0, 0);
    TCHAR buf[1024];
    wsprintf(buf, L"Level %d... Your Turn", level);
    SetWindowText(msgText_, buf);
  });
}

void MainWindow::OnComTurn() {
  ComLevel level = gameManager_.GetComLevel();
  PushMainThreadJob([this, level]() {
    SendMessage(mainWindow_, WM_BELUGA_REPAINT, 0, 0);
    TCHAR buf[1024];
    wsprintf(buf, L"Level %d... Beluga's Turn", level);
    SetWindowText(msgText_, buf);
  });

  gameManager_.Sleep(std::chrono::milliseconds(200));
}

void MainWindow::OnMove() {
  Square move = gameManager_.GetLastMove();
  PushMainThreadJob([this, move]() {
    TCHAR buf[1024];
    GetWindowText(kifuText_, buf, sizeof(buf) / sizeof(buf[0]));
    lstrcpy(buf + lstrlen(buf), move.ToString());
    SetWindowText(kifuText_, buf);
  });
}

void MainWindow::OnSearchScore() {
  SearchResult searchResult = gameManager_.GetSearchResult();
  PushMainThreadJob([this, searchResult]() {
    TCHAR buf[1024];
    wsprintf(buf, L"%d ", searchResult.score / ScoreScale);
    LRESULT len = SendMessage(evalText_, WM_GETTEXTLENGTH, 0, 0);
    SendMessage(evalText_, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(evalText_, EM_REPLACESEL, (WPARAM)false, (LPARAM)buf);
  });
}

void MainWindow::OnPass() {
}

void MainWindow::OnResult() {
  Board board           = gameManager_.GetBoard();
  DiskColor playerColor = gameManager_.GetPlayerColor();
  ComLevel level        = gameManager_.GetComLevel();
  PushMainThreadJob([this, board, playerColor, level]() {
    SendMessage(mainWindow_, WM_BELUGA_REPAINT, 0, 0);
    TotalScore totalScore = board.GetTotalScore();
    TCHAR buf[1024];
    if (totalScore.winner == playerColor) {
      wsprintf(buf, L"Level %d... You win!", level);
    } else if (totalScore.winner != Winner::Draw) {
      wsprintf(buf, L"Level %d... You lose.", level);
    } else {
      wsprintf(buf, L"Level %d... Draw", level);
    }
    SetWindowText(msgText_, buf);
  });

  gameManager_.Sleep(std::chrono::seconds(2));
}

void MainWindow::OnEnd() {
  PushMainThreadJob([this]() {
    SendMessage(mainWindow_, WM_BELUGA_STATE_BACK, 0, 0);
  });
}

void MainWindow::OnLog(LPCTSTR msg) {
  LRESULT len = SendMessage(debugText_, WM_GETTEXTLENGTH, 0, 0);
  SendMessage(debugText_, EM_SETSEL, (WPARAM)len, (LPARAM)len);
  SendMessage(debugText_, EM_REPLACESEL, (WPARAM)false, (LPARAM)msg);
}

} // namespace beluga
