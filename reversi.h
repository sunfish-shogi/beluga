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
extern const char* SquareStrings[64];

class Square {
public:

  using RawType = int8_t;

  Square() noexcept = default;
  Square(const Square& src) noexcept = default;
  Square(Square&& src) noexcept = default;
  Square(RawType raw) noexcept : raw_(raw) {}
  Square(RawType x, RawType y) noexcept : raw_(y * 8 + x) {}

  Square& operator=(const Square&) = default;
  Square& operator=(Square&&) = default;

  constexpr bool operator==(const Square& rhs) const {
    return raw_ == rhs.raw_;
  }

  constexpr bool operator!=(const Square& rhs) const {
    return raw_ != rhs.raw_;
  }

  constexpr RawType GetX() const {
    return raw_ % 8;
  }

  constexpr RawType GetY() const {
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

  constexpr bool IsInvalid() const {
    return (*this) == Invalid();
  }

  const char* ToString() const {
    if (raw_ < 0 || raw_ > 63) {
      return "NA";
    }
    return SquareStrings[raw_];
  }

private:

  RawType raw_;

};

class Bitboard {
public:

  Bitboard() = default;
  constexpr Bitboard(const Bitboard& src) = default;
  Bitboard(Bitboard&&) = default;
  constexpr Bitboard(uint64_t raw) : raw_(raw) {}

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

  constexpr Bitboard operator&(const Bitboard& rhs) const {
    return Bitboard(raw_ & rhs.raw_);
  }

  Bitboard operator|=(const Bitboard& rhs) {
    raw_ |= rhs.raw_;
    return *this;
  }

  constexpr Bitboard operator|(const Bitboard& rhs) const {
    return Bitboard(raw_ | rhs.raw_);
  }

  Bitboard operator^=(const Bitboard& rhs) {
    raw_ ^= rhs.raw_;
    return *this;
  }

  constexpr Bitboard operator^(const Bitboard& rhs) const {
    return Bitboard(raw_ ^ rhs.raw_);
  }

  constexpr Bitboard operator~() const {
    return Bitboard(~raw_);
  }

  constexpr Bitboard LeftUp() const {
    return Bitboard(raw_ >> 9) & MaskCol1to7();
  }

  constexpr Bitboard Up() const {
    return Bitboard(raw_ >> 8);
  }

  constexpr Bitboard RightUp() const {
    return Bitboard(raw_ >> 7) & MaskCol2to8();
  }

  constexpr Bitboard Left() const {
    return Bitboard(raw_ >> 1) & MaskCol1to7();
  }

  constexpr Bitboard Right() const {
    return Bitboard(raw_ << 1) & MaskCol2to8();
  }

  constexpr Bitboard LeftDown() const {
    return Bitboard(raw_ << 7) & MaskCol1to7();
  }

  constexpr Bitboard Down() const {
    return Bitboard(raw_ << 8);
  }

  constexpr Bitboard RightDown() const {
    return Bitboard(raw_ << 9) & MaskCol2to8();
  }

  static constexpr Bitboard MaskCol1to7() {
    return 0b0111111101111111011111110111111101111111011111110111111101111111;
  }

  static constexpr Bitboard MaskCol2to8() {
    return 0b1111111011111110111111101111111011111110111111101111111011111110;
  }

  static constexpr Bitboard MaskBox() {
    return 0b0000000000000000001111000011110000111100001111000000000000000000;
  }

  static constexpr Bitboard MaskInnerSide() {
    return 0b0000000000111100010000100100001001000010010000100011110000000000;
  }

  static constexpr Bitboard MaskSide() {
    return 0b0111111010000001100000011000000110000001100000011000000101111110;
  }

  static constexpr Bitboard MaskX() {
    return 0b0000000001000010000000000000000000000000000000000100001000000000;
  }

  static constexpr Bitboard MaskCorner() {
    return 0b1000000100000000000000000000000000000000000000000000000010000001;
  }

  static constexpr Bitboard MaskA() {
    return 0b0010010000000000100000010000000000000000100000010000000000100100;
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

  Square Pick() {
    if (raw_ == 0) {
      return Square::Invalid();
    }
    uint64_t mask = raw_ - 1;
    raw_ &= mask;
    return Square(Bitboard(raw_ ^ mask).Count());
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

  void SetNextDisk(DiskColor nextDisk) {
    nextDisk_ = nextDisk;
  }

  bool CanMove(const Square& square) const;

  bool IsEnd() const;

  bool MustPass() const;

  Bitboard DoMove(const Square& square);

  void UndoMove(const Square& square, const Bitboard& mask);

  void Pass();

  Bitboard GenerateMoves() const;

  Bitboard GenerateMoves(DiskColor color) const;

  TotalScore GetTotalScore() const;

  uint64_t GetHash() const;

private:

  bool CanMove(const Square& square, DiskColor color) const;

  Bitboard GetOpenSquares(DiskColor color) const;

  Bitboard black_;
  Bitboard white_;
  DiskColor nextDisk_;

};

} // namespace beluga
