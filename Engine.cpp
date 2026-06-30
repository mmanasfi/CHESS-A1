#include "Engine.h"
#include "Magic.h"

const int Qcap = 4;

int Engine::eval(Board& board)
{
	int ret = 0;
	int mg_score = 0;
	int eg_score = 0;
	int phase_det = 0;

	unsigned long general_source_index = 0;

	//0-5 for white, 6-11 for black
	for (int i = 0; i < 12; i++)
	{
		uint64_t current_all_side_pieces = board.pieces_occ[i];
		while (_BitScanForward64(&general_source_index, current_all_side_pieces))
		{
			phase_det += ENGINE_CONSTANTS::PHASE_VALUE[i % 6]; // wrap around, starting to optimize shit instead of having duplicate entries for black/white.
			int xor_ = i < 6 ? 0 : 56; // xoring square index with 56 gives black's position in "white" terms to use the white PST (mirrored)
			int multip = i < 6 ? 1 : -1;
			mg_score += ((ENGINE_CONSTANTS::PIECE_VALUE[i]) + (ENGINE_CONSTANTS::MG_PST[i % 6][general_source_index ^ xor_])* multip);
			eg_score += ((ENGINE_CONSTANTS::PIECE_VALUE[i]) + (ENGINE_CONSTANTS::EG_PST[i % 6][general_source_index ^ xor_])* multip);

			current_all_side_pieces &= (current_all_side_pieces - 1);
		}
	}
	float how_close_to_eg = phase_det / 40.f; // 1 implies furthest, as in starting position, 0 implies eg
	ret += (mg_score * how_close_to_eg) + (eg_score * (1 - how_close_to_eg));
	return ret;
}

int Engine::Quiesce(Board& board, int alpha, int beta, int cap)
{
	cur_qsearch_nodes++;

	int starting_eval = eval(board);
	if (board.white_to_move && starting_eval > alpha)
		alpha = starting_eval;
	else if (!board.white_to_move && starting_eval < beta)
		beta = starting_eval;

	if (alpha >= beta)
	{
		return starting_eval;
	}
	if (cap == 0)
		return starting_eval;
	MoveList all_moves;
	board.generate_moves(all_moves);
	int most_beneficial_move_eval = board.white_to_move ? alpha : beta;
	for (int i = 0; i < all_moves.count; i++)
	{
		uint16_t curMove = all_moves.moves[i];
		int cur_move_flags = (curMove >> 12);
		if (cur_move_flags == MoveFlags::CAPTURE|| cur_move_flags == MoveFlags::EN_PASSANT_CAPTURE || cur_move_flags >= 8)
		{
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
			int cur_move_best_eval = Quiesce(board, alpha, beta,cap-1);
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
					if (cur_move_best_eval > alpha)
						alpha = cur_move_best_eval;
				}
			}
			board.unmake_move(piece_possibly_captured, curMove);
			board.castling_allowed = old_cas_flags;
		}
	}
	return most_beneficial_move_eval;
}

uint16_t Engine::minmax_best_move(Board& board, int depth, uint16_t cur_best_move_ID)
{
	cur_search_nodes = 0;
	cur_qsearch_nodes = 0;

	MoveList all_moves;
	board.generate_moves(all_moves);
	uint16_t most_beneficial_move = UINT_MAX;
	int most_beneficial_move_eval = board.white_to_move ? INT_MIN : INT_MAX;

	// place ID move at index = 0, to play first then move on with selection sort 
	if (cur_best_move_ID != 0)
	{
		for (int i = 0; i < all_moves.count; i++)
		{
			uint16_t cur_move = all_moves.moves[i];
			if (cur_move == cur_best_move_ID)
			{
				uint16_t first = all_moves.moves[0];
				int first_score = all_moves.scores[0];
				all_moves.moves[0] = cur_best_move_ID;
				all_moves.scores[0] = all_moves.scores[i];
				all_moves.moves[i] = first;
				all_moves.scores[i] = first_score;
				break;
			}
		}
	}

	// selection sort
	for (int i = cur_best_move_ID == 0 ? 0 : 1; i <	all_moves.count; i++)
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
	
	}
	for (int i = 0; i < all_moves.count; i++)
	{
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

	cur_prediction_eval = most_beneficial_move_eval;

	return most_beneficial_move;
}

int Engine::search(Board& board, int depth, int alpha, int beta)
{
	cur_search_nodes++;

	if (depth == 0)
		return Quiesce(board, alpha, beta, Qcap);

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
