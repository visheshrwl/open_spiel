// Copyright 2026 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "open_spiel/games/shogi/shogi_board.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "open_spiel/abseil-cpp/absl/strings/ascii.h"
#include "open_spiel/abseil-cpp/absl/strings/match.h"
#include "open_spiel/abseil-cpp/absl/strings/str_cat.h"
#include "open_spiel/abseil-cpp/absl/strings/str_split.h"
#include "open_spiel/abseil-cpp/absl/strings/string_view.h"
#include "open_spiel/abseil-cpp/absl/types/optional.h"
#include "open_spiel/spiel_utils.h"

namespace open_spiel {
namespace shogi {

PieceType PromotedType(PieceType type) {
  switch (type) {

    case PieceType::kLance:
      return PieceType::kLanceP;
    case PieceType::kKnight:
      return PieceType::kKnightP;
    case PieceType::kSilver:
      return PieceType::kSilverP;
    case PieceType::kBishop:
      return PieceType::kBishopP;
    case PieceType::kRook:
      return PieceType::kRookP;
    case PieceType::kPawn:
      return PieceType::kPawnP;
	}
	return PieceType::kEmpty; // Does not promote
}

PieceType UnpromotedType(PieceType type) {
  switch (type) {

    case PieceType::kLanceP:
      return PieceType::kLance;
    case PieceType::kKnightP:
      return PieceType::kKnight;
    case PieceType::kSilverP:
      return PieceType::kSilver;
    case PieceType::kBishopP:
      return PieceType::kBishop;
    case PieceType::kRookP:
      return PieceType::kRook;
    case PieceType::kPawnP:
      return PieceType::kPawn;
	}
	return PieceType::kEmpty; // Does not unpromote
}


bool IsMoveCharacter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9');
}

std::pair<std::string, std::string> SplitAnnotations(const std::string& move) {
  for (int i = 0; i < move.size(); ++i) {
    if (!IsMoveCharacter(move[i])) {
      return {move.substr(0, i), std::string(absl::ClippedSubstr(move, i))};
    }
  }
  return {move, ""};
}

std::string ColorToString(Color c) {
  switch (c) {
    case Color::kBlack:
      return "black";
    case Color::kWhite:
      return "white";
    case Color::kEmpty:
      return "empty";
    default:
      SpielFatalError(absl::StrCat("Unknown color: ", c));
      return "This will never return.";
  }
}

absl::optional<PieceType> PieceTypeFromChar(char c) {
  switch (toupper(c)) {
    case 'L':
      return PieceType::kLance;
    case 'N':
      return PieceType::kKnight;
    case 'S':
      return PieceType::kSilver;
    case 'G':
      return PieceType::kGold;
    case 'K':
      return PieceType::kKing;
    case 'B':
      return PieceType::kBishop;
    case 'R':
      return PieceType::kRook;
    case 'P':
      return PieceType::kPawn;
    default:
      std::cerr << "Invalid piece type: " << c << std::endl;
      return absl::nullopt;
  }
}

std::string PieceTypeToString(PieceType p, bool uppercase) {
  switch (p) {
    case PieceType::kEmpty:
      return " ";
    case PieceType::kPawn:
      return uppercase ? "P" : "p";
    case PieceType::kLance:
      return uppercase ? "L" : "l";
    case PieceType::kKnight:
      return uppercase ? "N" : "n";
    case PieceType::kSilver:
      return uppercase ? "S" : "s";
    case PieceType::kGold:
      return uppercase ? "G" : "G";
    case PieceType::kBishop:
      return uppercase ? "B" : "b";
    case PieceType::kRook:
      return uppercase ? "R" : "r";
    case PieceType::kKing:
      return uppercase ? "K" : "k";
    case PieceType::kPawnP:
      return uppercase ? "+P" : "+p";
    case PieceType::kLanceP:
      return uppercase ? "+L" : "+l";
    case PieceType::kKnightP:
      return uppercase ? "+N" : "+n";
    case PieceType::kSilverP:
      return uppercase ? "+S" : "+s";
    case PieceType::kRookP:
      return uppercase ? "+R" : "+R";
    case PieceType::kBishopP:
      return uppercase ? "+B" : "+b";
    default:
      SpielFatalError(std::string("Unknown piece (ptts): ") +
                      std::to_string(static_cast<int>(p)));
      return "This will never return.";
  }
}

std::string Piece::ToString() const {
  std::string base = PieceTypeToString(type);
  return color == Color::kWhite ? absl::AsciiStrToUpper(base)
                                : absl::AsciiStrToLower(base);
}

absl::optional<Square> SquareFromString(const std::string& s) {
  if (s.size() != 2) return kInvalidSquare;

  auto file = ParseFile(s[0]);
  auto rank = ParseRank(s[1]);
  if (file && rank) return Square{*file, *rank};
  return absl::nullopt;
}


std::string Move::ToString() const {
  std::string extra;

  if (promote) {
		extra = "+";
  }
  return absl::StrCat(SquareToString(from), SquareToString(to), extra);
}

std::string Move::ToLAN() const {
  if (drop) {
    std::string move_text;
    PieceType from_type = piece.type;
    move_text += PieceTypeToString(from_type);
    move_text += '@';
    absl::StrAppend(&move_text, SquareToString(to));
    return move_text;
  }
	std::string promotion;
	if (promote) {
		promotion = "+";
	}
	return absl::StrCat(SquareToString(from), SquareToString(to), promotion);
}

ShogiBoard::ShogiBoard() {
  board_.fill(kEmptyPiece);
}

/*static*/ absl::optional<ShogiBoard> ShogiBoard::BoardFromSFEN(
    const std::string& fen) {
  ShogiBoard board;
  std::vector<std::string> fen_parts = absl::StrSplit(fen, ' ');
  if (fen_parts.size() != 4) {
    std::cerr << "Invalid FEN: " << fen << std::endl;
    return absl::nullopt;
  }
	std::string& piece_configuration = fen_parts[0];
  std::string& side_to_move = fen_parts[1];
  std::string& hand_str = fen_parts[2];
  std::string& move_number = fen_parts[3];

  std::vector<std::string> piece_config_by_rank =
      absl::StrSplit(piece_configuration, '/');
	for (int8_t current_y = kBoardSize - 1; current_y >= 0; --current_y) {
		std::string& rank = piece_config_by_rank[kBoardSize - current_y - 1];
		int8_t current_x = 0;

		for (int i = 0; i < rank.size(); ++i) {
			char c = rank[i];

			if (current_x >= kBoardSize) {
				std::cerr << "Too many things on SFEN rank: " << rank << std::endl;
				return absl::nullopt;
			}

			// Empty squares
			if (c >= '1' && c <= '9') {
				current_x += c - '0';
				continue;
			}
			bool promoted = false;
			if (c == '+') {  // Promotion marker
				promoted = true;
				++i;
				if (i >= rank.size()) {
					std::cerr << "Dangling + in SFEN rank: " << rank << std::endl;
					return absl::nullopt;
				}
				c = rank[i];
			}
			auto piece_type = PieceTypeFromChar(c);
			if (!piece_type) {
				std::cerr << "Invalid piece type in SFEN: " << c << std::endl;
				return absl::nullopt;
			}

			PieceType final_type = promoted
					? PromotedType(*piece_type)
					: *piece_type;

			Color color = std::isupper(c) ? Color::kBlack : Color::kWhite;

			board.set_square(Square{current_x, current_y},
											 Piece{color, final_type});

			++current_x;
		}

		if (current_x != kBoardSize) {
			std::cerr << "Incorrect number of squares in rank: " << rank << std::endl;
			return absl::nullopt;
		}
	}
  if (hand_str != "-") {

		int count = 0;

		for (size_t i = 0; i < hand_str.size(); ++i) {
			char c = hand_str[i];

			// Accumulate multi-digit counts
			if (std::isdigit(c)) {
				count = count * 10 + (c - '0');
				continue;
			}

			// If no digit before piece, count is 1
			if (count == 0) {
				count = 1;
			}

			auto piece_type = PieceTypeFromChar(c);
			if (!piece_type) {
				std::cerr << "Invalid piece in SFEN hand: " << c << std::endl;
				return absl::nullopt;
			}
			Color color = std::isupper(c) ? Color::kBlack : Color::kWhite;

			Pocket& pocket = (color == Color::kBlack)
													 ? board.black_pocket_
													 : board.white_pocket_;

			for (int j = 0; j < count; ++j) {
				pocket.Increment(*piece_type);
			}

			count = 0;  // reset for next piece
		}
	}

  if (side_to_move == "b") {
    board.SetToPlay(Color::kBlack);
  } else if (side_to_move == "w") {
    board.SetToPlay(Color::kWhite);
  } else {
    std::cerr << "Invalid side to move in FEN: " << side_to_move << std::endl;
    return absl::nullopt;
  }
  board.SetMovenumber(std::stoi(move_number));
}


Square ShogiBoard::find(const Piece& piece) const {
  for (int8_t y = 0; y < kBoardSize; ++y) {
    for (int8_t x = 0; x < kBoardSize; ++x) {
      Square sq{x, y};
      if (at(sq) == piece) {
        return sq;
      }
    }
  }

  return kInvalidSquare;
}


void ShogiBoard::GenerateLegalMoves(
    const MoveYieldFn& yield, Color color) const {
  bool generating = true;

#define YIELD(move)     \
  if (!yield(move)) {   \
    generating = false; \
  }

  GenerateDropDestinations_(color, yield);

  for (int8_t y = 0; y < kBoardSize && generating; ++y) {
    for (int8_t x = 0; x < kBoardSize && generating; ++x) {
      Square sq{x, y};
      auto& piece = at(sq);
      if (piece.type != PieceType::kEmpty && piece.color == color) {
        switch (piece.type) {
          case PieceType::kKing:
            GenerateKingDestinations_(
                sq, color,
                [&yield, &piece, &sq, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kPawn:
            GeneratePawnDestinations_(
                sq, color,
                [&yield, &piece, &sq, &generating, color](const Square& to) {
								  if (!StuckPiece(color, piece, to.y)) {
                    YIELD(Move(sq, to, piece));
									}
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
					case PieceType::kLance:
            GenerateLanceDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating, color](const Square& to) {
								  if (!StuckPiece(color, piece, to.y)) {
                    YIELD(Move(sq, to, piece));
									}
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
          case PieceType::kKnight:
            GenerateKnightDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating, color](const Square& to) {
								  if (!StuckPiece(color, piece, to.y)) {
                    YIELD(Move(sq, to, piece));
									}
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
					case PieceType::kSilver:
            GenerateSilverDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating, color](const Square& to) {
                  YIELD(Move(sq, to, piece));
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
					case PieceType::kGold:
					case PieceType::kPawnP:
					case PieceType::kLanceP:
					case PieceType::kKnightP:
					case PieceType::kSilverP:
            GenerateGoldDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          case PieceType::kRook:
            GenerateRookDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating, color](const Square& to) {
                  YIELD(Move(sq, to, piece));
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
          case PieceType::kRookP:
            GenerateRookDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            GenerateRookPDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
						break;
          case PieceType::kBishop:
            GenerateBishopDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece, color));
									if (InPromoZone(color, sq.y) || InPromoZone(color, to.y)) {
                     YIELD(Move(sq, to, piece, true));
									}
                });
            break;
          case PieceType::kBishopP:
            GenerateBishopDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
            GenerateBishopPDestinations_(
                sq, color,
                [&yield, &sq, &piece, &generating](const Square& to) {
                  YIELD(Move(sq, to, piece));
                });
            break;
          default:
            std::cerr << "Unknown piece type: " << static_cast<int>(piece.type)
                      << std::endl;
        }
      }
    }
  }

#undef YIELD
}


// TODO forbid pawn drop with checkmate
template <typename YieldFn>
void ShogiBoard::GenerateDropDestinations_(
    Color player, const PseudoLegalMoveSettings& settings,
    const YieldFn& yield) const {
  // Get the pocket for the player
  Pocket pocket = (player == Color::kWhite ? white_pocket_ : black_pocket_);

  // Loop over drop-capable piece types
  for (PieceType ptype : Pocket::PieceTypes()) {
    if (pocket.Count(ptype) == 0) continue;

    for (int8_t y = 0; y < kBoardSize; ++y) {
      for (int8_t x = 0; x < kBoardSize; ++x) {
        Square sq{x, y};

        // Only drop on empty squares
        if (at(sq) != kEmptyPiece) continue;

        // Cannot drop pieces that will never move
				if StuckPiece(player, ptype, y) continue;
				// check if already a pawn in column
				bool pawn_already = false;
				if (ptype == PieceType::kPawn){
					for (int8_t y1 = 0; y1 < kBoardSize; ++y) {
						Piece there = at(Square{x, y1});
						if (there == Piece{player, PieceType::kPawn}){
							pawn_already = true;
							break;
						}
					}
				}
        if (pawn_already) continue;
        // Build the Move
        Move m;
        m.from = Square {-1, -1};
        m.to = sq;
				m.piece = Piece{color, ptype};
				m_promotion = true;

        // Output the move
        yield(m);
      }
    }
  }
}

bool StuckPiece(Color player, PieceType ptype, int8_t y) {
	if (player == Color::Black) {
	 if ((ptype == PieceType::kPawn || ptype == PieceType::kLance) 
			 && y == kBoardSize - 1) return true;
	 if (ptype == PieceType::kKnight && 
			 (y == kBoardSize - 1 || y == kBoardSize - 2)) return true;
	} else {
	 if ((ptype == PieceType::kPawn || ptype == PieceType::kLance) 
			 && y == 0) return true;
	 if (ptype == PieceType::kKnight && 
			 (y == 0 || y == 1)) return true;
	}
	return false;
}

bool InPromoZone(Color player, int8_t y) {
	if (player == Color::Black && y >= kBoardSize - 3) return true;
	if (player == Color::White && y <= 2) return true;
	return false;
}
absl::optional<Move> ShogiBoard::ParseMove(const std::string& move) const {
  // First see if they are in the long form -
  // "anan" (eg. "e2e4") or "anana" (eg. "f7f8q")
  // SAN moves will never have this form because an SAN move that starts with
  // a lowercase letter must be a pawn move, and pawn moves will never require
  // rank disambiguation (meaning the second character will never be a number).
  auto lan_move = ParseLANMove(move);
  if (lan_move) {
    return lan_move;
  }

  return absl::nullopt;
}


absl::optional<Move> ShogiBoard::ParseDropMove(
    const std::string& move) const {
  if (move.empty()) {
    return absl::nullopt;
  }
  if (move.size() == 4 && move[1] == '@') {
    char pc = move[0];
    char file = move[2];
    char rank = move[3];

    // Validate square
    if (file < 'a' || file >= ('a' + kBoardSize) || rank < '1' ||
        rank >= ('1' + kBoardSize)) {
      return absl::nullopt;
    }

    // Parse piece type
    absl::optional<PieceType> opt = PieceTypeFromChar(pc);
    if (!opt) return absl::nullopt;

    PieceType ptype = *opt;

    // Disallow illegal drops
    if (ptype == PieceType::kKing) return absl::nullopt;

    auto to = SquareFromString(move.substr(2, 2));
    if (!to) return absl::nullopt;

    // Construct drop move
    Move drop;
    drop.from = Square{-1, -1}; // dummpy
    drop.to = *to;
    drop.piece = Piece{to_play_, ptype};
		drop.drop = true;
    return drop;
  }

  return absl::nullopt;
}

absl::optional<Move> ShogiBoard::ParseLANMove(const std::string& move) const {
  if (move.empty()) return absl::nullopt;

  // Try drop syntax first.
  if (auto drop_move = ParseDropMove(move)) {
    return drop_move;
  }

  // Non-drop LAN: "a1b2" or "a1b2+"
  if (move.size() != 4 && move.size() != 5) return absl::nullopt;

  // Validate coordinate characters (optional if SquareFromString fully validates).
  auto in_file = [](char c) { return c >= 'a' && c < ('a' + kBoardSize); };
  auto in_rank = [](char c) { return c >= '1' && c < ('1' + kBoardSize); };

  if (!in_file(move[0]) || !in_rank(move[1]) || !in_file(move[2]) || !in_rank(move[3])) {
    return absl::nullopt;
  }

  bool promo = false;
  if (move.size() == 5) {
    if (move[4] != '+') {
      std::cerr << "Illegal move - " << move << std::endl;
      return absl::nullopt;
    }
    promo = true;
  }

  auto from_opt = SquareFromString(move.substr(0, 2));
  auto to_opt   = SquareFromString(move.substr(2, 2));
  if (!from_opt || !to_opt) return absl::nullopt;

  Square from = *from_opt;
  Square to   = *to_opt;

  if (from == to) return absl::nullopt;

  Piece on_from = at(from);

  // Check there is a piece to move.
  if (on_from.type == PieceType::kEmpty) {
    std::cerr << "No piece on from-square in move - " << move << std::endl;
    return absl::nullopt;
  }

  // Check correct side.
  if (on_from.color != ToPlay()) {
    std::cerr << "Piece on from-square is not side-to-move in move - " << move << std::endl;
    return absl::nullopt;
  }

  // Construct Move with the piece actually on the board.
  // (If your Move expects piece.color == ToPlay(), this satisfies it.)
  return Move{from, to, on_from, promo, /*drop=*/false};
}

void ShogiBoard::ApplyMove(const Move& move) {

  // We remove the moving piece from the original
  // square or pocket and put it on the destination square, overwriting whatever was
  // there before. If we capture, put the captured piece in the pocket.
  //
  Piece moving_piece;
  Piece destination_piece = at(move.to);

  if (move.IsDropMove()) {
    PieceType from_type = move.piece.type;
    moving_piece = Piece{to_play_, from_type};
    RemoveFromPocket(to_play_, from_type);
  } else {
    moving_piece = at(move.from);
    set_square(move.from, kEmptyPiece);
  }

  set_square(move.to, moving_piece);
  // Increment pockets for capture.
  if (destination_piece != kEmptyPiece) {
    PieceType dpt = destination_piece.type;
    if (dpt == PieceType::kKing) {
      std::cerr << "King capture from" << SquareToString(move.from)
                << std::endl;
      SpielFatalError("King capture detected.");
    }
    AddToPocket(to_play_, dpt);
  }

  if (to_play_ == Color::kBlack) {
    ++move_number_;
  }

  SetToPlay(OppColor(to_play_));
}

bool ShogiBoard::TestApplyMove(const Move& move) {
  Color color = to_play_;
  ApplyMove(move);
  return !UnderAttack(find(Piece{color, PieceType::kKing}), color);
}

bool ShogiBoard::UnderAttack(const Square& sq, Color our_color) const {
  SPIEL_CHECK_NE(sq, kInvalidSquare);

  bool under_attack = false;
  Color opponent_color = OppColor(our_color);

  // We do this by pretending we are a piece of different types, and seeing if
  // we can attack opponent pieces. Eg. if we pretend we are a knight, and can
  // attack an opponent knight, that means the knight can also attack us.

  // King moves (this is possible because we use this function for checking
  // whether we are moving into check, and we can be trying to move the king
  // into a square attacked by opponent king).
  GenerateKingDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kKing}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Rook moves
  GenerateRookDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kRook}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GenerateRookPDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kRookP}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Bishop moves
  GenerateBishopDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kBishop}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GenerateBishopPDestinations_(
      sq, our_color, PseudoLegalMoveSettings::kAcknowledgeEnemyPieces,
      [this, &under_attack, &opponent_color](const Square& to) {
        if (at(to) == Piece{opponent_color, PieceType::kBishop}) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GenerateLanceDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kLance})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  // Knight moves
  GenerateKnightDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kKnight})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GenerateSliverDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kSilver})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GenerateGoldDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kGold}) ||
            (at(to) == Piece{opponent_color, PieceType::kPawnP}) ||
            (at(to) == Piece{opponent_color, PieceType::kLanceP}) ||
            (at(to) == Piece{opponent_color, PieceType::kKnightP}) ||
            (at(to) == Piece{opponent_color, PieceType::kSilverP})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }
  GeneratePawnDestinations_(
      sq, our_color, [this, &under_attack, &opponent_color](const Square& to) {
        if ((at(to) == Piece{opponent_color, PieceType::kPawn})) {
          under_attack = true;
        }
      });
  if (under_attack) {
    return true;
  }

  return false;
}

