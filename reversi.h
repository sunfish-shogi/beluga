#pragma once

#include <cstdint>

namespace beluga {

enum Direction : int {
  LeftUp = 0,
  Up,
  RightUp,
  Left,
  Right,
  LeftDown,
  Down,
  RightDown,
};

extern const int DirDelta[8];
extern const bool DirWall[8][64];
extern LPCTSTR SquareStrings[64];

class Square {
public:

  Square() noexcept = default;
  Square(const Square& src) noexcept = default;
  Square(Square&& src) noexcept = default;
  Square(int raw) noexcept : raw_(raw) {}
  Square(int x, int y) noexcept : raw_(y * 8 + x) {}

  Square& operator=(const Square&) = default;
  Square& operator=(Square&&) = default;

  bool operator==(const Square& rhs) {
    return raw_ == rhs.raw_;
  }

  bool operator!=(const Square& rhs) {
    return raw_ != rhs.raw_;
  }

  int GetX() const {
      return raw_ % 8;
  }

  int GetY() const {
      return raw_ / 8;
  }

  int GetRaw() const {
      return raw_;
  }

  Square Dir(Direction dir) const {
      return Square(raw_ + DirDelta[dir]);
  }

  bool IsWall(Direction dir) const {
    return DirWall[dir][raw_];
  }

  Square Prev() const {
      return Square(raw_ - 1);
  }

  Square Next() const {
      return Square(raw_ + 1);
  }

  static Square Begin() {
      return Square(0);
  }

  static Square End() {
    return Square(64);
  }

  static Square Invalid() {
      return Square(-1);
  }

  bool IsInvalid() const {
      return raw_ == -1;
  }

  LPCTSTR ToString() const {
    if (raw_ < 0 || raw_ > 63) {
      return L"NA";
    }
    return SquareStrings[raw_];
  }

private:

  int raw_;

};

class Bitboard {
public:

  Bitboard() = default;
  Bitboard(const Bitboard& src) = default;
  Bitboard(Bitboard&&) = default;
  Bitboard(uint64_t raw) : raw_(raw) {}

  Bitboard& operator=(const Bitboard&) = default;
  Bitboard& operator=(Bitboard&&) = default;

  bool operator==(const Bitboard& rhs) {
    return raw_ == rhs.raw_;
  }

  bool operator!=(const Bitboard& rhs) {
    return raw_ != rhs.raw_;
  }

  Bitboard operator&=(const Bitboard& rhs) {
    raw_ &= rhs.raw_;
    return *this;
  }

  Bitboard operator&(const Bitboard& rhs) const {
    return Bitboard(raw_ & rhs.raw_);
  }

  Bitboard operator|=(const Bitboard& rhs) {
    raw_ |= rhs.raw_;
    return *this;
  }

  Bitboard operator|(const Bitboard& rhs) const {
    return Bitboard(raw_ | rhs.raw_);
  }

  Bitboard operator^=(const Bitboard& rhs) {
    raw_ ^= rhs.raw_;
    return *this;
  }

  Bitboard operator^(const Bitboard& rhs) const {
    return Bitboard(raw_ ^ rhs.raw_);
  }

  Bitboard operator~() const {
    return Bitboard(~raw_);
  }

  Bitboard LeftUp() const {
    return Bitboard(raw_ >> 9) & MaskCol1to7();
  }

  Bitboard Up() const {
    return Bitboard(raw_ >> 8);
  }

  Bitboard RightUp() const {
    return Bitboard(raw_ >> 7) & MaskCol2to8();
  }

  Bitboard Left() const {
    return Bitboard(raw_ >> 1) & MaskCol1to7();
  }

  Bitboard Right() const {
    return Bitboard(raw_ << 1) & MaskCol2to8();
  }

  Bitboard LeftDown() const {
    return Bitboard(raw_ << 7) & MaskCol1to7();
  }

  Bitboard Down() const {
    return Bitboard(raw_ << 8);
  }

