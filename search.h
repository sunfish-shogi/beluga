#pragma once

#include "reversi.h"
#include <atomic>
#include <random>

namespace beluga {

using Score = int;
constexpr int ScoreScale = 100;
constexpr int ScoreInfinity = 100 * ScoreScale;

inline Score ScoreMax(Score a, Score b) {
  return a > b ? a : b;
}

inline Score ScoreMin(Score a, Score b) {
  return a < b ? a : b;
}

struct PV {
  Square moves[60];
  int length;

  void Clear() {
    length = 0;
  }

  void Set(Square curr, PV& child) {
    moves[0] = curr;
    memcpy(&moves[1], child.moves, sizeof(child.moves[0]) * child.length);
    length = child.length + 1;
  }

  LPCTSTR ToString() const {
    thread_local TCHAR buf[128];
    for (int i = 0; i < length; i++) {
      memcpy(&buf[i * 2], moves[i].ToString(), sizeof(TCHAR) * 2);
    }
    buf[length * 2] = L'\0';
    return buf;
  }
};

struct SearchResult {
  Square move;
  Score score;
  PV pv;
};

class SearchHandler {
public:
  virtual void OnIterate(int depth, const PV& pv, Score score, int nodes) = 0;
  virtual void OnFailHigh(int depth, Score score, int nodes) = 0;
  virtual void OnFailLow(int depth, Score score, int nodes) = 0;
};

class Searcher {
public:

  constexpr static int DepthOnePly = 1;
  constexpr static int TTSize        = 0x100000;
  constexpr static int TTMask        = 0x0fffff;

  Searcher(SearchHandler* handler = nullptr);

  SearchResult Search(const Board& board, int depth);

  void Reset() {
    stop_ = false;
  }

  void Stop() {
    stop_ = true;
  }

private:

  struct Move {
    Square move;
    Score score;
  };

  struct Node {
    Move moves[48];
    PV pv;
    int nmoves;
    int mi;
  };

  struct Tree {
    Board board;
    int ply;
    Node stack[64];
    int nodes;
  };

  enum TTType {
    TTUpper,
    TTLower,
    TTActual,
  };

  struct TTElement {
    uint64_t hash;
    Score score;
    int depth;
    TTType type;
    Square bestMove;
  };

  void StorePV(Board board, const PV& pv, Score score);

  Score Search(Tree& tree, int depth, Score alpha, Score beta);

  void GenerateMoves(Tree& tree, Square ttMove, int depth, Score alpha, Score beta);

  Score Evaluate(const Board& board);

  int CountFixedDisk(const Bitboard& bitboard);

  std::atomic<bool> stop_;
  std::mt19937 random_;
  SearchHandler* handler_;
  std::unique_ptr<TTElement[]> tt_;

};

} // namespace beluga
