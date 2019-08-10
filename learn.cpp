#include "search.h"
#include <list>
#include <iostream>
#include <random>
#include <set>
#include <cmath>

using namespace beluga;

std::mt19937 r(static_cast<unsigned>(time(nullptr)));

void LearnBatch(std::shared_ptr<Evaluator> eval, int sampleSize, std::set<int> targetCounts, int batchCount);

int main(int, const char**, const char**) {
  std::shared_ptr<Evaluator> eval(new Evaluator);

  LearnBatch(eval, 10000, std::set<int>{ 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 1024);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 512);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 256);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 128);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 64);
  LearnBatch(eval, 10000, std::set<int>{ 15, 16, 20, 21, 25, 26, 30, 31, 35, 36, 40, 41, 45, 46, 50, 51, 54, 55 }, 32);

  return 0;
}

struct Sample {
  Board board;
  Score score;
};

void LearnBatch(std::shared_ptr<Evaluator> eval, int sampleSize, std::set<int> targetCounts, int batchCount) {
  std::cout << "begin LearnBatch" << std::endl;
  std::cout << "sampleSize: " << sampleSize << std::endl;
  std::cout << "targets   : " << targetCounts.size() << std::endl;
  std::cout << "batchCount: " << batchCount << std::endl;

  // generate samples
  std::list<Sample> samples;

  std::cout << "generating samples..." << std::flush;
  for (int i = 0; i < sampleSize; i++) {
    Board board = Board::GetNormalInitBoard();

    while (!board.IsEnd()) {
      if (board.MustPass()) {
        board.Pass();
        continue;
      }

      auto count = (board.GetBlackBoard() | board.GetWhiteBoard()).Count();
      if (targetCounts.count(count) != 0) {
        samples.push_back({ board, 0 });
      }

      auto moves = board.GenerateMoves();
      std::uniform_int_distribution<int32_t> d(0, moves.Count() - 1);
      Square move;
      for (auto index = d(r); index >= 0; index--) {
        move = moves.Pick();
      }

      board.DoMove(move);
    }
  }
  std::cout << "done" << std::endl;

  // evaluate
  std::cout << "evaluating..." << std::flush;
  Searcher searcher(eval);
  for (auto& sample : samples) {
    auto count = (sample.board.GetBlackBoard() | sample.board.GetWhiteBoard()).Count();
    auto searchResult = searcher.Search(sample.board, 5, 12);
    sample.score = sample.board.GetNextDisk() == ColorBlack
                 ? searchResult.score
                 : -searchResult.score;
    std::cout << "." << std::flush;
  }
  std::cout << "done" << std::endl;

  // adjust
  std::cout << "adjusting..." << std::flush;
  std::uniform_int_distribution<Score> s(0, 1);
  for (int bi = 0; bi < batchCount; bi++) {
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