std::string ShogiBoard::DebugString(bool shredder_fen) const {
  std::string s;
  s = absl::StrCat("FEN: ", ToFEN(shredder_fen), "\n");
  absl::StrAppend(&s, "\n  ---------------------------------\n");
  for (int8_t y = kBoardSize - 1; y >= 0; --y) {
    // Rank label.
    absl::StrAppend(&s, RankToString(y), " ");

    // Pieces on the rank.
    for (int8_t x = 0; x < kBoardSize; ++x) {
      Square sq{x, y};
      absl::StrAppend(&s, "| ", at(sq).ToString(), " ");
    }
    absl::StrAppend(&s, "|\n");
    absl::StrAppend(&s, "  ---------------------------------\n");
  }

  // File labels.
  absl::StrAppend(&s, "    ");
  for (int8_t x = 0; x < kBoardSize; ++x) {
    absl::StrAppend(&s, FileToString(x), "   ");
  }
  absl::StrAppend(&s, "\n");

  absl::StrAppend(&s, "To play: ", to_play_ == Color::kWhite ? "W" : "B", "\n");
  absl::StrAppend(&s, "Move number: ", move_number_, "\n\n");

  absl::StrAppend(&s, "\n");

  return s;
}

// King moves.
template <typename YieldFn>
void ShogiBoard::GenerateKingDestinations_(Square sq, Color color,
                                                const YieldFn& yield) const {
  static const std::array<Offset, 8> kOffsets = {
      {{1, 0}, {1, 1}, {1, -1}, {0, 1}, {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}}};

  for (const auto& offset : kOffsets) {
    Square dest = sq + offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}


template <typename YieldFn>
void ShogiBoard::GenerateRookDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  GenerateRayDestinations_(sq, color, settings, {1, 0}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, 0}, yield);
  GenerateRayDestinations_(sq, color, settings, {0, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {0, -1}, yield);
}

