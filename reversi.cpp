#include "stdafx.h"
#include "reversi.h"
#include "zobrist.h"

namespace beluga {

const int DirDelta[8] = {
  /* LeftUp    */ -9,
  /* Up        */ -8,
  /* RightUp   */ -7,
  /* Left      */ -1,
  /* Right     */  1,
  /* LeftDown  */  7,
  /* Down      */  8,
  /* RightDown */  9,
};

const bool DirWall[8][64] = {
  { // LeftUp
    /* 0 */ true , true , true , true , true , true , true , true ,
    /* 1 */ true , false, false, false, false, false, false, false,
    /* 2 */ true , false, false, false, false, false, false, false,
    /* 3 */ true , false, false, false, false, false, false, false,
    /* 4 */ true , false, false, false, false, false, false, false,
    /* 5 */ true , false, false, false, false, false, false, false,
    /* 6 */ true , false, false, false, false, false, false, false,
    /* 7 */ true , false, false, false, false, false, false, false,
  }, { // Up
    /* 0 */ true , true , true , true , true , true , true , true ,
    /* 1 */ false, false, false, false, false, false, false, false,
    /* 2 */ false, false, false, false, false, false, false, false,
    /* 3 */ false, false, false, false, false, false, false, false,
    /* 4 */ false, false, false, false, false, false, false, false,
    /* 5 */ false, false, false, false, false, false, false, false,
    /* 6 */ false, false, false, false, false, false, false, false,
    /* 7 */ false, false, false, false, false, false, false, false,
  }, { // RightUp
    /* 0 */ true , true , true , true , true , true , true , true ,
    /* 1 */ false, false, false, false, false, false, false, true ,
    /* 2 */ false, false, false, false, false, false, false, true ,
    /* 3 */ false, false, false, false, false, false, false, true ,
    /* 4 */ false, false, false, false, false, false, false, true ,
    /* 5 */ false, false, false, false, false, false, false, true ,
    /* 6 */ false, false, false, false, false, false, false, true ,
    /* 7 */ false, false, false, false, false, false, false, true ,
  },  { // Left
    /* 0 */ true , false, false, false, false, false, false, false,
    /* 1 */ true , false, false, false, false, false, false, false,
    /* 2 */ true , false, false, false, false, false, false, false,
    /* 3 */ true , false, false, false, false, false, false, false,
    /* 4 */ true , false, false, false, false, false, false, false,
    /* 5 */ true , false, false, false, false, false, false, false,
    /* 6 */ true , false, false, false, false, false, false, false,
    /* 7 */ true , false, false, false, false, false, false, false,
  },  { // Right
    /* 0 */ false, false, false, false, false, false, false, true ,
    /* 1 */ false, false, false, false, false, false, false, true ,
    /* 2 */ false, false, false, false, false, false, false, true ,
    /* 3 */ false, false, false, false, false, false, false, true ,
    /* 4 */ false, false, false, false, false, false, false, true ,
    /* 5 */ false, false, false, false, false, false, false, true ,
    /* 6 */ false, false, false, false, false, false, false, true ,
    /* 7 */ false, false, false, false, false, false, false, true ,
  }, { // LeftDown
    /* 0 */ true , false, false, false, false, false, false, false,
    /* 1 */ true , false, false, false, false, false, false, false,
    /* 2 */ true , false, false, false, false, false, false, false,
    /* 3 */ true , false, false, false, false, false, false, false,
    /* 4 */ true , false, false, false, false, false, false, false,
    /* 5 */ true , false, false, false, false, false, false, false,
    /* 6 */ true , false, false, false, false, false, false, false,
    /* 7 */ true , true , true , true , true , true , true , true ,
  }, { // Down
    /* 0 */ false, false, false, false, false, false, false, false,
    /* 1 */ false, false, false, false, false, false, false, false,
    /* 2 */ false, false, false, false, false, false, false, false,
    /* 3 */ false, false, false, false, false, false, false, false,
    /* 4 */ false, false, false, false, false, false, false, false,
    /* 5 */ false, false, false, false, false, false, false, false,
    /* 6 */ false, false, false, false, false, false, false, false,
    /* 7 */ true , true , true , true , true , true , true , true ,
  }, { // RightDown
    /* 0 */ false, false, false, false, false, false, false, true ,
    /* 1 */ false, false, false, false, false, false, false, true ,
    /* 2 */ false, false, false, false, false, false, false, true ,
    /* 3 */ false, false, false, false, false, false, false, true ,
    /* 4 */ false, false, false, false, false, false, false, true ,
    /* 5 */ false, false, false, false, false, false, false, true ,
    /* 6 */ false, false, false, false, false, false, false, true ,
    /* 7 */ true , true , true , true , true , true , true , true ,
  },
};

extern LPCTSTR SquareStrings[64] = {
  L"a1", L"b1", L"c1", L"d1", L"e1", L"f1", L"g1", L"h1",
  L"a2", L"b2", L"c2", L"d2", L"e2", L"f2", L"g2", L"h2",
  L"a3", L"b3", L"c3", L"d3", L"e3", L"f3", L"g3", L"h3",
  L"a4", L"b4", L"c4", L"d4", L"e4", L"f4", L"g4", L"h4",
  L"a5", L"b5", L"c5", L"d5", L"e5", L"f5", L"g5", L"h5",
  L"a6", L"b6", L"c6", L"d6", L"e6", L"f6", L"g6", L"h6",
  L"a7", L"b7", L"c7", L"d7", L"e7", L"f7", L"g7", L"h7",
  L"a8", L"b8", L"c8", L"d8", L"e8", L"f8", L"g8", L"h8",
};

bool Board::CanMove(const Square& square) const {
  if (Get(square) != ColorNone) {
    return false;
  }

  return CanMove(square, nextDisk_);
}

bool Board::CanMove(const Square& square, DiskColor color) const {
  if (color == ColorBlack) {
    for (int di = 0; di < 8; di++) {
      Direction dir = (Direction)di;
      bool reversible = false;

      if (square.IsWall(dir)) {
        continue;
      }

      for (Square sq = square.Dir(dir); ; sq = sq.Dir(dir)) {
        if (white_.Get(sq)) {
          reversible = true;
        } else if (reversible && black_.Get(sq)) {
          return true;
        } else {
          break;
        }

        if (sq.IsWall(dir)) {
          break;
        }
      }
    }
  } else {
    for (int di = 0; di < 8; di++) {
      Direction dir = (Direction)di;
      bool reversible = false;

      if (square.IsWall(dir)) {
        continue;
      }

      for (Square sq = square.Dir(dir); ; sq = sq.Dir(dir)) {
        if (black_.Get(sq)) {
          reversible = true;
        } else if (reversible && white_.Get(sq)) {
          return true;
        } else {
          break;
        }

        if (sq.IsWall(dir)) {
          break;
        }
      }
    }
  }
  return false;
}

Bitboard Board::DoMove(const Square& square) {
  Bitboard mask(0);
  for (int di = 0; di < 8; di++) {
    Direction dir = (Direction)di;
    Bitboard m(0);

    if (square.IsWall(dir)) {
      continue;
    }

    for (Square sq = square.Dir(dir); ; sq = sq.Dir(dir)) {
      if (nextDisk_ == ColorBlack) {
        if (white_.Get(sq)) {
          m.Set(sq);
        } else {
          if (black_.Get(sq)) {
            Reverse(m);
            mask |= m;
          }
          break;
        }

      } else {
        if (black_.Get(sq)) {
          m.Set(sq);
        } else {
          if (white_.Get(sq)) {
            Reverse(m);
            mask |= m;
          }
          break;
        }
      }

      if (sq.IsWall(dir)) {
        break;
      }
    }
  }

  if (nextDisk_ == ColorBlack) {
    black_.Set(square);
    nextDisk_ = ColorWhite;
  } else {
    white_.Set(square);
    nextDisk_ = ColorBlack;
  }

  return mask;
}

void Board::UndoMove(const Square& square, const Bitboard& mask) {
  Reverse(mask);
  if (nextDisk_ == ColorBlack) {
    white_.Unset(square);
    nextDisk_ = ColorWhite;
  } else {
    black_.Unset(square);
    nextDisk_ = ColorBlack;
  }
}

bool Board::IsEnd() const {
  Bitboard open = GetOpenSquares(ColorWhite);
  for (Square square = open.Pick(); !square.IsInvalid(); square = open.Pick()) {
    if (CanMove(square, ColorBlack)) {
      return false;
    }
  }
  open = GetOpenSquares(ColorBlack);
  for (Square square = open.Pick(); !square.IsInvalid(); square = open.Pick()) {
    if (CanMove(square, ColorWhite)) {
      return false;
    }
  }
  return true;
}

bool Board::MustPass() const {
  Bitboard open = GetOpenSquares(nextDisk_ == ColorBlack ? ColorWhite : ColorBlack);
  for (Square square = open.Pick(); !square.IsInvalid(); square = open.Pick()) {
    if (CanMove(square, nextDisk_)) {
      return false;
    }
  }
  return true;
}

void Board::Pass() {
  nextDisk_ = nextDisk_ == ColorBlack ? ColorWhite : ColorBlack;
}

Bitboard Board::GenerateMoves() const {
  return GenerateMoves(nextDisk_);
}

Bitboard Board::GenerateMoves(DiskColor color) const {
  Bitboard moves = Bitboard(0);
  Bitboard open = GetOpenSquares(color == ColorBlack ? ColorWhite : ColorBlack);
  for (Square square = open.Pick(); !square.IsInvalid(); square = open.Pick()) {
    if (CanMove(square, color)) {
      moves.Set(square);
    }
  }
  return moves;
}

Bitboard Board::GetOpenSquares(DiskColor color) const {
  Bitboard occupied = black_ | white_;
  Bitboard empty = ~occupied;
  Bitboard open = color == ColorBlack
                ? (black_.Up() | black_.Down() | black_.Left() | black_.Right()
                 | black_.LeftUp() | black_.LeftDown() | black_.RightUp() | black_.RightDown()) & empty
                : (white_.Up() | white_.Down() | white_.Left() | white_.Right()
                 | white_.LeftUp() | white_.LeftDown() | white_.RightUp() | white_.RightDown()) & empty;
  return open;
}

TotalScore Board::GetTotalScore() const {
  TotalScore score;
  score.black = black_.Count();
  score.white = white_.Count();
  if (score.black > score.white) {
    score.winner = BlackWon;
  } else if (score.black < score.white) {
    score.winner = WhiteWon;
  } else {
    score.winner = Draw;
  }
  return score;
}

uint64_t Board::GetHash() const {
  uint64_t hash = ZobristTable1[black_.GetRaw() & 0x0f];
  hash ^= ZobristTable2 [(black_.GetRaw() >> 4 ) & 0x0f];
  hash ^= ZobristTable3 [(black_.GetRaw() >> 8 ) & 0x0f];
  hash ^= ZobristTable4 [(black_.GetRaw() >> 12) & 0x0f];
  hash ^= ZobristTable5 [(black_.GetRaw() >> 16) & 0x0f];
  hash ^= ZobristTable6 [(black_.GetRaw() >> 20) & 0x0f];
  hash ^= ZobristTable7 [(black_.GetRaw() >> 24) & 0x0f];
  hash ^= ZobristTable8 [(black_.GetRaw() >> 28) & 0x0f];
  hash ^= ZobristTable9 [(black_.GetRaw() >> 32) & 0x0f];
  hash ^= ZobristTable10[(black_.GetRaw() >> 36) & 0x0f];
  hash ^= ZobristTable11[(black_.GetRaw() >> 40) & 0x0f];
  hash ^= ZobristTable12[(black_.GetRaw() >> 44) & 0x0f];
  hash ^= ZobristTable13[(black_.GetRaw() >> 48) & 0x0f];
  hash ^= ZobristTable14[(black_.GetRaw() >> 52) & 0x0f];
  hash ^= ZobristTable15[(black_.GetRaw() >> 56) & 0x0f];
  hash ^= ZobristTable16[(black_.GetRaw() >> 60)       ];
  hash ^= ZobristTable17[(white_.GetRaw()      ) & 0x0f];
  hash ^= ZobristTable18[(white_.GetRaw() >> 4 ) & 0x0f];
  hash ^= ZobristTable19[(white_.GetRaw() >> 8 ) & 0x0f];
  hash ^= ZobristTable20[(white_.GetRaw() >> 12) & 0x0f];
  hash ^= ZobristTable21[(white_.GetRaw() >> 16) & 0x0f];
  hash ^= ZobristTable22[(white_.GetRaw() >> 20) & 0x0f];
  hash ^= ZobristTable23[(white_.GetRaw() >> 24) & 0x0f];
  hash ^= ZobristTable24[(white_.GetRaw() >> 28) & 0x0f];
  hash ^= ZobristTable25[(white_.GetRaw() >> 32) & 0x0f];
  hash ^= ZobristTable26[(white_.GetRaw() >> 36) & 0x0f];
  hash ^= ZobristTable27[(white_.GetRaw() >> 40) & 0x0f];
  hash ^= ZobristTable28[(white_.GetRaw() >> 44) & 0x0f];
  hash ^= ZobristTable29[(white_.GetRaw() >> 48) & 0x0f];
  hash ^= ZobristTable30[(white_.GetRaw() >> 52) & 0x0f];
  hash ^= ZobristTable31[(white_.GetRaw() >> 56) & 0x0f];
  hash ^= ZobristTable32[(white_.GetRaw() >> 60)       ];
  return hash;
}

} // namespace beluga
