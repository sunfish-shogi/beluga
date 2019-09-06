#include "search.h"
#include <list>
#include <set>
#include <iostream>
#include <random>
#include <cmath>

using namespace beluga;

std::mt19937 r(static_cast<unsigned>(time(nullptr)));

void Learn(std::shared_ptr<Evaluator> eval, int gameCount, int depth, int endingDepth, int updateCount);

int main(int, const char**, const char**) {
  std::shared_ptr<Evaluator> eval(new Evaluator);

  auto err = eval->LoadParam();
  if (err != nullptr) {
    std::cerr << err << std::endl;
    std::cerr << "eval.bin has not loaded, and initialized by zero" << std::endl;
    eval->InitZero();
  }

  for (int i = 0; i < 10; i++) {
    Learn(eval, 100000, 3, 10, 256);
  }

  return 0;
}

struct Sample {
  Board board;
  Score score;
};

void Learn(std::shared_ptr<Evaluator> eval, int gameCount, int depth, int endingDepth, int updateCount) {
  std::cout << "begin Learn" << std::endl;
  std::cout << "gameCount  : " << gameCount << std::endl;
  std::cout << "updateCount: " << updateCount << std::endl;

  // generate samples
  std::list<Sample> samples;
  std::set<Board> bset;

  Searcher searcher(eval);
  for (int i = 0; i < gameCount; ) {
    std::cout << "\rgenerating samples...(" << (i + 1) << "/" << gameCount << ")" << std::flush;

    Board board = Board::GetNormalInitBoard();
    std::list<Board> boards;

    while (!board.IsEnd() && (board.GetBlackBoard() | board.GetWhiteBoard()).Count() < 12) {
      auto moves = board.GenerateMoves();
      std::uniform_int_distribution<int32_t> d(0, moves.Count() - 1);
      Square move;
      for (auto index = d(r); index >= 0; index--) {
        move = moves.Pick();
      }
      board.DoMove(move);
    }

    if (bset.find(board) != bset.end()) {
      continue;
    } else {
      bset.insert(board);
    }

    while (!board.IsEnd()) {
      if (board.MustPass()) {
        board.Pass();
        continue;
      }

      auto count = (board.GetBlackBoard() | board.GetWhiteBoard()).Count();
      if (count < 64 - endingDepth) {
        boards.push_back(board);
      }

      auto searchResult = searcher.Search(board, depth, endingDepth);
      board.DoMove(searchResult.move);
    }

    Score score = (board.GetBlackBoard().Count() - board.GetWhiteBoard().Count()) * ScoreScale;
    for (auto b : boards) {
      samples.push_back({ b, score });
    }

    i++;
  }
  std::cout << "\rgenerating samples...done                 " << std::endl;

  // adjust
  std::cout << "adjusting..." << std::flush;
  std::uniform_int_distribution<Score> s(0, 1);
  for (int bi = 0; bi < updateCount; bi++) {
    float lossSum = 0.0f;
    int lossCount = 0;
    Gradient gradient;
    for (auto sample : samples) {
        auto staticScore = eval->Evaluate(sample.board);
        float loss = static_cast<float>(sample.score - staticScore) / static_cast<float>(ScoreScale);
        auto g = loss * (1e-4f);
        gradient.Add(sample.board, g);

        lossSum += std::fabs(loss);
        lossCount++;
    }
    gradient.Symmetrize();

    for (unsigned i = 0; i < sizeof(Gradient) / sizeof(typename Gradient::Type); i++) {
      const float norm = 1e-3f;
      auto g = reinterpret_cast<Gradient::Type*>(&gradient)[i];
      g += g > 0.0f ? -norm : g < 0.0f ? norm : 0.0f;
      Score step = 0;
      if (g > 0.0f) {
        step = s(r) + s(r);
      } else if (g < 0.0f) {
        step = -(s(r) + s(r));
      }
      reinterpret_cast<Evaluator::Type*>(eval.get())[i] += step;
    }
    eval->Symmetrize();

    float lossAve = lossSum / static_cast<float>(lossCount);
    std::cout << "[" << bi << "]loss=" << lossAve << "..." << std::flush;
  }
  std::cout << "done" << std::endl;

  std::cout << eval->StringifyParameters();

  auto err = eval->SaveParam();
  if (err != nullptr) {
    std::cerr << "ERROR: " << err << std::endl;
  }

  std::cout << "end LearnBatch" << std::endl;
}
