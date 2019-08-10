#include "search.h"
#include <algorithm>

#define ROOT_MOVE_SHUFFLE 1
#define TT                1
#define TT_MOVE           1
#define NEGA_SCOUT        1
#define PROBCUT           1

namespace beluga {

Searcher::Searcher(const std::shared_ptr<Evaluator>& eval, SearchHandler* handler)
  : eval_(eval), handler_(handler)
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
    return { Square::Invalid(), 0 , false };
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
    Score score = eval_->Evaluate(tree.board);
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

} // namespace beluga
