#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

using namespace std;

struct DELTA {
    int8_t  X, Y;
    bool operator==(const DELTA& other) const {
        return X == other.X && Y == other.Y;
    }
    bool operator!=(const DELTA& other) const {
        return X != other.X || Y != other.Y;
    }
    bool operator>(const DELTA& other) const {
        return (X > other.X) || (X == other.X && Y > other.Y);
    }
};
struct CHESS_MOVE {
    int8_t  FROM_FILE, FROM_RANK, MOVED_PIECE, TO_FILE, TO_RANK, TARGET_PIECE, RESULTING_STATE;
};
static bool operator==(const CHESS_MOVE& lhs, const CHESS_MOVE& rhs) {
    return
        lhs.FROM_FILE == rhs.FROM_FILE &&
        lhs.FROM_RANK == rhs.FROM_RANK &&
        lhs.MOVED_PIECE == rhs.MOVED_PIECE &&
        lhs.TO_FILE == rhs.TO_FILE &&
        lhs.TO_RANK == rhs.TO_RANK &&
        lhs.TARGET_PIECE == rhs.TARGET_PIECE &&
        lhs.RESULTING_STATE == rhs.RESULTING_STATE;
}
enum SPECIAL_MOVES : int8_t {
    ILLEGAL_MOVE = -1,
    NON_ZERO_MOVES = -2,
    RESIGN = -3
};
constexpr CHESS_MOVE ILLEGAL_CHESS_MOVE = { ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE };
constexpr CHESS_MOVE NON_ZERO_CHESS_MOVES = { NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES };
constexpr CHESS_MOVE RESIGNATION_CHESS_MOVE = { RESIGN, RESIGN, RESIGN, RESIGN, RESIGN, RESIGN, RESIGN };
constexpr  int8_t  NO_INFO = -1;
enum CASTLE : int8_t {
    UNAVAILABLE = 0,
    QUEENSIDE = 1,
    KINGSIDE = 2,
    BOTH_SIDES = 3,
    CASTLED = 4
};
enum TURN : int8_t {
    WHITE = 1,
    BLACK = -1
};
enum PIECE : int8_t {
    NONE = 0,
    PAWN = 1,
    BISHOP = 3,
    KNIGHT = 4,
    ROOK = 5,
    QUEEN = 9,
    KING = 10
};
enum CONCLUSION : int8_t {
    IN_PROGRESS = -2,
    BLACK_WON = -1,
    DRAW = 0,
    WHITE_WON = 1
};
enum CONCLUSION_REASON : int8_t {
    NOT_CONCLUDED = -1,
    BY_AGREEMENT = 0,
    BY_STALEMATE = 1,
    BY_THREEFOLD_REPETITION = 2,
    BY_50MOVE_RULE = 3,
    BY_DEAD_POSITION = 4,
    BY_TIME_AND_INSUFFICIENT_MATERIAL = 5,
    BY_TIME = 6,
    BY_CHECKMATE = 7,
    BY_RESIGNATION = 8
};
enum RESULTING_STATE : int8_t {
    UNCHANGED = 0,
    CHECK = 1,
    STALEMATE = 2,
    CHECKMATE = 3
};

class CHESS_GAME {
public:
    vector<CHESS_MOVE> move_history;
    vector<int8_t> irreversability_history;
    vector<CHESS_MOVE> LegalMoves;
    bool irreversability_flag = false;
    int turn_count = 0;
    int8_t  whose_turn = WHITE, enpassant = NO_INFO, white_castling = BOTH_SIDES, black_castling = BOTH_SIDES
        , moves_since_irreversability = 0, game_conclusion = IN_PROGRESS, conclusion_reason = NOT_CONCLUDED;

    struct CASTLING_MEM {
        // For complete game reversability you need to have the castling state stored somewhere in memory,
        //unless you want to run the entire game from the beginning 
        // The castling state can get only 3 values for one side in one game. For example 3->4. 3->2->4. 3->1->0.
        //But the starting state is always 3, so to remember it you only need 2 state variables and 2
        //corresponding turn variables
        pair< int8_t  /*CASTLING_STATE*/, int8_t  /*CASTLING_CHANGE_TURN*/> CASTLING_CHANGE1, CASTLING_CHANGE2;
    };
    CASTLING_MEM white_castling_memory = { {NO_INFO, NO_INFO}, {NO_INFO, NO_INFO} };
    CASTLING_MEM black_castling_memory = { {NO_INFO, NO_INFO}, {NO_INFO, NO_INFO} };
    int8_t updateCastling(int8_t color) {
        const CASTLING_MEM& castlingMemory = color == WHITE ? white_castling_memory : black_castling_memory;
        int8_t& castling = color == WHITE ? white_castling : black_castling;
        if (castlingMemory.CASTLING_CHANGE2 != make_pair(NO_INFO, NO_INFO)) {
            castling = castlingMemory.CASTLING_CHANGE2.first;
        }
        else if (castlingMemory.CASTLING_CHANGE1 != make_pair(NO_INFO, NO_INFO)) {
            castling = castlingMemory.CASTLING_CHANGE1.first;
        }
        else {
            // Both are {-1 -1}, ensure a default value BOTH_SIDES
            castling = BOTH_SIDES;
        }
        return castling;
    }
    pair< int8_t, int8_t >& findLastCastling() {
        int8_t  maxWhite = max(white_castling_memory.CASTLING_CHANGE1.second, white_castling_memory.CASTLING_CHANGE2.second);
        int8_t  maxBlack = max(black_castling_memory.CASTLING_CHANGE1.second, black_castling_memory.CASTLING_CHANGE2.second);
        auto& castling_memory = maxWhite >= maxBlack ? white_castling_memory : black_castling_memory;
        if (castling_memory.CASTLING_CHANGE1.second >= castling_memory.CASTLING_CHANGE2.second) {
            return castling_memory.CASTLING_CHANGE1;
            cout << "castling order mistake" << endl;
        }
        else {
            return castling_memory.CASTLING_CHANGE2;
        }
    }
    pair< int8_t, int8_t >& findLastCastling(CASTLING_MEM& castling_memory) {
        if (castling_memory.CASTLING_CHANGE1.second >= castling_memory.CASTLING_CHANGE2.second) {
            return castling_memory.CASTLING_CHANGE1;
        }
        else {
            return castling_memory.CASTLING_CHANGE2;
        }
    }

    //En passant possibility memory, analogical to castling
    struct ENPASSANT_MEM {
        // For complete game reversability you need to have 8 slots for each side
        array</*ENPASSANT_ROW is the number of the element*/ int8_t  /*ENPASSANT_TURN*/, 8> WHITE_ENPASSANT_MEM_ARRAY;
        array< int8_t, 8> BLACK_ENPASSANT_MEM_ARRAY;
    };
    ENPASSANT_MEM enpassant_game_memory = { {
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO
    },{
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO,
         NO_INFO
    } };
    //Returns the turn of the enpassant
    int8_t& findLastEnpassant() {
        int8_t  maxE = -1;
        int8_t  maxI = -8;
        for (int8_t i = 0; i < 8; i++) {
            if (enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[i] > maxE) {
                maxE = enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[i];
                maxI = i;
            }
            if (enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[i] > maxE) {
                maxE = enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[i];
                maxI = -i - 1;
            }
        }
        return (maxI >= 0) ? enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[maxI] : enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[-maxI - 1];
    }
    // Returns a pair of both turn and col* in that order (* its not just col, for black its -col-1)
    pair< int8_t, int8_t > findLastEnpassantFull() {
        int8_t  maxE = -1;
        int8_t  maxI = -8;
        for (int8_t i = 0; i < 8; i++) {
            if (enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[i] > maxE) {
                maxE = enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[i];
                maxI = i;
            }
            if (enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[i] > maxE) {
                maxE = enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[i];
                maxI = -i - 1;
            }
        }
        return make_pair((maxI >= 0) ? enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[maxI] : enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[-maxI - 1], (maxI > 0) ? maxI : -maxI - 1);
    }

    int8_t  BoardState[8][8] = {
        {5, 4, 3, 9, 10, 3, 4, 5},
        {1, 1, 1, 1, 1, 1, 1, 1},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {-1, -1, -1, -1, -1, -1, -1, -1},
        {-5, -4, -3, -9, -10, -3, -4, -5}
    };

