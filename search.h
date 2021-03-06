#pragma once

#include "reversi.h"
#include "evaluate.h"
#include <atomic>
#include <random>
#include <memory>
#include <cstring>

namespace beluga {

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

  const char* ToString() const {
    thread_local char buf[128];
    for (int i = 0; i < length; i++) {
      memcpy(&buf[i * 2], moves[i].ToString(), sizeof(char) * 2);
    }
    buf[length * 2] = '\0';
    return buf;
  }
};

struct SearchResult {
  Square move;
  Score score;
  bool ending;
};

class SearchHandler {
public:
  virtual void OnIterate(int depth, const PV& pv, Score score, int nodes) = 0;
  virtual void OnFailHigh(int depth, Score score, int nodes) = 0;
  virtual void OnFailLow(int depth, Score score, int nodes) = 0;
  virtual void OnEnding(const PV& pv, Score score, int nodes) = 0;
};

class Searcher {
public:

  constexpr static int DepthOnePly = 1;
  constexpr static int TTSize        = 0x100000;
  constexpr static int TTMask        = 0x0fffff;

  Searcher(const std::shared_ptr<Evaluator>& eval, SearchHandler* handler = nullptr);

  SearchResult Search(const Board& board, int depth, int endingDepth);

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

  Score SearchEnding(Tree& tree, Score alpha, Score beta, bool passed);

  Score Search(Tree& tree, int depth, Score alpha, Score beta, bool passed);

  void GenerateEndingMoves(Tree& tree, Score alpha, Score beta);

  void GenerateMoves(Tree& tree, Square ttMove, int depth, Score alpha, Score beta);

  std::atomic<bool> stop_;
  std::mt19937 random_;
  const std::shared_ptr<Evaluator> eval_;
  SearchHandler* handler_;
  std::unique_ptr<TTElement[]> tt_;

};

} // namespace beluga
