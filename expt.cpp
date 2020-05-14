#include "search.h"
#include <set>
#include <iostream>
#include <random>

using namespace beluga;

std::mt19937 r(static_cast<unsigned>(time(nullptr)));

void expt(int gameCount);

int main(int, const char**, const char**) {
  expt(200000);

  return 0;
}

void expt(int gameCount) {
  std::set<Board> boards;
  int duplicate = 0;

  for (int i = 0; i < gameCount; i++) {
    std::cout << "\rgenerating...(" << (i + 1) << "/" << gameCount << ")" << std::flush;

    Board board = Board::GetNormalInitBoard();

    while (!board.IsEnd() && (board.GetBlackBoard() | board.GetWhiteBoard()).Count() < 12) {
      if (board.MustPass()) {
        board.Pass();
        continue;
      }

      auto moves = board.GenerateMoves();
      std::uniform_int_distribution<int32_t> d(0, moves.Count() - 1); 
      Square move; 
      for (auto index = d(r); index >= 0; index--) { 
        move = moves.Pick();
      }
      board.DoMove(move);
    }

    if (boards.find(board) != boards.end()) {
      duplicate++;
    } else {
      boards.insert(board);
    }
  }
  std::cout << "\rgenerating...done                 " << std::endl;
  std::cout << "duplicate count: " << duplicate << std::endl;
}
