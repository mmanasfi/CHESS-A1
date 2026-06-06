#include "Engine.h"
#include "Magic.h"

int Engine::eval(Board& board)
{
	int ret = 0;

	unsigned long general_source_index = 0;

	//0-5 for white, 6-11 for black
	for (int i = 0; i < 12; i++)
	{
		uint64_t current_all_side_pieces = board.pieces_occ[i];
		while (_BitScanForward64(&general_source_index, current_all_side_pieces))
		{
			ret += ((ENGINE_CONSTANTS::PIECE_VALUE[i]) + (ENGINE_CONSTANTS::ALL_PST[i][general_source_index])); // raw value + pos value
			current_all_side_pieces &= (current_all_side_pieces - 1);
		}
	}
	return ret;
}

uint16_t Engine::minmax_best_move(Board& board, int depth)
{
	MoveList all_moves;
	board.generate_moves(all_moves);
	uint16_t most_beneficial_move = UINT_MAX;
	int most_beneficial_move_eval = board.white_to_move ? INT_MIN : INT_MAX;

	// selection sort
	for (int i = 0; i <	all_moves.count; i++)
	{
		int max = all_moves.scores[i];
		int max_idx = i;
		for (int j = i + 1; j < all_moves.count; j++)
		{
			if (all_moves.scores[j] > max)
			{
				max = all_moves.scores[j];
				max_idx = j;
			}
		}
		if (max_idx != i)
		{
			int dst_s = all_moves.scores[i];
			all_moves.scores[i] = all_moves.scores[max_idx];
			all_moves.scores[max_idx] = dst_s;

			int dst_m = all_moves.moves[i];
			all_moves.moves[i] = all_moves.moves[max_idx];
			all_moves.moves[max_idx] = dst_m;
		}
		uint16_t curMove = all_moves.moves[i];
		Piece piece_possibly_captured = board.get_piece((curMove & 0x0fc0) >> 6);
		uint8_t old_cas_flags = board.castling_allowed;
		board.make_move(curMove);
		unsigned long temp_index = 0;
		if (board.white_to_move)
			BitScanForward64(&temp_index, board.pieces_occ[Piece::B_KING]);
		else
			BitScanForward64(&temp_index, board.pieces_occ[Piece::W_KING]);
		if (board.is_square_attacked(temp_index, board.white_to_move))
		{
			board.unmake_move(piece_possibly_captured, curMove);
			board.castling_allowed = old_cas_flags;
			continue;
		}
		int cur_move_best_eval = search(board, depth - 1, INT_MIN, INT_MAX);
		if (board.white_to_move)
		{
			if (cur_move_best_eval < most_beneficial_move_eval)
			{
				most_beneficial_move_eval = cur_move_best_eval;
				most_beneficial_move = curMove;
			}
		}
		else
		{
			if (cur_move_best_eval > most_beneficial_move_eval)
			{
				most_beneficial_move_eval = cur_move_best_eval;
				most_beneficial_move = curMove;
			}
		}
		board.unmake_move(piece_possibly_captured, curMove);
		board.castling_allowed = old_cas_flags;
	}

	return most_beneficial_move;
}

int Engine::search(Board& board, int depth, int alpha, int beta)
{
	if (depth == 0)
		return eval(board);

	MoveList all_moves;
	board.generate_moves(all_moves);
	uint16_t most_beneficial_move = UINT_MAX;
	int most_beneficial_move_eval = board.white_to_move ? INT_MIN : INT_MAX;

	for (int i = 0; i < all_moves.count; i++)
	{
		int max = all_moves.scores[i];
		int max_idx = i;
		for (int j = i + 1; j < all_moves.count; j++)
		{
			if (all_moves.scores[j] > max)
			{
				max = all_moves.scores[j];
				max_idx = j;
			}
		}
		if (max_idx != i)
		{
			int dst_s = all_moves.scores[i];
			all_moves.scores[i] = all_moves.scores[max_idx];
			all_moves.scores[max_idx] = dst_s;

			int dst_m = all_moves.moves[i];
			all_moves.moves[i] = all_moves.moves[max_idx];
			all_moves.moves[max_idx] = dst_m;
		}
		uint16_t curMove = all_moves.moves[i];
		Piece piece_possibly_captured = board.get_piece((curMove & 0x0fc0) >> 6);
		uint8_t old_cas_flags = board.castling_allowed;
		board.make_move(curMove);
		unsigned long temp_index = 0;
		if (board.white_to_move)
			BitScanForward64(&temp_index, board.pieces_occ[Piece::B_KING]);
		else
			BitScanForward64(&temp_index, board.pieces_occ[Piece::W_KING]);
		if (board.is_square_attacked(temp_index, board.white_to_move))
		{
			board.unmake_move(piece_possibly_captured, curMove);
			board.castling_allowed = old_cas_flags;
			continue;
		}
		int cur_move_best_eval = search(board, depth - 1, alpha, beta); //, INT_MAX, INT_MIN
		if (board.white_to_move)
		{
			if (cur_move_best_eval <= alpha)
			{
				board.unmake_move(piece_possibly_captured, curMove);
				board.castling_allowed = old_cas_flags;
				return cur_move_best_eval;
			}
			if (cur_move_best_eval < most_beneficial_move_eval)
			{
				most_beneficial_move_eval = cur_move_best_eval;
				most_beneficial_move = curMove;
				if (cur_move_best_eval < beta)
					beta = cur_move_best_eval;
			}

		}
		else
		{
			if (cur_move_best_eval >= beta)
			{
				board.unmake_move(piece_possibly_captured, curMove);
				board.castling_allowed = old_cas_flags;
				return cur_move_best_eval;
			}
			if (cur_move_best_eval > most_beneficial_move_eval)
			{
				most_beneficial_move_eval = cur_move_best_eval;
				most_beneficial_move = curMove;
				if (cur_move_best_eval > alpha)
					alpha = cur_move_best_eval;
			}
		}
		board.unmake_move(piece_possibly_captured, curMove);
		board.castling_allowed = old_cas_flags;
	}
	return most_beneficial_move_eval;
}
