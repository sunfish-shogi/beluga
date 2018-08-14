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

struct SearchResult {
  Square move;
  Score score;
};

class SearchHandler {
public:
  virtual void OnIterate(int depth, Square move, Score score, int nodes) = 0;
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
    Move moves[64];
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
