#include "search.h"
#include <algorithm>

#define ROOT_MOVE_SHUFFLE 1
#define TT                1
#define TT_MOVE           1
#define NEGA_SCOUT        1
#define PROBCUT           1

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

SearchResult Searcher::Search(const Board& board, int maxDepth, int endingDepth) {
  if (board.MustPass()) {
    return { Square::Invalid(), 0 };
  }

  Tree tree;
  tree.ply = 0;
  tree.board = board;
  tree.nodes = 0;

  Node& node = tree.stack[0];

  Bitboard occupied = board.GetBlackBoard() | board.GetWhiteBoard();
  Bitboard empty = ~occupied;

  // ending search
  if (empty.Count() <= endingDepth) {
    SearchEnding(tree, -64 * ScoreScale, 64 * ScoreScale, false);

    // sort by score value
    std::stable_sort(node.moves, node.moves + node.nmoves, [](const Move& lhs, const Move& rhs) {
      return lhs.score > rhs.score;
    });

    if (handler_ != nullptr) {
      handler_->OnEnding(node.pv, node.moves[0].score, tree.nodes);
    }

    return { node.moves[0].move, node.moves[0].score, true };
  }

  node.pv.Clear();

  GenerateMoves(tree, Square::Invalid(), 0, 0, 0);

#if ROOT_MOVE_SHUFFLE
  std::shuffle(node.moves, node.moves + node.nmoves, random_);
#endif

  for (int depth = DepthOnePly; depth < maxDepth + DepthOnePly; depth += DepthOnePly) {
    // clear score values
    for (int mi = 1; mi < node.nmoves; mi++) {
      node.moves[mi].score = -ScoreInfinity;
    }

    if (depth == DepthOnePly) {
      // initial depth
      Search(tree, depth, -ScoreInfinity, ScoreInfinity, false);

    } else {
      // aspiration search
      Score delta = 8 * ScoreScale;
      Score alpha = node.moves[0].score - delta;
      Score beta  = node.moves[0].score + delta;
      while (true) {
        Score score = Search(tree, depth, alpha, beta, false);
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
        delta += 10 * ScoreScale;
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

  return { node.moves[0].move, node.moves[0].score, false };
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

Score Searcher::SearchEnding(Tree& tree, Score alpha, Score beta, bool passed) {
  tree.nodes++;

  Node& node = tree.stack[tree.ply];
  node.pv.Clear();

  GenerateEndingMoves(tree, alpha, beta);

  // pass
  if (node.nmoves == 0) {
    if (passed) {
      int bc      = tree.board.GetBlackBoard().Count();
      int wc      = tree.board.GetWhiteBoard().Count();
      Score score = (bc - wc) * ScoreScale;
      return tree.board.GetNextDisk() == ColorBlack ? score : -score;
    }

    tree.board.Pass();
    Score score = -SearchEnding(tree, -beta, -alpha, true);
    tree.board.Pass();
    return score;
  }

  Score bestScore = -ScoreInfinity;

  while (true) {
    if (node.mi >= node.nmoves) {
      break;
    }
    Move& m = node.moves[node.mi++];

    Score newAlpha = ScoreMax(alpha, bestScore);

    Bitboard mask = tree.board.DoMove(m.move);
    tree.ply++;
    m.score = -SearchEnding(tree, -beta, -newAlpha, false);
    tree.board.UndoMove(m.move, mask);
    tree.ply--;

    if (stop_.load()) {
      return 0;
    }

    if (m.score > bestScore) {
      bestScore = m.score;
      Node& child = tree.stack[tree.ply + 1];
      node.pv.Set(m.move, child.pv);
      if (bestScore >= beta) {
        break;
      }
    }
  }

  return bestScore;
}

Score Searcher::Search(Tree& tree, int depth, Score alpha, Score beta, bool passed) {
  tree.nodes++;

  Node& node = tree.stack[tree.ply];
  if (tree.ply != 0) {
    node.pv.Clear();
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

#if PROBCUT
  if (tree.ply != 0 && depth >= 5 * DepthOnePly && beta < 40 * ScoreScale) {
    int pbeta = beta + 10 * ScoreScale;
    int pdepth = depth - 1 * DepthOnePly;
    Score score = Search(tree, pdepth, pbeta - 1, pbeta, false);
    if (score >= pbeta) {
      return beta;
    }
  }
#endif

  if (tree.ply == 0) {
    node.mi = 0;
  } else {
    GenerateMoves(tree, ttMove, depth, alpha, beta);
  }

  // pass
  if (node.nmoves == 0) {
    if (passed) {
      int bc      = tree.board.GetBlackBoard().Count();
      int wc      = tree.board.GetWhiteBoard().Count();
      Score score = (bc - wc) * ScoreScale;
      return tree.board.GetNextDisk() == ColorBlack ? score : -score;
    }

    tree.board.Pass();
    Score score = -Search(tree, depth, -beta, -alpha, true);
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
      m.score = -Search(tree, newDepth, -beta, -newAlpha, false);
#if NEGA_SCOUT
    } else {
      m.score = -Search(tree, newDepth, -(newAlpha + 1), -newAlpha, false);
      if (m.score >= newAlpha + 1) {
        m.score = -Search(tree, newDepth, -beta, -newAlpha, false);
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

void Searcher::GenerateEndingMoves(Tree& tree, Score alpha, Score beta) {
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

  int newDepth = depth <= DepthOnePly * 4 ? DepthOnePly
               : depth <= DepthOnePly * 7 ? depth - DepthOnePly * 4
                                          : DepthOnePly * 3;
  for (int mi = 0; mi < node.nmoves; mi++) {
#if TT_MOVE
    if (node.moves[mi].move == ttMove) {
      node.moves[mi].score = ScoreInfinity;
      continue;
    }
#endif
    Bitboard mask = tree.board.DoMove(node.moves[mi].move);
    tree.ply++;
    node.moves[mi].score = -Search(tree, newDepth, -beta, -alpha, false);
    tree.board.UndoMove(node.moves[mi].move, mask);
    tree.ply--;
  }
  
  std::sort(node.moves, node.moves + node.nmoves, [](const Move& lhs, const Move& rhs) {
    return lhs.score > rhs.score;
  });
}

inline int linear(int begin, int end, int count, int maxCount) {
  return begin + (end - begin) * count / maxCount;
}

Score Searcher::Evaluate(const Board& board) {
  Score score = 0;
  Bitboard black = board.GetBlackBoard();
  Bitboard white = board.GetWhiteBoard();

  Bitboard occupied = black | white;
  Bitboard empty = ~occupied;
  Bitboard outerMask = empty.Up()     | empty.Down()     | empty.Left()    | empty.Right()
                     | empty.LeftUp() | empty.LeftDown() | empty.RightUp() | empty.RightDown();
  Bitboard innerMask = ~outerMask;
  int count = occupied.Count();

  Bitboard blackMovable = board.GenerateMoves(ColorBlack);
  Bitboard whiteMovable = board.GenerateMoves(ColorWhite);

  // Danger X
  const Score DangerX = linear(-800, 0,  count, 64);
  Bitboard emptyCorner = empty & Bitboard::MaskCorner();
  Bitboard dangerXMask = (emptyCorner.LeftUp() | emptyCorner.LeftDown() | emptyCorner.RightUp() | emptyCorner.RightDown());
  score += (black & dangerXMask).Count() * DangerX;
  score -= (white & dangerXMask).Count() * DangerX;

  // Safe X
  const Score SafeX = linear(500, 0,  count, 64);
  Bitboard safeXMask = dangerXMask ^ Bitboard::MaskX();
  score += (black & safeXMask).Count() * SafeX;
  score -= (white & safeXMask).Count() * SafeX;

  // Danger C
  const Score DangerC = linear(-500, 0,  count, 64);
  Bitboard emptyA = empty & Bitboard::MaskA();
  Bitboard dangerCMask = (emptyCorner.Left() | emptyCorner.Right() | emptyCorner.Up() | emptyCorner.Down())
                       & (emptyA.Left() | emptyA.Right() | emptyA.Up() | emptyA.Down());
  score += (black & dangerCMask).Count() * DangerC;
  score -= (white & dangerCMask).Count() * DangerC;

  // Inner (Box)
  const Score InnerScore1 = linear(-200, 100, count, 64);
  score += (black & innerMask & Bitboard::MaskBox()).Count() * InnerScore1;
  score -= (white & innerMask & Bitboard::MaskBox()).Count() * InnerScore1;

  // Innner (others)
  const Score InnerScore2 = linear(-200, 100, count, 64);
  score += (black & innerMask & ~Bitboard::MaskBox()).Count() * InnerScore2;
  score -= (white & innerMask & ~Bitboard::MaskBox()).Count() * InnerScore2;

  // Outer
  const Score OuterScore = linear(-200, 0, count, 64);
  score += (black & outerMask).Count() * OuterScore;
  score -= (white & outerMask).Count() * OuterScore;

  // Fixed
  const Score FixedDiskScore = linear(1000, 0, count, 64);
  score += CountFixedDisk(black) * FixedDiskScore;
  score -= CountFixedDisk(white) * FixedDiskScore;

  // Movable Count
  const Score MovableCountScore = linear(300, 100, count, 64);
  score += blackMovable.Count() * MovableCountScore;
  score -= whiteMovable.Count() * MovableCountScore;

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