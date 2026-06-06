#include "Board.h"
#include "Magic.h"
#include <Engine.h>

Board::Board()
{
	clear_and_setup();
}

void Board::clear_and_setup()
{
	white_to_move = true;
	total_moves_amt = 0;
	total_occ = 0;
	current_enpassant_index = 64;
	memset(pieces_occ, 0, sizeof(pieces_occ));
	memset(side_total_occ, 0, sizeof(side_total_occ));
	for (int i = 0; i < 64; i++)
	{
		board_arr[i] = Piece::EMPTY;
	}
	Piece piece_order[] = { W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, W_BISHOP, W_KNIGHT, W_ROOK };
	for (int i = 0; i < 8; i++)
	{
		set_piece(56 + i, piece_order[i]);
		set_piece(48 + i, W_PAWN);

		set_piece(i, (Piece)(piece_order[i] + 6));
		set_piece(8 + i, B_PAWN);
	}
	castling_allowed = 0b1111;
}

void Board::set_piece(uint8_t square, Piece piece)
{

	uint64_t temp = (1ULL << square);
	board_arr[square] = piece;
	pieces_occ[piece] |= temp;
	side_total_occ[piece / 6] |= temp;
	total_occ |= temp;
}

void Board::remove_piece(uint8_t square)
{
	uint64_t temp = (1ULL << square);
	Piece to_remove = (Piece)board_arr[square];
	if (to_remove == Piece::EMPTY)
		return;
	board_arr[square] = Piece::EMPTY;
	pieces_occ[to_remove] &= ~temp;
	side_total_occ[to_remove / 6] &= ~temp;
	total_occ &= ~temp;
}

void Board::move_piece(uint8_t squareSrc, uint8_t squareDst)
{
	Piece p = get_piece(squareSrc);
	remove_piece(squareDst);
	remove_piece(squareSrc);
	set_piece(squareDst, p);
	total_moves_amt++;

	white_to_move = !white_to_move;
	castling_allowed &= (MAGIC_CONSTANTS::CASTLE_LUT[squareSrc] & MAGIC_CONSTANTS::CASTLE_LUT[squareDst]);
}

Piece Board::get_piece(uint8_t square)
{
	return (Piece)board_arr[square];
}

