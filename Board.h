#pragma once
#include <Windows.h>
#include <cstdint>


enum MoveFlags
{
	NORMAL_MOVE,
	DOUBLE_PUSH_PAWN,
	CAPTURE,
	EN_PASSANT_CAPTURE,
	CASTLE_KING_WHITE,
	CASTLE_QUEEN_BLACK,
	CASTLE_KING_BLACK,
	CASTLE_QUEEN_WHITE,
	PROMO_ROOK,
	PROMO_KNIGHT,
	PROMO_BISHOP,
	PROMO_QUEEN,
	CAP_PROMO_ROOK,
	CAP_PROMO_KNIGHT,
	CAP_PROMO_BISHOP,
	CAP_PROMO_QUEEN
};


struct MoveList
{
	uint16_t moves[256];// each contains: source, destination and flags (0-5, 6-11, 12-15)
	int scores[256];
	int count = 0;
	static constexpr int IS_CAPTURE[16] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
	static constexpr int MoveScores[16] = { 0, 5, 0, 950, 20, 20, 20, 20, 900, 320, 550, 330, 900, 320, 550, 330 };
	void add_move(uint16_t move, int score) 
	{
		moves[count] = move;
		scores[count] = score;
		count++;
	}
};

// to be used in both the bitboards and the board array
enum Piece {
	W_PAWN, 
	W_ROOK, 
	W_KNIGHT, 
	W_BISHOP, 
	W_QUEEN, 
	W_KING,
	B_PAWN, 
	B_ROOK, 
	B_KNIGHT, 
	B_BISHOP, 
	B_QUEEN, 
	B_KING,
	EMPTY
};

class Board
{
public:
	uint64_t pieces_occ[13]; // w_pawn, w_rook, w_knight, w_bishop, w_queen, w_king, b_pawn, ... 13th entry is for EMPTY's (faster than running if statements)
	uint8_t board_arr[64];
	// LSB, WKS
	// second bit, WQS
	// third bit, BKS
	// MSB, BQS
	uint8_t castling_allowed; 
	uint8_t current_enpassant_index = 64; // 64 if none, else "skipped" square index when any pawn moves two squares
	int total_moves_amt;
	bool white_to_move = true;
	uint64_t total_occ;
	uint64_t side_total_occ[3]; // first for white, other for black, third for EMPTY's 
	Board();
	void clear_and_setup();
	void set_piece(uint8_t square, Piece piece);
	void remove_piece(uint8_t square);
	void move_piece(uint8_t squareSrc, uint8_t squareDst);
	Piece get_piece(uint8_t square);
	void generate_moves(MoveList& list);
	void unmake_move(Piece dst_prev, uint16_t move);
	void make_move(uint16_t move);
	bool is_square_attacked(int king_index, bool white_attacking);
	int score_move(uint16_t move);
};