    // Function to calculate whether the king is in check
    bool isChecked(const  int8_t(&inputState)[8][8], int8_t  whoseTurn, int8_t  kx, int8_t  ky) {
        // Checking whether the king is being checked by a rook or a queen through a horizontal or vertical line
        for (int8_t row = kx + 1; row < 8; ++row) {
            if (inputState[row][ky] * whoseTurn == -ROOK || inputState[row][ky] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[row][ky] != NONE) {
                break;
            }
        }

        for (int8_t row = kx - 1; row >= NONE; --row) {
            if (inputState[row][ky] * whoseTurn == -ROOK || inputState[row][ky] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[row][ky] != NONE) {
                break;
            }
        }

        for (int8_t col = ky + 1; col < 8; ++col) {
            if (inputState[kx][col] * whoseTurn == -ROOK || inputState[kx][col] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[kx][col] != NONE) {
                break;
            }
        }

        for (int8_t col = ky - 1; col >= NONE; --col) {
            if (inputState[kx][col] * whoseTurn == -ROOK || inputState[kx][col] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[kx][col] != NONE) {
                break;
            }
        }

        // Checking whether the king is being checked by a bishop or a queen through a diagonal line
        int8_t  x = kx + 1;
        int8_t  y = ky + 1;
        while (x < 8 && y < 8) {
            if (inputState[x][y] * whoseTurn == -BISHOP || inputState[x][y] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[x][y] != NONE) {
                break;
            }
            ++x;
            ++y;
        }
        x = kx - 1;
        y = ky - 1;
        while (x >= 0 && y >= 0) {
            if (inputState[x][y] * whoseTurn == -BISHOP || inputState[x][y] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[x][y] != NONE) {
                break;
            }
            --x;
            --y;
        }

        x = kx + 1;
        y = ky - 1;
        while (x < 8 && y >= 0) {
            if (inputState[x][y] * whoseTurn == -BISHOP || inputState[x][y] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[x][y] != NONE) {
                break;
            }
            ++x;
            --y;
        }

        x = kx - 1;
        y = ky + 1;
        while (x >= 0 && y < 8) {
            if (inputState[x][y] * whoseTurn == -BISHOP || inputState[x][y] * whoseTurn == -QUEEN) {
                return true;
            }
            if (inputState[x][y] != NONE) {
                break;
            }
            --x;
            ++y;
        }

        // Checking whether the king is being checked by a pawn
        if (inputState[kx + whoseTurn][ky - 1] * whoseTurn == BLACK ||
            inputState[kx + whoseTurn][ky + 1] * whoseTurn == BLACK) {
            return true;
        }

        // Checking whether the king is being checked by a knight
        if (kx > 1 && ky > 0 &&
            inputState[kx - 2][ky - 1] * whoseTurn == -KNIGHT ||
            kx > 1 && ky < 7 &&
            inputState[kx - 2][ky + 1] * whoseTurn == -KNIGHT ||
            kx > 0 && ky > 1 &&
            inputState[kx - 1][ky - 2] * whoseTurn == -KNIGHT ||
            kx > 0 && ky < 6 &&
            inputState[kx - 1][ky + 2] * whoseTurn == -KNIGHT ||
            kx < 7 && ky > 1 &&
            inputState[kx + 1][ky - 2] * whoseTurn == -KNIGHT ||
            kx < 7 && ky < 6 &&
            inputState[kx + 1][ky + 2] * whoseTurn == -KNIGHT ||
            kx < 6 && ky > 0 &&
            inputState[kx + 2][ky - 1] * whoseTurn == -KNIGHT ||
            kx < 6 && ky < 7 &&
            inputState[kx + 2][ky + 1] * whoseTurn == -KNIGHT) {
            return true;
        }
        return false;
    }
    // Function to calculate whether the king is in check
    bool isChecked(const  int8_t(&inputState)[8][8], int8_t  whoseTurn) {
        int8_t  kx = -1, ky = -1;

        // Finding the position of the king
        for (int8_t row = 0; row < 8; ++row) {
            for (int8_t col = 0; col < 8; ++col) {
                if (inputState[row][col] == KING * whoseTurn) {
                    kx = row;
                    ky = col;
                    break;
                }
            }
        }
        return isChecked(inputState, whoseTurn, kx, ky);
    }
    // Function to calculate whether the king is in check
    bool isChecked(int8_t  kx, int8_t  ky) {
        return isChecked(BoardState, whose_turn, kx, ky);
    }
    // Function to calculate whether the king is in check
    bool isChecked() {
        return isChecked(BoardState, whose_turn);
    }

    // Function to promote pawns to specified pieces with possibility of underpromotion
    // Note: desired piece is always positive. The color is added in the process.
    void promotePawn(int8_t  col, int8_t  desired_piece) {
        if (BoardState[whose_turn == WHITE ? 7 : 0][col] == whose_turn)
            switch (desired_piece) {
            case 3:
                BoardState[whose_turn == WHITE ? 7 : 0][col] *= BISHOP;
                break;
            case 4:
                BoardState[whose_turn == WHITE ? 7 : 0][col] *= KNIGHT;
                break;
            case 5:
                BoardState[whose_turn == WHITE ? 7 : 0][col] *= ROOK;
                break;
            case 9:
                BoardState[whose_turn == WHITE ? 7 : 0][col] *= QUEEN;
                break;
            }
    }
    // Function to promote pawns to queens
    void promotePawn(int8_t  col) {
        if (BoardState[whose_turn == WHITE ? 7 : 0][col] == whose_turn)
            BoardState[whose_turn == WHITE ? 7 : 0][col] *= QUEEN;
    }

    // CASTLING:
    // There are 2 castling variables. The first is for white, the second is for black.
    // If the king has already castled then 4, if the king can castle with both its 3, if king can only castle with
    // kingside rook its 2, if the king can castle only with queenside rook its 1, if the king can't castle anymore its 0
    int8_t  canCastle(const  int8_t(&inputState)[8][8], int8_t  whoseTurn, int8_t  castling) {
        if (isChecked(inputState, whoseTurn)) {
            if (castling == CASTLED)
                return CASTLED;
            return UNAVAILABLE;
        }
        int8_t  result = UNAVAILABLE;
        const  int8_t  row = (whoseTurn == WHITE) ? 0 : 7;
        switch (castling) {
        case BOTH_SIDES:
        case QUEENSIDE:
            //Check whether cols 1-3 are empty and whether cols 2-3 are hit
            if (inputState[row][1] == NONE and inputState[row][2] == NONE and inputState[row][3] == NONE and
                !isChecked(inputState, whoseTurn, row, 2) and !isChecked(inputState, whoseTurn, row, 3))
            {
                result = QUEENSIDE;
            }
        case KINGSIDE:
            if (castling == QUEENSIDE)
                break;
            //Check whether cols 5-6 are empty and whether cols 5-6 are hit
            if (inputState[row][5] == NONE and inputState[row][5] == NONE and inputState[row][6] == NONE and
                !isChecked(inputState, whoseTurn, row, 5) and !isChecked(inputState, whoseTurn, row, 6))
            {
                if (result == QUEENSIDE)
                    result = BOTH_SIDES;
                else
                    result = KINGSIDE;
            }
            break;
        case CASTLED:
            return CASTLED;
        default:
            return UNAVAILABLE;
            break;
        }
        return result;
    }

    CHESS_MOVE generateGenericMove(int8_t(&inputState)[8][8], int8_t  row, int8_t  col, int8_t  xdelta, int8_t  ydelta, int8_t  whoseTurn, bool just_counting, int8_t  enemy_castling, int8_t  enpassant = -1) {
        if ((inputState[row + xdelta][col + ydelta] * whoseTurn > 0) or row < 0 or row > 7 or col < 0 or col > 7 or
            row + xdelta < 0 or row + xdelta>7 or col + ydelta < 0 or col + ydelta>7)
            return CHESS_MOVE{ ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE };
        CHESS_MOVE move = { col, row, inputState[row][col], col + ydelta, row + xdelta, inputState[row + xdelta][col + ydelta], 0 };
        inputState[row + xdelta][col + ydelta] = move.MOVED_PIECE;
        inputState[row][col] = NONE;
        if (!isChecked(inputState, whoseTurn)) {
            if (just_counting)
            {
                inputState[row + xdelta][col + ydelta] = move.TARGET_PIECE;
                inputState[row][col] = move.MOVED_PIECE;
                return CHESS_MOVE{ NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES, NON_ZERO_MOVES };
            }
            else
            {
                vector <CHESS_MOVE> enemyMoves;
                calculateAllLegalMoves(inputState, -whoseTurn, enpassant, whoseTurn == WHITE ? 0 : enemy_castling, whoseTurn == WHITE ? enemy_castling : 0, enemyMoves, true);
                if (enemyMoves.empty())
                    move.RESULTING_STATE = 2;
                if (isChecked(inputState, -whoseTurn))
                    move.RESULTING_STATE++;
            }
            inputState[row + xdelta][col + ydelta] = move.TARGET_PIECE;
            inputState[row][col] = move.MOVED_PIECE;
            return move;
        }
        inputState[row + xdelta][col + ydelta] = move.TARGET_PIECE;
        inputState[row][col] = move.MOVED_PIECE;
        // If the move is not valid, return a move with a special marker, for example, -1
        // Posibly returing nothing and not doing an "if" everytime would be better
        return CHESS_MOVE{ ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE };
    }
    vector<CHESS_MOVE> generateLineMoves(int8_t(&inputState)[8][8], int8_t  row, int8_t  col, int8_t  xdelta, int8_t  ydelta, int8_t  whoseTurn, bool just_counting, int8_t  enemy_castling, int8_t  enpassant = -1) {
        vector<CHESS_MOVE> result;
        for (int8_t i = row + xdelta, j = col + ydelta; i < 8 and j < 8 and i >= 0 and j >= 0; i += xdelta, j += ydelta) {
            if (inputState[i][j] * whoseTurn <= 0) {
                CHESS_MOVE move = { col, row, inputState[row][col], j, i, inputState[i][j], 0 };
                inputState[i][j] = inputState[row][col];
                inputState[row][col] = NONE;

                if (!isChecked(inputState, whoseTurn)) {
                    if (just_counting)
                    {
                        inputState[i][j] = move.TARGET_PIECE;
                        inputState[row][col] = move.MOVED_PIECE;
                        result.push_back(CHESS_MOVE{ -2, -2, -2, -2, -2, -2, -2 });
                        return result;
                    }
                    else
                    {
                        vector <CHESS_MOVE> enemyMoves;
                        calculateAllLegalMoves(inputState, -whoseTurn, enpassant, whoseTurn == WHITE ? 0 : enemy_castling, whoseTurn == WHITE ? enemy_castling : 0, enemyMoves, true);
                        if (enemyMoves.empty())
                            move.RESULTING_STATE = 2;
                        if (isChecked(inputState, -whoseTurn))
                            move.RESULTING_STATE++;
                    }
                    result.push_back(move);
                }
                inputState[i][j] = move.TARGET_PIECE;
                inputState[row][col] = move.MOVED_PIECE;
            }
            if (inputState[i][j] != NONE)
                break;
        }
        return result;
    };
    vector<CHESS_MOVE> generatePawnPromotions(CHESS_MOVE move)
    {
        vector<CHESS_MOVE> result;
        if (move.MOVED_PIECE != whose_turn or move.TO_RANK != (whose_turn == WHITE ? 7 : 0))
            return result;
        const int8_t state = move.RESULTING_STATE;
        move.RESULTING_STATE += QUEEN * 10;
        result.push_back(move);
        move.RESULTING_STATE = ROOK * 10 + state;
        result.push_back(move);
        move.RESULTING_STATE = KNIGHT * 10 + state;
        result.push_back(move);
        move.RESULTING_STATE = BISHOP * 10 + state;
        result.push_back(move);
        return result;
    }