int Board::score_move(uint16_t move)
{
	int flag = (move >> 12);
	int cap_piece_value = ENGINE_CONSTANTS::PIECE_VALUE[get_piece((move & 0x0fc0) >> 6)];
	int mask_c = cap_piece_value >> 31;

	int src_piece_value = ENGINE_CONSTANTS::PIECE_VALUE[get_piece(move & 0x003f)];
	int mask_s = src_piece_value >> 31;

	return MoveList::MoveScores[flag] + (((MoveList::IS_CAPTURE[flag] * ((cap_piece_value ^ mask_c) - (mask_c))) * 10) - (MoveList::IS_CAPTURE[flag] * ((src_piece_value ^ mask_s) - (mask_s)))); // nice lil abs value trick
}
void Board::generate_moves(MoveList& list)
{
	memset(list.moves, 0, sizeof(list.moves));
	list.count = 0;
	if (white_to_move)
	{
		unsigned long general_source_index = 0;
		uint8_t flags = MoveFlags::NORMAL_MOVE;
		uint16_t curMove = 0;

		uint64_t pawn_forward_push = (pieces_occ[Piece::W_PAWN] >> 8) & ~total_occ;
		uint64_t pawn_double_push = ((pawn_forward_push & 0x0000FF0000000000ULL) >> 8) & ~total_occ; // did the forward push land the pawn on the third row? -> send that shit forward
		uint64_t pawn_left_capture = ((pieces_occ[Piece::W_PAWN] & 0xfefefefefefefefe) >> 9) & side_total_occ[1]; // 1111 1110 mask, to pretend A pawns dont exit
		uint64_t pawn_right_capture = ((pieces_occ[Piece::W_PAWN] & 0x7f7f7f7f7f7f7f7f) >> 7) & side_total_occ[1]; // 0111 1111 mask, to pretend H pawns dont exit
		uint64_t pawn_left_in_pissing_capture = ((pieces_occ[Piece::W_PAWN] & 0xfefefefefefefefe) >> 9) & (1ULL << current_enpassant_index);
		uint64_t pawn_right_in_pissing_capture = ((pieces_occ[Piece::W_PAWN] & 0x7f7f7f7f7f7f7f7f) >> 7) & (1ULL << current_enpassant_index);

		uint64_t all_white_rooks = pieces_occ[Piece::W_ROOK];
		while (_BitScanForward64(&general_source_index, all_white_rooks))
		{
			uint64_t rook_masked_board = MAGIC_CONSTANTS::ROOK_MASKS[general_source_index] & total_occ;

			int magic_index = (rook_masked_board * MAGIC_CONSTANTS::ROOK_MAGICS[general_source_index]) >> (64 - 12);
			uint64_t rook_moves = MAGIC_CONSTANTS::ROOK_MOVES[general_source_index][magic_index] & ~side_total_occ[0];
			unsigned long rook_move_index = 0;
			while (_BitScanForward64(&rook_move_index, rook_moves))
			{
				curMove = (((((1ULL << rook_move_index)&side_total_occ[1]) != 0) << 1) << 12) | (rook_move_index << 6) | (general_source_index); // flag fuckery just checks if its a capture if true (1) shfits by one to make it 2 (CAP flag)
				list.add_move(curMove, score_move(curMove));
				rook_moves &= (rook_moves - 1);
			}
			all_white_rooks &= (all_white_rooks - 1);
		}

		uint64_t all_white_bishops = pieces_occ[Piece::W_BISHOP];
		while (_BitScanForward64(&general_source_index, all_white_bishops))
		{
			uint64_t bishop_masked_board = MAGIC_CONSTANTS::BISHOP_MASKS[general_source_index] & total_occ;

			int magic_index = (bishop_masked_board * MAGIC_CONSTANTS::BISHOP_MAGICS[general_source_index]) >> (64 - 9);
			uint64_t bishop_moves = MAGIC_CONSTANTS::BISHOP_MOVES[general_source_index][magic_index] & ~side_total_occ[0];
			unsigned long bishop_move_index = 0;
			while (_BitScanForward64(&bishop_move_index, bishop_moves))
			{
				curMove = (((((1ULL << bishop_move_index) & side_total_occ[1]) != 0) << 1) << 12) | (bishop_move_index << 6) | (general_source_index);
				list.add_move(curMove, score_move(curMove));
				bishop_moves &= (bishop_moves - 1);
			}
			all_white_bishops &= (all_white_bishops - 1);
		}

		uint64_t all_white_knights = pieces_occ[Piece::W_KNIGHT];
		while (_BitScanForward64( &general_source_index, all_white_knights))
		{
			uint64_t knight_moves = MAGIC_CONSTANTS::KNIGHT_MOVES[general_source_index] & ~side_total_occ[0];
			unsigned long knight_move_index = 0;
			while (_BitScanForward64( &knight_move_index, knight_moves))
			{
				curMove = (((((1ULL << knight_move_index) & side_total_occ[1]) != 0) << 1) << 12) | (knight_move_index << 6) | (general_source_index);
				list.add_move(curMove, score_move(curMove));
				knight_moves &= (knight_moves - 1);
			}			
			all_white_knights &= (all_white_knights - 1);
		}

		uint64_t king = pieces_occ[Piece::W_KING];
		unsigned long king_index = 0;
		_BitScanForward64( &king_index, king);
		if (king_index < 64)
		{
			uint64_t king_moves = MAGIC_CONSTANTS::KING_MOVES[king_index] & ~side_total_occ[0];
			unsigned long king_move_index = 0;
			while (_BitScanForward64( &king_move_index, king_moves))
			{
				curMove = (((((1ULL << king_move_index) & side_total_occ[1]) != 0) << 1) << 12) | (king_move_index << 6) | (king_index);
				list.add_move(curMove, score_move(curMove));
				king_moves &= (king_moves - 1);
			}
			if (castling_allowed & 1)
			{
				if (get_piece(61) == EMPTY && get_piece(62) == EMPTY) 
				{
					if (!is_square_attacked(king_index, false) && !is_square_attacked(61, false) && !is_square_attacked(62, false))
					{
						uint16_t WKS = (king_index) | (62 << 6) | (MoveFlags::CASTLE_KING_WHITE << 12);
						list.add_move(WKS, score_move(curMove));
					}
				}
			}
			if (castling_allowed & 0b10)
			{
				if (get_piece(57) == EMPTY && get_piece(58) == EMPTY && get_piece(59) == EMPTY)
				{
					if (!is_square_attacked(58, false) && !is_square_attacked(59, false) && !is_square_attacked(king_index, false))
					{
						uint16_t WQS = (king_index) | (58 << 6) | (MoveFlags::CASTLE_QUEEN_WHITE << 12);
						list.add_move(WQS, score_move(curMove));
					}
				}
			}
		}


		uint64_t all_white_queens = pieces_occ[Piece::W_QUEEN];
		while (_BitScanForward64( &general_source_index, all_white_queens))
		{
			uint64_t queen_bishop_masked_board = MAGIC_CONSTANTS::BISHOP_MASKS[general_source_index] & total_occ;
			int queen_diag_magic_index = (queen_bishop_masked_board * MAGIC_CONSTANTS::BISHOP_MAGICS[general_source_index]) >> (64 - 9);
			uint64_t queen_diag_moves = MAGIC_CONSTANTS::BISHOP_MOVES[general_source_index][queen_diag_magic_index] & ~side_total_occ[0];

			uint64_t queen_rook_masked_board = MAGIC_CONSTANTS::ROOK_MASKS[general_source_index] & total_occ;
			int queen_side_magic_index = (queen_rook_masked_board * MAGIC_CONSTANTS::ROOK_MAGICS[general_source_index]) >> (64 - 12);
			uint64_t queen_side_moves = MAGIC_CONSTANTS::ROOK_MOVES[general_source_index][queen_side_magic_index] & ~side_total_occ[0];

			uint64_t queen_moves = queen_diag_moves | queen_side_moves;
			unsigned long queen_move_index = 0;
			while (_BitScanForward64( &queen_move_index, queen_moves))
			{
				curMove = (((((1ULL << queen_move_index) & side_total_occ[1]) != 0) << 1) << 12) | (queen_move_index << 6) | (general_source_index);
				list.add_move(curMove, score_move(curMove));
				queen_moves &= (queen_moves - 1);
			}
			all_white_queens &= (all_white_queens - 1);
		}

		if (_BitScanForward64( &general_source_index, pawn_left_in_pissing_capture))
		{
			curMove = (MoveFlags::EN_PASSANT_CAPTURE << 12) | (general_source_index << 6) | (general_source_index + 9);
			list.add_move(curMove, score_move(curMove));
		}
		if (_BitScanForward64( &general_source_index, pawn_right_in_pissing_capture))
		{
			curMove = (MoveFlags::EN_PASSANT_CAPTURE << 12) | (general_source_index << 6) | (general_source_index + 7);
			list.add_move(curMove, score_move(curMove));
		}
		while (_BitScanForward64( &general_source_index, pawn_left_capture))
		{
			flags = MoveFlags::CAPTURE;
			if (general_source_index < 8)
			{
				for (int i = MoveFlags::CAP_PROMO_ROOK; i <= MoveFlags::CAP_PROMO_QUEEN; i++)
				{
					curMove = (i << 12) | (general_source_index << 6) | (general_source_index + 9);
					list.add_move(curMove, score_move(curMove));
				}
			}
			else
			{
				curMove = (flags << 12) | (general_source_index << 6) | (general_source_index + 9);
				list.add_move(curMove, score_move(curMove));
			}
			pawn_left_capture &= (pawn_left_capture - 1);
		}
		while (_BitScanForward64( &general_source_index, pawn_right_capture))
		{
			flags = MoveFlags::CAPTURE;
			curMove = 0;
			if (general_source_index < 8)
			{
				for (int i = MoveFlags::CAP_PROMO_ROOK; i <= MoveFlags::CAP_PROMO_QUEEN; i++)
				{
					curMove = (i << 12) | (general_source_index << 6) | (general_source_index + 7);
					list.add_move(curMove, score_move(curMove));
				}
			}
			else
			{
				curMove = (flags << 12) | (general_source_index << 6) | (general_source_index + 7);
				list.add_move(curMove, score_move(curMove));
			}
			pawn_right_capture &= (pawn_right_capture - 1);
		}
		while (_BitScanForward64(  &general_source_index, pawn_forward_push))
		{
			flags = 0;
			curMove = 0;
			if (general_source_index < 8)
			{
				for (int i = MoveFlags::PROMO_ROOK; i <= MoveFlags::PROMO_QUEEN; i++)
				{
					curMove = (i << 12) | (general_source_index << 6) | (general_source_index + 8);
					list.add_move(curMove, score_move(curMove));
				}
			}
			else
			{
				curMove = (0 << 12) | (general_source_index << 6) | (general_source_index + 8); // no flags
				list.add_move(curMove, score_move(curMove));
			}	
			pawn_forward_push &= (pawn_forward_push - 1);
		}
		while (_BitScanForward64( &general_source_index, pawn_double_push))
		{
			curMove = (MoveFlags::DOUBLE_PUSH_PAWN << 12) | (general_source_index << 6) | (general_source_index + 16); // no flags
			list.add_move(curMove, score_move(curMove));
			pawn_double_push &= (pawn_double_push - 1);
		}
	}
	else
	{
		unsigned long general_source_index = 0;
		uint8_t flags = MoveFlags::NORMAL_MOVE;
		uint16_t curMove = 0;

		uint64_t pawn_forward_push = (pieces_occ[Piece::B_PAWN] << 8) & ~total_occ;
		uint64_t pawn_double_push = ((pawn_forward_push & 0x0000000000FF0000ULL) << 8) & ~total_occ;
		uint64_t pawn_left_capture = ((pieces_occ[Piece::B_PAWN] & 0x7f7f7f7f7f7f7f7f) << 9) & side_total_occ[0]; // 1111 1110 mask, to pretend A pawns dont exit
		uint64_t pawn_right_capture = ((pieces_occ[Piece::B_PAWN] & 0xfefefefefefefefe) << 7) & side_total_occ[0]; // 0111 1111 mask, to pretend H pawns dont exit
		uint64_t pawn_left_in_pissing_capture = ((pieces_occ[Piece::B_PAWN] & 0x7f7f7f7f7f7f7f7f) << 9) & (1ULL << current_enpassant_index);
		uint64_t pawn_right_in_pissing_capture = ((pieces_occ[Piece::B_PAWN] & 0xfefefefefefefefe) << 7) & (1ULL << current_enpassant_index);

		uint64_t all_black_rooks = pieces_occ[Piece::B_ROOK];
	while (_BitScanForward64( &general_source_index, all_black_rooks))
	{
		uint64_t rook_masked_board = MAGIC_CONSTANTS::ROOK_MASKS[general_source_index] & total_occ;

		int magic_index = (rook_masked_board * MAGIC_CONSTANTS::ROOK_MAGICS[general_source_index]) >> (64 - 12);
		uint64_t rook_moves = MAGIC_CONSTANTS::ROOK_MOVES[general_source_index][magic_index] & ~side_total_occ[1];
		unsigned long rook_move_index = 0;
		while (_BitScanForward64( &rook_move_index, rook_moves))
		{
			curMove = (((((1ULL << rook_move_index) & side_total_occ[0]) != 0) << 1) << 12) | (rook_move_index << 6) | (general_source_index); // flag fuckery just checks if its a capture if true (1) shfits by one to make it 2 (CAP flag)
			list.add_move(curMove, score_move(curMove));
			rook_moves &= (rook_moves - 1);
		}
		all_black_rooks &= (all_black_rooks - 1);
	}

	uint64_t all_black_bishops = pieces_occ[Piece::B_BISHOP];
	while (_BitScanForward64( &general_source_index, all_black_bishops))
	{
		uint64_t bishop_masked_board = MAGIC_CONSTANTS::BISHOP_MASKS[general_source_index] & total_occ;

		int magic_index = (bishop_masked_board * MAGIC_CONSTANTS::BISHOP_MAGICS[general_source_index]) >> (64 - 9);
		uint64_t bishop_moves = MAGIC_CONSTANTS::BISHOP_MOVES[general_source_index][magic_index] & ~side_total_occ[1];
		unsigned long bishop_move_index = 0;
		while (_BitScanForward64( &bishop_move_index, bishop_moves))
		{
			curMove = (((((1ULL << bishop_move_index) & side_total_occ[0]) != 0) << 1) << 12) | (bishop_move_index << 6) | (general_source_index);
			list.add_move(curMove, score_move(curMove));
			bishop_moves &= (bishop_moves - 1);
		}
		all_black_bishops &= (all_black_bishops - 1);
	}

	uint64_t all_black_knights = pieces_occ[Piece::B_KNIGHT];
	while (_BitScanForward64( &general_source_index, all_black_knights))
	{
		uint64_t knight_moves = MAGIC_CONSTANTS::KNIGHT_MOVES[general_source_index] & ~side_total_occ[1];
		unsigned long knight_move_index = 0;
		while (_BitScanForward64( &knight_move_index, knight_moves))
		{
			curMove = (((((1ULL << knight_move_index) & side_total_occ[0]) != 0) << 1) << 12) | (knight_move_index << 6) | (general_source_index);
			list.add_move(curMove, score_move(curMove));
			knight_moves &= (knight_moves - 1);
		}
		all_black_knights &= (all_black_knights - 1);
	}

	uint64_t king = pieces_occ[Piece::B_KING];
	unsigned long king_index = 0;
	_BitScanForward64( &king_index, king);
	if (king_index < 64)
	{
		uint64_t king_moves = MAGIC_CONSTANTS::KING_MOVES[king_index] & ~side_total_occ[1];
		unsigned long king_move_index = 0;
		while (_BitScanForward64( &king_move_index, king_moves))
		{
			curMove = (((((1ULL << king_move_index) & side_total_occ[0]) != 0) << 1) << 12) | (king_move_index << 6) | (king_index);
			list.add_move(curMove, score_move(curMove));
			king_moves &= (king_moves - 1);
		}
		if (castling_allowed & 0b100)
		{
			if (get_piece(5) == EMPTY && get_piece(6) == EMPTY)
			{
				if (!is_square_attacked(5, true) && !is_square_attacked(6, true) && !is_square_attacked(king_index, true))
				{
					uint16_t BKS = (king_index) | (6 << 6) | (MoveFlags::CASTLE_KING_BLACK << 12);
					list.add_move(BKS, score_move(curMove));
				}
			}
		}
		if (castling_allowed & 0b1000)
		{
			if (get_piece(1) == EMPTY && get_piece(2) == EMPTY && get_piece(3) == EMPTY)
			{
				if (!is_square_attacked(3, true) && !is_square_attacked(2, true) && !is_square_attacked(king_index, true))
				{
					uint16_t BQS = (king_index) | (2 << 6) | (MoveFlags::CASTLE_QUEEN_BLACK << 12);
					list.add_move(BQS, score_move(curMove));
				}
			}
		}
	}

	uint64_t all_black_queens = pieces_occ[Piece::B_QUEEN];
	while (_BitScanForward64( &general_source_index, all_black_queens))
	{
		uint64_t queen_bishop_masked_board = MAGIC_CONSTANTS::BISHOP_MASKS[general_source_index] & total_occ;
		int queen_diag_magic_index = (queen_bishop_masked_board * MAGIC_CONSTANTS::BISHOP_MAGICS[general_source_index]) >> (64 - 9);
		uint64_t queen_diag_moves = MAGIC_CONSTANTS::BISHOP_MOVES[general_source_index][queen_diag_magic_index] & ~side_total_occ[1];

		uint64_t queen_rook_masked_board = MAGIC_CONSTANTS::ROOK_MASKS[general_source_index] & total_occ;
		int queen_side_magic_index = (queen_rook_masked_board * MAGIC_CONSTANTS::ROOK_MAGICS[general_source_index]) >> (64 - 12);
		uint64_t queen_side_moves = MAGIC_CONSTANTS::ROOK_MOVES[general_source_index][queen_side_magic_index] & ~side_total_occ[1];

		uint64_t queen_moves = queen_diag_moves | queen_side_moves;
		unsigned long queen_move_index = 0;
		while (_BitScanForward64( &queen_move_index, queen_moves))
		{
			curMove = (((((1ULL << queen_move_index) & side_total_occ[0]) != 0) << 1) << 12) | (queen_move_index << 6) | (general_source_index);
			list.add_move(curMove, score_move(curMove));
			queen_moves &= (queen_moves - 1);
		}
		all_black_queens &= (all_black_queens - 1);
	}

	if (_BitScanForward64( &general_source_index, pawn_left_in_pissing_capture))
	{
		curMove = (MoveFlags::EN_PASSANT_CAPTURE << 12) | (general_source_index << 6) | (general_source_index - 9);
		list.add_move(curMove, score_move(curMove));
	}
	if (_BitScanForward64( &general_source_index, pawn_right_in_pissing_capture))
	{
		curMove = (MoveFlags::EN_PASSANT_CAPTURE << 12) | (general_source_index << 6) | (general_source_index - 7);
		list.add_move(curMove, score_move(curMove));
	}
	while (_BitScanForward64( &general_source_index, pawn_left_capture))
	{
		flags = MoveFlags::CAPTURE;
		if (general_source_index > 55)
		{
			for (int i = MoveFlags::CAP_PROMO_ROOK; i <= MoveFlags::CAP_PROMO_QUEEN; i++)
			{
				curMove = (i << 12) | (general_source_index << 6) | (general_source_index - 9);
				list.add_move(curMove, score_move(curMove));
			}
		}
		else
		{
			curMove = (flags << 12) | (general_source_index << 6) | (general_source_index - 9);
			list.add_move(curMove, score_move(curMove));
		}
		pawn_left_capture &= (pawn_left_capture - 1);
	}
	while (_BitScanForward64( &general_source_index, pawn_right_capture))
	{
		flags = MoveFlags::CAPTURE;
		curMove = 0;
		if (general_source_index > 55) 
		{
			for (int i = MoveFlags::CAP_PROMO_ROOK; i <= MoveFlags::CAP_PROMO_QUEEN; i++)
			{
				curMove = (i << 12) | (general_source_index << 6) | (general_source_index - 7);
				list.add_move(curMove, score_move(curMove));
			}
		}
		else
		{
			curMove = (flags << 12) | (general_source_index << 6) | (general_source_index - 7);
			list.add_move(curMove, score_move(curMove));
		}
		pawn_right_capture &= (pawn_right_capture - 1);
	}
	while (_BitScanForward64( &general_source_index, pawn_forward_push))
	{
		flags = 0;
		curMove = 0;
		if (general_source_index > 55)
		{
			for (int i = MoveFlags::PROMO_ROOK; i <= MoveFlags::PROMO_QUEEN; i++)
			{
				curMove = (i << 12) | (general_source_index << 6) | (general_source_index - 8);
				list.add_move(curMove, score_move(curMove));
			}
		}
		else
		{
			curMove = (0 << 12) | (general_source_index << 6) | (general_source_index - 8); 
			list.add_move(curMove, score_move(curMove));
		}
		pawn_forward_push &= (pawn_forward_push - 1);
	}
	while (_BitScanForward64( &general_source_index, pawn_double_push))
	{
		curMove = (MoveFlags::DOUBLE_PUSH_PAWN << 12) | (general_source_index << 6) | (general_source_index - 16); 
		list.add_move(curMove, score_move(curMove));
		pawn_double_push &= (pawn_double_push - 1);
	}
	}
}

