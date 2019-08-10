#include "evaluate.h"
#include "reversi.h"
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>

namespace {

const char Signature[] = {
   'b',  'e',  'l',  'u',  'g',  'a', 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

} // namespace

namespace beluga {

template <int8_t s>
inline int Horner(const Board& board) {
  auto diskColor = board.Get(Square(s));
  return static_cast<int>(diskColor);
}

template <int8_t s0, int8_t s1, int8_t ...ss>
inline int Horner(const Board& board) {
  auto diskColor = board.Get(Square(s0));
  return static_cast<int>(diskColor) + Horner<s1, ss...>(board) * 3;
}

template <class T, bool eval>
T ExtractFeature(const Board& board, FeatureParameters<T>& vector, T gradient = 0) {
  T score = 0;

#define EXTRACT(elem) if (eval) { score += (elem); } else { (elem) += gradient; }

  EXTRACT((vector.Edge[Horner<011, 000, 001, 002, 003, 004, 005, 006, 007, 016>(board)]));
  EXTRACT((vector.Edge[Horner<061, 070, 071, 072, 073, 074, 075, 076, 077, 066>(board)]));
  EXTRACT((vector.Edge[Horner<011, 000, 010, 020, 030, 040, 050, 060, 070, 061>(board)]));
  EXTRACT((vector.Edge[Horner<016, 007, 017, 027, 037, 047, 057, 067, 077, 066>(board)]));

  EXTRACT((vector.Hor2[Horner<010, 011, 012, 013, 014, 015, 016, 017>(board)]));
  EXTRACT((vector.Hor2[Horner<060, 061, 062, 063, 064, 065, 066, 067>(board)]));
  EXTRACT((vector.Hor2[Horner<001, 011, 021, 031, 041, 051, 061, 071>(board)]));
  EXTRACT((vector.Hor2[Horner<006, 016, 026, 036, 046, 056, 066, 076>(board)]));

  EXTRACT((vector.Hor3[Horner<020, 021, 022, 023, 024, 025, 026, 027>(board)]));
  EXTRACT((vector.Hor3[Horner<050, 051, 052, 053, 054, 055, 056, 057>(board)]));
  EXTRACT((vector.Hor3[Horner<002, 012, 022, 032, 042, 052, 062, 072>(board)]));
  EXTRACT((vector.Hor3[Horner<005, 015, 025, 035, 045, 055, 065, 075>(board)]));

  EXTRACT((vector.Hor4[Horner<030, 031, 032, 033, 034, 035, 036, 037>(board)]));
  EXTRACT((vector.Hor4[Horner<040, 041, 042, 043, 044, 045, 046, 047>(board)]));
  EXTRACT((vector.Hor4[Horner<003, 013, 023, 033, 043, 053, 063, 073>(board)]));
  EXTRACT((vector.Hor4[Horner<004, 014, 024, 034, 044, 054, 064, 074>(board)]));

  EXTRACT((vector.Diag8[Horner<000, 011, 022, 033, 044, 055, 066, 077>(board)]));
  EXTRACT((vector.Diag8[Horner<070, 061, 052, 043, 034, 025, 016, 007>(board)]));

  EXTRACT((vector.Diag7[Horner<001, 012, 023, 034, 045, 056, 067>(board)]));
  EXTRACT((vector.Diag7[Horner<010, 021, 032, 043, 054, 065, 076>(board)]));
  EXTRACT((vector.Diag7[Horner<071, 062, 053, 044, 035, 026, 017>(board)]));
  EXTRACT((vector.Diag7[Horner<060, 051, 042, 033, 024, 015, 006>(board)]));

  EXTRACT((vector.Diag6[Horner<002, 013, 024, 035, 046, 057>(board)]));
  EXTRACT((vector.Diag6[Horner<020, 031, 042, 053, 064, 075>(board)]));
  EXTRACT((vector.Diag6[Horner<072, 063, 054, 045, 036, 027>(board)]));
  EXTRACT((vector.Diag6[Horner<050, 041, 032, 023, 014, 005>(board)]));

  EXTRACT((vector.Diag5[Horner<003, 014, 025, 036, 047>(board)]));
  EXTRACT((vector.Diag5[Horner<030, 041, 052, 063, 074>(board)]));
  EXTRACT((vector.Diag5[Horner<073, 064, 055, 046, 037>(board)]));
  EXTRACT((vector.Diag5[Horner<040, 031, 022, 013, 004>(board)]));

  EXTRACT((vector.Diag4[Horner<004, 015, 026, 037>(board)]));
  EXTRACT((vector.Diag4[Horner<040, 051, 062, 073>(board)]));
  EXTRACT((vector.Diag4[Horner<074, 065, 056, 047>(board)]));
  EXTRACT((vector.Diag4[Horner<030, 021, 012, 003>(board)]));

  EXTRACT((vector.Corner3x3[Horner<000, 001, 002,
                                   010, 011, 012,
                                   020, 021, 022>(board)]));
  EXTRACT((vector.Corner3x3[Horner<007, 006, 005,
                                   017, 016, 015,
                                   027, 026, 025>(board)]));
  EXTRACT((vector.Corner3x3[Horner<070, 071, 072,
                                   060, 061, 062,
                                   050, 051, 052>(board)]));
  EXTRACT((vector.Corner3x3[Horner<077, 076, 075,
                                   067, 066, 065,
                                   057, 056, 055>(board)]));

  EXTRACT((vector.Corner5x2[Horner<000, 001, 002, 003, 004,
                                   010, 011, 012, 013, 014>(board)]));
  EXTRACT((vector.Corner5x2[Horner<007, 006, 005, 004, 003,
                                   017, 016, 015, 014, 013>(board)]));
  EXTRACT((vector.Corner5x2[Horner<070, 071, 072, 073, 074,
                                   060, 061, 062, 063, 064>(board)]));
  EXTRACT((vector.Corner5x2[Horner<077, 076, 075, 074, 073,
                                   067, 066, 065, 064, 063>(board)]));
  EXTRACT((vector.Corner5x2[Horner<000, 010, 020, 030, 040,
                                   001, 011, 021, 031, 041>(board)]));
  EXTRACT((vector.Corner5x2[Horner<070, 060, 050, 040, 030,
                                   071, 061, 051, 041, 031>(board)]));
  EXTRACT((vector.Corner5x2[Horner<007, 017, 027, 037, 047,
                                   006, 016, 026, 036, 046>(board)]));
  EXTRACT((vector.Corner5x2[Horner<077, 067, 057, 047, 037,
                                   076, 066, 056, 046, 036>(board)]));

#undef EXTRACT

  return score;
}

template <class T, class F>
void Symmetrize(FeatureParameters<T>& vector, F&& func) {
#define BEGIN(var) for (int var = 0; var < 3; var++) {
#define END }

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5) BEGIN(c6) BEGIN(c7) BEGIN(c8) BEGIN(c9)
    int i0 = ((((((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5) * 3 + c6) * 3 + c7) * 3 + c8) * 3 + c9;
    int i1 = ((((((((c9 * 3 + c8) * 3 + c7) * 3 + c6) * 3 + c5) * 3 + c4) * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Edge[i0] = vector.Edge[i1] = func(vector.Edge[i0], vector.Edge[i1]);
    }
  END END END END END END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5) BEGIN(c6) BEGIN(c7)
    int i0 = ((((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5) * 3 + c6) * 3 + c7;
    int i1 = ((((((c7 * 3 + c6) * 3 + c5) * 3 + c4) * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Hor2[i0] = vector.Hor2[i1] = func(vector.Hor2[i0], vector.Hor2[i1]);
      vector.Hor3[i0] = vector.Hor3[i1] = func(vector.Hor3[i0], vector.Hor3[i1]);
      vector.Hor4[i0] = vector.Hor4[i1] = func(vector.Hor4[i0], vector.Hor4[i1]);
    }
  END END END END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5) BEGIN(c6) BEGIN(c7)
    int i0 = ((((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5) * 3 + c6) * 3 + c7;
    int i1 = ((((((c7 * 3 + c6) * 3 + c5) * 3 + c4) * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Diag8[i0] = vector.Diag8[i1] = func(vector.Diag8[i0], vector.Diag8[i1]);
    }
  END END END END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5) BEGIN(c6)
    int i0 = (((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5) * 3 + c6;
    int i1 = (((((c6 * 3 + c5) * 3 + c4) * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Diag7[i0] = vector.Diag7[i1] = func(vector.Diag7[i0], vector.Diag7[i1]);
    }
  END END END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5)
    int i0 = ((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5;
    int i1 = ((((c5 * 3 + c4) * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Diag6[i0] = vector.Diag6[i1] = func(vector.Diag6[i0], vector.Diag6[i1]);
    }
  END END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4)
    int i0 = (((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4;
    int i1 = (((c4 * 3 + c3) * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Diag5[i0] = vector.Diag5[i1] = func(vector.Diag5[i0], vector.Diag5[i1]);
    }
  END END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3)
    int i0 = ((c0 * 3 + c1) * 3 + c2) * 3 + c3;
    int i1 = ((c3 * 3 + c2) * 3 + c1) * 3 + c0;
    if (i0 < i1) {
      vector.Diag4[i0] = vector.Diag4[i1] = func(vector.Diag4[i0], vector.Diag4[i1]);
    }
  END END END END

  BEGIN(c0) BEGIN(c1) BEGIN(c2) BEGIN(c3) BEGIN(c4) BEGIN(c5) BEGIN(c6) BEGIN(c7) BEGIN(c8)
    int i0 = (((((((c0 * 3 + c1) * 3 + c2) * 3 + c3) * 3 + c4) * 3 + c5) * 3 + c6) * 3 + c7) * 3 + c8;
    int i1 = (((((((c0 * 3 + c3) * 3 + c6) * 3 + c1) * 3 + c4) * 3 + c7) * 3 + c2) * 3 + c5) * 3 + c8;
    if (i0 < i1) {
      vector.Corner3x3[i0] = vector.Corner3x3[i1] = func(vector.Corner3x3[i0], vector.Corner3x3[i1]);
    }
  END END END END END END END END END

#undef BEGIN
#undef END
}

void Gradient::Add(const Board& board, float gradient) {
  ExtractFeature<float, false>(board, *this, gradient);
}

void Gradient::Symmetrize() {
  beluga::Symmetrize(*this, [](float a, float b) {
      return a + b;
  });
}

char Evaluator::EvaluationParamFileName[] = "eval.bin";

const char* Evaluator::SaveParam(const char* fileName) const {
  std::ofstream file(fileName, std::ios::binary);

  if (!file) {
    return "ERROR: Filed to open evaluation parameter file";
  }

  file.write(Signature, sizeof(Signature));

  file.write(reinterpret_cast<const char*>(this), sizeof(*this));

  file.close();

  return nullptr;
}

const char* Evaluator::LoadParam(const char* fileName) {
  std::ifstream file(fileName, std::ios::binary);

  if (!file) {
    return "ERROR: Filed to open evaluation parameter file";
  }

  char signature[sizeof(Signature)];
  file.read(signature, sizeof(signature));

  if (memcmp(signature, Signature, sizeof(Signature)) != 0) {
    return "ERROR: The evaluation parameter file has an invalid signature";
  }

  file.read(reinterpret_cast<char*>(this), sizeof(*this));

  if (!file) {
    return "ERROR: Filed to load evaluation parameter file";
  }

  file.close();

  return nullptr;
}

Score Evaluator::Evaluate(const Board& board) {
  return ExtractFeature<Score, true>(board, *this);
}

void Evaluator::Symmetrize() {
  beluga::Symmetrize(*this, [](int16_t a, int16_t b) {
      return a;
  });
}

std::string Evaluator::StringifyParameters() {
  std::ostringstream oss;

  // FIXME

  oss << std::flush;

  return oss.str();
}

} // namespace beluga
