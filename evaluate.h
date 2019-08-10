#pragma once

#include <string>
#include <cstring>
#include <cstdint>

namespace beluga {

class Board;
class Bitboard;

using Score = int16_t;
constexpr int16_t ScoreScale = 100;
constexpr int16_t ScoreInfinity = 100 * ScoreScale;

inline Score ScoreMax(Score a, Score b) {
  return a > b ? a : b;
}

inline Score ScoreMin(Score a, Score b) {
  return a < b ? a : b;
}

template <class T>
struct FeatureParameters {
  using Type = T;

  FeatureParameters() {
    memset(reinterpret_cast<char*>(this), 0, sizeof(*this));
  }

  Type Edge[59049];
  Type Hor2[6561];
  Type Hor3[6561];
  Type Hor4[6561];
  Type Diag8[6561];
  Type Diag7[2187];
  Type Diag6[729];
  Type Diag5[243];
  Type Diag4[81];
  Type Corner3x3[19683];
  Type Corner5x2[59049];
};

class Gradient : public FeatureParameters<float> {
public:
  void Add(const Board& board, float gradient);
  void Symmetrize();
};

class Evaluator : public FeatureParameters<Score> {
public:
  static char EvaluationParamFileName[];

  const char* SaveParam() const {
    return SaveParam(EvaluationParamFileName);
  }
  const char* SaveParam(const char* fileName) const;
  const char* LoadParam() {
    return LoadParam(EvaluationParamFileName);
  }
  const char* LoadParam(const char* fileName);

  Score Evaluate(const Board& board);

  void Symmetrize();

  std::string StringifyParameters();
};

} // namespace beluga
