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
  searchScore_.store(0);

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
    case ComLevel1: searchDepth_ = 3; break;
    case ComLevel2: searchDepth_ = 4; break;
    case ComLevel3: searchDepth_ = 5; break;
    case ComLevel4: searchDepth_ = 7; break;
    default:        searchDepth_ = 9; break;
    }

    comLevel_.store(comLevel);
    playerColor_.store(playerColor);
    board_.store(Board::GetNormalInitBoard());
    searchScore_.store(0);
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

  if (board.GenerateMoves() == Bitboard(0)) {
    // pass
    board.Pass();
    board_.store(board);
    handler_->OnPass();
    return;
  }

  // move
  Square move;
  while (true) {
    if (stop_) { return; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    move = playerMove_.exchange(Square::Invalid());
    if (!move.IsInvalid() && board.CanMove(move)) {
      break;
    }
  }
  board.DoMove(move);
  board_.store(board);
  lastMove_.store(move);
  handler_->OnMove();
}

void GameManager::ComTurn() {
  playerTurn_ = false;
  handler_->OnComTurn();

  auto board = board_.load();

  if (board == Board::GetNormalInitBoard()) {
    // first move
    Square move(5, 4);
    board.DoMove(move);
    board_.store(board);
    lastMove_.store(move);
    searchScore_.store(Score(0));
    handler_->OnSearchScore();
    handler_->OnMove();
    return;
  }

  auto begin = std::chrono::system_clock::now();
  auto searchResult = searcher_.Search(board_, searchDepth_);
  auto end = std::chrono::system_clock::now();
  TCHAR buf[1024];
  wsprintf(buf, L"%d milliseconds\r\n", (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());
  handler_->OnLog(buf);
  if (searchResult.move.IsInvalid()) {
    // pass
    board.Pass();
    board_.store(board);
    handler_->OnPass();
    return;
  }

  // move
  board.DoMove(searchResult.move);
  board_.store(board);
  lastMove_.store(searchResult.move);
  searchScore_.store(searchResult.score);
  handler_->OnSearchScore();
  handler_->OnMove();
}

void GameManager::OnIterate(int depth, Square move, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %d: %s: Score %d: Nodes %d\r\n", depth, move.ToString(), score, nodes);
  handler_->OnLog(buf);
}

void GameManager::OnFailHigh(int depth, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %d: fail-high: Score %d: Nodes %d\r\n", depth, score, nodes);
  handler_->OnLog(buf);
}

void GameManager::OnFailLow(int depth, Score score, int nodes) {
  TCHAR buf[1024];
  wsprintf(buf, L"Depth %d: fail-low: Score %d: Nodes %d\r\n", depth, score, nodes);
  handler_->OnLog(buf);
}

} // namespace beluga