void Board::unmake_move(Piece dst_prev, uint16_t move)
{
	move_piece(((move & 0x0fc0) >> 6), move & 0x003f);
	if(dst_prev != 12)
		set_piece(((move & 0x0fc0) >> 6), dst_prev);
	if ((move >> 12) > 7)	
	{
		remove_piece(move & 0x003f);
		set_piece(move & 0x003f, (Piece)(Piece::W_PAWN+!white_to_move*6));
	}
	if ((move >> 12) == MoveFlags::CASTLE_KING_WHITE)
	{
		move_piece(61, 63); 
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_QUEEN_WHITE)
	{
		move_piece(59, 56);
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_QUEEN_BLACK)
	{
		move_piece(3, 0);
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_KING_BLACK)
	{
		move_piece(5, 7);
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
	total_moves_amt -= 2;
}

Piece promo_arr[2][8] = { {Piece::B_ROOK,Piece::B_KNIGHT,Piece::B_BISHOP,Piece::B_QUEEN,Piece::B_ROOK,Piece::B_KNIGHT,Piece::B_BISHOP,Piece::B_QUEEN},{Piece::W_ROOK,Piece::W_KNIGHT,Piece::W_BISHOP,Piece::W_QUEEN,Piece::W_ROOK,Piece::W_KNIGHT,Piece::W_BISHOP,Piece::W_QUEEN} };
void Board::make_move(uint16_t move)
{
	move_piece(move & 0x003f, ((move & 0x0fc0) >> 6));
	if ((move >> 12) > 7)
	{
		remove_piece(((move & 0x0fc0) >> 6));
		set_piece(((move & 0x0fc0) >> 6), promo_arr[!white_to_move][((move >> 12) - 8)]);
	}

	if ((move >> 12) == MoveFlags::CASTLE_KING_WHITE)
	{
		move_piece(63, 61);
		white_to_move = !white_to_move; 
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_QUEEN_WHITE)
	{
		move_piece(56, 59);
		white_to_move = !white_to_move; 
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_QUEEN_BLACK)
	{
		move_piece(0, 3);
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
	else if ((move >> 12) == MoveFlags::CASTLE_KING_BLACK)
	{
		move_piece(7, 5);
		white_to_move = !white_to_move;
		total_moves_amt--;
	}
}
bool Board::is_square_attacked(int king_index, bool white_attacking)
{
	if (white_attacking)
	{
		uint64_t GKS_rook_mask = MAGIC_CONSTANTS::ROOK_MASKS[king_index] & total_occ; //GKS: ghost-king-square
		int GKS_rook_magic_index = (GKS_rook_mask * MAGIC_CONSTANTS::ROOK_MAGICS[king_index]) >> (64 - 12);
		uint64_t GKS_rook_moves = MAGIC_CONSTANTS::ROOK_MOVES[king_index][GKS_rook_magic_index];

		if (GKS_rook_moves & (pieces_occ[Piece::W_ROOK] | pieces_occ[Piece::W_QUEEN]))
			return true;

		uint64_t GKS_bishop_mask = MAGIC_CONSTANTS::BISHOP_MASKS[king_index] & total_occ;
		int GKS_bishop_magic_index = (GKS_bishop_mask * MAGIC_CONSTANTS::BISHOP_MAGICS[king_index]) >> (64 - 9);
		uint64_t GKS_bishop_moves = MAGIC_CONSTANTS::BISHOP_MOVES[king_index][GKS_bishop_magic_index];

		if (GKS_bishop_moves & (pieces_occ[Piece::W_BISHOP] | pieces_occ[Piece::W_QUEEN]))
			return true;

		uint64_t GKS_knight_moves = MAGIC_CONSTANTS::KNIGHT_MOVES[king_index];

		if (GKS_knight_moves & pieces_occ[Piece::W_KNIGHT])
			return true;

		uint64_t GKS_king_moves = MAGIC_CONSTANTS::KING_MOVES[king_index];
		if (GKS_king_moves & pieces_occ[Piece::W_KING])
			return true;

		uint64_t pawn_left_capture = ((pieces_occ[Piece::W_PAWN] & 0xfefefefefefefefe) >> 9) & (1ULL << king_index);
		uint64_t pawn_right_capture = ((pieces_occ[Piece::W_PAWN] & 0x7f7f7f7f7f7f7f7f) >> 7) & (1ULL << king_index);
		if (pawn_left_capture | pawn_right_capture)
			return true;
	}
	else
	{
		uint64_t GKS_rook_mask = MAGIC_CONSTANTS::ROOK_MASKS[king_index] & total_occ; //GKS: ghost-king-square
		int GKS_rook_magic_index = (GKS_rook_mask * MAGIC_CONSTANTS::ROOK_MAGICS[king_index]) >> (64 - 12);
		uint64_t GKS_rook_moves = MAGIC_CONSTANTS::ROOK_MOVES[king_index][GKS_rook_magic_index];

		if (GKS_rook_moves & (pieces_occ[Piece::B_ROOK] | pieces_occ[Piece::B_QUEEN]))
			return true;

		uint64_t GKS_bishop_mask = MAGIC_CONSTANTS::BISHOP_MASKS[king_index] & total_occ;
		int GKS_bishop_magic_index = (GKS_bishop_mask * MAGIC_CONSTANTS::BISHOP_MAGICS[king_index]) >> (64 - 9);
		uint64_t GKS_bishop_moves = MAGIC_CONSTANTS::BISHOP_MOVES[king_index][GKS_bishop_magic_index];

		if (GKS_bishop_moves & (pieces_occ[Piece::B_BISHOP] | pieces_occ[Piece::B_QUEEN]))
			return true;

		uint64_t GKS_knight_moves = MAGIC_CONSTANTS::KNIGHT_MOVES[king_index];

		if (GKS_knight_moves & pieces_occ[Piece::B_KNIGHT])
			return true;

		uint64_t GKS_king_moves = MAGIC_CONSTANTS::KING_MOVES[king_index];
		if (GKS_king_moves & pieces_occ[Piece::B_KING])
			return true;

		uint64_t pawn_left_capture = ((pieces_occ[Piece::B_PAWN] & 0x7f7f7f7f7f7f7f7f) << 9) & (1ULL << king_index);
		uint64_t pawn_right_capture = ((pieces_occ[Piece::B_PAWN] & 0xfefefefefefefefe) << 7) & (1ULL << king_index);
		if (pawn_left_capture | pawn_right_capture)
			return true;
		
	}
	return false;
}


