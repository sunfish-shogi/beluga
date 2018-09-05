#include "stdafx.h"
#include "search.h"
#include <algorithm>

#define TT         1
#define TT_MOVE    1
#define NEGA_SCOUT 1

namespace beluga {

Searcher::Searcher(SearchHandler* handler)
  : handler_(handler)
#if TT
  , tt_(new TTElement[TTSize])
#endif
{
#if TT
  for (uint64_t hashKey = 0; hashKey < TTSize; hashKey++) {
    tt_.get()[hashKey].hash = ~hashKey;
  }
#endif
}

SearchResult Searcher::Search(const Board& board, int maxDepth) {
  Tree tree;
  tree.ply = 0;
  tree.board = board;
  tree.nodes = 0;

  Node& node = tree.stack[0];
  node.pv.Clear();

  GenerateMoves(tree, Square::Invalid(), 0, 0, 0);
  if (node.nmoves == 0) {
    return { Square::Invalid(), 0 };
  }

  std::shuffle(node.moves, node.moves + node.nmoves, random_);

  for (int depth = DepthOnePly; depth < maxDepth + DepthOnePly; depth += DepthOnePly) {
    // clear score values
    for (int mi = 1; mi < node.nmoves; mi++) {
      node.moves[mi].score = -ScoreInfinity;
    }

    if (depth == DepthOnePly) {
      // initial depth
      Search(tree, depth, -ScoreInfinity, ScoreInfinity);

    } else {
      // aspiration search
      Score delta = 5 * ScoreScale;
      Score alpha = node.moves[0].score - delta;
      Score beta  = node.moves[0].score + delta;
      while (true) {
        Score score = Search(tree, depth, alpha, beta);
        if (stop_.load()) {
          break;
        }

        if (alpha < score && score < beta) {
          break;
        } else if (score <= alpha) {
          alpha = score - delta;
          if (handler_ != nullptr) {
            handler_->OnFailLow(depth, score, tree.nodes);
          }
        } else if (score >= beta) {
          beta = score + delta;
          if (handler_ != nullptr) {
            handler_->OnFailHigh(depth, score, tree.nodes);
          }
        }
        delta += 5 * ScoreScale;
      }
    }

    if (stop_.load()) {
      break;
    }

    // sort by score value
    std::stable_sort(node.moves, node.moves + node.nmoves, [](const Move& lhs, const Move& rhs) {
      return lhs.score > rhs.score;
    });

    if (handler_ != nullptr) {
      handler_->OnIterate(depth, node.pv, node.moves[0].score, tree.nodes);
    }

    StorePV(board, node.pv, node.moves[0].score);
  }

  return { node.moves[0].move, node.moves[0].score, node.pv };
}

void Searcher::StorePV(Board board, const PV& pv, Score score) {
  for (int i = 0; i < pv.length; i++) {
    if (board.MustPass()) {
      board.Pass();
    }

    uint64_t hash = board.GetHash();
    uint64_t hashKey = hash & TTMask;
    int depth = (pv.length - i) * DepthOnePly;
    tt_.get()[hashKey] = { hash, score, depth, TTActual, pv.moves[i] };

    board.DoMove(pv.moves[i]);
  }
}

Score Searcher::Search(Tree& tree, int depth, Score alpha, Score beta) {
  Node& node = tree.stack[tree.ply];
  if (tree.ply != 0) {
    node.pv.Clear();
  }

  tree.nodes++;

  if (tree.board.IsEnd()) {
    int bc = tree.board.GetBlackBoard().Count();
    int wc = tree.board.GetWhiteBoard().Count();
    Score score = (bc - wc) * ScoreScale;
    return tree.board.GetNextDisk() == ColorBlack ? score : -score;
  }

  if (depth < DepthOnePly) {
    Score score = Evaluate(tree.board);
    return tree.board.GetNextDisk() == ColorBlack ? score : -score;
  }

  bool isPV = (beta != alpha + 1);

  Square ttMove = Square::Invalid();
#if TT
  uint64_t hash = tree.board.GetHash();
  uint64_t hashKey = hash & TTMask;
  auto ttElem = tt_.get()[hashKey];
  if (ttElem.hash == hash) {
    if (!isPV && ttElem.depth >= depth) {
      switch (ttElem.type) {
      case TTActual:
        return ttElem.score;
      case TTUpper:
        if (ttElem.score <= alpha) {
          return ttElem.score;
        }
      case TTLower:
        if (ttElem.score >= beta) {
          return ttElem.score;
        }
      }
    }
    ttMove = ttElem.bestMove;
  }
#endif

  if (tree.ply == 0) {
    node.mi = 0;
  } else {
    GenerateMoves(tree, ttMove, depth, alpha, beta);
  }

  if (node.nmoves == 0) {
    // pass
    tree.board.Pass();
    Score score = -Search(tree, depth - DepthOnePly, -beta, -alpha);
    tree.board.Pass();
    return score;
  }

  Score bestScore = -ScoreInfinity;
  Square bestMove = Square::Invalid();

  while (true) {
    if (node.mi >= node.nmoves) {
      break;
    }
    bool isFirst = node.mi == 0;
    Move& m = node.moves[node.mi++];

    Score newAlpha = ScoreMax(alpha, bestScore);
    int newDepth = depth - DepthOnePly;

    Bitboard mask = tree.board.DoMove(m.move);
    tree.ply++;
#if NEGA_SCOUT
    if (isFirst || beta == newAlpha + 1) {
#endif
      m.score = -Search(tree, newDepth, -beta, -newAlpha);
#if NEGA_SCOUT
    } else {
      m.score = -Search(tree, newDepth, -(newAlpha + 1), -newAlpha);
      if (m.score >= newAlpha + 1) {
        m.score = -Search(tree, newDepth, -beta, -newAlpha);
      }
    }
#endif
    tree.board.UndoMove(m.move, mask);
    tree.ply--;

    if (stop_.load()) {
      return 0;
    }

    if (m.score > bestScore) {
      bestScore = m.score;
      bestMove = m.move;
      Node& child = tree.stack[tree.ply + 1];
      node.pv.Set(m.move, child.pv);
      if (bestScore >= beta) {
        break;
      }
    }
  }

#if TT
  if (ttElem.hash != hash || ttElem.depth <= depth) {
    TTType ttType;
    if (bestScore >= beta) {
      ttType = TTLower;
    } else if (bestScore <= alpha) {
      ttType = TTUpper;
    } else {
      ttType = TTActual;
    }
    tt_.get()[hashKey] = { hash, bestScore, depth, ttType, bestMove };
  }
#endif

  return bestScore;
}

void Searcher::GenerateMoves(Tree& tree, Square ttMove, int depth, Score alpha, Score beta) {
  Node& node = tree.stack[tree.ply];
  node.nmoves = 0;
  node.mi = 0;

  Bitboard moves = tree.board.GenerateMoves();
  if (moves == Bitboard(0)) {
    return;
  }

  for (Square move = Square::Begin(); move != Square::End(); move = move.Next()) {
    if (moves.Get(move)) {
      node.moves[node.nmoves++] = { move, 0 };
    }
  }

  if (node.nmoves <= 1) {
    return;
  }

  if (depth <= DepthOnePly * 1) {
#if TT_MOVE
    for (int mi = 0; mi < node.nmoves; mi++) {
      if (node.moves[mi].move == ttMove) {
        auto tmp = node.moves[mi];
        node.moves[mi] = node.moves[0];
        node.moves[0] = tmp;
        break;
      }
    }
#endif
    return;
  }

  int newDepth = depth <= DepthOnePly * 7 ? DepthOnePly : depth - DepthOnePly * 6;
  for (int mi = 0; mi < node.nmoves; mi++) {
#if TT_MOVE
    if (node.moves[mi].move == ttMove) {
      node.moves[mi].score = ScoreInfinity;
      continue;
    }
#endif
    Bitboard mask = tree.board.DoMove(node.moves[mi].move);
    tree.ply++;
    node.moves[mi].score = -Search(tree, newDepth, -beta, -alpha);
    tree.board.UndoMove(node.moves[mi].move, mask);
    tree.ply--;
  }
  
  std::stable_sort(node.moves, node.moves + node.nmoves, [](const Move& lhs, const Move& rhs) {
    return lhs.score > rhs.score;
  });
}

const Score Material1[64] = {
   800, -100,  -40,  -30,  -30,  -40, -400,  800,
  -400, -800,  -30,  -20,  -20,  -30, -800, -400,
   -40,  -30,  -20,  -10,  -10,  -20,  -30,  -40,
   -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
   -30,  -20,  -10,    0,    0,  -10,  -20,  -30,
   -40,  -30,  -20,  -10,  -10,  -20,  -30,  -40,
  -400, -800,  -30,  -20,  -20,  -30, -800, -400,
   800, -400,  -40,  -30,  -30,  -40, -400,  800,
};

const Score Material2[64] = {
   500, -100,    0,   20,   20,    0, -100,  500,
  -100, -300,  -40,    0,    0,  -40, -300, -100,
     0,  -40,  -80,   20,   20,  -80,  -40,    0,
    20,    0,   20,   50,   50,   20,    0,   20,
    20,    0,   20,   50,   50,   20,    0,   20,
     0,  -40,  -80,   20,   20,  -80,  -40,    0,
  -100, -300,  -40,    0,    0,  -40, -300, -100,
   500, -100,    0,   20,   20,    0, -100,  500,
};

const Score Material3[64] = {
   150,   50,   80,   80,   80,   80,   50,  150,
    50,   20,   40,   60,   60,   40,   20,   50,
    80,   40,   80,   80,   80,   80,   40,   80,
    80,   60,   80,   80,   80,   80,   60,   80,
    80,   60,   80,   80,   80,   80,   60,   80,
    80,   40,   80,   80,   80,   80,   40,   80,
    50,   20,   40,   60,   60,   40,   20,   50,
   150,   50,   80,   80,   80,   80,   50,  150,
};

Score Searcher::Evaluate(const Board& board) {
  Score score = 0;
  Bitboard black = board.GetBlackBoard();
  Bitboard white = board.GetWhiteBoard();
  Bitboard occupied = black | white;
  Bitboard empty = ~occupied;

  int count = occupied.Count();
  if (count <= 32) {
    Score material1 = 0;
    Score material2 = 0;
    for (Square square = Square::Begin(); square != Square::End(); square = square.Next()) {
      if (black.Get(square)) {
        material1 += Material1[square.GetRaw()];
        material2 += Material2[square.GetRaw()];
      } else if (white.Get(square)) {
        material1 -= Material1[square.GetRaw()];
        material2 -= Material2[square.GetRaw()];
      }
    }
    score += (material1 * (32 - count) + material2 * count) / 32;
  } else {
    Score material2 = 0;
    Score material3 = 0;
    for (Square square = Square::Begin(); square != Square::End(); square = square.Next()) {
      if (black.Get(square)) {
        material2 += Material2[square.GetRaw()];
        material3 += Material3[square.GetRaw()];
      } else if (white.Get(square)) {
        material2 -= Material2[square.GetRaw()];
        material3 -= Material3[square.GetRaw()];
      }
    }
    score += (material2 * (64 - count) + material3 * (count - 32)) / 32;
  }

  const Score FixedDiskScore = 1000 * (64 - count) / 64;
  int fixb = CountFixedDisk(black);
  score += fixb * FixedDiskScore;
  int fixw = CountFixedDisk(white);
  score -= fixw * FixedDiskScore;

  const Score OpenScore = -300;
  Bitboard bopen = black.Up()     | black.Down()     | black.Left()    | black.Right()
                 | black.LeftUp() | black.LeftDown() | black.RightUp() | black.RightDown();
  score += (bopen & empty).Count() * OpenScore;
  Bitboard wopen = white.Up()     | white.Down()     | white.Left()    | white.Right()
                 | white.LeftUp() | white.LeftDown() | white.RightUp() | white.RightDown();
  score -= (wopen & empty).Count() * OpenScore;

  return score;
}

int Searcher::CountFixedDisk(const Bitboard& bitboard) {
  Bitboard fixed = 0;

  for (int x = 0; x <= 6; x++) {
    if (!bitboard.Get(Square(x, 0))) { break; }
    fixed.Set(Square(x, 0));
  }

  for (int x = 0; x <= 6; x++) {
    if (!bitboard.Get(Square(x, 7))) { break; }
    fixed.Set(Square(x, 7));
  }

  for (int x = 7; x >= 1; x--) {
    if (!bitboard.Get(Square(x, 0))) { break; }
    fixed.Set(Square(x, 0));
  }

  for (int x = 7; x >= 1; x--) {
    if (!bitboard.Get(Square(x, 7))) { break; }
    fixed.Set(Square(x, 7));
  }

  for (int y = 0; y <= 6; y++) {
    if (!bitboard.Get(Square(0, y))) { break; }
    fixed.Set(Square(0, y));
  }

  for (int y = 0; y <= 6; y++) {
    if (!bitboard.Get(Square(7, y))) { break; }
    fixed.Set(Square(7, y));
  }

  for (int y = 7; y >= 1; y--) {
    if (!bitboard.Get(Square(0, y))) { break; }
    fixed.Set(Square(0, y));
  }

  for (int y = 7; y >= 1; y--) {
    if (!bitboard.Get(Square(7, y))) { break; }
    fixed.Set(Square(7, y));
  }

  return fixed.Count();
}

} // namespace beluga