    //This function should return a dynamic size array that consists of all possible moves for a input side (1 is white, -1 is black)
    // 1 possible move should be depicted with 7 values, X Y of the piece to be moved, value of the piece, X Y of the target
    // squareand the value of the piece in the target square, and lastly 1 if the move makes a check, 3 if it makes checkmate,
    // 2 if the move makes a stalemate, 0 if nothing of the above.This differentiation is to make it more descriptive.
    vector<CHESS_MOVE> calculateAllLegalMoves(int8_t(&inputState)[8][8], int8_t  whoseTurn, int8_t  enpassant, int8_t  white_castling, int8_t  black_castling, vector<CHESS_MOVE>& LegalMoves, bool just_counting = false) {
        // EN PASSANT: en passant variable given to the function equals to the spot to which a pawn can strike by this rule
        // but in 1d form, so e2->e4 gives en passant = e3 = col, if black's turn * -1 = 5
        if (enpassant != NO_INFO) {
            int8_t  x;
            int8_t  y;
            if (whoseTurn == WHITE)
            {
                x = 5;
                y = enpassant;
            }
            else
            {
                x = 2;
                y = enpassant;
            }
            CHESS_MOVE move = generateGenericMove(inputState, x - whoseTurn, y - 1, whoseTurn, 1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
            if (move.FROM_RANK != ILLEGAL_MOVE) {
                move.TARGET_PIECE = -whoseTurn;
                LegalMoves.push_back(move);
                if (move.FROM_RANK == NON_ZERO_MOVES) {
                    LegalMoves.push_back(move);
                    return LegalMoves;
                }
            }
            move = generateGenericMove(inputState, x - whoseTurn, y + 1, whoseTurn, -1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
            if (move.FROM_RANK != ILLEGAL_MOVE) {
                move.TARGET_PIECE = -whoseTurn;
                LegalMoves.push_back(move);
                if (move.FROM_RANK == NON_ZERO_MOVES) {
                    LegalMoves.push_back(move);
                    return LegalMoves;
                }
            }
        }
        // CASTLING: There are 2 castling variables given to the function. The first is for white, the second is for black.
        // If the king has already castled then 4, if the king can castle with both its 3, if king can only castle with
        // kingside rook its 2, if the king can castle only with queenside rook its 1, if the king can't castle anymore its 0
        const int8_t  castling = canCastle(inputState, whoseTurn, whoseTurn == WHITE ? white_castling : black_castling);
        const int8_t  f_row = whoseTurn == WHITE ? 0 : 7;
        switch (castling) {
        case BOTH_SIDES:
        case QUEENSIDE:
            if (just_counting) {
                LegalMoves.push_back(NON_ZERO_CHESS_MOVES);
                return LegalMoves;
            }
            LegalMoves.push_back(CHESS_MOVE{ 4, f_row, int8_t(KING * whoseTurn), 2, f_row, 0, 0 });
        case KINGSIDE:
            if (castling == QUEENSIDE)
                break;
            if (just_counting) {
                LegalMoves.push_back(NON_ZERO_CHESS_MOVES);
                return LegalMoves;
            }
            LegalMoves.push_back(CHESS_MOVE{ 4, f_row, int8_t(KING * whoseTurn), 6, f_row, 0, 0 });
            break;
        default:
            break;
        }

        for (int8_t row = 0; row < 8; ++row) {
            for (int8_t col = 0; col < 8; ++col) {
                const int8_t  piece = inputState[row][col] * whoseTurn;
                // Finding all pieces of the side that is to move, and calculating all possible moves of those pieces
                if (piece == PAWN) {
                    if (inputState[row + whoseTurn][col] == NONE) {
                        auto move = generateGenericMove(inputState, row, col, whoseTurn, 0, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE) {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                LegalMoves.insert(LegalMoves.end(), promo_moves.begin(), promo_moves.end());
                            else
                                LegalMoves.push_back(move);
                        }
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }

                    if (row == ((whoseTurn == WHITE) ? 1 : 6) and inputState[row + whoseTurn][col] == 0 and inputState[row + 2 * whoseTurn][col] == 0) {
                        auto move = generateGenericMove(inputState, row, col, 2 * whoseTurn, 0, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling, col);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            LegalMoves.push_back(move);
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }

                    if (inputState[row + whoseTurn][col - 1] * whoseTurn < 0) {
                        auto move = generateGenericMove(inputState, row, col, whoseTurn, -1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                        {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                LegalMoves.insert(LegalMoves.end(), promo_moves.begin(), promo_moves.end());
                            else
                                LegalMoves.push_back(move);
                        }
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }

                    if (inputState[row + whoseTurn][col + 1] * whoseTurn < 0) {
                        auto move = generateGenericMove(inputState, row, col, whoseTurn, 1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                        {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                LegalMoves.insert(LegalMoves.end(), promo_moves.begin(), promo_moves.end());
                            else
                                LegalMoves.push_back(move);
                        }
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }
                }
                else if (piece == BISHOP) {
                    auto moves = generateLineMoves(inputState, row, col, 1, 1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                    LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());

                    moves = generateLineMoves(inputState, row, col, -1, -1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                    LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());

                    moves = generateLineMoves(inputState, row, col, 1, -1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                    LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());

                    moves = generateLineMoves(inputState, row, col, -1, 1, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                    LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());

                }
                else if (piece == KNIGHT) {
                    constexpr static array knight_deltas{ DELTA{2,1}, DELTA{1,2}, DELTA{-2,1}, DELTA{-1,2}, DELTA{-2,-1}, DELTA{-1,-2}, DELTA{2,-1}, DELTA{1,-2} };
                    for (const DELTA& knight_delta : knight_deltas)
                    {
                        auto move = generateGenericMove(inputState, row, col, knight_delta.X, knight_delta.Y, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            LegalMoves.push_back(move);
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }
                }
                else if (piece == ROOK) {
                    constexpr static array rook_deltas{ DELTA{1,0}, DELTA{0,1}, DELTA{-1,0}, DELTA{0,-1} };
                    for (const DELTA& rook_delta : rook_deltas)
                    {
                        auto moves = generateLineMoves(inputState, row, col, rook_delta.X, rook_delta.Y, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());
                    }
                }
                else if (piece == QUEEN) {
                    constexpr static array queen_deltas{ DELTA{1,1}, DELTA{0,1}, DELTA{-1,1}, DELTA{-1,0}, DELTA{-1,-1}, DELTA{0,-1}, DELTA{1,-1}, DELTA{1,0} };
                    for (const DELTA& queen_delta : queen_deltas)
                    {
                        auto moves = generateLineMoves(inputState, row, col, queen_delta.X, queen_delta.Y, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        LegalMoves.insert(LegalMoves.end(), moves.begin(), moves.end());
                    }
                }
                else if (piece == KING) {
                    constexpr static array king_deltas{ DELTA{1,1}, DELTA{0,1}, DELTA{-1,1}, DELTA{-1,0}, DELTA{-1,-1}, DELTA{0,-1}, DELTA{1,-1}, DELTA{1,0} };
                    for (const DELTA& king_delta : king_deltas)
                    {
                        auto move = generateGenericMove(inputState, row, col, king_delta.X, king_delta.Y, whoseTurn, just_counting, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            LegalMoves.push_back(move);
                        if (move.FROM_RANK == NON_ZERO_MOVES) {
                            LegalMoves.push_back(move);
                            return LegalMoves;
                        }
                    }
                }
            }
        }

        return LegalMoves;
    }
    vector<CHESS_MOVE> calculateAllLegalMoves(vector<CHESS_MOVE>& LegalMoves, bool just_counting = false) {
        return calculateAllLegalMoves(BoardState, whose_turn, enpassant, white_castling, black_castling, LegalMoves, just_counting);
    }

    bool checkmateEventuallyPossible(int8_t  color) {
        // I need to add an array that keeps track of each player pieces.
        // In this function if the color has no knighs, rooks, queens, not more than 1 bishop
        int8_t  bishops = 0;
        for (int8_t row = 0; row < 8; ++row) {
            for (int8_t col = 0; col < 8; ++col) {
                if (BoardState[row][col] == color or BoardState[row][col] == KNIGHT * color
                    or BoardState[row][col] == ROOK * color or BoardState[row][col] == QUEEN * color) {
                    return true;
                }
                else {
                    if (BoardState[row][col] == BISHOP * color)
                    {
                        bishops++;
                        if (bishops > 1)
                            return true;
                    }
                }
            }
        }//doesn't calculate positions like where the panws stop the kings from ever advacing
        return false;
    }

    bool checkInterchangedPieces(vector<DELTA>& place_history) {
        map <int8_t, vector<DELTA>> origins_map;
        map <int8_t, vector<DELTA>> eventualities_map;
        for (size_t i = 0; i + 1 < place_history.size(); i += 2) {
            DELTA& origin = place_history[i];
            DELTA& eventuality = place_history[i + 1];
            if (origin != eventuality) {
                int8_t key = BoardState[origin.X][origin.Y];
                origins_map[key].push_back(origin);
                eventualities_map[key].push_back(eventuality);
            }
        }
        // Sorting each vector in the unordered maps
        for (auto& pair : origins_map) {
            vector<DELTA>& deltas = pair.second;
            sort(deltas.begin(), deltas.end(), greater<DELTA>());
        }
        for (auto& pair : eventualities_map) {
            vector<DELTA>& deltas = pair.second;
            sort(deltas.begin(), deltas.end(), greater<DELTA>());
        }
        return origins_map == eventualities_map;
    }
    bool checkThreeFoldRepetition() {
        // This function goes through last turns_since_irreversability moves and using only the moves themselves,
        //checks if all pairs of deltas equating is achieved twice
        vector<DELTA> move_delta_history;
        int8_t  repetition_counter = 0;
        const size_t len = move_history.size() - 1;
        DELTA piece, delta;
        piece.X = move_history[len].TO_RANK;
        piece.Y = move_history[len].TO_FILE;
        delta.X = move_history[len].FROM_RANK;
        delta.Y = move_history[len].FROM_FILE;
        move_delta_history.push_back(piece);
        move_delta_history.push_back(delta);
        for (int8_t i = 1; i < moves_since_irreversability; i++) {
            piece.X = move_history[len - i].TO_RANK;
            piece.Y = move_history[len - i].TO_FILE;
            delta.X = move_history[len - i].FROM_RANK;
            delta.Y = move_history[len - i].FROM_FILE;
            auto instance = find(move_delta_history.begin(), move_delta_history.end(), piece);
            if (instance != move_delta_history.end()) {
                if ((distance(move_delta_history.begin(), instance) % 2) == 1) {// this is wrong
                    *instance = delta;
                }
            }
            else {
                move_delta_history.push_back(piece);
                move_delta_history.push_back(delta);
            }
            bool flag = true;
            for (size_t j = 0; j < move_delta_history.size(); j += 2) {
                if (move_delta_history[j] != move_delta_history[j + 1]) {
                    flag = false;
                    break;
                }
            }
            if (!flag) {
                flag = checkInterchangedPieces(move_delta_history);
            }
            if (flag) {
                repetition_counter++;
                if (repetition_counter >= 2)
                    return true;
            }
        }
        return false;
    }


    //Calculates the resulting board state by taking in the current board state and the move
    bool processMove(CHESS_MOVE move, bool do_check_legality = true) {
        if (do_check_legality) {
            if (find(LegalMoves.begin(), LegalMoves.end(), move) == LegalMoves.end())
                return false;
            LegalMoves.clear();
            if (game_conclusion != IN_PROGRESS)
                return false;
        }
        move_history.push_back(move);

        BoardState[move.FROM_RANK][move.FROM_FILE] = NONE;
        BoardState[move.TO_RANK][move.TO_FILE] = move.MOVED_PIECE;
        int move_count = turn_count * 2 + (whose_turn == WHITE ? 0 : 1);

        // If enpassant was used
        if (enpassant != -1 and enpassant == move.TO_FILE and move.MOVED_PIECE == whose_turn and (move.FROM_FILE == enpassant - 1 or move.FROM_FILE == enpassant + 1) and (move.TO_RANK == whose_turn ? 5 : 2))
        {
            BoardState[move.TO_RANK - whose_turn][move.TO_FILE] = NONE;
        }
        // If enpassant option was created
        if (move.MOVED_PIECE == whose_turn and abs(move.TO_RANK - move.FROM_RANK) == 2)
        {
            enpassant = move.FROM_FILE;
            if (whose_turn == WHITE) {
                enpassant_game_memory.WHITE_ENPASSANT_MEM_ARRAY[move.FROM_FILE] = move_count + 1;
            }
            else
            {
                enpassant_game_memory.BLACK_ENPASSANT_MEM_ARRAY[move.FROM_FILE] = move_count + 1;
            }
        }
        else
            enpassant = NO_INFO;
        // If the move is castling
        if (move.MOVED_PIECE == KING * whose_turn and abs(move.TO_FILE - move.FROM_FILE) == 2) {
            irreversability_flag = true;
            if (whose_turn == WHITE) {
                white_castling = CASTLED;
                white_castling_memory.CASTLING_CHANGE2.first = CASTLED;
                white_castling_memory.CASTLING_CHANGE2.second = move_count;
            }
            else
            {
                black_castling = CASTLED;
                black_castling_memory.CASTLING_CHANGE2.first = CASTLED;
                black_castling_memory.CASTLING_CHANGE2.second = move_count;
            }
            if (move.TO_FILE == 2) {
                swap(BoardState[move.FROM_RANK][0], BoardState[move.FROM_RANK][3]);
            }
            else if (move.TO_FILE == 6) {
                swap(BoardState[move.FROM_RANK][5], BoardState[move.FROM_RANK][7]);
            }
            else {
                cout << "mistake somewhere in process castling" << endl;
            }
        }

        // If the move promotes a pawn
        //The desired piece is encoded into the "resulting state part", to not create a separate slot just for this
        //unique chance of underpromotion. The AI for underpromotion doesn't need to be part of the general game AI.
        int8_t promotion = move.RESULTING_STATE / 10;
        if (promotion != NONE) {
            BoardState[move.TO_RANK][move.TO_FILE] = whose_turn * promotion;
        }

        // If the move affected castling options, but isn't castling itself
        if (move.MOVED_PIECE == whose_turn * ROOK) {
            auto& castling = whose_turn == WHITE ? white_castling : black_castling;
            auto& castling_mem = whose_turn == WHITE ? white_castling_memory : black_castling_memory;
            if ((move.FROM_RANK == whose_turn == WHITE ? 0 : 7) and move.FROM_FILE == 0) {
                if (castling == QUEENSIDE)
                    castling = UNAVAILABLE;
                castling_mem.CASTLING_CHANGE2.first = UNAVAILABLE;
                castling_mem.CASTLING_CHANGE2.second = move_count;
                if (castling == BOTH_SIDES)
                    castling = KINGSIDE;
                if (castling_mem.CASTLING_CHANGE1.first == NO_INFO) {
                    castling_mem.CASTLING_CHANGE1.first = KINGSIDE;
                    castling_mem.CASTLING_CHANGE1.second = move_count;
                }
                else
                {
                    castling_mem.CASTLING_CHANGE2.first = KINGSIDE;
                    castling_mem.CASTLING_CHANGE2.second = move_count;
                }
            }
            if ((move.FROM_RANK == whose_turn == WHITE ? 0 : 7) and move.FROM_FILE == 7) {
                if (castling == KINGSIDE)
                {
                    castling = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.first = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.second = move_count;
                }
                if (castling == BOTH_SIDES)
                    if (whose_turn == WHITE) {
                        castling = QUEENSIDE;
                        if (castling_mem.CASTLING_CHANGE1.first == NO_INFO) {
                            castling_mem.CASTLING_CHANGE1.first = QUEENSIDE;
                            castling_mem.CASTLING_CHANGE1.second = move_count;
                        }
                        else {
                            castling_mem.CASTLING_CHANGE2.first = QUEENSIDE;
                            castling_mem.CASTLING_CHANGE2.second = move_count;
                        }
                    }
            }
        }
        if (move.TARGET_PIECE == -whose_turn * ROOK) {
            auto& castling = -whose_turn == WHITE ? white_castling : black_castling;
            auto& castling_mem = -whose_turn == WHITE ? white_castling_memory : black_castling_memory;
            if ((move.TO_RANK == -whose_turn == WHITE ? 0 : 7) and move.TO_FILE == 7) {
                if (castling == QUEENSIDE)
                {
                    castling = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.first = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.second = move_count;
                }
                if (castling == BOTH_SIDES)
                {
                    castling = KINGSIDE;
                    if (castling_mem.CASTLING_CHANGE1.first == NO_INFO) {
                        castling_mem.CASTLING_CHANGE1.first = KINGSIDE;
                        castling_mem.CASTLING_CHANGE1.second = move_count;
                    }
                    else {
                        castling_mem.CASTLING_CHANGE2.first = KINGSIDE;
                        castling_mem.CASTLING_CHANGE2.second = move_count;
                    }
                }
            }
            if ((move.TO_RANK == -whose_turn == WHITE ? 0 : 7) and move.TO_FILE == 0)
            {
                if (castling == KINGSIDE)
                {
                    castling = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.first = UNAVAILABLE;
                    castling_mem.CASTLING_CHANGE2.second = move_count;
                }
                if (castling == BOTH_SIDES)
                {
                    castling = QUEENSIDE;
                    if (castling_mem.CASTLING_CHANGE1.first == NO_INFO) {
                        castling_mem.CASTLING_CHANGE1.first = QUEENSIDE;
                        castling_mem.CASTLING_CHANGE1.second = move_count;
                    }
                    else {
                        castling_mem.CASTLING_CHANGE2.first = QUEENSIDE;
                        castling_mem.CASTLING_CHANGE2.second = move_count;
                    }
                }
            }
        }
        // Changing turns, but could be done elsewhere
        whose_turn = -whose_turn;
        if (whose_turn == WHITE)
            turn_count++;
        //Repetition and 50-move draws
        if (move.TARGET_PIECE != NONE or abs(move.MOVED_PIECE) == PAWN)
            irreversability_flag = true;
        if (irreversability_flag)
        {
            irreversability_history.push_back(move_count);
            moves_since_irreversability = 0;
        }
        else
            moves_since_irreversability++;
        if (moves_since_irreversability >= 50) {
            game_conclusion = DRAW;
            conclusion_reason = BY_50MOVE_RULE;
            return true;
        }
        if (moves_since_irreversability > 2 and checkThreeFoldRepetition()) {
            game_conclusion = DRAW;
            conclusion_reason = BY_THREEFOLD_REPETITION;
            return true;
        }
        if (!checkmateEventuallyPossible(WHITE) and !checkmateEventuallyPossible(BLACK))
        {
            game_conclusion = DRAW;
            conclusion_reason = BY_DEAD_POSITION;
            return true;
        }



        irreversability_flag = false;
        vector<CHESS_MOVE> enemyMoves;
        calculateAllLegalMoves(enemyMoves, true);
        if (enemyMoves.empty())
        {
            if (isChecked()) {
                game_conclusion = whose_turn == WHITE ? BLACK_WON : WHITE_WON;
                conclusion_reason = BY_CHECKMATE;
                return true;
            }
            else {
                game_conclusion = DRAW;
                conclusion_reason = BY_STALEMATE;
                return true;
            }
        }
        return true;
    }

    //Calculates the boards state prior to using "processMove" with the same move, but current board state.
    bool processMoveReverse(CHESS_MOVE move) {
        if (move_history.empty())
            return false;
        if (this->game_conclusion != IN_PROGRESS or this->conclusion_reason != NOT_CONCLUDED)
        {
            this->game_conclusion = IN_PROGRESS;
            this->conclusion_reason = NOT_CONCLUDED;
        }
        LegalMoves.clear();
        BoardState[move.FROM_RANK][move.FROM_FILE] = move.MOVED_PIECE;
        BoardState[move.TO_RANK][move.TO_FILE] = move.TARGET_PIECE;
        if (whose_turn == WHITE)
            turn_count--;
        whose_turn = -whose_turn;
        int move_count = turn_count * 2 + (whose_turn == WHITE ? 0 : 1);
        move_history.erase(move_history.end() - 1);
        if (irreversability_history.empty())
            moves_since_irreversability = move_count;
        else {
            int8_t last_rev = irreversability_history.back();
            if (last_rev >= move_count)
            {
                irreversability_history.erase(irreversability_history.end() - 1);
                if (!irreversability_history.empty())
                {
                    last_rev = irreversability_history.back();
                    moves_since_irreversability = move_count - last_rev;
                }
                else {
                    moves_since_irreversability = move_count;
                }
            }
            else {
                moves_since_irreversability = move_count - last_rev;
            }
        }

        // If enpassant was used
        auto full_enpassant = findLastEnpassantFull();
        if (full_enpassant.second != -1 and full_enpassant.first == move.TO_FILE and move.MOVED_PIECE == whose_turn and (move.FROM_FILE == full_enpassant.first - 1 or move.FROM_FILE == full_enpassant.first + 1))
            BoardState[move.TO_RANK][move.TO_FILE - whose_turn] = -whose_turn;
        // En passant memory reversal
        int8_t& last_enpassant = findLastEnpassant();
        if (last_enpassant > move_count) {
            last_enpassant = NO_INFO;
        }

        // If the move was castling
        if (move.MOVED_PIECE == KING * whose_turn and abs(move.TO_FILE - move.FROM_FILE) == 2) {
            if (move.TO_FILE == 1) {
                swap(BoardState[move.FROM_RANK][0], BoardState[move.FROM_RANK][2]);
            }
            else if (move.TO_FILE == 5) {
                swap(BoardState[move.FROM_RANK][4], BoardState[move.FROM_RANK][7]);
            }
        }
        // Castling memory reversal
        pair< int8_t, int8_t >& last_castling_change = findLastCastling();
        if (last_castling_change.second > move_count) {//TODO: make sure its moves and not turns, also multiply in this line 
            last_castling_change = make_pair(NO_INFO, NO_INFO);
            updateCastling(WHITE);
            updateCastling(BLACK);
        }
        return true;
    }
    bool processMoveReverse() {
        if (move_history.empty())
            return false;
        return processMoveReverse(move_history.back());
    }

    void agreedToDraw() {
        game_conclusion = DRAW;
        conclusion_reason = BY_AGREEMENT;
        return;
    }
    void resigned(int8_t  color) {
        game_conclusion = color == WHITE ? BLACK_WON : WHITE_WON;
        conclusion_reason = BY_RESIGNATION;
        return;
    }
    void outOfTime(int8_t  color) {
        if (checkmateEventuallyPossible(-color)) {
            game_conclusion = color == WHITE ? WHITE_WON : BLACK_WON;
            conclusion_reason = BY_TIME;
            return;
        }
        else {
            game_conclusion = DRAW;
            conclusion_reason = BY_TIME_AND_INSUFFICIENT_MATERIAL;
            return;
        }
    }

    //All AI stuff: #####################################################################################################
    //###################################################################################################################
    //###################################################################################################################

    // Define a hash function for the pair<int8_t, int8_t>
    struct pair_hash {
        template <class T1, class T2>
        size_t operator () (const pair<T1, T2>& p) const {
            auto h1 = hash<T1>{}(p.first);
            auto h2 = hash<T2>{}(p.second);

            // Combine the hashes using a simple hash_combine function
            return hash_combine(h1, h2);
        }

        // Simple hash_combine function
        template <class T>
        static size_t hash_combine(size_t seed, const T& v) {
            return seed ^ (hash<T>{}(v)+0x9e3779b9 + (seed << 6) + (seed >> 2));
        }
    };

    struct pair_eq {
        bool operator ()(const pair<int8_t, int8_t>& lhs, const pair<int8_t, int8_t>& rhs) const {
            return lhs.first == rhs.first && lhs.second == rhs.second;
        }
    };
    //Predicates for find_if
    struct ResultingStateMatch {
        int8_t resultingState;
        ResultingStateMatch(int8_t state) : resultingState(state) {}
        bool operator()(const CHESS_MOVE& move) const {
            return move.RESULTING_STATE == resultingState;
        }
    };
    struct AllExceptResultingState {
        CHESS_MOVE compareMove;
        AllExceptResultingState(const CHESS_MOVE& move) : compareMove(move) {}
        bool operator()(const CHESS_MOVE& move) const {
            return memcmp(&move, &compareMove, sizeof(CHESS_MOVE) - sizeof(int8_t)) == 0;
        }
    };
    using my_map = unordered_map<pair<int8_t, int8_t>, vector<CHESS_MOVE>, pair_hash, pair_eq>;

    my_map attacksNDefences;
    my_map enemyAttacksNDefences;


    CHESS_MOVE generateGenericAttackDefenceMove(int8_t(&inputState)[8][8], int8_t  row, int8_t  col, int8_t  xdelta, int8_t  ydelta, int8_t  whoseTurn, int8_t  enemy_castling, int8_t  enpassant = -1) {
        if (row < 0 or row > 7 or col < 0 or col > 7 or
            row + xdelta < 0 or row + xdelta > 7 or col + ydelta < 0 or col + ydelta > 7)
            return CHESS_MOVE{ ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE };
        CHESS_MOVE move = { col, row, inputState[row][col], col + ydelta, row + xdelta, inputState[row + xdelta][col + ydelta], 0 };
        inputState[row + xdelta][col + ydelta] = move.MOVED_PIECE;
        inputState[row][col] = NONE;
        if (!isChecked(inputState, whoseTurn)) {
            if (move.TARGET_PIECE * whoseTurn <= 0) {
                vector<CHESS_MOVE> enemyMoves;
                calculateAllLegalMoves(inputState, -whoseTurn, enpassant, whoseTurn == WHITE ? 0 : enemy_castling, whoseTurn == WHITE ? enemy_castling : 0, enemyMoves, true);
                if (enemyMoves.empty())
                    move.RESULTING_STATE = 2;
                if (isChecked(inputState, -whoseTurn))
                    move.RESULTING_STATE++;
            }
            inputState[row + xdelta][col + ydelta] = move.TARGET_PIECE;
            inputState[row][col] = move.MOVED_PIECE;
            return move;
        }
        inputState[row + xdelta][col + ydelta] = move.TARGET_PIECE;
        inputState[row][col] = move.MOVED_PIECE;
        // If the move is not valid, return a move with a special marker, for example, -1
        return CHESS_MOVE{ ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE, ILLEGAL_MOVE };
    }
    vector<CHESS_MOVE> generateLineAttackDefenceMoves(int8_t(&inputState)[8][8], int8_t  row, int8_t  col, int8_t  xdelta, int8_t  ydelta, int8_t  whoseTurn, int8_t  enemy_castling, int8_t  enpassant = -1) {
        vector<CHESS_MOVE> result;
        for (int8_t i = row + xdelta, j = col + ydelta; i < 8 and j < 8 and i >= 0 and j >= 0; i += xdelta, j += ydelta) {
            CHESS_MOVE move = { col, row, inputState[row][col], j, i, inputState[i][j], 0 };
            inputState[i][j] = inputState[row][col];
            inputState[row][col] = NONE;
            if (!isChecked(inputState, whoseTurn)) {
                vector<CHESS_MOVE> enemyMoves;
                calculateAllLegalMoves(inputState, -whoseTurn, enpassant, whoseTurn == WHITE ? 0 : enemy_castling, whoseTurn == WHITE ? enemy_castling : 0, enemyMoves, true);
                if (enemyMoves.empty())
                    move.RESULTING_STATE = 2;
                if (isChecked(inputState, -whoseTurn))
                    move.RESULTING_STATE++;
                result.push_back(move);
            }
            inputState[i][j] = move.TARGET_PIECE;
            inputState[row][col] = move.MOVED_PIECE;
            if (inputState[i][j] != NONE)
                break;
        }
        return result;
    };

    void addAttacksToMap(vector<CHESS_MOVE>& Moves, my_map& attacks) {
        for (CHESS_MOVE& move : Moves) {
            if (move.TARGET_PIECE != 0) {
                pair<int8_t, int8_t> targetPos = make_pair(move.TO_RANK, move.TO_FILE);
                auto target_attacks = attacks.find(targetPos);
                if (target_attacks != attacks.end()) {
                    // Found an attack with a matching target, push the address of the found attack
                    target_attacks->second.push_back(move);
                }
                else {
                    // Making and pushbacking a new vector with the single element
                    attacks[targetPos] = { move };
                }

            }
        }
    }
    void findAttacksAndDefences(int8_t whoseTurn, int8_t(&inputState)[8][8], my_map& enemyAttacks)
    {
        enemyAttacks.clear();
        vector<CHESS_MOVE> enemyAttacksVec;
        for (int8_t row = 0; row < 8; ++row) {
            for (int8_t col = 0; col < 8; ++col) {
                int8_t  piece = inputState[row][col] * whoseTurn;
                // Finding all pieces of the side that is to move, and calculating all possible moves of those pieces
                if (piece == PAWN) {
                    if (inputState[row + whoseTurn][col] == NONE) {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, whoseTurn, 0, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE) {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                enemyAttacksVec.insert(enemyAttacksVec.end(), promo_moves.begin(), promo_moves.end());
                            else
                                enemyAttacksVec.push_back(move);
                        }
                    }

                    if (row == ((whoseTurn == WHITE) ? 1 : 6) and inputState[row + whoseTurn][col] == 0 and inputState[row + 2 * whoseTurn][col] == 0) {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, 2 * whoseTurn, 0, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling, col);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            enemyAttacksVec.push_back(move);
                    }

                    if (inputState[row + whoseTurn][col - 1] * whoseTurn < 0) {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, whoseTurn, -1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                        {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                enemyAttacksVec.insert(enemyAttacksVec.end(), promo_moves.begin(), promo_moves.end());
                            else
                                enemyAttacksVec.push_back(move);
                        }
                    }

                    if (inputState[row + whoseTurn][col + 1] * whoseTurn < 0) {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, whoseTurn, 1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                        {
                            vector<CHESS_MOVE> promo_moves = generatePawnPromotions(move);
                            if (!promo_moves.empty())
                                enemyAttacksVec.insert(enemyAttacksVec.end(), promo_moves.begin(), promo_moves.end());
                            else
                                enemyAttacksVec.push_back(move);
                        }
                    }
                }
                else if (piece == BISHOP) {
                    auto moves = generateLineAttackDefenceMoves(inputState, row, col, 1, 1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                    enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());

                    moves = generateLineAttackDefenceMoves(inputState, row, col, -1, -1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                    enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());

                    moves = generateLineAttackDefenceMoves(inputState, row, col, 1, -1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                    enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());

                    moves = generateLineAttackDefenceMoves(inputState, row, col, -1, 1, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                    enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());

                }
                else if (piece == KNIGHT) {
                    constexpr static array knight_deltas{ DELTA{2,1}, DELTA{1,2}, DELTA{-2,1}, DELTA{-1,2}, DELTA{-2,-1}, DELTA{-1,-2}, DELTA{2,-1}, DELTA{1,-2} };
                    for (const DELTA& knight_delta : knight_deltas)
                    {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, knight_delta.X, knight_delta.Y, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            enemyAttacksVec.push_back(move);
                    }
                }
                else if (piece == ROOK) {
                    constexpr static array rook_deltas{ DELTA{1,0}, DELTA{0,1}, DELTA{-1,0}, DELTA{0,-1} };
                    for (const DELTA& rook_delta : rook_deltas)
                    {
                        auto moves = generateLineAttackDefenceMoves(inputState, row, col, rook_delta.X, rook_delta.Y, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());
                    }
                }
                else if (piece == QUEEN) {
                    constexpr static array queen_deltas{ DELTA{1,1}, DELTA{0,1}, DELTA{-1,1}, DELTA{-1,0}, DELTA{-1,-1}, DELTA{0,-1}, DELTA{1,-1}, DELTA{1,0} };
                    for (const DELTA& queen_delta : queen_deltas)
                    {
                        auto moves = generateLineAttackDefenceMoves(inputState, row, col, queen_delta.X, queen_delta.Y, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        enemyAttacksVec.insert(enemyAttacksVec.end(), moves.begin(), moves.end());
                    }
                }
                else if (piece == KING) {
                    constexpr static array king_deltas{ DELTA{1,1}, DELTA{0,1}, DELTA{-1,1}, DELTA{-1,0}, DELTA{-1,-1}, DELTA{0,-1}, DELTA{1,-1}, DELTA{1,0} };
                    for (const DELTA& king_delta : king_deltas)
                    {
                        auto move = generateGenericAttackDefenceMove(inputState, row, col, king_delta.X, king_delta.Y, whoseTurn, whoseTurn == WHITE ? black_castling : white_castling);
                        if (move.FROM_RANK != ILLEGAL_MOVE)
                            enemyAttacksVec.push_back(move);
                    }
                }
            }
        }
        addAttacksToMap(enemyAttacksVec, enemyAttacks);
    }

    // Looks for moves that aren't only attacks, but set up attacks, and goes into them to find out how good they are
    vector<CHESS_MOVE> findForks(int8_t whoseTurn) {
        vector<CHESS_MOVE> moves;
        auto LegalMoves = this->LegalMoves;
        for (CHESS_MOVE& move : LegalMoves) {
            if (move.RESULTING_STATE != UNCHANGED and move.RESULTING_STATE != CHECK)
                continue;
            my_map forkAttacks;
            processMove(move, false);
            findAttacksAndDefences(whoseTurn, BoardState, forkAttacks);
            findAttacksAndDefences(-whoseTurn, BoardState, enemyAttacksNDefences);
            vector<CHESS_MOVE> best_moves = resolveAttacksFast(enemyAttacksNDefences, forkAttacks);
            CHESS_MOVE best_move = { -3, -3, -3, -3, -3, -3, -3 };
            if (best_moves.size() == 1)
                best_move = best_moves[0];
            if (best_moves.size() > 1) {
                best_move = *min_element(best_moves.begin(), best_moves.end(), compareChessMovesResult);
            }
            processMoveReverse(move);
            if (best_move.FROM_RANK != RESIGN)
                move.RESULTING_STATE = -best_move.RESULTING_STATE;
            else
                move.RESULTING_STATE = appraise_move(abs(move.TARGET_PIECE));
            if (best_move.RESULTING_STATE == INT8_MAX)
                move.RESULTING_STATE = INT8_MIN;
            moves.push_back(move);
        }
        return moves;
    }

    // Comparison functions for sort and min/max _element alike
    static bool compareChessMovesResult(const CHESS_MOVE& a, const CHESS_MOVE& b) {
        return a.RESULTING_STATE > b.RESULTING_STATE;
    }
    static bool compareChessMovesMovedPiece(const CHESS_MOVE& a, const CHESS_MOVE& b) {
        return abs(a.MOVED_PIECE) < abs(b.MOVED_PIECE);
    }

    // Arbitrarily appraises just the positional state of the board, without deep understanding
    int8_t appraisePosition(int8_t(&inputState)[8][8], int8_t  whoseTurn) {
        int8_t constexpr pawnBoard[8][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {1, 2, 2, -4, -4, 2, 2, 1},
        {1, -1, -2, 0, 0, -2, -1, 1},
        {0, 0, 0, 4, 4, 0, 0, 0},
        {1, 1, 2, 5, 5, 2, 1, 1},
        {2, 2, 4, 6, 6, 4, 2, 2},
        {9, 10, 10, 10, 10, 10, 10, 10},
        {0, 0, 0, 0, 0, 0, 0, 0}
        },
            bishopBoard[8][8] = {
            {-4, -2, -2, -2, -2, -2, -2, -4},
            {-2, 1, 0, 0, 0, 0, 1, -2},
            {-2, 2, 2, 2, 2, 2, 2, -2},
            {-2, 0, 2, 2, 2, 2, 0, -2},
            {-2, 1, 1, 2, 2, 1, 1, -2},
            {-2, 0, 1, 2, 2, 1, 0, -2},
            {-2, 0, 0, 0, 0, 0, 0, -2},
            {-4, -2, -2, -2, -2, -2, -2, -4}
        },
            knightBoard[8][8] = {
            {-10, -8, -6, -6, -6, -6, -8, -10},
            {-8, -4, 0, 1, 1, 0, -4, -8},
            {-6, 1, 2, 3, 3, 2, 1, -6},
            {-6, 0, 3, 4, 4, 3, 0, -6},
            {-6, 1, 3, 4, 4, 3, 1, -6},
            {-6, 0, 2, 3, 3, 2, 0, -6},
            {-8, -4, 0, 0, 0, 0, -4, -8},
            {-10, -8, -6, -6, -6, -6, -8, -10}
        },
            rookBoard[8][8] = {
            {0, 0, 0, 1, 1, 0, 0, 0},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {-1, 0, 0, 0, 0, 0, 0, -1},
            {-1, 0, 0, 0, 0, 0, 0, -1},
            {-1, 0, 0, 0, 0, 0, 0, -1},
            {-1, 0, 0, 0, 0, 0, 0, -1},
            {-1, 2, 2, 2, 2, 2, 2, -1},
            {0, 0, 0, 0, 0, 0, 0, 0}
        },
            queenBoard[8][8] = {
            {-4, -2, -2, -1, -1, -2, -2, -4},
            {-2, 0, 1, 0, 0, 0, 0, -2},
            {-2, 1, 1, 1, 1, 1, 0, -2},
            {0, 0, 1, 1, 1, 1, 0, -1},
            {-1, 0, 1, 1, 1, 1, 0, -1},
            {-2, 0, 1, 1, 1, 1, 0, -2},
            {-2, 0, 0, 0, 0, 0, 0, -2},
            {-4, -2, -2, -1, -1, -2, -2, -4}
        },
            kingBoard[8][8] = {
            {4, 6, 2, 0, 0, 2, 6, 4},
            {4, 4, 0, 0, 0, 0, 4, 4},
            {-2, -4, -4, -4, -4, -4, -4, -2},
            {-4, -6, -6, -8, -8, -6, -6, -4},
            {-6, -8, -8, -10, -10, -8, -8, -6},
            {-6, -8, -8, -10, -10, -8, -8, -6},
            {-6, -8, -8, -10, -10, -8, -8, -6},
            {-6, -8, -8, -10, -10, -8, -8, -6}
        };

        int position_score = 0;
        int8_t kx = -1, ky = -1;
        int8_t ekx = -1, eky = -1;
        uint8_t piece_counter = 0;
        for (int8_t row = 0; row < 8; row++) {
            for (int8_t col = 0; col < 8; col++) {
                int8_t piece = inputState[row][col];
                if (piece != NONE) {
                    piece_counter++;
                    switch (abs(piece)) {
                    case PAWN:
                        position_score += abs(piece) * (whoseTurn == WHITE ? pawnBoard[row][col] : -pawnBoard[col][row]);
                        break;
                    case BISHOP:
                        position_score += abs(piece) * (whoseTurn == WHITE ? bishopBoard[row][col] : -bishopBoard[col][row]);
                        break;
                    case KNIGHT:
                        position_score += abs(piece) * (whoseTurn == WHITE ? knightBoard[row][col] : -knightBoard[col][row]);
                        break;
                    case ROOK:
                        position_score += abs(piece) * (whoseTurn == WHITE ? rookBoard[row][col] : -rookBoard[col][row]);
                        break;
                    case QUEEN:
                        position_score += abs(piece) * (whoseTurn == WHITE ? queenBoard[row][col] : -queenBoard[col][row]);
                        break;
                    case KING:
                        position_score += abs(piece) * (whoseTurn == WHITE ? kingBoard[row][col] : -kingBoard[col][row]);
                        if (piece == KING * whoseTurn) {
                            kx = row;
                            ky = col;
                        }
                        else {
                            ekx = row;
                            eky = col;
                        }
                        break;
                    }
                }
            }
        }
        position_score /= piece_counter;
        position_score += (whoseTurn == WHITE ? -1 : 1) * (white_castling - black_castling) + isChecked(inputState, whoseTurn, ekx, eky) ? 3 : 0 + isChecked(inputState, -whoseTurn, kx, ky) ? -3 : 0;
        return position_score;
    }
    // Combines given material change and purely positional state calculated inside
    int8_t appraise_move(int8_t material_adv_since) {
        int current_position = appraisePosition(BoardState, whose_turn);
        int move_score = material_adv_since * 2 + current_position;
        vector<CHESS_MOVE> openEnemyLegalMoves;
        calculateAllLegalMoves(BoardState, -whose_turn, -1, white_castling, black_castling, openEnemyLegalMoves);
        auto checkmate = find_if(openEnemyLegalMoves.begin(), openEnemyLegalMoves.end(), ResultingStateMatch(CHECKMATE));
        if (checkmate != openEnemyLegalMoves.end()) {
            move_score = INT8_MIN;
            return move_score;
        }
        auto stalemate = find_if(openEnemyLegalMoves.begin(), openEnemyLegalMoves.end(), ResultingStateMatch(STALEMATE));
        if (stalemate != openEnemyLegalMoves.end() and move_score > 0) {
            move_score = 0;
        }
        if (move_score > INT8_MAX)
            move_score = INT8_MAX;
        if (move_score < INT8_MIN)
            move_score = INT8_MIN + 1;
        return move_score;
    }


    // Arbitrarily appraises the value of all trades based solely on pieces attacking and defending the target
    // Returns a vector(maybe some other type is better) in the descending order of RESULTING_STATE
    vector<CHESS_MOVE> resolveAttacksFast(my_map& attNdef, my_map& enemyAttNdef) {
        vector<CHESS_MOVE> bestMoves;
        // Collect keys
        // Define the unordered sets
        unordered_set<pair<int8_t, int8_t>, pair_hash, pair_eq> yourAttacksKeys;
        unordered_set<pair<int8_t, int8_t>, pair_hash, pair_eq> yourDefencesKeys;
        unordered_set<pair<int8_t, int8_t>, pair_hash, pair_eq> enemyAttacksKeys;
        unordered_set<pair<int8_t, int8_t>, pair_hash, pair_eq> enemyDefencesKeys;
        my_map impendingDoomMap;

        // Iterate through the map and populate the sets accordingly
        for (const auto& entry : attNdef) {
            for (const auto& move : entry.second) {
                // Check if MOVED_PIECE and TARGET_PIECE signs are different
                if ((move.MOVED_PIECE < 0 && move.TARGET_PIECE > 0) || (move.MOVED_PIECE > 0 && move.TARGET_PIECE < 0)) {
                    yourAttacksKeys.insert(entry.first);
                }
                else {
                    yourDefencesKeys.insert(entry.first);
                }
            }
        }

        for (const auto& entry : enemyAttNdef) {
            for (const auto& move : entry.second) {
                // Check if MOVED_PIECE and TARGET_PIECE signs are different
                if ((move.MOVED_PIECE < 0 && move.TARGET_PIECE > 0) || (move.MOVED_PIECE > 0 && move.TARGET_PIECE < 0)) {
                    enemyAttacksKeys.insert(entry.first);
                }
                else {
                    enemyDefencesKeys.insert(entry.first);
                }
            }
        }
        // This part tries to find "impending doom moves", so moves that the enemy has availible that
        //might be critical, so they are found, and should be used in further move appraisal, where
        //failed prevention of impending doom is punished.
        unordered_set<pair<int8_t, int8_t>, pair_hash, pair_eq> impendingDoomKeys;
        vector<CHESS_MOVE> impendingDoomMoves;
        for (auto& key : enemyAttacksKeys) {
            size_t defence_count = 0;
            // Check if the key exists in the 'attacks' map
            auto defencesIter = attNdef.find(key);
            vector<CHESS_MOVE>* moves = nullptr;
            if (defencesIter != attNdef.end()) {
                moves = &defencesIter->second;
                sort(moves->begin(), moves->end(), compareChessMovesMovedPiece);
                defence_count = moves->size();
            }


            int8_t mat_adv = 0;
            // Check if the key exists in the 'enemyAttacks' map
            auto enemyAttacksIter = enemyAttNdef.find(key);
            if (enemyAttacksIter == enemyAttNdef.end())
                break;
            vector<CHESS_MOVE>* movesE = &enemyAttacksIter->second;
            sort(movesE->begin(), movesE->end(), compareChessMovesMovedPiece);

            impendingDoomKeys.insert(key);
            impendingDoomMoves.insert(impendingDoomMoves.end(), movesE->begin(), movesE->end());
            int8_t fake_turn_counter = 0;
            const int8_t original_adv = appraisePosition(BoardState, whose_turn);
            whose_turn = -whose_turn;
            if (whose_turn == WHITE)
                turn_count++;
            for (int8_t i = 0; i < movesE->size(); i++) {
                auto& movEi = (*movesE)[i];
                movEi.TARGET_PIECE = BoardState[movEi.TO_RANK][movEi.TO_FILE];
                processMove(movEi, 0);
                mat_adv -= whose_turn * movEi.TARGET_PIECE;
                fake_turn_counter++;
                int8_t temp_enemy_adv = appraise_move(mat_adv);
                movEi.RESULTING_STATE = temp_enemy_adv - original_adv;
                if (game_conclusion != IN_PROGRESS) {
                    if (conclusion_reason == BY_CHECKMATE)
                        movEi.RESULTING_STATE = INT8_MIN;
                    else
                        movEi.RESULTING_STATE = INT8_MAX;
                }
                if (moves != nullptr and i < moves->size()) {
                    auto& movi = (*moves)[i];
                    movi.TARGET_PIECE = BoardState[movi.TO_RANK][movi.TO_FILE];
                    processMove(movi, 0);
                    mat_adv -= whose_turn * movEi.TARGET_PIECE;
                    fake_turn_counter++;
                }
                else {
                    mat_adv += whose_turn * movEi.TARGET_PIECE;
                    processMoveReverse();
                    fake_turn_counter--;
                    i++;
                    for (; i < movesE->size(); i++) {
                        movEi.TARGET_PIECE = BoardState[movEi.TO_RANK][movEi.TO_FILE];
                        processMove(movEi, 0);
                        mat_adv -= whose_turn * movEi.TARGET_PIECE;
                        int8_t temp_dis = appraise_move(mat_adv);
                        if (game_conclusion != IN_PROGRESS) {
                            if (conclusion_reason == BY_CHECKMATE)
                                movEi.RESULTING_STATE = INT8_MIN;
                            else
                                movEi.RESULTING_STATE = INT8_MAX;
                        }
                        int8_t a = (temp_dis - original_adv), b = -movEi.TARGET_PIECE * whose_turn;
                        movEi.RESULTING_STATE = min(a, b);
                        mat_adv += whose_turn * movEi.TARGET_PIECE;
                        processMoveReverse();
                    }
                }
            }
            for (int i = 0; i < fake_turn_counter; i++) {
                processMoveReverse();
            }
            whose_turn = -whose_turn;
            if (whose_turn == BLACK)
                turn_count--;
            sort(movesE->begin(), movesE->end(), compareChessMovesResult);
        }
        sort(impendingDoomMoves.begin(), impendingDoomMoves.end(), compareChessMovesResult);
        // Copy impending doom elements into impendingDoomMap so that we still have access to sorted by doom moves
        for (const auto& entry : enemyAttacksNDefences) {
            // Check if the key is in impendingDoomKeys
            if (impendingDoomKeys.find(entry.first) != impendingDoomKeys.end()) {
                // Insert the key-value pair into impendingDoomMap
                impendingDoomMap[entry.first] = entry.second;
            }
        }

        auto calculateDoomImpedingCondition = [](const CHESS_MOVE& move_i, const CHESS_MOVE& doomMove_k, int whose_turn) {
            return (
                ((move_i.TO_RANK - doomMove_k.FROM_RANK) * (doomMove_k.TO_FILE - doomMove_k.FROM_FILE) ==
                    (doomMove_k.TO_RANK - doomMove_k.FROM_RANK) * (move_i.TO_FILE - doomMove_k.FROM_FILE)) &&
                (doomMove_k.MOVED_PIECE * whose_turn == 3 || doomMove_k.MOVED_PIECE * whose_turn == 5 ||
                    doomMove_k.MOVED_PIECE * whose_turn == 9) &&
                ((doomMove_k.FROM_RANK <= doomMove_k.TO_RANK) ?
                    (move_i.TO_RANK >= doomMove_k.FROM_RANK && move_i.TO_RANK <= doomMove_k.TO_RANK) :
                    (move_i.TO_RANK >= doomMove_k.TO_RANK && move_i.TO_RANK <= doomMove_k.FROM_RANK)) &&
                ((doomMove_k.FROM_FILE <= doomMove_k.TO_FILE) ?
                    (move_i.TO_FILE >= doomMove_k.FROM_FILE && move_i.TO_FILE <= doomMove_k.TO_FILE) :
                    (move_i.TO_FILE >= doomMove_k.TO_FILE && move_i.TO_FILE <= doomMove_k.FROM_FILE))
                ) ||
                (move_i.FROM_RANK == doomMove_k.TO_RANK && move_i.FROM_FILE == doomMove_k.TO_FILE) ||
                (move_i.TO_RANK == doomMove_k.FROM_RANK && move_i.TO_FILE == doomMove_k.FROM_FILE);
        };

        // Iterate through your attack keys
        for (auto& key : yourAttacksKeys) {
            // Check if the key exists in the 'attacks' map
            auto attacksIter = attNdef.find(key);
            vector<CHESS_MOVE>& moves = attacksIter->second;
            sort(moves.begin(), moves.end(), compareChessMovesMovedPiece);
            int8_t mat_adv = 0;
            auto enemyAttacksIter = enemyAttNdef.find(key);
            vector<CHESS_MOVE>* movesE = nullptr;
            if (enemyAttacksIter != enemyAttNdef.end()) { // necessary
                movesE = &enemyAttacksIter->second;
                sort(movesE->begin(), movesE->end(), compareChessMovesMovedPiece);
            }
            int8_t max_adv = INT8_MIN;
            int8_t max_i = 0;
            int8_t fake_turn_counter = 0;
            for (int8_t i = 0; i < moves.size(); i++) {
                moves[i].TARGET_PIECE = BoardState[moves[i].TO_RANK][moves[i].TO_FILE];
                processMove(moves[i], 0);
                mat_adv -= whose_turn * moves[i].TARGET_PIECE;
                fake_turn_counter++;
                int8_t temp_adv = appraise_move(mat_adv);
                if (!impendingDoomMoves.empty()) {
                    uint8_t k = 0;
                    // This is a conditional statement that checks whether a move moves[i] impedes
                    //impending doom move doomMoves[k] from achieving anything. Very shallowly.
                    //Mainly by checking whether it is on the line of attack if the attacker is a bishop, 
                    //rook or queen, and within the limits of the attack. Also whether it prevents the attack
                    //by killing the attacker or while being the target moving out of the way.
                    while (k < impendingDoomMoves.size()) {
                        auto& doomMove = impendingDoomMoves[k];
                        bool stopsDoom = calculateDoomImpedingCondition(moves[i], doomMove, whose_turn);
                        if (!stopsDoom)
                        {
                            temp_adv -= doomMove.RESULTING_STATE;
                            break;
                        }
                        k++;
                    }
                }
                if (temp_adv > max_adv) {
                    max_i = i;
                    max_adv = temp_adv;
                }
                if (movesE != nullptr and i < movesE->size()) {
                    auto& movEi = (*movesE)[i];
                    movEi.TARGET_PIECE = BoardState[movEi.TO_RANK][movEi.TO_FILE];
                    processMove(movEi, 0);
                    mat_adv -= whose_turn * movEi.TARGET_PIECE;
                    fake_turn_counter++;
                }
                else {
                    mat_adv += whose_turn * moves[i].TARGET_PIECE;
                    processMoveReverse();
                    fake_turn_counter--;
                    i++;
                    for (i; i < moves.size(); i++) {
                        moves[i].TARGET_PIECE = BoardState[moves[i].TO_RANK][moves[i].TO_FILE];
                        processMove(moves[i], 0);
                        mat_adv -= whose_turn * moves[i].TARGET_PIECE;
                        temp_adv = appraise_move(mat_adv);
                        if (!impendingDoomMoves.empty()) {
                            uint8_t k = 0;
                            while (k < impendingDoomMoves.size()) {
                                auto& doomMove = impendingDoomMoves[k];
                                bool stopsDoom = calculateDoomImpedingCondition(moves[i], doomMove, whose_turn);
                                if (!stopsDoom)
                                {
                                    temp_adv -= doomMove.RESULTING_STATE;
                                    break;
                                }
                                k++;
                            }
                        }
                        if (temp_adv > max_adv) {
                            max_i = i;
                            max_adv = temp_adv;
                        }
                        mat_adv += whose_turn * moves[i].TARGET_PIECE;
                        processMoveReverse();
                    }
                }
            }
            for (int8_t i = 0; i < fake_turn_counter; i++) {
                processMoveReverse();
            }
            bestMoves.push_back(moves[max_i]);
            if (moves[max_i].RESULTING_STATE == INT8_MAX)
                break;
        }
        // Sorting the vector based on RESULTING_STATE in descending order
        sort(bestMoves.begin(), bestMoves.end(), compareChessMovesResult);
        return bestMoves;
    }

    // Calculates the single best move in a position (by its estimation), for a given position, turn's color, and castling states
    CHESS_MOVE calculateAIMove() {
        auto LegalMoves = this->LegalMoves;

        if (LegalMoves.size() == 1)
            return LegalMoves[0];
        auto checkmate = find_if(LegalMoves.begin(), LegalMoves.end(), ResultingStateMatch(CHECKMATE));
        if (checkmate != LegalMoves.end()) {
            return *checkmate;
        }
        auto stalemate = find_if(LegalMoves.begin(), LegalMoves.end(), ResultingStateMatch(STALEMATE));
        findAttacksAndDefences(whose_turn, BoardState, attacksNDefences);
        findAttacksAndDefences(-whose_turn, BoardState, enemyAttacksNDefences);
        int current_enemy_position = appraisePosition(BoardState, -whose_turn);

        int8_t max_strength = INT8_MIN;
        vector<CHESS_MOVE> graded_fork_moves = findForks(whose_turn);
        vector<CHESS_MOVE> graded_legal_moves = resolveAttacksFast(attacksNDefences, enemyAttacksNDefences);
        vector<CHESS_MOVE> finallyGradedMoves;

        // Iterate through graded_fork_moves
        for (const auto& forkMove : graded_fork_moves) {
            // Check if the move is in graded_legal_moves
            auto legalMoveIter = find_if(graded_legal_moves.begin(), graded_legal_moves.end(), AllExceptResultingState(forkMove));

            if (legalMoveIter != graded_legal_moves.end()) {
                // Move is present in both vectors, calculate average and add to finallyGradedMoves
                CHESS_MOVE averagedMove = forkMove;
                averagedMove.RESULTING_STATE = (forkMove.RESULTING_STATE + legalMoveIter->RESULTING_STATE) / 2;
                finallyGradedMoves.push_back(averagedMove);
            }
            else {
                // Move is only in graded_fork_moves, add to finallyGradedMoves
                finallyGradedMoves.push_back(forkMove);
            }
        }

        // Iterate through graded_legal_moves
        for (const auto& legalMove : graded_legal_moves) {
            // Check if the move is not in graded_fork_moves
            auto forkMoveIter = find_if(graded_fork_moves.begin(), graded_fork_moves.end(), AllExceptResultingState(legalMove));
            if (forkMoveIter == graded_fork_moves.end()) {
                // Move is only in graded_legal_moves, add to finallyGradedMoves
                finallyGradedMoves.push_back(legalMove);
            }
            // Note: If the move is present in both vectors, it has already been processed in the first loop.
        }

        sort(finallyGradedMoves.begin(), finallyGradedMoves.end(), compareChessMovesResult);
        int8_t i = 0;
        if (graded_legal_moves.size() > 0 and max_strength < graded_legal_moves[graded_legal_moves.size() - 1].RESULTING_STATE)
            max_strength = finallyGradedMoves[i].RESULTING_STATE;
        auto result = LegalMoves.end();
        while (i >= 0) {
            CHESS_MOVE best_move = finallyGradedMoves[i];
            if (max_strength <= current_enemy_position) {
                if (stalemate != LegalMoves.end()) {
                    return *stalemate;
                }
            }
            result = find_if(LegalMoves.begin(), LegalMoves.end(), AllExceptResultingState(best_move));
            if (result != LegalMoves.end())
                return *result;
            i++;
        }
        CHESS_MOVE resign = { -3, -3, -3, -3, -3, -3, -3 }; // will later parse this as resign
        return resign;
    }

    //############################################################################################################
    //JUST FOR TESTING############################################################################################
    void printBoard() {
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                cout << static_cast<int>(BoardState[i][j]) << ' ';
            }
            cout << endl;
        }
    }
    //MOVE TRANSLATOR#############################################################################################
    // As name suggests, this function translates a move from UCI standard to mine.
    CHESS_MOVE translateNprocessFromUCI(string uciMove, bool boardless) {
        CHESS_MOVE native_move;
        if (uciMove.length() > 5)
            return ILLEGAL_CHESS_MOVE;
        native_move.FROM_FILE = static_cast<int8_t>(uciMove[0] - 'a' + 1);
        native_move.FROM_RANK = static_cast<int8_t>(uciMove[1] - '0');
        native_move.MOVED_PIECE = BoardState[native_move.FROM_RANK][native_move.FROM_FILE];
        native_move.TO_FILE = static_cast<int8_t>(uciMove[2] - 'a' + 1);
        native_move.TO_RANK = static_cast<int8_t>(uciMove[3] - '0');
        native_move.TARGET_PIECE = BoardState[native_move.TO_RANK][native_move.TO_FILE];
        if (uciMove.length() == 5) {
            int8_t& res_ref = native_move.RESULTING_STATE;
            switch (uciMove[4]) {
            default:
            q: res_ref = 90;
                break;
            r: res_ref = 50;
                break;
            k: res_ref = 40;
                break;
            b: res_ref = 30;
                break;
            }
            if (boardless) {
                native_move.RESULTING_STATE = 0;
                return native_move;
            }
            native_move.RESULTING_STATE = 0;
            calculateAllLegalMoves(LegalMoves, false);
            auto legal_move_iter = find_if(LegalMoves.begin(), LegalMoves.end(), AllExceptResultingState(native_move));
            if (legal_move_iter != LegalMoves.end()) {
                native_move.RESULTING_STATE += legal_move_iter->RESULTING_STATE % 10;
            }
            else
            {
                return ILLEGAL_CHESS_MOVE;
            }
            return native_move;
        }
        if (boardless) {
            native_move.RESULTING_STATE = 0;
            return native_move;
        }
        native_move.RESULTING_STATE = 0;
        calculateAllLegalMoves(LegalMoves, false);
        auto legal_move_iter = find_if(LegalMoves.begin(), LegalMoves.end(), AllExceptResultingState(native_move));
        if (legal_move_iter != LegalMoves.end()) {
            native_move.RESULTING_STATE = legal_move_iter->RESULTING_STATE;
        }
        else
        {
            return ILLEGAL_CHESS_MOVE;
        }
        return native_move;
    }
};

void save_game(const CHESS_GAME& game, const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Unable to open file for writing");
    }
    ofs.write(reinterpret_cast<const char*>(&game), sizeof(game));
}

void load_game(CHESS_GAME& game, const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Unable to open file for reading");
    }
    ifs.read(reinterpret_cast<char*>(&game), sizeof(game));
}


int main() {
    CHESS_GAME game;
    string input;
    //SCENARIO 1: M4114300 - M4614400 - M3095200 - M1740500 - M5032300 - M5732400 - M5295613 (checkmate) : scholar's mate works
    //SCENARIO 2: M4114300 - M1740500 - M4314400 - M3613400 - M4413510 (enpassant) - M4614500 - M0110300 - M3795500 - M3513601 (check) - M47k4600 - M361370q (promotion)
    bool move_made_flag = true;
    int k = 1;
    game.printBoard();
    bool flag = false;
    do {
        if (move_made_flag) {
            if (k == 2) {
                //cout << k << endl;
            }
            game.calculateAllLegalMoves(game.LegalMoves);
            move_made_flag = false;
        }
        if (!flag)
            cin >> input;
        if (input == "BOT") {
            flag = true;
            CHESS_MOVE move = game.calculateAIMove();
            game.calculateAllLegalMoves(game.LegalMoves);
            if (game.processMove(move))
            {
                cout << "BOT MOVE " << k << endl;
                k++;
                move_made_flag = true;
            }
            else
            {
                cout << "BOT MOVE FAILED" << endl;
                game.printBoard();
            }
        }
        else if (input[0] == 'M') {
            if (input[3] == 'k') {
                input[3] = '0' + 10;
            }
            if (input[7] == 'q') {
                input[7] = '0' + 90;
            }
            if (input[7] == 'r') {
                input[7] = '0' + 50;
            }
            if (input[7] == 'k') {
                input[7] = '0' + 40;
            }
            if (input[7] == 'b') {
                input[7] = '0' + 30;
            }
            CHESS_MOVE move{ static_cast<int8_t>(input[1] - '0'), static_cast<int8_t>(input[2] - '0'), static_cast<int8_t>(input[3] - '0') * game.whose_turn, static_cast<int8_t>(input[4] - '0'), static_cast<int8_t>(input[5] - '0'), static_cast<int8_t>(input[6] - '0') * (-game.whose_turn), static_cast<int8_t>(input[7] - '0') };
            if (game.processMove(move))
            {
                cout << "PLAYER MOVE " << k << endl;
                k++;
                move_made_flag = true;
            }
            else
                cout << "PLAYER MOVE FAILED" << endl;
        }
        game.printBoard();
    } while (input != "exit" and game.game_conclusion == -2);
    switch (game.game_conclusion) {
    case WHITE_WON: cout << "WHITE WON BY ";
        break;
    case BLACK_WON: cout << "BLACK WON BY ";
        break;
    case DRAW: cout << "DRAW BECAUSE OF ";
        break;
    default:
        cout << "GAME INTERRUPTED" << endl;
        break;
    }
    switch (game.conclusion_reason) {
    case BY_CHECKMATE: cout << "CHECKMATE";
        break;
    case BY_STALEMATE: cout << "STALEMATE";
        break;
    case BY_DEAD_POSITION: cout << "DEAD POSITION";
        break;
    case BY_50MOVE_RULE: cout << "50 MOVE RULE";
        break;
    case BY_THREEFOLD_REPETITION: cout << "THREEFOLD REPETITION";
        break;
    default:
        cout << ", GAME NOT CONCLUDED" << endl;
        break;
    }
    return 0;
}