template <typename YieldFn>
void ShogiBoard::GenerateLanceDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
	if (color == Color::Black) {
    GenerateRayDestinations_(sq, color, settings, {1, 0}, yield);
	} else {
    GenerateRayDestinations_(sq, color, settings, {-1, 0}, yield);
	}
}

template <typename YieldFn>
void ShogiBoard::GenerateGoldDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
	int8_t y_direction = color == Color::kBlack ? 1 : -1;
	static const std::array<Offset, 6> kGoldOffsets =
	 {{0,  1}, {0, -1}, {1,  0}, {-1, 0}, {1,  1},{-1, 1}};
  for (const auto& offset : kGoldOffsets) {
		Offset real_offset = Offset{offset.x_offset, y_direction * offset.y_offset}; 
    Square dest = sq + real_offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

template <typename YieldFn>
void ShogiBoard::GenerateSilverDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  int8_t y_direction = color == Color::kBlack ? 1 : -1;
  static const std::array<Offset, 5> kSilverOffsets =
   {{1,  -1}, {1, 0}, {1,  1}, {-1, -1}, {-1, 1}};
  for (const auto& offset : kGoldOffsets) {
    Offset real_offset = Offset{offset.x_offset, y_direction * offset.y_offset};
    Square dest = sq + real_offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

template <typename YieldFn>
void ShogiBoard::GenerateKnightDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  int8_t y_direction = color == Color::kBlack ? 1 : -1;
  static const std::array<Offset, 2> kKnightOffsets =
   {{-1,  2}, {1, 2}};
  for (const auto& offset : kKnightOffsets) {
    Offset real_offset = Offset{offset.x_offset, y_direction * offset.y_offset};
    Square dest = sq + real_offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

template <typename YieldFn>
void ShogiBoard::GeneratePawnDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  int8_t y_direction = color == Color::kBlack ? 1 : -1;
  static const std::array<Offset, 1> kPawnOffsets =
   {{1,  0}};
  for (const auto& offset : kPawnOffsets) {
    Offset real_offset = Offset{offset.x_offset, y_direction * offset.y_offset};
    Square dest = sq + real_offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

// Extra squares for promoted rook
template <typename YieldFn>
void ShogiBoard::GenerateRookPDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  static const std::array<Offset, 4> kRookPOffsets =
   {{-1,  -1}, {-1, 1}, {1, -1}, {1,1}};
  for (const auto& offset : kRookPOffsets) {
    Square dest = sq + offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}

// Extra squares for promoted bishop
template <typename YieldFn>
void ShogiBoard::GenerateBishopPDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  static const std::array<Offset, 4> kBishopPOffsets =
   {{-1,  -1}, {-1, 1}, {1, -1}, {1,1}};
  for (const auto& offset : kBishopPoffsets) {
    Square dest = sq + offset;
    if (InBoardArea(dest) && IsEmptyOrEnemy(dest, color)) {
      yield(dest);
    }
  }
}


template <typename YieldFn>
void ShogiBoard::GenerateBishopDestinations_(
    Square sq, Color color, PseudoLegalMoveSettings settings,
    const YieldFn& yield) const {
  GenerateRayDestinations_(sq, color, settings, {1, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, 1}, yield);
  GenerateRayDestinations_(sq, color, settings, {1, -1}, yield);
  GenerateRayDestinations_(sq, color, settings, {-1, -1}, yield);
}


template <typename YieldFn>
void ShogiBoard::GenerateRayDestinations_(Square sq, Color color,
                                               PseudoLegalMoveSettings settings,
                                               Offset offset_step,
                                               const YieldFn& yield) const {
  for (Square dest = sq + offset_step; InBoardArea(dest); dest += offset_step) {
    if (IsEmpty(dest)) {
      yield(dest);
    } else if (IsEnemy(dest, color)) {
      yield(dest);
      if (settings == PseudoLegalMoveSettings::kAcknowledgeEnemyPieces) {
        break;
      }
    } else {
      // We have a friendly piece.
      break;
    }
  }
}

std::string ShogiBoard::ToSFEN(bool /*shredder*/) const {
  std::string fen;

  // 1. Board
  for (int y = kBoardSize - 1; y >= 0; --y) {
    int empty = 0;

    for (int x = 0; x < kBoardSize; ++x) {
      Piece p = at(Square{x, y});

      if (p.type == PieceType::kEmpty) {
        ++empty;
        continue;
      }

      if (empty > 0) {
        fen += std::to_string(empty);
        empty = 0;
      }

      if (IsPromoted(p.type)) {
        fen += '+';
        fen += PieceChar(UnpromotedType(p.type), p.color);
      } else {
        fen += PieceChar(p.type, p.color);
      }
    }

    if (empty > 0) {
      fen += std::to_string(empty);
    }

    if (y > 0) fen += '/';
  }

  // 2. Side
  fen += ' ';
  fen += (ToPlay() == Color::kBlack ? 'b' : 'w');

  // 3. Hands
  fen += ' ';
  if (BothPocketsEmpty()) {
    fen += '-';
  } else {
    AppendHands(fen);
  }

  // 4. Move number
  fen += ' ';
  fen += std::to_string(move_number_);

  return fen;
}

// For purposes of the hash
// we  will saturate the pocket piece count at 16,
// although the actual piece count can go beyond that.
static constexpr int kMaxPocketHashCount = 16;
static const ZobristTableU64<2, 5, kMaxPocketHashCount + 1> kPocketZobrist(
    /*seed=*/2825712);

inline int HashCount(int n) { return std::min(n, kMaxPocketHashCount); }

void ShogiBoard::AddToPocket(Color owner, PieceType piece) {
  Pocket& pocket = owner == Color::kWhite ? white_pocket_ : black_pocket_;

	int old = pocket.Count(piece);
	int new_ = old + 1;

	int old_hash = HashCount(old);
	int new_hash = HashCount(new_);

	zobrist_hash_ ^=
			kPocketZobrist[ToInt(owner)][Pocket::Index(piece)][old_hash];
	zobrist_hash_ ^=
			kPocketZobrist[ToInt(owner)][Pocket::Index(piece)][new_hash];

	pocket.Increment(piece, 1);
}

void ShogiBoard::RemoveFromPocket(Color owner, PieceType piece) {
  Pocket& pocket = owner == Color::kWhite ? white_pocket_ : black_pocket_;
  int old = pocket.Count(piece);
  SPIEL_CHECK_GT(old, 0);

  int new_ = old - 1;

  int old_hash = HashCount(old);
  int new_hash = HashCount(new_);

  if (old_hash != new_hash) {
    zobrist_hash_ ^=
        kPocketZobrist[ToInt(owner)][Pocket:Index(piece)][old_hash];
    zobrist_hash_ ^=
        kPocketZobrist[ToInt(owner)][Pocket:Index(piece)][new_hash];
  }

  pocket.Decrement(piece);
}

// 13 piece types * 2 colors + 1 for empty = 27
void ShogiBoard::set_square(Square sq, Piece piece) {
  static const ZobristTableU64<kNumSquares, 3, 27> kZobristValues(
      /*seed=*/2765481);

  // First, remove the current piece from the hash.
  auto position = SquareToIndex_(sq);
  auto current_piece = at(sq);
  zobrist_hash_ ^=
      kZobristValues[position][static_cast<int>(current_piece.color)]
                    [static_cast<int>(current_piece.type)];

  // Then add the new piece
  zobrist_hash_ ^= kZobristValues[position][static_cast<int>(piece.color)]
                                 [static_cast<int>(piece.type)];

  board_[position] = piece;
}

void ShogiBoard::SetToPlay(Color c) {
  static const ZobristTableU64<2> kZobristValues(/*seed=*/284628);

  // Remove old color and add new to play.
  zobrist_hash_ ^= kZobristValues[ToInt(to_play_)];
  zobrist_hash_ ^= kZobristValues[ToInt(c)];
  to_play_ = c;
}


void ShogiBoard::SetMovenumber(int move_number) {
  move_number_ = move_number;
}

std::string DefaultFen() {
    return shogi::kDefaultStandardFEN;
}


static Pocket::PieceType PocketPieceType(int index){
  switch (index) {
		case 0:
      return PieceType::kPawn;
		case 1:
      return PieceType::kLance;
		case 2:
      return PieceType::kKnight;
		case 3:
      return PieceType::kSilver;
		case 4:
      return PieceType::kGold;
		case 5:
      return PieceType::kBishop;
		case 6:
      return PieceType::kRook;
    default: {
      SpielFatalError(absl::StrCat("Invalid PieceType for Pocket: ",
                                   static_cast<int>(ptype)));
    }
      return PieceType::kPawn;  // Never happens.
  }
}

void Pocket::Increment(PieceType piece) {
  const std::size_t i = Index(piece);
  counts_[i] += 1;
}


void Pocket::Decrement(PieceType piece) {
  const std::size_t i = Index(piece);
  SPIEL_CHECK_GT(counts_[i], 0);
  --counts_[i];
}

int Pocket::Count(PieceType piece) const { return counts_[Index(piece)]; }

static Pocket::Index(PieceType ptype) {
  switch (ptype) {
    case PieceType::kPawn:
      return 0;
    case PieceType::kLance:
      return 1;
    case PieceType::kKnight:
      return 2;
    case PieceType::kSilver:
      return 3;
    case PieceType::kGold:
      return 4;
    case PieceType::kBishop:
      return 5;
    case PieceType::kRook:
      return 6;
    default: {
      SpielFatalError(absl::StrCat("Invalid PieceType for Pocket: ",
                                   static_cast<int>(ptype)));
    }
      return 0;  // never happens
  }
}

}  // namespace shogi
}  // namespace open_spiel
