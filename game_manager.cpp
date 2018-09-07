#include "stdafx.h"
#include "game_manager.h"
#include <chrono>
#include <vector>

namespace beluga {

void GameManager::Start(const GameSetting& setting) {
  setting_ = setting;
  stop_ = false;
  searcher_.Reset();
  board_ = Board::GetEmptyBoard();

  thread_ = std::thread([this]() {
    GameLoop();
  });
}

void GameManager::Stop() {
  if (thread_.joinable()) {
    stop_ = true;
    searcher_.Stop();
    thread_.join();
  }
}

void GameManager::GameLoop() {
  std::vector<ComLevel> comLevels;
  if (setting_.gameType == GameType::GameTypeFree) {
    comLevels.push_back(setting_.comLevel);
  } else {
    comLevels.push_back(ComLevel::ComLevel1);
    comLevels.push_back(ComLevel::ComLevel2);
    comLevels.push_back(ComLevel::ComLevel3);
    comLevels.push_back(ComLevel::ComLevel4);
    comLevels.push_back(ComLevel::ComLevel5);
  }

  std::uniform_int_distribution<unsigned> dstBit(0, 1);
  for (const auto& comLevel : comLevels) {
    DiskColor playerColor = setting_.playerDisk == PlayerDiskBlack ? ColorBlack
                          : setting_.playerDisk == PlayerDiskWhite ? ColorWhite
                          : dstBit(random_) ? ColorBlack :  ColorWhite;
    bool lastPassed = false;

    switch (comLevel) {
    case ComLevel1: searchDepth_ = 3; endingSearchDepth_ = 5; break;
    case ComLevel2: searchDepth_ = 4; endingSearchDepth_ = 7; break;
    case ComLevel3: searchDepth_ = 5; endingSearchDepth_ = 10; break;
    case ComLevel4: searchDepth_ = 6; endingSearchDepth_ = 12; break;
    default:        searchDepth_ = 8; endingSearchDepth_ = 14; break;
    }

    comLevel_     = comLevel;
    playerColor_  = playerColor;
    board_        = Board::GetNormalInitBoard();
    searchResult_ = { Square::Invalid(), 0 };

    handler_->OnNewGame();

    while (!board_.load().IsEnd()) {
      if (board_.load().GetNextDisk() == playerColor) {
        PlayerTurn();
      } else {
        ComTurn();
      }

      if (stop_) { return; }
    }

    handler_->OnResult();

    TotalScore totalScore = board_.load().GetTotalScore();
    if (totalScore.winner != playerColor) {
      break;
    }

    if (stop_) { return; }
  }

  handler_->OnEnd();
}

void GameManager::PlayerTurn() {
  playerMove_ = Square::Invalid();
  playerTurn_ = true;
  handler_->OnPlayerTurn();

  auto board = board_.load();

  // pass
  if (board.GenerateMoves() == Bitboard(0)) {
    board.Pass();
    board_ = board;

    handler_->OnPass();

    return;
  }

  // wait for human player's move
  Square move;
  while (true) {
    if (stop_) { return; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    move = playerMove_.exchange(Square::Invalid());
    if (!move.IsInvalid() && board.CanMove(move)) {
      break;
    }
  }

  // do move
  board.DoMove(move);

  board_    = board;
  lastMove_ = move;

  handler_->OnMove();
}

void GameManager::ComTurn() {
  playerTurn_ = false;
  handler_->OnComTurn();

  auto board = board_.load();

  if (board == Board::GetNormalInitBoard()) {
    // first move is always F5
    Square move(5, 4);
    board.DoMove(move);

    board_    = board;
    lastMove_ = move;

    handler_->OnSearchScore();
    handler_->OnMove();

    return;
  }

  if (board.MustPass()) {
    // pass
    board.Pass();
    board_.store(board);
    handler_->OnPass();
    return;
  }

  auto begin = std::chrono::system_clock::now();
  SearchResult searchResult = searcher_.Search(board_, searchDepth_, endingSearchDepth_);
  auto end = std::chrono::system_clock::now();

  if (stop_) { return; }

  TCHAR buf[1024];
  wsprintf(buf, L"%d milliseconds\r\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
  handler_->OnLog(buf);

  board.DoMove(searchResult.move);

  board_        = board;
  lastMove_     = searchResult.move;
  searchResult_ = searchResult;

  handler_->OnSearchScore();
  handler_->OnMove();
}

void GameManager::OnIterate(int depth, const PV& pv, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %2d: %8d: %s: %d\r\n", depth, nodes, pv.ToString(), score);
  handler_->OnLog(buf);
}

void GameManager::OnFailHigh(int depth, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %2d: %8d: fail-high: %d\r\n", depth, nodes, score);
  handler_->OnLog(buf);
}

void GameManager::OnFailLow(int depth, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %2d: %8d: fail-low: %d\r\n", depth, nodes, score);
  handler_->OnLog(buf);
}

void GameManager::OnEnding(const PV& pv, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Ending: %8d: %s: %d\r\n", nodes, pv.ToString(), score);
  handler_->OnLog(buf);
}

} // namespace beluga