  Bitboard RightDown() const {
    return Bitboard(raw_ << 9) & MaskCol2to8();
  }

  static Bitboard MaskCol1to7() {
    return 0b01111111,01111111,01111111,01111111,01111111,01111111,01111111,01111111;
  }

  static Bitboard MaskCol2to8() {
    return 0b11111110,11111110,11111110,11111110,11111110,11111110,11111110,11111110;
  }

  int Count() const {
    uint64_t x = raw_;
    x = x - ((x >> 1) & 0x5555555555555555llu);
    x = (x & 0x3333333333333333llu) + ((x >> 2) & 0x3333333333333333llu);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Fllu;
    x = x + (x >> 8);
    x = x + (x >> 16);
    x = x + (x >> 32);
    return static_cast<int>(x & 0x000000000000007Fllu);
  }

  bool Get(const Square& square) const {
    return (raw_ & (1LLU << square.GetRaw())) != 0LLU;
  }

  void Set(const Square& square) {
    raw_ |= 1LLU << square.GetRaw();
  }

  void Unset(const Square& square) {
    raw_ &= ~(1LLU << square.GetRaw());
  }

  uint64_t GetRaw() const {
    return raw_;
  }

private:

  uint64_t raw_;

};

enum DiskColor {
  ColorNone,
  ColorBlack,
  ColorWhite,
};

enum Winner {
  BlackWon = ColorBlack,
  WhiteWon = ColorWhite,
  Draw  = 255,
};

struct TotalScore {
  int  black;
  int  white;
  Winner winner;
};

class Board {
public:

  Board() = default;
  Board(const Board& src) = default;
  Board(Board&& src) = default;

  Board& operator=(const Board&) = default;
  Board& operator=(Board&&) = default;

  bool operator==(const Board& rhs) {
    return black_ == rhs.black_
        && white_ == rhs.white_
        && nextDisk_ == rhs.nextDisk_;
  }

  static Board GetEmptyBoard() {
    Board board;
    board.black_ = Bitboard{0LLU};
    board.white_ = Bitboard{0LLU};
    board.nextDisk_ = ColorBlack;
    return board;
  }

  static Board GetNormalInitBoard() {
    Board board = GetEmptyBoard();
    board.SetWhite(Square(3, 3));
    board.SetBlack(Square(3, 4));
    board.SetBlack(Square(4, 3));
    board.SetWhite(Square(4, 4));
    return board;
  }

  void Set(const Square& square, DiskColor disk) {
    switch (disk) {
    case ColorBlack:
        black_.Set(square);
    case ColorWhite:
        white_.Set(square);
    }
  }

  DiskColor Get(const Square& square) const {
    if (black_.Get(square)) {
        return ColorBlack;
    } else if (white_.Get(square)) {
        return ColorWhite;
    }
    return ColorNone;
  }

  Bitboard GetBlackBoard() const {
    return black_;
  }

  Bitboard GetWhiteBoard() const {
    return white_;
  }

  DiskColor GetNextDisk() const {
    return nextDisk_;
  }

  void SetBlack(const Square& square) {
    black_.Set(square);
  }

  void SetWhite(const Square& square) {
    white_.Set(square);
  }

  void UnsetBlack(const Square& square) {
    black_.Set(square);
  }

  void UnsetWhite(const Square& square) {
    white_.Set(square);
  }

  void Reverse(const Bitboard& mask) {
    black_ ^= mask;
    white_ ^= mask;
  }

  bool CanMove(const Square& square) const;

  bool IsEnd() const;

  Bitboard DoMove(const Square& square);

  void UndoMove(const Square& square, const Bitboard& mask);

  void Pass();

  Bitboard GenerateMoves() const;

  TotalScore GetTotalScore() const;

  uint64_t GetHash() const;

private:

  bool CanMove(const Square& square, DiskColor color) const;

  Bitboard black_;
  Bitboard white_;
  DiskColor nextDisk_;

};

} // namespace beluga
