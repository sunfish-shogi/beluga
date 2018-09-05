#include "reversi.h"
#include "search.h"
#include <thread>
#include <atomic>
#include <random>
#include <ctime>

namespace beluga {

enum GameType {
  GameTypeFree,
  GameTypeTournament,
};

enum ComLevel {
  ComLevel1 = 1,
  ComLevel2,
  ComLevel3,
  ComLevel4,
  ComLevel5,
};

enum PlayerDisk {
  PlayerDiskBlack  = ColorBlack,
  PlayerDiskWhite  = ColorWhite,
  PlayerDiskRandom = 100,
};

struct GameSetting {
  GameType   gameType;
  ComLevel   comLevel;
  PlayerDisk playerDisk;
};

class GameManagerHandler {
public:
  virtual void OnNewGame() = 0;
  virtual void OnPlayerTurn() = 0;
  virtual void OnComTurn() = 0;
  virtual void OnMove() = 0;
  virtual void OnSearchScore() = 0;
  virtual void OnPass() = 0;
  virtual void OnResult() = 0;
  virtual void OnEnd() = 0;
  virtual void OnLog(LPCTSTR msg) = 0;
};

class GameManager : public SearchHandler {
public:

  GameManager(GameManagerHandler* handler)
    : searcher_(this),
      random_(static_cast<unsigned>(time(nullptr))),
      handler_(handler) {}

  void Start(const GameSetting& setting);

  void Stop();

  Board GetBoard() {
    return board_;
  }

  ComLevel GetComLevel() {
    return comLevel_;
  }

  DiskColor GetPlayerColor() {
    return playerColor_;
  }

  SearchResult GetSearchResult() {
    return searchResult_;
  }

  Square GetLastMove() {
    return lastMove_;
  }

  bool IsPlayerTurn() {
    return playerTurn_;
  }

  void InputPlayerMove(const Square& move) {
    playerMove_ = move;
  }

  template <class Rep, class Period>
  void Sleep(const std::chrono::duration<Rep, Period>& duration) const {
    auto start = std::chrono::system_clock::now();
    while (!stop_) {
      auto now = std::chrono::system_clock::now();
      if (now - start >= duration) {
        break;
      }
    }
  }

  void OnIterate(int depth, const PV& pv, Score score, int nodes);
  void OnFailHigh(int depth, Score score, int nodes);
  void OnFailLow(int depth, Score score, int nodes);

private:

  void GameLoop();

  void PlayerTurn();

  void ComTurn();

  GameManagerHandler* handler_;
  Searcher searcher_;
  GameSetting setting_;
  int searchDepth_;

  std::atomic<Board> board_;
  std::atomic<ComLevel> comLevel_;
  std::atomic<DiskColor> playerColor_;
  std::atomic<Square> lastMove_;
  std::atomic<SearchResult> searchResult_;
  std::atomic<Square> playerMove_;
  std::atomic<bool> playerTurn_;
  std::atomic<bool> stop_;

  std::thread thread_;
  std::mt19937 random_;

};

} // namespace beluga